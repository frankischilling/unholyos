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
