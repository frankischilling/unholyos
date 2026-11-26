#include "../KernelB.hpp"

#include <bit>

constexpr int64_t FONT_WIDTH = 8, FONT_HEIGHT = 8;

static constexpr uint64_t F64(double value)
{
  return std::bit_cast<uint64_t>(value);
}

static int64_t Trunc(uint64_t bits)
{
  return F64ToI64(bits, 0x0C00);
}

static int64_t ScalePos(int64_t pos, uint64_t scale)
{
  return Trunc(F64MulBits(F64FromI64(pos), scale));
}

static int64_t SnapPos(uint64_t pos, uint64_t spacing, uint64_t offset)
{
  auto grid = Trunc(F64DivBits(pos, spacing));
  return Trunc(F64AddBits(F64MulBits(F64FromI64(grid), spacing), offset));
}

extern "C" void GridInitCpp()
{
  ms_grid.x = ms_grid.y = ms_grid.z = F64(8.0);
  ms_grid.x_offset = ms_grid.y_offset = ms_grid.z_offset = 0;
  ms_grid.x_speed = ms_grid.y_speed = ms_grid.z_speed = F64(1.0);
  ms_grid.show = ms_grid.snap = ms_grid.coord = false;
}

void MsUpdate(int64_t x, int64_t y, int64_t z, bool l, bool r)
{
  ms.presnap.x = ScalePos(x, ms.scale.x) + ms.offset.x;
  ms.presnap.y = ScalePos(y, ms.scale.y) + ms.offset.y;
  ms.presnap.z = ScalePos(z, ms.scale.z) + ms.offset.z;
  if (ms_grid.snap) {
    ms.pos.x = SnapPos(F64FromI64(ms.presnap.x), ms_grid.x, ms_grid.x_offset);
    ms.pos.y = SnapPos(F64FromI64(ms.presnap.y), ms_grid.y, ms_grid.y_offset);
    ms.pos.z = SnapPos(F64FromI64(ms.presnap.z), ms_grid.z, ms_grid.z_offset);
  } else
    ms.pos = ms.presnap;

  if (ms.pos.x < 0)
    ms.pos.x = 0;
  else if (ms.pos.x >= GR_WIDTH)
    ms.pos.x = GR_WIDTH - 1;
  if (ms.pos.y < 0)
    ms.pos.y = 0;
  else if (ms.pos.y >= GR_HEIGHT)
    ms.pos.y = GR_HEIGHT - 1;
  ms.pos_text.x = ms.pos.x / FONT_WIDTH;
  ms.pos_text.y = ms.pos.y / FONT_HEIGHT;
  ms.lb = l;
  ms.rb = r;
  kbd.scan_code = (kbd.scan_code & ~int64_t(SCF_MS_L_DOWN | SCF_MS_R_DOWN)) |
                  (l ? SCF_MS_L_DOWN : 0) | (r ? SCF_MS_R_DOWN : 0);
}

static void MsHardSet(int64_t x, int64_t y, int64_t z, bool l, bool r);

extern "C" void MsSetCpp(int64_t x, int64_t y, int64_t z, int64_t l, int64_t r)
{
  if (x < 0 || x >= GR_WIDTH)
    x = ms.pos.x;
  if (y < 0 || y >= GR_HEIGHT)
    y = ms.pos.y;
  if (z == INT64_MAX)
    z = ms.pos.z;
  if (l < 0 || l > 1)
    l = ms.lb;
  if (r < 0 || r > 1)
    r = ms.rb;

  x = Trunc(F64DivBits(F64FromI64(x - ms.offset.x), ms.scale.x));
  y = Trunc(F64DivBits(F64FromI64(y - ms.offset.y), ms.scale.y));
  z = Trunc(F64DivBits(F64FromI64(z - ms.offset.z), ms.scale.z));
  MsUpdate(x, y, z, l, r);
  MsHardSet(x, y, z, l, r);
}

void MsInit()
{
  ms.scale.x = ms.scale.y = ms.scale.z = F64(1.0);
  ms.show = true;
  ms.timestamp = GetTSC();
  ms.dbl_time = F64(0.350);
  GridInitCpp();
}

bool KbdMsInit()
{
  auto *kbd_fifo = FifoU8New(8);
  auto *kbd_fifo2 = FifoU8New(0x1000);
  auto *scan_fifo = FifoI64New(0x1000);
  auto *ms_fifo = FifoU8New(8);
  auto *ms_fifo2 = FifoU8New(0x1000);
  if (!kbd_fifo || !kbd_fifo2 || !scan_fifo || !ms_fifo || !ms_fifo2) {
    FifoU8Del(kbd_fifo);
    FifoU8Del(kbd_fifo2);
    FifoI64Del(scan_fifo);
    FifoU8Del(ms_fifo);
    FifoU8Del(ms_fifo2);
    return false;
  }
  auto flags = GetRFlags();
  asm volatile("cli" : : : "memory");
  MemSet(&kbd, 0, sizeof(kbd));
  kbd.fifo = kbd_fifo;
  kbd.fifo2 = kbd_fifo2;
  kbd.scan_code_fifo = scan_fifo;
  MemSet(&ms_hard, 0, sizeof(ms_hard));
  ms_hard.fifo = ms_fifo;
  ms_hard.fifo2 = ms_fifo2;
  ms_hard.scale.x = ms_hard.scale.y = F64(0.5);
  ms_hard.scale.z = F64(1.0);
  ms_hard.prescale.x = GR_WIDTH;
  ms_hard.prescale.y = GR_HEIGHT;
  ms_hard.pos.x = GR_WIDTH / 2;
  ms_hard.pos.y = GR_HEIGHT / 2;
  MemCpy(&ms_hard_last, &ms_hard, sizeof(ms_hard));
  SetRFlags(flags);
  return true;
}

void MsHardStageByte(uint8_t raw_byte)
{
  if (!ms_hard.installed)
    return;
  auto now = GetTSC();
  auto &packet = *ms_hard.fifo;
  if (now - uint64_t(ms_hard.timestamp) > uint64_t(cnts.time_stamp_freq) / 8)
    packet.out_ptr = packet.in_ptr;
  ms_hard.timestamp = now;
  FifoIns(packet, raw_byte);
  if (FifoCnt(packet) == ms_hard.pkt_size) {
    auto &raw = *ms_hard.fifo2;
    bool space = raw.mask - FifoCnt(raw) >= ms_hard.pkt_size;
    while (FifoRem(packet, raw_byte))
      if (space)
        FifoIns(raw, raw_byte);
  }
}

extern "C" void IRQMsHard();
extern "C" void IRQMsHardCpp()
{
  OutU8(0xA0, 0x20);
  OutU8(0x20, 0x20);
  ms_hard.irqs_working = true;
  Ps2PktRead();
}

static bool KbdMsCmdAck(uint8_t value)
{
  for (unsigned retry = 0; retry < 5; ++retry) {
    uint8_t reply;
    if (KbdCmdSend(KBD_CTRL, 0xD4) && KbdCmdSend(KBD_PORT, value) &&
        KbdCmdRead(reply, true) && reply == 0xFA)
      return true;
  }
  return false;
}

static bool MsHardGetType()
{
  uint8_t type;
  if (!KbdMsCmdAck(0xF2) || !KbdCmdRead(type, true))
    return false;
  if (type == 3)
    ms_hard.has_wheel = true;
  else if (type == 4)
    ms_hard.has_ext_bttns = true;
  return true;
}

static bool MsHardRst()
{
  uint8_t reply;
  bool okay = KbdCmdSend(KBD_CTRL, 0xAD) && KbdCmdSend(KBD_CTRL, 0xA8) &&
              KbdMsCmdAck(0xFF) && KbdCmdRead(reply, true) && reply == 0xAA &&
              KbdCmdRead(reply, true);
  if (okay)
    okay = KbdMsCmdAck(0xF3) && KbdMsCmdAck(200) &&
           KbdMsCmdAck(0xF3) && KbdMsCmdAck(100) &&
           KbdMsCmdAck(0xF3) && KbdMsCmdAck(80) && MsHardGetType() &&
           KbdMsCmdAck(0xF3) && KbdMsCmdAck(10) && MsHardGetType() &&
           KbdMsCmdAck(0xE8) && KbdMsCmdAck(3) && KbdMsCmdAck(0xE6) &&
           KbdMsCmdAck(0xF3) && KbdMsCmdAck(100) && KbdMsCmdAck(0xF4) &&
           KbdCmdSend(KBD_CTRL, 0x20) && KbdCmdRead(reply) &&
           KbdCmdSend(KBD_CTRL, 0x60) &&
           KbdCmdSend(KBD_PORT, (reply | 2) & ~0x20);

  KbdCmdSend(KBD_CTRL, 0xAE);
  if (!okay)
    KbdCmdSend(KBD_CTRL, 0xA7);
  ms_hard.pkt_size = ms_hard.has_wheel || ms_hard.has_ext_bttns ? 4 : 3;
  return okay;
}

static void MsHardSpeedSet()
{
  auto dx = uint64_t(ms_hard_last.pos.x) - uint64_t(ms_hard.pos.x);
  auto dy = uint64_t(ms_hard_last.pos.y) - uint64_t(ms_hard.pos.y);
  auto dd = int64_t(dx * dx + dy * dy);
  auto elapsed = ms_hard.timestamp - ms_hard_last.timestamp;
  if (dd && elapsed)
    ms_hard.speed = F64DivBits(F64MulBits(F64SqrtBits(F64FromI64(dd)),
                                         F64FromI64(cnts.time_stamp_freq)),
                                F64FromI64(elapsed));
  ms_hard_last.timestamp = ms_hard.timestamp;
}

static void MsHardSetPre()
{
  auto timestamp = ms_hard_last.timestamp;
  MemCpy(&ms_hard_last, &ms_hard, sizeof(ms_hard));
  ms_hard_last.timestamp = timestamp;
}

static int64_t MsHardScale(int64_t pos, uint64_t scale, uint64_t speed)
{
  return Trunc(F64MulBits(F64MulBits(F64FromI64(pos), scale), speed));
}

static void MsHardSetPost()
{
  ms_hard.pos.x = MsHardScale(ms_hard.prescale.x, ms_hard.scale.x, ms_grid.x_speed);
  ms_hard.pos.y = MsHardScale(ms_hard.prescale.y, ms_hard.scale.y, ms_grid.y_speed);
  ms_hard.pos.z = MsHardScale(ms_hard.prescale.z, ms_hard.scale.z, ms_grid.z_speed);

  auto x = SnapPos(F64MulBits(F64FromI64(ms_hard.pos.x), ms.scale.x),
                   ms_grid.x, F64FromI64(ms.offset.x));
  if (x < 0)
    ms.offset.x -= x;
  else if (x >= GR_WIDTH)
    ms.offset.x += GR_WIDTH - 1 - x;
  auto y = SnapPos(F64MulBits(F64FromI64(ms_hard.pos.y), ms.scale.y),
                   ms_grid.y, F64FromI64(ms.offset.y));
  if (y < 0)
    ms.offset.y -= y;
  else if (y >= GR_HEIGHT)
    ms.offset.y += GR_HEIGHT - 1 - y;

  if (ms_hard.pos.x != ms_hard_last.pos.x || ms_hard.pos.y != ms_hard_last.pos.y ||
      ms_hard.pos.z != ms_hard_last.pos.z) {
    ms_hard.evt = true;
    MsHardSpeedSet();
  } else
    for (unsigned i = 0; i < 5; ++i)
      if (ms_hard.bttns[i] != ms_hard_last.bttns[i]) {
        ms_hard.evt = true;
        break;
      }
}

void MsHardHndlr()
{
  auto pkt_size = ms_hard.pkt_size;
  if (!ms_hard.installed || (pkt_size != 3 && pkt_size != 4))
    return;
  while (FifoCnt(*ms_hard.fifo2) >= pkt_size) {
    MsHardSetPre();
    uint8_t buf[4]{};
    for (int64_t i = 0; i < pkt_size; ++i)
      FifoRem(*ms_hard.fifo2, buf[i]);

    ms_hard.bttns[0] = buf[0] & 1;
    ms_hard.bttns[1] = buf[0] & 2;
    ms_hard.bttns[2] = buf[0] & 4;
    ms_hard.bttns[3] = buf[3] & 0x10;
    ms_hard.bttns[4] = buf[3] & 0x20;
    int64_t dx = int64_t(buf[1]) - (buf[0] & 0x10 ? 256 : 0);
    int64_t dy = (buf[0] & 0x20 ? 256 : 0) - int64_t(buf[2]);
    int64_t dz = int64_t(buf[3] & 7) - (buf[3] & 8 ? 8 : 0);
    ms_hard.prescale.x += dx;
    ms_hard.prescale.y += dy;
    ms_hard.prescale.z += dz;
    MsHardSetPost();
  }
}

static int64_t MsHardUnscale(int64_t pos, uint64_t scale, uint64_t speed)
{
  return Trunc(F64DivBits(F64DivBits(F64FromI64(pos), scale), speed));
}

static void MsHardSet(int64_t x, int64_t y, int64_t z, bool l, bool r)
{
  ms_hard.timestamp = GetTSC();
  MsHardSetPre();
  ms_hard.prescale.x = MsHardUnscale(x, ms_hard.scale.x, ms_grid.x_speed);
  ms_hard.prescale.y = MsHardUnscale(y, ms_hard.scale.y, ms_grid.y_speed);
  ms_hard.prescale.z = MsHardUnscale(z, ms_hard.scale.z, ms_grid.z_speed);
  ms_hard.bttns[0] = l;
  ms_hard.bttns[1] = r;
  MsHardSetPost();
}

bool MsHardDrvrInstall()
{
  auto flags = GetRFlags();
  asm volatile("cli" : : : "memory");
  ms_hard.install_in_progress = true;
  OutU8(0xA1, InU8(0xA1) | 0x10);
  ms_hard.installed = ms_hard.irqs_working = false;
  IntEntrySet(I_MS, IRQMsHard);
  for (unsigned i = 0; i < 5; ++i)
    ms_hard.bttns[i] = false;
  bool okay = MsHardRst();
  if (okay)
    OutU8(0xA1, InU8(0xA1) & ~0x10);
  ++ms_hard.install_attempts;
  ms_hard.installed = ms_hard.evt = okay;
  ms_hard.install_in_progress = false;
  SetRFlags(flags);
  return okay;
}
