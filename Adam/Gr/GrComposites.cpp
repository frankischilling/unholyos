#include "GrExt.hpp"

extern "C" int64_t GrRectBCpp(CDC *dc, int64_t x1, int64_t y1, int64_t x2,
                              int64_t y2)
{
  if (x2 < x1) {
    auto x = x1;
    x1 = x2;
    x2 = x;
  }
  if (y2 < y1) {
    auto y = y1;
    y1 = y2;
    y2 = y;
  }
  return GrRectCpp(dc, x1, y1, static_cast<uint64_t>(x2) - x1 + 1,
                   static_cast<uint64_t>(y2) - y1 + 1);
}

static bool DrawStdMsPlot(CDC *dc, int64_t x, int64_t y, int64_t z)
{
  dc->db_z = z;
  return GrPlotCpp(dc, x, y);
}

extern "C" void DrawStdMsCpp(CDC *dc, int64_t x, int64_t y)
{
  dc->thick = 1;
  dc->flags &= ~(DCF_TRANSFORMATION | DCF_SYMMETRY);
  // GrArrow3's 2.75-pixel width gives four-pixel wings at this angle.
  Line(dc, x + 8, y + 8, 0, x, y, 0, DrawStdMsPlot, 1, 0);
  Line(dc, x, y + 4, 0, x, y, 0, DrawStdMsPlot, 1, 0);
  Line(dc, x + 4, y, 0, x, y, 0, DrawStdMsPlot, 1, 0);
}
