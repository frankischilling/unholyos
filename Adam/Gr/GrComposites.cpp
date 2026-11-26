#include "GrExt.hpp"
#include "GrGlbls.hpp"

static int64_t GrFillPoly3Pass(CDC &dc, int64_t n, const CD3I32 *poly,
                               bool reflect)
{
  auto vertex = [&](const CD3I32 &p) {
    CD3I64 v{p.x, p.y, p.z};
    if (dc.flags & DCF_TRANSFORMATION)
      DCTransformPoint(dc, v);
    if (reflect)
      DCReflectCpp(&dc, &v.x, &v.y, &v.z);
    return CD3I32{int32_t(v.x), int32_t(v.y), int32_t(v.z)};
  };
  auto p1 = vertex(poly[0]), p2 = vertex(poly[1]);
  int64_t res = 0;
  for (int64_t i = 2; i < n; i++) {
    auto p3 = vertex(poly[i]);
    res += GrFillTri0Cpp(&dc, &p1, &p2, &p3);
    p2 = p3;
  }
  return res;
}

extern "C" int64_t GrFillPoly3Cpp(CDC *dc, int64_t n, const CD3I32 *poly)
{
  if (!dc)
    dc = gr.dc;
  if (n < 3)
    return 0;
  int64_t res = 0;
  if (dc->flags & DCF_SYMMETRY)
    res = GrFillPoly3Pass(*dc, n, poly, true);
  if (!(dc->flags & DCF_JUST_MIRROR))
    res += GrFillPoly3Pass(*dc, n, poly, false);
  return res;
}

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

extern "C" int64_t GrRect3Cpp(CDC *dc, int64_t x, int64_t y, int64_t z,
                              int64_t w, int64_t h)
{
  int32_t x1 = x, x2 = uint64_t(x) + w, y1 = y, y2 = uint64_t(y) + h;
  int32_t z1 = z;
  CD3I32 poly[4]{{x1, y1, z1}, {x2, y1, z1}, {x2, y2, z1}, {x1, y2, z1}};
  return GrFillPoly3Cpp(dc, 4, poly);
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
