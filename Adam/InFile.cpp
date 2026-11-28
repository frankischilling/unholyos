#include "DolDoc/DolDoc.hpp"

extern "C" bool LineCpp(uintptr_t aux, int64_t x1, int64_t y1, int64_t z1,
                         int64_t x2, int64_t y2, int64_t z2, uintptr_t fp_plot,
                         int64_t step, int64_t start);
extern "C" void InSetMsPlotHC();

extern "C" void InGetStrCpp(const uint8_t *st)
{
  int64_t ch, sc;
  char buf[256];
  while (*st) {
    ch = GetKeyCpp(&sc, false, false);
    const auto key = uint8_t(sc);
    if (key != SC_SHIFT && key != SC_ALT && key != SC_CTRL) {
      if (ch == *st) {
        PutKeyCpp(ch, 0);
        st++;
      } else {
        auto *st2 = Char2KeyNameCpp(*st, true);
        const uint64_t arg = reinterpret_cast<uintptr_t>(st2);
        StrPrintJoin(buf, "Press the $GREEN$<%s>$FG$ key.", 1, &arg);
        Free(st2);
        PopUpOkCpp(buf, nullptr);
      }
    }
  }
}

extern "C" void InPrintCpp(int64_t mS, const char *fmt, int64_t argc,
                            const uint64_t *argv)
{
  auto *buf = StrPrintJoin(nullptr, fmt, argc, argv);
  const auto *st = reinterpret_cast<const uint8_t *>(buf);
  while (int64_t ch = *st++) {
    PutKeyCpp(ch, 0);
    SleepCpp(mS);
  }
  Free(buf);
}

extern "C" void InGetKeyCpp(int64_t scan_code, int64_t sc_mask)
{
  int64_t sc, ch;
  char buf[144];
  do {
    ch = GetKeyCpp(&sc, false, false);
    const auto key = uint8_t(sc);
    if (key != SC_PRTSCRN1 &&
        !(key == SC_SHIFT && (scan_code & SCF_SHIFT)) &&
        !(key == SC_CTRL && (scan_code & SCF_CTRL)) &&
        !(key == SC_ALT && (scan_code & SCF_ALT))) {
      if ((sc & sc_mask) != (scan_code & sc_mask)) {
        auto *st = ScanCode2KeyNameCpp(scan_code);
        const uint64_t arg = reinterpret_cast<uintptr_t>(st);
        StrPrintJoin(buf, "Press the $GREEN$<%s>$FG$ key", 1, &arg);
        Free(st);
        PopUpOkCpp(buf, nullptr);
      }
    }
  } while ((sc & sc_mask) != (scan_code & sc_mask));
  MsgCpp(MSG_KEY_DOWN, ch, sc, 0);
}

extern "C" int64_t InGetCharCpp(int64_t argc, const int64_t *argv)
{
  int64_t sc, ch;
  while (true) {
    ch = GetKeyCpp(&sc, false, false);
    const auto key = uint8_t(sc);
    if (key != SC_SHIFT && key != SC_ALT && key != SC_CTRL) {
      for (int64_t i = 0; i < argc; i++)
        if (ch == argv[i]) {
          MsgCpp(MSG_KEY_DOWN, ch, sc, 0);
          return ch;
        }
      // Each key label and its DolDoc markup fit in 32 bytes.
      auto *buf = reinterpret_cast<char *>(
          MAlloc(8 + argc * 32, CurrentTask()->data_heap));
      if (!buf)
        RaiseHolyC(0x6D654D74754FULL); // OutMem
      StrCpyCpp(buf, "Press ");
      for (int64_t i = 0; i < argc; i++) {
        auto *st = Char2KeyNameCpp(argv[i], true);
        const uint64_t arg = reinterpret_cast<uintptr_t>(st);
        CatPrintCpp(buf, "$GREEN$<%s>$FG$", 1, &arg);
        Free(st);
        if (argc == i + 1)
          CatPrintCpp(buf, ".", 0, nullptr);
        else if (argc == i + 2)
          CatPrintCpp(buf, " or ", 0, nullptr);
        else
          CatPrintCpp(buf, ", ", 0, nullptr);
      }
      PopUpOkCpp(buf, nullptr);
      Free(buf);
    }
  }
}

extern "C" void InUntilKeyCpp(int64_t scan_code, int64_t sc_mask)
{
  int64_t sc, ch;
  do {
    ch = GetKeyCpp(&sc, false, false);
    MsgCpp(MSG_KEY_DOWN, ch, sc, 0);
  } while ((sc & sc_mask) != (scan_code & sc_mask));
}

extern "C" int64_t InUntilCharCpp(int64_t argc, const int64_t *argv)
{
  int64_t sc, ch;
  while (true) {
    ch = GetKeyCpp(&sc, false, false);
    MsgCpp(MSG_KEY_DOWN, ch, sc, 0);
    for (int64_t i = 0; i < argc; i++)
      if (ch == argv[i])
        return ch;
  }
}

extern "C" bool InViewCpp()
{
  const bool res = ViewCpp();
  DocBottomCpp(nullptr);
  return res;
}

static int64_t in_plot_l, in_plot_r;

extern "C" bool InSetMsPlotCpp(int64_t mS, int64_t x, int64_t y, int64_t z)
{
  MsSetCpp(x, y, z, in_plot_l, in_plot_r);
  SleepCpp(mS);
  return true;
}

extern "C" void InSetMsCpp(int64_t mS, int64_t x, int64_t y, int64_t z,
                            int64_t l, int64_t r)
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
  in_plot_l = l;
  in_plot_r = r;
  LineCpp(mS, ms.pos.x, ms.pos.y, ms.pos.z, x, y, z,
          reinterpret_cast<uintptr_t>(InSetMsPlotHC), 1, 0);
}
