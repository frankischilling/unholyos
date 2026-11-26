#include "../../Kernel/KernelB.hpp"
#include "GrGlbls.hpp"

extern "C" void DrawStdMsHC(), WinFinalUpdateHC();

static bool GrSetUpTables()
{
  for (unsigned i = 1; i <= GR_SCRN_ZOOM_MAX; i++) {
    auto *dst = MAllocCpp(256 * i, nullptr);
    if (!dst) {
      for (unsigned j = 1; j < i; j++) {
        Free(gr.scrn_zoom_tables[j]);
        gr.scrn_zoom_tables[j] = nullptr;
      }
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
  CDC *dc[6]{};
  bool okay = text_base && vga_text_cache && win_z_buf;
  for (auto &surface : dc) {
    if (!okay)
      break;
    surface = DCNewCpp(GR_WIDTH, GR_HEIGHT, nullptr, false);
    okay = surface != nullptr;
  }
  if (!okay || !GrSetUpTables()) {
    for (auto *surface : dc)
      DCDelCpp(surface);
    Free(win_z_buf);
    Free(vga_text_cache);
    Free(text_base);
    return false;
  }
  gr.text_base = text_base;
  gr.vga_text_cache = vga_text_cache;
  gr.win_z_buf = win_z_buf;
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
