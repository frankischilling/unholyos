#include "../../Kernel/KernelB.hpp"
#include "GrGlbls.hpp"

bool GrInit2()
{
  gr.scrn_zoom = 1;
  PaletteSetStdCpp();
  GrPaletteIndicesSet();

  auto *win_z_buf = reinterpret_cast<uint16_t *>(
      CAllocCpp(TEXT_COLS * TEXT_ROWS * sizeof(uint16_t), nullptr));
  if (!win_z_buf)
    return false;
  CDC *dc[6]{};
  for (auto &surface : dc) {
    surface = DCNewCpp(GR_WIDTH, GR_HEIGHT, nullptr, false);
    if (!surface) {
      for (auto *allocated : dc)
        DCDelCpp(allocated);
      Free(win_z_buf);
      return false;
    }
  }
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
  auto *task = CurrentTask();
  task->win_left = task->win_top = 0;
  task->win_right = TEXT_COLS - 1;
  task->win_bottom = TEXT_ROWS - 1;
  WinDerivedValsUpdate(task);
  return true;
}
