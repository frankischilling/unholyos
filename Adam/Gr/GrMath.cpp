#include "../../Kernel/KernelB.hpp"
#include "GrExt.hpp"

static uint64_t AbsI64(int64_t n)
{
  return n < 0 ? -static_cast<uint64_t>(n) : n;
}

static int64_t GrFixed32(int64_t n)
{
  return static_cast<uint64_t>(n) << 32;
}

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
