#include "../../Kernel/KernelB.hpp"
#include "GrExt.hpp"
#include "GrGlbls.hpp"

bool Line(CDC *dc, int64_t x1, int64_t y1, int64_t z1, int64_t x2, int64_t y2,
          int64_t z2, GrLinePlotFn fp_plot, int64_t step, int64_t start)
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
    if ((first || x != nx || y != ny || z != nz) && !fp_plot(dc, nx, ny, nz))
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
         fp_plot(dc, x2, y2, z2);
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
