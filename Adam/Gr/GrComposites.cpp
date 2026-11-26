#include "../../Kernel/KernelB.hpp"
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

extern "C" void GrBorderCpp(CDC *dc, int64_t x1, int64_t y1, int64_t x2,
                            int64_t y2, int64_t step, int64_t start)
{
  GrLine3Cpp(dc, x1, y1, 0, x2, y1, 0, step, start);
  GrLine3Cpp(dc, x2, y1, 0, x2, y2, 0, step, start);
  GrLine3Cpp(dc, x2, y2, 0, x1, y2, 0, step, start);
  GrLine3Cpp(dc, x1, y2, 0, x1, y1, 0, step, start);
}

static bool GrArrow3Pass(CDC *dc, const CD3I64 &p1, const CD3I64 &p2,
                         uint64_t w, int64_t step, int64_t start)
{
  bool res =
      Line(dc, p1.x, p1.y, p1.z, p2.x, p2.y, p2.z, GrPlot3Cpp, step, start);
  uint64_t dx = uint64_t(p2.x) - p1.x, dy = uint64_t(p2.y) - p1.y;
  auto d = F64SqrtBits(F64FromI64(dx * dx + dy * dy));
  if (d) {
    d = F64DivBits(F64MulBits(w, F64FromI64(dc->thick)), d);
    const auto xx = F64MulBits(F64FromI64(dx), d);
    const auto yy = F64MulBits(F64FromI64(dy), d);
    constexpr uint64_t sign = 1ULL << 63, half = 0x3FE0000000000000;
    const auto x = F64AddBits(F64FromI64(p2.x), xx ^ sign);
    const auto y = F64AddBits(F64FromI64(p2.y), yy ^ sign);
    auto coord = [=](uint64_t base, uint64_t offset) {
      return F64ToI64(F64AddBits(F64AddBits(base, offset), half), 0x0C00);
    };
    res |= Line(dc, coord(x, yy), coord(y, xx ^ sign), p2.z, p2.x, p2.y, p2.z,
                GrPlot3Cpp, step, 0);
    res |= Line(dc, coord(x, yy ^ sign), coord(y, xx), p2.z, p2.x, p2.y, p2.z,
                GrPlot3Cpp, step, 0);
  }
  return res;
}

extern "C" bool GrArrow3Cpp(CDC *dc, int64_t x1, int64_t y1, int64_t z1,
                            int64_t x2, int64_t y2, int64_t z2, uint64_t w,
                            int64_t step, int64_t start)
{
  if (!dc)
    dc = gr.dc;
  CD3I64 p1{x1, y1, z1}, p2{x2, y2, z2};
  const auto was_transform = dc->flags & DCF_TRANSFORMATION;
  if (was_transform) {
    DCTransformPoint(*dc, p1);
    DCTransformPoint(*dc, p2);
    dc->flags &= ~DCF_TRANSFORMATION;
  }
  const auto was_symmetry = dc->flags & DCF_SYMMETRY;
  bool res = false;
  if (was_symmetry) {
    auto m1 = p1, m2 = p2;
    DCReflectCpp(dc, &m1.x, &m1.y, &m1.z);
    DCReflectCpp(dc, &m2.x, &m2.y, &m2.z);
    dc->flags &= ~DCF_SYMMETRY;
    res = GrArrow3Pass(dc, m1, m2, w, step, start);
  }
  if (!was_symmetry || !(dc->flags & DCF_JUST_MIRROR))
    res |= GrArrow3Pass(dc, p1, p2, w, step, start);
  dc->flags |= was_transform | was_symmetry;
  return res;
}

extern "C" void DrawStdMsCpp(CDC *dc, int64_t x, int64_t y)
{
  dc->thick = 1;
  dc->flags &= ~(DCF_TRANSFORMATION | DCF_SYMMETRY);
  GrArrow3Cpp(dc, uint64_t(x) + 8, uint64_t(y) + 8, 0, x, y, 0);
}
