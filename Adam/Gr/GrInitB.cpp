#include "../../Kernel/KernelB.hpp"
#include "GrExt.hpp"
#include "GrGlbls.hpp"

extern "C" void DrawStdMsHC(), WinFinalUpdateHC();

static void GrFreePenBrushes()
{
  for (unsigned i = 0; i < GR_PEN_BRUSHES_NUM; i++) {
    DCDelCpp(gr.pen_brushes[i]);
    DCDelCpp(gr.collision_pen_brushes[i]);
    DCDelCpp(gr.even_pen_brushes[i]);
    DCDelCpp(gr.odd_pen_brushes[i]);
    gr.pen_brushes[i] = gr.collision_pen_brushes[i] = nullptr;
    gr.even_pen_brushes[i] = gr.odd_pen_brushes[i] = nullptr;
  }
}

static bool GrSetUpTables()
{
  for (int64_t i = 0; i < GR_PEN_BRUSHES_NUM; i++) {
    auto *pen = gr.pen_brushes[i] = DCNewCpp(i, i, nullptr, false);
    auto *collision = gr.collision_pen_brushes[i] =
        DCNewCpp(i, i, nullptr, false);
    auto *even = gr.even_pen_brushes[i] = DCNewCpp(i, i, nullptr, false);
    auto *odd = gr.odd_pen_brushes[i] = DCNewCpp(i, i, nullptr, false);
    if (!pen || !collision || !even || !odd) {
      GrFreePenBrushes();
      return false;
    }
    pen->color.c0.color = even->color.c0.color = odd->color.c0.color =
        COLOR_MONO;
    collision->color.c0.color = COLOR_INVALID;
    const int64_t k = i + 1;
    for (int64_t y = 1; y < k; y++) {
      int64_t dy = y * 2 - k;
      for (int64_t x = 1; x < k; x++) {
        int64_t dx = x * 2 - k;
        if (dx * dx + dy * dy < k * k) {
          int64_t offset = x - 1 - (i >> 1);
          if (offset < gr.circle_lo[i][i - y])
            gr.circle_lo[i][i - y] = offset;
          if (offset > gr.circle_hi[i][i - y])
            gr.circle_hi[i][i - y] = offset;
          GrPlot0Cpp(pen, x - 1, y - 1);
          GrPlot0Cpp(collision, x - 1, y - 1);
          GrPlot0Cpp(((x ^ y) & 1) ? odd : even, x - 1, y - 1);
        }
      }
    }
  }
  for (unsigned i = 1; i <= GR_SCRN_ZOOM_MAX; i++) {
    auto *dst = MAllocCpp(256 * i, nullptr);
    if (!dst) {
      for (unsigned j = 1; j < i; j++) {
        Free(gr.scrn_zoom_tables[j]);
        gr.scrn_zoom_tables[j] = nullptr;
      }
      GrFreePenBrushes();
      return false;
    }
    gr.scrn_zoom_tables[i] = dst;
    for (unsigned byte = 0; byte < 256; byte++) {
      uint64_t expanded = 0;
      for (unsigned bit = 0; bit < 8; bit++)
        if (byte & (1U << bit))
          expanded |= ((1ULL << i) - 1) << (bit * i);
      for (unsigned slice = 0; slice < i; slice++)
        dst[byte + slice * 256] = expanded >> (slice * 8);
    }
  }
  return true;
}

static void WinZBufFill(CTask *task)
{
  const int64_t border = !(task->display_flags & DISPLAYF_NO_BORDER);
  auto clamp = [](int64_t value, int64_t lo, int64_t hi) {
    return value < lo ? lo : value > hi ? hi : value;
  };
  auto t = clamp(uint64_t(task->win_top) - border, 0, TEXT_ROWS - 1);
  auto b = clamp(uint64_t(task->win_bottom) + border, t, TEXT_ROWS - 1);
  auto l = clamp(uint64_t(task->win_left) - border, 0, TEXT_COLS - 1);
  auto r = clamp(uint64_t(task->win_right) + border, l, TEXT_COLS - 1);
  for (int64_t y = t; y <= b; y++)
    for (int64_t x = l; x <= r; x++)
      gr.win_z_buf[y * TEXT_COLS + x] = task->win_z_num;
}

extern "C" void WinZBufUpdateCpp()
{
  if (!sys_winmgr_task || !gr.win_z_buf)
    return;
  auto flags = GetRFlags();
  asm volatile("cli" : : : "memory");
  __atomic_fetch_and(&sys_semas[SEMA_UPDATE_WIN_Z_BUF].flags, ~uint64_t{1},
                     __ATOMIC_RELAXED);
  uint32_t z = 1;
  MemSet(gr.win_z_buf, 0, TEXT_COLS * TEXT_ROWS * sizeof(uint16_t));
  auto *task = sys_winmgr_task;
  do {
    if (!TaskValidateCpp(task))
      break;
    if (task->display_flags & DISPLAYF_SHOW) {
      task->win_z_num = z++;
      WinZBufFill(task);
    }
    task = task->next_task;
  } while (task != sys_winmgr_task && z < 0x10000);

  MemSet(gr.win_uncovered_bitmap, 0,
         (gr.highest_uncovered / 16 + 1) * sizeof(uint16_t));
  gr.highest_uncovered = z - 1;
  for (size_t i = 0; i < TEXT_ROWS * TEXT_COLS; i++) {
    auto depth = gr.win_z_buf[i];
    gr.win_uncovered_bitmap[depth >> 4] |= 1 << (depth & 15);
  }
  SetRFlags(flags);
}

bool GrInit2()
{
  gr.scrn_zoom = 1;
  PaletteSetStdCpp();
  GrPaletteIndicesSet();

  auto *text_base = reinterpret_cast<uint32_t *>(
      CAllocCpp(TEXT_COLS * TEXT_ROWS * sizeof(uint32_t), nullptr));
  auto *vga_text_cache = reinterpret_cast<uint16_t *>(
      CAllocCpp(TEXT_COLS * TEXT_ROWS * sizeof(uint16_t), nullptr));
  auto *win_z_buf = reinterpret_cast<uint16_t *>(
      CAllocCpp(TEXT_COLS * TEXT_ROWS * sizeof(uint16_t), nullptr));
  auto *win_uncovered_bitmap =
      reinterpret_cast<uint16_t *>(CAllocCpp(0x10000 / 8, nullptr));
  CDC *dc[6]{};
  bool okay = text_base && vga_text_cache && win_z_buf && win_uncovered_bitmap;
  for (auto &surface : dc) {
    if (!okay)
      break;
    surface = DCNewCpp(GR_WIDTH, GR_HEIGHT, nullptr, false);
    okay = surface != nullptr;
  }
  if (!okay || !GrSetUpTables()) {
    for (auto *surface : dc)
      DCDelCpp(surface);
    Free(win_uncovered_bitmap);
    Free(win_z_buf);
    Free(vga_text_cache);
    Free(text_base);
    return false;
  }
  gr.text_base = text_base;
  gr.vga_text_cache = vga_text_cache;
  gr.win_z_buf = win_z_buf;
  gr.win_uncovered_bitmap = win_uncovered_bitmap;
  gr.dc2 = dc[0];
  gr.dc2->flags |= DCF_SCRN_BITMAP;
  gr.dc_cache = dc[1];
  gr.dc = dc[2];
  gr.dc->flags |= DCF_SCRN_BITMAP | DCF_ON_TOP;
  DCFillCpp(nullptr, TRANSPARENT);
  gr.dc1 = dc[3];
  gr.dc1->flags |= DCF_SCRN_BITMAP;
  gr.scrn_image = dc[4];
  gr.zoomed_dc = dc[5];
  gr.zoomed_dc->flags |= DCF_SCRN_BITMAP;
  gr.fp_draw_ms = reinterpret_cast<uintptr_t>(DrawStdMsHC);
  gr.fp_final_scrn_update = reinterpret_cast<uintptr_t>(WinFinalUpdateHC);
  auto *task = CurrentTask();
  task->win_left = task->win_top = 0;
  task->win_right = TEXT_COLS - 1;
  task->win_bottom = TEXT_ROWS - 1;
  WinDerivedValsUpdate(task);
  return true;
}
