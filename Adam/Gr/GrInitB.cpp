#include "../../Kernel/KernelB.hpp"
#include "GrGlbls.hpp"

bool GrInit2()
{
  gr.scrn_zoom = 1;
  PaletteSetStdCpp();
  GrPaletteIndicesSet();

  CDC *dc[6]{};
  for (auto &surface : dc) {
    surface = DCNewCpp(GR_WIDTH, GR_HEIGHT, nullptr, false);
    if (!surface) {
      for (auto *allocated : dc)
        DCDelCpp(allocated);
      return false;
    }
  }
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
  return true;
}
