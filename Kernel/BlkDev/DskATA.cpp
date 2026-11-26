#include "../KernelB.hpp"

constexpr uint8_t ATA_NOP = 0, ATA_PACKET = 0xA0, ATA_ID_DEV = 0xEC;
constexpr uint8_t ATAS_ERR = 1, ATAS_DRQ = 8, ATAS_DF = 0x20, ATAS_BSY = 0x80;
constexpr uint16_t ATAR0_DATA = 0, ATAR0_FEAT = 1, ATAR0_LCYL = 4,
                   ATAR0_HCYL = 5, ATAR0_SEL = 6, ATAR0_STAT = 7, ATAR0_CMD = 7;

static uint64_t ATANow()
{
  return cnts.HPET_freq ? HPET() : SysTimerRead();
}

static uint64_t ATADeadline(uint64_t milliseconds)
{
  uint64_t rate = cnts.HPET_freq ? cnts.HPET_freq : SYS_TIMER_FREQ;
  return ATANow() + (milliseconds / 1000) * rate +
         (milliseconds % 1000) * rate / 1000;
}

static void ATASel(const CBlkDev &bd)
{
  OutU8(bd.base0 + ATAR0_SEL, 0xE0 | (bd.unit << 4));
  for (unsigned i = 0; i < 4; i++)
    InU8(bd.base0 + ATAR0_STAT);
}

static void ATACmd(const CBlkDev &bd, uint8_t cmd)
{
  OutU8(bd.base0 + ATAR0_FEAT, 0);
  OutU8(bd.base0 + ATAR0_CMD, cmd);
  InU8(0x21);
}

static int ATAWait(const CBlkDev &bd, uint64_t deadline, uint8_t mask)
{
  do {
    for (unsigned i = 0; i < 3; i++) {
      uint8_t status = InU8(bd.base0 + ATAR0_STAT);
      if (status == 0xFF)
        return -1;
      if (!(status & ATAS_BSY) &&
          (!mask || (status & (mask | ATAS_ERR | ATAS_DF))))
        return status;
    }
    asm volatile("pause");
  } while (ATANow() < deadline);
  return -1;
}

static bool ATAWaitDRQ(const CBlkDev &bd, uint64_t deadline)
{
  int status = ATAWait(bd, deadline, ATAS_DRQ);
  return status >= 0 && (status & (ATAS_DRQ | ATAS_ERR | ATAS_DF)) == ATAS_DRQ;
}

static bool ATAGetRes(const CBlkDev &bd, uint64_t deadline, uint8_t *buf,
                      size_t bytes, uint16_t fixed_avail = 0)
{
  uint16_t port = bd.base0 + ATAR0_DATA;
  while (bytes) {
    if (!ATAWaitDRQ(bd, deadline))
      return false;
    size_t avail = fixed_avail;
    if (!avail) {
      uint16_t hi = InU8(bd.base0 + ATAR0_HCYL);
      avail = (hi << 8) | InU8(bd.base0 + ATAR0_LCYL);
      if (!avail)
        return false;
    }
    size_t take = avail < bytes ? avail : bytes;
    auto *dest = buf;
    if (!(take & 3)) {
      size_t count = take / 4;
      asm volatile("rep insl" : "+D"(dest), "+c"(count) : "d"(port) : "memory");
    } else {
      size_t count = take / 2;
      asm volatile("rep insw" : "+D"(dest), "+c"(count) : "d"(port) : "memory");
      if (take & 1)
        *dest = InU16(port);
    }
    for (size_t i = (take + 1) / 2; i < (avail + 1) / 2; i++)
      InU16(port);
    buf += take;
    bytes -= take;
  }
  int status = ATAWait(bd, deadline, 0);
  return status >= 0 && !(status & (ATAS_DRQ | ATAS_ERR | ATAS_DF));
}

uint32_t ATAProbe(const CBlkDev &bd)
{
  ATASel(bd);
  uint64_t deadline = ATADeadline(100);
  if (ATAWait(bd, deadline, 0) <= 0)
    return BDT_NULL;
  ATACmd(bd, ATA_ID_DEV);
  int status = ATAWait(bd, deadline, 0);
  if (status < 0)
    return BDT_NULL;
  if (status & ATAS_ERR) {
    uint8_t lo = InU8(bd.base0 + ATAR0_LCYL);
    uint8_t hi = InU8(bd.base0 + ATAR0_HCYL);
    return lo == 0x14 && hi == 0xEB ? BDT_ATAPI : BDT_NULL;
  }
  uint8_t id[BLK_SIZE];
  return (status & ATAS_DRQ) &&
                 ATAGetRes(bd, deadline, id, sizeof(id), sizeof(id))
             ? BDT_ATA
             : BDT_NULL;
}

bool ATAReadCapacity(CBlkDev &bd)
{
  ATASel(bd);
  uint64_t deadline=ATADeadline(1000);
  if (ATAWait(bd,deadline,0)<=0)
    return false;
  ATACmd(bd,ATA_ID_DEV);
  uint16_t id[256];
  if (!ATAGetRes(bd,deadline,reinterpret_cast<uint8_t *>(id),sizeof(id),sizeof(id)) ||
      !(id[49] & (1<<9)))
    return false;
  uint32_t sectors=uint32_t(id[60]) | uint32_t(id[61])<<16;
  if (!sectors)
    return false;
  bd.max_blk=(sectors>0x10000000?0x10000000:sectors)-1;
  bd.blk_size=BLK_SIZE;
  return true;
}

static bool ATAReadBlks(const CBlkDev &bd,uint8_t *buf,uint32_t lba,uint8_t cnt)
{
  if (!cnt || uint64_t(lba)+cnt>0x10000000 ||
      uint64_t(lba)+cnt>uint64_t(bd.max_blk)+1)
    return false;
  ATASel(bd);
  uint64_t deadline=ATADeadline(1000+uint64_t(cnt)*10);
  if (ATAWait(bd,deadline,0)<=0)
    return false;
  OutU8(bd.base0+ATAR0_SEL,0xE0 | bd.unit<<4 | (lba>>24));
  OutU8(bd.base0+2,cnt);
  OutU8(bd.base0+3,lba);
  OutU8(bd.base0+4,lba>>8);
  OutU8(bd.base0+5,lba>>16);
  ATACmd(bd,0x20);
  if (!ATAGetRes(bd,deadline,buf,size_t(cnt)*BLK_SIZE,BLK_SIZE))
    return false;
  __atomic_add_fetch(&blkdev.read_cnt,cnt,__ATOMIC_RELAXED);
  return true;
}

bool ATAReadBytes(const CBlkDev &bd,uint8_t *buf,uint64_t pos,size_t bytes)
{
  uint64_t limit=(uint64_t(bd.max_blk)+1)*BLK_SIZE;
  if (pos>limit || bytes>limit-pos)
    return false;
  uint8_t sector[BLK_SIZE];
  while (bytes) {
    size_t offset=pos%BLK_SIZE,take;
    if (!offset && bytes>=BLK_SIZE) {
      auto cnt=bytes/BLK_SIZE>128?128:bytes/BLK_SIZE;
      if (!ATAReadBlks(bd,buf,pos/BLK_SIZE,cnt))
        return false;
      take=cnt*BLK_SIZE;
    } else {
      if (!ATAReadBlks(bd,sector,pos/BLK_SIZE,1))
        return false;
      take=BLK_SIZE-offset;
      if (take>bytes)
        take=bytes;
      MemCpy(buf,sector+offset,take);
    }
    pos+=take;
    buf+=take;
    bytes-=take;
  }
  return true;
}

static bool ATAPIWritePktWord(const CBlkDev &bd, uint64_t deadline,
                              const uint16_t (&packet)[6])
{
  for (uint16_t word : packet) {
    if (!ATAWaitDRQ(bd, deadline))
      return false;
    OutU16(bd.base0 + ATAR0_DATA, __builtin_bswap16(word));
  }
  return true;
}

static bool ATAPIStartStop(const CBlkDev &bd, uint64_t deadline, bool start)
{
  ATASel(bd);
  ATACmd(bd, ATA_PACKET);
  const uint16_t pkt[6]{0x1B00, 0, uint16_t(start ? 0x100 : 0), 0, 0, 0};
  return ATAPIWritePktWord(bd, deadline, pkt) && ATAWait(bd, deadline, 0) >= 0;
}

bool ATAPIStartStop(const CBlkDev &bd, bool start)
{
  return ATAPIStartStop(bd, ATADeadline(5000), start);
}

static bool ATAPIWaitReady(const CBlkDev &bd, uint64_t deadline)
{
  ATASel(bd);
  if (ATAWait(bd, deadline, 0) < 0)
    return false;
  ATACmd(bd, ATA_NOP);
  return ATAWait(bd, deadline, 0) >= 0 && ATAPIStartStop(bd, deadline, true);
}

bool ATAPIReadCapacity(CBlkDev &bd)
{
  uint64_t deadline = ATADeadline(7000);
  ATASel(bd);
  if (ATAWait(bd, deadline, 0) < 0)
    return false;
  uint32_t data[2];
  OutU8(bd.base0 + ATAR0_LCYL, sizeof(data));
  OutU8(bd.base0 + ATAR0_HCYL, 0);
  ATACmd(bd, ATA_PACKET);
  const uint16_t pkt[6]{0x2500, 0, 0, 0, 0, 0};
  if (!ATAPIWritePktWord(bd, deadline, pkt) ||
      !ATAGetRes(bd, deadline, reinterpret_cast<uint8_t *>(data), sizeof(data)))
    return false;
  bd.blk_size = __builtin_bswap32(data[1]);
  if (bd.blk_size != DVD_BLK_SIZE)
    return false;
  uint64_t sectors = uint64_t{__builtin_bswap32(data[0])} + 1;
  bd.max_blk = sectors * (DVD_BLK_SIZE / BLK_SIZE) - 1;
  return true;
}

bool ATAPIReadBlks2(const CBlkDev &bd, uint8_t *buf, uint32_t lba,
                    uint32_t count)
{
  if (!count || uint64_t{lba} + count - 1 > UINT32_MAX)
    return false;
  uint64_t deadline = ATADeadline(7000 + uint64_t{count} * 4);
  if (!ATAPIWaitReady(bd, deadline))
    return false;
  ATASel(bd);
  OutU8(bd.base0 + ATAR0_LCYL, bd.blk_size & 255);
  OutU8(bd.base0 + ATAR0_HCYL, bd.blk_size >> 8);
  ATACmd(bd, ATA_PACKET);
  const uint16_t pkt[6]{0xA800,          uint16_t(lba >> 16),
                        uint16_t(lba),   uint16_t(count >> 16),
                        uint16_t(count), 0};
  if (!ATAPIWritePktWord(bd, deadline, pkt) ||
      !ATAGetRes(bd, deadline, buf, size_t{count} * bd.blk_size))
    return false;
  __atomic_add_fetch(&blkdev.read_cnt, uint64_t{count} * bd.blk_size / BLK_SIZE,
                     __ATOMIC_RELAXED);
  return true;
}

bool ATAPIReadBytes(const CBlkDev &bd, uint8_t *buf, uint64_t pos, size_t bytes)
{
  uint64_t limit = (uint64_t(bd.max_blk) + 1) * BLK_SIZE;
  if (pos > limit || bytes > limit - pos)
    return false;
  uint8_t sector[DVD_BLK_SIZE];
  while (bytes) {
    uint32_t lba = pos / DVD_BLK_SIZE;
    size_t offset = pos % DVD_BLK_SIZE;
    size_t take, left = DVD_BLK_SIZE - offset;
    if (!offset && bytes >= DVD_BLK_SIZE) {
      size_t sectors = bytes / DVD_BLK_SIZE;
      uint32_t count = sectors < 0x800 ? sectors : 0x800;
      if (!ATAPIReadBlks2(bd, buf, lba, count))
        return false;
      take = size_t{count} * DVD_BLK_SIZE;
    } else {
      if (!ATAPIReadBlks2(bd, sector, lba, 1))
        return false;
      take = bytes < left ? bytes : left;
      MemCpy(buf, sector + offset, take);
    }
    bytes -= take;
    pos += take;
    buf += take;
  }
  return true;
}
