#include "GodDoodle.hpp"
#include "../DolDoc/DolDoc.hpp"
#include "../Gr/Gr.hpp"
#include "../Gr/GrExt.hpp"
#include "GodExt.hpp"

extern "C" void GodDoodleDrawHC(), GodDoodleRunHC();
extern "C" CTaskSettings *SettingsPushCpp(CTask *task, int64_t flags);
extern "C" void SettingsPopCpp(CTask *task, int64_t flags);
extern "C" int64_t GetCharCpp(int64_t *scan_code, uint8_t echo,
                              uint8_t raw_cursor);

extern "C" void GodDoodleDrawCpp(CTask *task, CDC *dc)
{
  constexpr int64_t FONT_WIDTH = 8, FONT_HEIGHT = 8;
  GrBlotCpp(dc, 0, 0, god.doodle_dc);
  if ((__atomic_load_n(&cnts.jiffies, __ATOMIC_RELAXED) * 5 / JIFFY_FREQ) & 1) {
    if (god.doodle_done) {
      dc->color = {{RED, 0}, {}};
      const auto y = (task->pix_height - 3 * FONT_HEIGHT) >> 1;
      GrPrintCpp(dc, (task->pix_width - FONT_WIDTH * 29) >> 1, y,
                 "Press <ESC> to insert sprite.", 0, nullptr);
      GrPrintCpp(dc, (task->pix_width - FONT_WIDTH * 39) >> 1,
                 y + 2 * FONT_HEIGHT, "Press <SHIFT-ESC> to throw-away sprite.",
                 0, nullptr);
    } else {
      dc->color = {{GREEN, 0}, {}};
      GrPrintCpp(dc, (task->pix_width - FONT_WIDTH * 25) >> 1,
                 (task->pix_height - FONT_HEIGHT) >> 1,
                 "Press <SPACE> repeatedly.", 0, nullptr);
    }
  }
}

static void GodDoodleSmooth(int64_t num)
{
  auto *out = god.doodle_dc;
  auto *dc = DCExtCpp(out, 0, 0, out->width - 1, out->height - 1, nullptr);
  if (!dc)
    return;
  const auto c_old = out->color;
  for (int64_t y = 0; y < out->height; y++)
    for (int64_t x = 0; x < out->width; x++) {
      int64_t histogram[16]{};
      for (int64_t y1 = y - num; y1 <= y + num; y1++)
        for (int64_t x1 = x - num; x1 <= x + num; x1++) {
          const auto c = GrPeekCpp(dc, x1, y1);
          if (0 <= c && c <= 15)
            histogram[c]++;
        }
      int64_t best = BLACK, best_cnt = -1;
      for (int64_t i = 0; i < 16; i++)
        if (histogram[i] > best_cnt) {
          best = i;
          best_cnt = histogram[i];
        }
      out->color = {{uint8_t(best), 0}, {}};
      GrPlotCpp(out, x, y);
    }
  out->color = c_old;
  DCDelCpp(dc);
}

static void GodDoodleBitsIns(int64_t num_bits, int64_t n)
{
  for (int64_t i = 0; i < num_bits; i++, n >>= 1)
    FifoIns(*god.doodle_fifo, uint8_t(n & 1));
}

static void GodDoodleHexIns(const char *st)
{
  if (!st)
    return;
  char buf[2]{};
  while ((buf[0] = *st++)) {
    const auto ch = uint8_t(buf[0]);
    if (char_bmp_hex_numeric[ch >> 5] & (1U << (ch & 31)))
      GodDoodleBitsIns(4, rev_bits_table[Str2I64Cpp(buf, 16, nullptr)] >> 4);
  }
}

static int64_t GodDoodleBits(int64_t num_bits)
{
  uint64_t res = 0;
  while (num_bits) {
    uint8_t bit;
    if (FifoRem(*god.doodle_fifo, bit)) {
      res = (res << 1) + bit;
      num_bits--;
    } else {
      god.doodle_ch = GetCharCpp(nullptr, false, false);
      if (god.doodle_ch == CH_ESC || god.doodle_ch == CH_SHIFT_ESC)
        RaiseHolyC(0);
      else if (god.doodle_ch == '\n') {
        DCFillCpp(god.doodle_dc, WHITE);
        FifoFlush(*god.doodle_fifo);
      } else if ('0' <= god.doodle_ch && god.doodle_ch <= '9')
        GodDoodleSmooth(god.doodle_ch - '0');
      else
        GodDoodleBitsIns(GOD_GOOD_BITS, KbdMsEvtTimeCpp() >> GOD_BAD_BITS);
    }
  }
  return res;
}

extern "C" int64_t GodDoodleRunCpp(CDC *dc, bool seeded)
{
  constexpr uint64_t full_circle = 0x401921FB54442D18;
  constexpr uint8_t fill_colors[]{BLACK, DKGRAY, LTGRAY, WHITE};
  const int64_t w = dc->width, h = dc->height;
  // 15.5 is 31/2. HolyC truncates after subtracting the center offset.
  auto point = [](int64_t width, int64_t offset) {
    return (2 * (width - 1) * GodDoodleBits(5) - 31 * offset) / 31;
  };
  for (int64_t i = 0; i < 3; i++) {
    dc->color = {{RED, 0}, {}};
    for (int64_t j = 0; j < 29; j++)
      // HolyC evaluates the drawing call's arguments from right to left.
      switch (GodDoodleBits(3)) {
      case 0: {
        const auto yr = point(h, 0), xr = point(w, 0);
        const auto cy = point(h, h / 2), cx = point(w, w / 2);
        GrEllipse3Cpp(dc, cx, cy, 0, xr, yr, 0, 1, 0, full_circle);
        break;
      }
      case 1: {
        const auto r = point(w, 0);
        const auto cy = point(h, h / 2), cx = point(w, w / 2);
        GrCircle3Cpp(dc, cx, cy, 0, r, 1, 0, full_circle);
        break;
      }
      case 2: {
        const auto y2 = point(h, 0), x2 = point(w, 0);
        const auto y1 = point(h, h / 2), x1 = point(w, w / 2);
        GrBorderCpp(dc, x1, y1, x2, y2, 1, 0);
        break;
      }
      default: {
        const auto y2 = (h - 1) * GodDoodleBits(4) / 15;
        const auto x2 = (w - 1) * GodDoodleBits(4) / 15;
        const auto y1 = (h - 1) * GodDoodleBits(4) / 15;
        const auto x1 = (w - 1) * GodDoodleBits(4) / 15;
        GrLine3Cpp(dc, x1, y1, 0, x2, y2, 0, 1, 0);
        break;
      }
      }
    for (int64_t j = 0; j < 6; j++) {
      const auto x = (w - 1) * GodDoodleBits(5) / 31 + w / 64;
      const auto y = (h - 1) * GodDoodleBits(5) / 31 + h / 64;
      dc->color = {{fill_colors[GodDoodleBits(2)], 0}, {}};
      GrFloodFill3Cpp(dc, x, y, 0, false);
    }
    GodDoodleSmooth(3);
  }
  god.doodle_done = true;
  if (seeded)
    return CH_ESC;
  int64_t ch;
  do
    ch = GetCharCpp(nullptr, false, false);
  while (ch != CH_ESC && ch != CH_SHIFT_ESC);
  return ch;
}

extern "C" uint8_t *GodDoodleSpriteCpp(const char *hex)
{
  if (god.doodle_dc)
    return nullptr;
  god.doodle_done = false;
  auto *task = CurrentTask();
  if (!SettingsPushCpp(task, 0))
    return nullptr;
  AutoCompleteCpp(false);
  WinBorderCpp(false, task);
  WinMaxCpp(task);

  if (!hex)
    PopUpOkCpp("The $LK+PU,\"Holy Spirit\",A=\"FI:::/Adam/God/HSNotes.DD\"$ "
               "can puppet you.\n\nPress $GREEN$<SPACE>$FG$ until it finishes.",
               nullptr);

  god.doodle_ch = 0;
  god.doodle_dc = DCNewCpp(task->pix_width, task->pix_height, nullptr, false);
  if (!god.doodle_dc) {
    SettingsPopCpp(task, 0);
    return nullptr;
  }
  DCFillCpp(god.doodle_dc, WHITE);

  task->draw_it = reinterpret_cast<decltype(task->draw_it)>(GodDoodleDrawHC);
  FifoFlush(*god.doodle_fifo);
  GodDoodleHexIns(hex);

  uint64_t ch;
  const bool completed = CallHolyC2Checked(
      reinterpret_cast<uintptr_t>(GodDoodleRunHC),
      reinterpret_cast<uintptr_t>(god.doodle_dc), hex != nullptr, &ch);
  DCFillCpp(nullptr, TRANSPARENT);
  SettingsPopCpp(task, 0);
  auto *elems =
      completed && ch == CH_ESC ? DC2SpriteCpp(god.doodle_dc) : nullptr;
  DCDelCpp(god.doodle_dc);
  god.doodle_dc = nullptr;
  return elems;
}

extern "C" void GodDoodleCpp(const char *hex)
{
  if (auto *elems = GodDoodleSpriteCpp(hex)) {
    SpriteCpp(elems, nullptr);
    Free(elems);
  }
}
