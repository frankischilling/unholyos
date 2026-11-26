#include "../../Kernel/KernelB.hpp"
#include "GrExt.hpp"
#include "GrGlbls.hpp"

template<class Plot>
static bool LineWalk(int64_t x1, int64_t y1, int64_t z1, int64_t x2, int64_t y2,
                     int64_t z2, Plot plot, int64_t step, int64_t start)
{
  int64_t dx = static_cast<uint64_t>(x2) - x1,
          dy = static_cast<uint64_t>(y2) - y1;
  int64_t dz = static_cast<uint64_t>(z2) - z1;
  auto adx = AbsI64(dx), ady = AbsI64(dy), adz = AbsI64(dz);
  const bool x_axis = adx >= ady && adx >= adz;
  const bool y_axis = ady > adx && ady >= adz;
  auto distance = x_axis ? adx : y_axis ? ady : adz;
  if (distance > INT64_MAX)
    return false;
  auto d = static_cast<int64_t>(distance);
  if (d) {
    dx = x_axis ? (dx < 0 ? -GR_SCALE : GR_SCALE) : GrFixed32(dx) / d;
    dy = y_axis ? (dy < 0 ? -GR_SCALE : GR_SCALE) : GrFixed32(dy) / d;
    dz = !x_axis && !y_axis ? (dz < 0 ? -GR_SCALE : GR_SCALE)
                            : GrFixed32(dz) / d;
  }
  uint64_t fx = GrFixed32(x1), fy = GrFixed32(y1), fz = GrFixed32(z1);
  if (start > 0) {
    fx += static_cast<uint64_t>(dx) * start;
    fy += static_cast<uint64_t>(dy) * start;
    fz += static_cast<uint64_t>(dz) * start;
  }
  if (step != 1 && step != 0) {
    dx *= static_cast<uint64_t>(step);
    dy *= static_cast<uint64_t>(step);
    dz *= static_cast<uint64_t>(step);
    d /= step;
  }
  int64_t x = 0, y = 0, z = 0;
  bool first = true;
  for (int64_t i = start; i <= d; i++) {
    const auto nx = static_cast<int32_t>(fx >> 32);
    const auto ny = static_cast<int32_t>(fy >> 32);
    const auto nz = static_cast<int32_t>(fz >> 32);
    if ((first || x != nx || y != ny || z != nz) && !plot(nx, ny, nz))
      return false;
    x = nx;
    y = ny;
    z = nz;
    first = false;
    fx += dx;
    fy += dy;
    fz += dz;
    if (i == d)
      break;
  }
  return step != 1 || (!first && x == x2 && y == y2 && z == z2) ||
         plot(x2, y2, z2);
}

bool Line(CDC *dc, int64_t x1, int64_t y1, int64_t z1, int64_t x2, int64_t y2,
          int64_t z2, GrLinePlotFn fp_plot, int64_t step, int64_t start)
{
  return LineWalk(x1, y1, z1, x2, y2, z2,
                  [=](int64_t x, int64_t y, int64_t z) {
                    return fp_plot(dc, x, y, z);
                  }, step, start);
}

extern "C" bool LineCpp(uintptr_t aux, int64_t x1, int64_t y1, int64_t z1,
                        int64_t x2, int64_t y2, int64_t z2, uintptr_t fp_plot,
                        int64_t step, int64_t start)
{
  return LineWalk(x1, y1, z1, x2, y2, z2,
                  [=](int64_t x, int64_t y, int64_t z) {
                    return uint8_t(CallHolyC4(fp_plot, aux, x, y, z)) != 0;
                  }, step, start);
}

// The arc walkers use a 24-bit rotation and 8-bit point fraction.
constexpr unsigned GR_SCALE1_BITS = 24, GR_SCALE2_BITS = 8;

static int64_t GrArcMul(int64_t a, uint64_t b)
{
  return F64ToI64(F64MulBits(F64FromI64(a), b), 0x0C00);
}

static int64_t GrArcRotate(int64_t a, int64_t b, int64_t c, int64_t d)
{
  return int64_t(uint64_t(a) * b + uint64_t(c) * d) >> GR_SCALE1_BITS;
}

struct CGrArc
{
  int64_t c, s, x, y, len, step;

  CGrArc(int64_t radius, int64_t max_radius, int64_t delta,
         uint64_t start, uint64_t length)
  {
    auto t = F64DivBits(F64FromI64(1), F64FromI64(max_radius));
    len = F64ToI64(F64MulBits(length, F64FromI64(max_radius)), 0x0800);
    c = GrArcMul(1 << GR_SCALE1_BITS, F64CosBits(t));
    s = GrArcMul(1 << GR_SCALE1_BITS, F64SinBits(t));
    if (delta < 0)
      s = -s;
    step = static_cast<int64_t>(AbsI64(delta));
    x = radius;
    y = 0;
    if (start & (UINT64_MAX >> 1)) {
      x = GrArcMul(radius, F64CosBits(start));
      y = -GrArcMul(radius, F64SinBits(start));
    }
    x = uint64_t(x) << GR_SCALE2_BITS;
    y = uint64_t(y) << GR_SCALE2_BITS;
  }

  void Advance()
  {
    for (int64_t j = 0; j < step; j++) {
      auto nx = GrArcRotate(c, x, s, y);
      y = GrArcRotate(-s, x, c, y);
      x = nx;
    }
  }
};

template<class Plot>
static bool CircleArc(int64_t cx, int64_t cy, int64_t cz, int64_t radius,
                      Plot plot, int64_t step, uint64_t start, uint64_t length)
{
  if (radius <= 0 || !step)
    return true;
  if (step == INT64_MIN)
    return false;
  CGrArc arc(radius, radius, step, start, length);
  for (int64_t i = 0; i <= arc.len;) {
    if (!plot(uint64_t(cx) + (arc.x >> GR_SCALE2_BITS),
              uint64_t(cy) + (arc.y >> GR_SCALE2_BITS), cz))
      return false;
    if (arc.step > arc.len - i)
      break;
    arc.Advance();
    i += arc.step;
  }
  return true;
}

template<class Plot>
static bool EllipseArc(int64_t cx, int64_t cy, int64_t cz, int64_t xr,
                       int64_t yr, Plot plot, uint64_t rotation, int64_t step,
                       uint64_t start, uint64_t length)
{
  if (xr <= 0 || yr <= 0 || !step)
    return true;
  if (step == INT64_MIN)
    return false;
  CGrArc arc(xr, xr >= yr ? xr : yr, step, start, length);
  auto c = GrArcMul(1 << GR_SCALE1_BITS, F64CosBits(rotation));
  auto s = GrArcMul(1 << GR_SCALE1_BITS, F64SinBits(rotation));
  int64_t old_x = 0, old_y = 0;
  bool first = true;
  for (int64_t i = 0; i <= arc.len;) {
    auto scaled_y = int64_t(uint64_t(arc.y) * yr) / xr;
    auto x = GrArcRotate(c, arc.x, s, scaled_y) >> GR_SCALE2_BITS;
    auto y = GrArcRotate(-s, arc.x, c, scaled_y) >> GR_SCALE2_BITS;
    if ((first || x != old_x || y != old_y) &&
        !plot(uint64_t(cx) + x, uint64_t(cy) + y, cz))
      return false;
    first = false;
    old_x = x;
    old_y = y;
    if (arc.step > arc.len - i)
      break;
    arc.Advance();
    i += arc.step;
  }
  return true;
}

bool Circle(CDC *dc, int64_t cx, int64_t cy, int64_t cz, int64_t radius,
            GrLinePlotFn fp_plot, int64_t step, uint64_t start, uint64_t length)
{
  return CircleArc(cx, cy, cz, radius,
                   [=](int64_t x, int64_t y, int64_t z) {
                     return fp_plot(dc, x, y, z);
                   }, step, start, length);
}

bool Ellipse(CDC *dc, int64_t cx, int64_t cy, int64_t cz, int64_t xr,
             int64_t yr, GrLinePlotFn fp_plot, uint64_t rotation, int64_t step,
             uint64_t start, uint64_t length)
{
  return EllipseArc(cx, cy, cz, xr, yr,
                    [=](int64_t x, int64_t y, int64_t z) {
                      return fp_plot(dc, x, y, z);
                    }, rotation, step, start, length);
}

extern "C" bool CircleCpp(uintptr_t aux, int64_t cx, int64_t cy, int64_t cz,
                          int64_t radius, uintptr_t fp_plot, int64_t step,
                          uint64_t start, uint64_t length)
{
  return CircleArc(cx, cy, cz, radius,
                   [=](int64_t x, int64_t y, int64_t z) {
                     return uint8_t(CallHolyC4(fp_plot, aux, x, y, z)) != 0;
                   }, step, start, length);
}

extern "C" bool EllipseCpp(uintptr_t aux, int64_t cx, int64_t cy, int64_t cz,
                           int64_t xr, int64_t yr, uintptr_t fp_plot,
                           uint64_t rotation, int64_t step, uint64_t start,
                           uint64_t length)
{
  return EllipseArc(cx, cy, cz, xr, yr,
                    [=](int64_t x, int64_t y, int64_t z) {
                      return uint8_t(CallHolyC4(fp_plot, aux, x, y, z)) != 0;
                    }, rotation, step, start, length);
}

template<class Plot>
static bool RegPolyWalk(int64_t cx, int64_t cy, int64_t cz, int64_t xr,
                        int64_t yr, int64_t sides, Plot plot, uint64_t rotation,
                        int64_t step, uint64_t start, uint64_t length)
{
  if (sides <= 0 || xr <= 0 || yr <= 0)
    return true;
  constexpr uint64_t tau = 0x401921FB54442D18;
  auto angle_step = F64DivBits(tau, F64FromI64(sides));
  auto n = F64ToI64(F64DivBits(F64MulBits(length, F64FromI64(sides)), tau), 0x0C00);
  auto s1 = GrArcMul(1 << GR_SCALE1_BITS, F64SinBits(angle_step));
  auto c = GrArcMul(1 << GR_SCALE1_BITS, F64CosBits(angle_step));
  auto s12 = GrArcMul(1 << GR_SCALE1_BITS, F64SinBits(rotation));
  auto c2 = GrArcMul(1 << GR_SCALE1_BITS, F64CosBits(rotation));
  int64_t x2 = xr, y2 = 0;
  if (start & (UINT64_MAX >> 1)) {
    x2 = GrArcMul(xr, F64CosBits(start));
    y2 = uint64_t(0) - GrArcMul(xr, F64SinBits(start));
  }
  x2 = uint64_t(x2) << GR_SCALE2_BITS;
  y2 = uint64_t(y2) << GR_SCALE2_BITS;
  auto project = [&](int64_t &x, int64_t &y) {
    auto sy = int64_t(uint64_t(y2) * yr) / xr;
    x = uint64_t(cx) + (GrArcRotate(c2, x2, s12, sy) >> GR_SCALE2_BITS);
    y = uint64_t(cy) + (GrArcRotate(-s12, x2, c2, sy) >> GR_SCALE2_BITS);
  };
  int64_t xx1, yy1, xx2, yy2;
  project(xx1, yy1);
  for (int64_t i = 0; i < n; i++) {
    auto x1 = GrArcRotate(c, x2, s1, y2);
    y2 = GrArcRotate(-s1, x2, c, y2);
    x2 = x1;
    project(xx2, yy2);
    if (!LineWalk(xx1, yy1, cz, xx2, yy2, cz, plot, step, 0))
      return false;
    xx1 = xx2;
    yy1 = yy2;
  }
  return true;
}

bool RegPoly(CDC *dc, int64_t cx, int64_t cy, int64_t cz, int64_t xr,
             int64_t yr, int64_t sides, GrLinePlotFn fp_plot, uint64_t rotation,
             int64_t step, uint64_t start, uint64_t length)
{
  return RegPolyWalk(cx, cy, cz, xr, yr, sides,
                     [=](int64_t x, int64_t y, int64_t z) {
                       return fp_plot(dc, x, y, z);
                     }, rotation, step, start, length);
}

extern "C" bool RegPolyCpp(uintptr_t aux, int64_t cx, int64_t cy, int64_t cz,
                           int64_t xr, int64_t yr, int64_t sides, uintptr_t fp_plot,
                           uint64_t rotation, int64_t step, uint64_t start,
                           uint64_t length)
{
  return RegPolyWalk(cx, cy, cz, xr, yr, sides,
                     [=](int64_t x, int64_t y, int64_t z) {
                       return uint8_t(CallHolyC4(fp_plot, aux, x, y, z)) != 0;
                     }, rotation, step, start, length);
}

extern "C" int64_t D3I32DistSqrCpp(const CD3I32 *p1, const CD3I32 *p2)
{
  int64_t x = int64_t(p1->x) - p2->x, y = int64_t(p1->y) - p2->y,
          z = int64_t(p1->z) - p2->z;
  return uint64_t(x) * x + uint64_t(y) * y + uint64_t(z) * z;
}

extern "C" uint64_t D3I32DistCpp(const CD3I32 *p1, const CD3I32 *p2)
{
  return F64SqrtBits(F64FromI64(D3I32DistSqrCpp(p1, p2)));
}

extern "C" int64_t D3I32NormSqrCpp(const CD3I32 *p)
{
  const CD3I32 origin{};
  return D3I32DistSqrCpp(p, &origin);
}

static uint64_t GrF64Sub(uint64_t a, uint64_t b)
{
  return F64AddBits(a, b ^ (uint64_t{1} << 63));
}

template<size_t order, class Plot>
static bool BezierWalk(const CD3I32 *ctrl, Plot plot, bool first)
{
  constexpr uint64_t one = 0x3FF0000000000000, half = 0x3FE0000000000000;
  constexpr uint64_t grow = 0x3FF199999999999A, shrink = 0x3FECCCCCCCCCCCCD;
  uint64_t x[order + 1], y[order + 1], z[order + 1];
  x[0] = F64FromI64(ctrl[0].x);
  y[0] = F64FromI64(ctrl[0].y);
  z[0] = F64FromI64(ctrl[0].z);
  for (size_t i = 1; i <= order; i++) {
    x[i] = F64FromI64(int64_t(ctrl[i].x) - ctrl[0].x);
    y[i] = F64FromI64(int64_t(ctrl[i].y) - ctrl[0].y);
    z[i] = F64FromI64(int64_t(ctrl[i].z) - ctrl[0].z);
  }
  // The source accumulates consecutive edges before the closing edge.
  uint64_t d = D3I32DistCpp(ctrl, ctrl + 1);
  for (size_t i = 1; i < order; i++)
    d = F64AddBits(d, D3I32DistCpp(ctrl + i, ctrl + i + 1));
  d = F64AddBits(d, D3I32DistCpp(ctrl + order, ctrl));
  auto s = F64DivBits(half, d);
  int64_t xx = ctrl[0].x, yy = ctrl[0].y, zz = ctrl[0].z;
  if (first && !plot(xx, yy, zz))
    return false;
  for (uint64_t t = 0; t <= one;) {
    auto nt = GrF64Sub(one, t), tt = F64MulBits(t, t);
    uint64_t w[order];
    w[0] = F64MulBits(t, nt);
    if constexpr (order == 3) {
      w[0] = F64MulBits(w[0], nt);
      w[1] = F64MulBits(tt, nt);
      w[2] = F64MulBits(tt, t);
    } else
      w[1] = tt;
    auto coord = [&](const uint64_t *a) {
      uint64_t value = a[0];
      for (size_t i = 0; i < order; i++)
        value = F64AddBits(value, F64MulBits(a[i + 1], w[i]));
      return F64ToI64(value, 0x0C00);
    };
    auto nx = coord(x), ny = coord(y), nz = coord(z);
    if (nx == xx && ny == yy && nz == zz)
      s = F64MulBits(s, grow);
    else {
      s = F64MulBits(s, shrink);
      if (!plot(nx, ny, nz))
        return false;
      xx = nx;
      yy = ny;
      zz = nz;
    }
    auto next = F64AddBits(t, s);
    if (next <= t)
      break;
    t = next;
  }
  return (xx == ctrl[order].x && yy == ctrl[order].y && zz == ctrl[order].z) ||
         plot(ctrl[order].x, ctrl[order].y, ctrl[order].z);
}

static CD3I32 GrMidPoint(const CD3I32 &a, const CD3I32 &b)
{
  return {int32_t((int64_t(a.x) + b.x) / 2),
          int32_t((int64_t(a.y) + b.y) / 2),
          int32_t((int64_t(a.z) + b.z) / 2)};
}

static CD3I32 GrThirdPoint(const CD3I32 &a, const CD3I32 &b, int64_t numerator)
{
  auto coord = [=](int64_t x, int64_t y) {
    auto d = F64MulBits(F64FromI64(y - x), F64FromI64(numerator));
    return int32_t(F64ToI64(F64AddBits(F64DivBits(d, F64FromI64(3)),
                                      F64FromI64(x)), 0x0C00));
  };
  return {coord(a.x, b.x), coord(a.y, b.y), coord(a.z, b.z)};
}

template<size_t order, class Plot>
static bool BSplineWalk(const CD3I32 *ctrl, int64_t cnt, Plot plot, bool closed)
{
  if (cnt < 3 || uint64_t(cnt) > (INT64_MAX / sizeof(CD3I32) - 1) / order)
    return false;
  int64_t segments = closed ? cnt : cnt - 1;
  auto *c = reinterpret_cast<CD3I32 *>(MAllocCpp(sizeof(CD3I32) *
                                                (segments * order + 1), nullptr));
  if (!c)
    return false;
  for (int64_t i = 0; i < segments; i++) {
    const auto &a = ctrl[i], &b = ctrl[(i + 1) % cnt];
    if constexpr (order == 2)
      c[i * order + 1] = GrMidPoint(a, b);
    else {
      c[i * order + 1] = GrThirdPoint(a, b, 1);
      c[i * order + 2] = GrThirdPoint(a, b, 2);
    }
  }
  for (int64_t i = 1; i < segments; i++)
    c[i * order] = GrMidPoint(c[i * order - 1], c[i * order + 1]);
  if (closed) {
    c[0] = GrMidPoint(c[1], c[segments * order - 1]);
    c[segments * order] = c[0];
  } else {
    c[0] = ctrl[0];
    c[segments * order] = ctrl[cnt - 1];
  }
  bool res = true;
  for (int64_t i = 0; i < segments; i++)
    if (!BezierWalk<order>(&c[i * order], plot, !i)) {
      res = false;
      break;
    }
  Free(c);
  return res;
}

bool Bezier2(CDC *dc, const CD3I32 *ctrl, GrLinePlotFn fp_plot, bool first)
{
  return BezierWalk<2>(ctrl, [=](int64_t x, int64_t y, int64_t z) {
    return fp_plot(dc, x, y, z);
  }, first);
}

bool Bezier3(CDC *dc, const CD3I32 *ctrl, GrLinePlotFn fp_plot, bool first)
{
  return BezierWalk<3>(ctrl, [=](int64_t x, int64_t y, int64_t z) {
    return fp_plot(dc, x, y, z);
  }, first);
}

bool BSpline2(CDC *dc, const CD3I32 *ctrl, int64_t cnt, GrLinePlotFn fp_plot, bool closed)
{
  return BSplineWalk<2>(ctrl, cnt, [=](int64_t x, int64_t y, int64_t z) {
    return fp_plot(dc, x, y, z);
  }, closed);
}

bool BSpline3(CDC *dc, const CD3I32 *ctrl, int64_t cnt, GrLinePlotFn fp_plot, bool closed)
{
  return BSplineWalk<3>(ctrl, cnt, [=](int64_t x, int64_t y, int64_t z) {
    return fp_plot(dc, x, y, z);
  }, closed);
}

extern "C" bool Bezier2Cpp(uintptr_t aux, const CD3I32 *ctrl, uintptr_t fp_plot, bool first)
{
  return BezierWalk<2>(ctrl, [=](int64_t x, int64_t y, int64_t z) {
    return uint8_t(CallHolyC4(fp_plot, aux, x, y, z)) != 0;
  }, first);
}

extern "C" bool Bezier3Cpp(uintptr_t aux, const CD3I32 *ctrl, uintptr_t fp_plot, bool first)
{
  return BezierWalk<3>(ctrl, [=](int64_t x, int64_t y, int64_t z) {
    return uint8_t(CallHolyC4(fp_plot, aux, x, y, z)) != 0;
  }, first);
}

extern "C" bool BSpline2Cpp(uintptr_t aux, const CD3I32 *ctrl, int64_t cnt,
                            uintptr_t fp_plot, bool closed)
{
  return BSplineWalk<2>(ctrl, cnt, [=](int64_t x, int64_t y, int64_t z) {
    return uint8_t(CallHolyC4(fp_plot, aux, x, y, z)) != 0;
  }, closed);
}

extern "C" bool BSpline3Cpp(uintptr_t aux, const CD3I32 *ctrl, int64_t cnt,
                            uintptr_t fp_plot, bool closed)
{
  return BSplineWalk<3>(ctrl, cnt, [=](int64_t x, int64_t y, int64_t z) {
    return uint8_t(CallHolyC4(fp_plot, aux, x, y, z)) != 0;
  }, closed);
}

extern "C" int64_t *Mat4x4MulMat4x4EquCpp(int64_t *dst, const int64_t *m1,
                                          const int64_t *m2)
{
  const auto scale = F64FromI64(GR_SCALE);
  for (size_t i = 0; i < 4; i++)
    for (size_t j = 0; j < 4; j++) {
      uint64_t sum = 0;
      for (size_t k = 0; k < 4; k++)
        sum = F64AddBits(sum, F64MulBits(F64FromI64(m1[k + 4 * j]),
                                         F64FromI64(m2[i + 4 * k])));
      dst[i + 4 * j] = F64ToI64(F64DivBits(sum, scale), 0x0C00);
    }
  return dst;
}

extern "C" int64_t *Mat4x4MulMat4x4NewCpp(const int64_t *m1, const int64_t *m2,
                                          CTask *mem_task)
{
  auto *dst =
      reinterpret_cast<int64_t *>(MAllocCpp(16 * sizeof(int64_t), mem_task));
  return dst ? Mat4x4MulMat4x4EquCpp(dst, m1, m2) : nullptr;
}

extern "C" int64_t *Mat4x4EquCpp(int64_t *dst, const int64_t *src)
{
  MemCpy(dst, src, 16 * sizeof(*dst));
  return dst;
}

extern "C" int64_t *Mat4x4NewCpp(const int64_t *src, CTask *mem_task)
{
  auto *dst =
      reinterpret_cast<int64_t *>(MAllocCpp(16 * sizeof(int64_t), mem_task));
  return dst ? Mat4x4EquCpp(dst, src) : nullptr;
}

static int64_t *Mat4x4Rot(int64_t *m, uint64_t angle, size_t i, size_t j)
{
  int64_t r[16], r2[16];
  Mat4x4IdentEquCpp(r);
  const auto scale = F64FromI64(GR_SCALE);
  auto my_cos = F64ToI64(F64MulBits(F64CosBits(angle), scale), 0x0C00);
  auto my_sin = F64ToI64(F64MulBits(F64SinBits(angle), scale), 0x0C00);
  r[5 * i] = r[5 * j] = my_cos;
  r[4 * j + i] = my_sin;
  r[4 * i + j] = 0 - uint64_t(my_sin);
  return Mat4x4EquCpp(m, Mat4x4MulMat4x4EquCpp(r2, r, m));
}

extern "C" int64_t *Mat4x4RotXCpp(int64_t *m, uint64_t angle)
{
  return Mat4x4Rot(m, angle, 1, 2);
}

extern "C" int64_t *Mat4x4RotYCpp(int64_t *m, uint64_t angle)
{
  return Mat4x4Rot(m, angle, 2, 0);
}

extern "C" int64_t *Mat4x4RotZCpp(int64_t *m, uint64_t angle)
{
  return Mat4x4Rot(m, angle, 0, 1);
}

extern "C" int64_t *Mat4x4ScaleCpp(int64_t *m, uint64_t scale)
{
  for (size_t i = 0; i < 16; i++)
    m[i] = F64ToI64(F64MulBits(F64FromI64(m[i]), scale), 0x0C00);
  return m;
}

extern "C" int64_t *Mat4x4TranslationEquCpp(int64_t *r, int64_t x, int64_t y,
                                            int64_t z)
{
  r[3] = GrFixed32(x);
  r[7] = GrFixed32(y);
  r[11] = GrFixed32(z);
  r[15] = GR_SCALE;
  return r;
}

extern "C" int64_t *Mat4x4TranslationAddCpp(int64_t *r, int64_t x, int64_t y,
                                            int64_t z)
{
  r[3] += uint64_t(GrFixed32(x));
  r[7] += uint64_t(GrFixed32(y));
  r[11] += uint64_t(GrFixed32(z));
  r[15] = GR_SCALE;
  return r;
}

void DCTransformPoint(CDC &dc, CD3I64 &p)
{
  CallHolyC4(
      reinterpret_cast<uintptr_t>(dc.transform),
      reinterpret_cast<uintptr_t>(&dc), reinterpret_cast<uintptr_t>(&p.x),
      reinterpret_cast<uintptr_t>(&p.y), reinterpret_cast<uintptr_t>(&p.z));
}

extern "C" void DCThickScaleCpp(CDC *dc)
{
  if (!dc)
    dc = gr.dc;
  if ((dc->flags & DCF_TRANSFORMATION) && dc->thick) {
    uint64_t d = uint64_t(dc->thick) * dc->r_norm + 0x80000000;
    dc->thick = int32_t(d >> 32);
    if (dc->thick < 1)
      dc->thick = 1;
  }
}

extern "C" uint64_t DCSymmetrySetCpp(CDC *dc, int64_t x1, int64_t y1,
                                     int64_t x2, int64_t y2)
{
  if (!dc)
    dc = gr.dc;
  if (x1 == x2 && y1 == y2)
    return false;
  uint64_t x = uint64_t(y2) - y1, y = uint64_t(x1) - x2;
  auto &sym = dc->sym;
  sym.snx = x;
  sym.sny = y;
  sym.snz = 0;
  auto d = F64SqrtBits(F64FromI64(x * x + y * y));
  if (d) {
    auto scale = F64DivBits(F64FromI64(GR_SCALE), d);
    sym.snx = F64ToI64(F64MulBits(F64FromI64(sym.snx), scale), 0x0C00);
    sym.sny = F64ToI64(F64MulBits(F64FromI64(sym.sny), scale), 0x0C00);
  }
  sym.sx = x1;
  sym.sy = y1;
  sym.sz = 0;
  return true;
}

extern "C" uint64_t DCSymmetry3SetCpp(CDC *dc, int64_t x1, int64_t y1,
                                      int64_t z1, int64_t x2, int64_t y2,
                                      int64_t z2, int64_t x3, int64_t y3,
                                      int64_t z3)
{
  if (!dc)
    dc = gr.dc;
  uint64_t xx1 = uint64_t(x1) - x2, yy1 = uint64_t(y1) - y2,
           zz1 = uint64_t(z1) - z2, xx2 = uint64_t(x3) - x2,
           yy2 = uint64_t(y3) - y2, zz2 = uint64_t(z3) - z2;
  if ((!xx1 && !yy1 && !zz1) || (!xx2 && !yy2 && !zz2) ||
      (xx1 == xx2 && yy1 == yy2 && zz1 == zz2))
    return false;

  auto x = F64FromI64(yy1 * zz2 - zz1 * yy2),
       y = F64FromI64(zz1 * xx2 - xx1 * zz2),
       z = F64FromI64(xx1 * yy2 - yy1 * xx2);
  if (dc->flags & DCF_TRANSFORMATION) {
    auto mul = [=](const int64_t *r) {
      return F64AddBits(F64AddBits(F64MulBits(x, F64FromI64(r[0])),
                                   F64MulBits(y, F64FromI64(r[1]))),
                        F64MulBits(z, F64FromI64(r[2])));
    };
    auto xx = mul(dc->r), yy = mul(dc->r + 4), zz = mul(dc->r + 8);
    x = xx;
    y = yy;
    z = zz;
  }
  auto d = F64SqrtBits(F64AddBits(
      F64AddBits(F64MulBits(x, x), F64MulBits(y, y)), F64MulBits(z, z)));
  if (d) {
    d = F64DivBits(F64FromI64(GR_SCALE), d);
    dc->sym.snx = F64ToI64(F64MulBits(d, x), 0x0C00);
    dc->sym.sny = F64ToI64(F64MulBits(d, y), 0x0C00);
    dc->sym.snz = F64ToI64(F64MulBits(d, z), 0x0C00);
  }
  CD3I64 p{x1, y1, z1};
  if (dc->flags & DCF_TRANSFORMATION)
    DCTransformPoint(*dc, p);
  dc->sym.sx = p.x;
  dc->sym.sy = p.y;
  dc->sym.sz = p.z;
  return true;
}

extern "C" void DCReflectCpp(CDC *dc, int64_t *_x, int64_t *_y, int64_t *_z)
{
  const auto &s = dc->sym;
  uint64_t x = GrFixed32(*_x), y = GrFixed32(*_y), z = GrFixed32(*_z);
  uint64_t xx = uint64_t(*_x) - s.sx, yy = uint64_t(*_y) - s.sy,
           zz = uint64_t(*_z) - s.sz;
  uint64_t d = int64_t(xx * s.snx + yy * s.sny + zz * s.snz) >> 16;
  uint64_t xn = int64_t(d * s.snx) >> 15, yn = int64_t(d * s.sny) >> 15,
           zn = int64_t(d * s.snz) >> 15;
  xx = x - xn;
  yy = y - yn;
  zz = z - zn;
  uint64_t xx2 = x + xn, yy2 = y + yn, zz2 = z + zn;
  auto distance = [&](uint64_t a, uint64_t b, uint64_t c) {
    a = (int64_t(a) >> 16) - (uint64_t(s.sx) << 16);
    b = (int64_t(b) >> 16) - (uint64_t(s.sy) << 16);
    c = (int64_t(c) >> 16) - (uint64_t(s.sz) << 16);
    return int64_t(a * a + b * b + c * c);
  };
  if (distance(xx, yy, zz) < distance(xx2, yy2, zz2)) {
    *_x = int32_t(xx >> 32);
    *_y = int32_t(yy >> 32);
    *_z = int32_t(zz >> 32);
  } else {
    *_x = int32_t(xx2 >> 32);
    *_y = int32_t(yy2 >> 32);
    *_z = int32_t(zz2 >> 32);
  }
}

extern "C" uint64_t D3I32NormCpp(const CD3I32 *p)
{
  uint64_t x = int64_t(p->x), y = int64_t(p->y), z = int64_t(p->z);
  return F64SqrtBits(F64FromI64(x * x + y * y + z * z));
}

constexpr unsigned CC_LEFT = 1, CC_RIGHT = 2, CC_TOP = 4, CC_BOTTOM = 8;

extern "C" uint64_t ClipLineCpp(int64_t *_x1, int64_t *_y1, int64_t *_x2,
                                int64_t *_y2, int64_t left, int64_t top,
                                int64_t right, int64_t bottom)
{
  int64_t x1 = *_x1, y1 = *_y1, x2 = *_x2, y2 = *_y2;
  auto outcode = [=](int64_t x, int64_t y) -> unsigned {
    return (y > bottom ? CC_BOTTOM
            : y < top  ? CC_TOP
                       : 0) |
           (x > right  ? CC_RIGHT
            : x < left ? CC_LEFT
                       : 0);
  };
  auto cc1 = outcode(x1, y1), cc2 = outcode(x2, y2);
  auto interpolate = [](int64_t a, int64_t b, int64_t c, int64_t d, int64_t k) {
    auto num = static_cast<int64_t>((static_cast<uint64_t>(b) - a) *
                                    (static_cast<uint64_t>(k) - c));
    auto den = static_cast<int64_t>(static_cast<uint64_t>(d) - c);
    auto q = num == INT64_MIN && den == -1 ? INT64_MIN : num / den;
    return static_cast<int64_t>(static_cast<uint64_t>(a) + q);
  };
  while (cc1 | cc2) {
    if (cc1 & cc2)
      return false;
    auto cc = cc1 ? cc1 : cc2;
    int64_t x, y;
    if (cc & (CC_BOTTOM | CC_TOP)) {
      y = cc & CC_BOTTOM ? bottom : top;
      x = interpolate(x1, x2, y1, y2, y);
    } else {
      x = cc & CC_RIGHT ? right : left;
      y = interpolate(y1, y2, x1, x2, x);
    }
    if (cc1) {
      *_x1 = x1 = x;
      *_y1 = y1 = y;
      cc1 = outcode(x, y);
    } else {
      *_x2 = x2 = x;
      *_y2 = y2 = y;
      cc2 = outcode(x, y);
    }
  }
  return true;
}
