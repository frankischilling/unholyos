#include "../../Kernel/KernelB.hpp"
#include "GrExt.hpp"
#include "GrGlbls.hpp"

extern "C" uint64_t GrClampCpp(CDC *dc, int64_t *left, int64_t *top,
                               int64_t *right, int64_t *bottom, int64_t width,
                               int64_t height)
{
  if (!dc)
    dc = gr.dc;
  *left = *top = 0;
  *right = static_cast<int64_t>(dc->width) - 1;
  *bottom = static_cast<int64_t>(dc->height) - 1;
  if (dc->flags & DCF_SCRN_BITMAP) {
    auto *task = dc->win_task;
    if (GR_WIDTH - 1 < *right)
      *right = GR_WIDTH - 1;
    if (GR_HEIGHT - 1 < *bottom)
      *bottom = GR_HEIGHT - 1;
    if (task->pix_left > *left)
      *left = task->pix_left;
    if (task->pix_top > *top)
      *top = task->pix_top;
    if (task->pix_right < *right)
      *right = task->pix_right;
    if (task->pix_bottom < *bottom)
      *bottom = task->pix_bottom;
  }
  *left -= static_cast<uint64_t>(width);
  *right += static_cast<uint64_t>(width);
  *top -= static_cast<uint64_t>(height);
  *bottom += static_cast<uint64_t>(height);
  return *left <= *right && *top <= *bottom;
}

static void GrWinCoords(const CTask *task, int64_t &x, int64_t &y)
{
  x = static_cast<uint64_t>(x) + task->pix_left + task->scroll_x;
  y = static_cast<uint64_t>(y) + task->pix_top + task->scroll_y;
}

static bool DCClipLine(CDC *dc, int64_t *x1, int64_t *y1, int64_t *x2,
                       int64_t *y2)
{
  int64_t left, top, right, bottom;
  if (!GrClampCpp(dc, &left, &top, &right, &bottom, 0, 0))
    return false;
  if (dc->flags & DCF_SCRN_BITMAP) {
    GrWinCoords(dc->win_task, *x1, *y1);
    GrWinCoords(dc->win_task, *x2, *y2);
  }
  return ClipLineCpp(x1, y1, x2, y2, left, top, right, bottom);
}

bool GrPixelVisible(const CDC *dc, int64_t x, int64_t y, bool skip_top_task)
{
  if (!(dc->flags & DCF_SCRN_BITMAP) || (dc->flags & DCF_ON_TOP))
    return true;
  auto *task = dc->win_task;
  // The shortcut applies once the window manager has prepared its z-order.
  return (skip_top_task && gr.highest_uncovered &&
          task->next_task == sys_winmgr_task) ||
         !IsPixCovered0Cpp(task, x, y);
}

static bool GrClipPoint(CDC *dc, int64_t &x, int64_t &y)
{
  if (dc->flags & DCF_SCRN_BITMAP) {
    auto *task = dc->win_task;
    GrWinCoords(task, x, y);
    if (x < task->pix_left || x > task->pix_right || y < task->pix_top ||
        y > task->pix_bottom)
      return false;
  }
  return 0 <= x && x < dc->width && 0 <= y && y < dc->height &&
         GrPixelVisible(dc, x, y, true);
}

extern "C" uint64_t GrPlotCpp(CDC *dc, int64_t x, int64_t y)
{
  if (!dc)
    dc = gr.dc;
  auto *db = dc->depth_buf;
  dc->depth_buf = nullptr;
  auto res = GrPlot1(dc, x, y);
  dc->depth_buf = db;
  return res;
}

bool GrPlot1(CDC *dc, int64_t x, int64_t y)
{
  if (dc->brush) {
    const auto old_color = dc->color;
    if (dc->color.c0.rop != ROPB_COLLISION)
      dc->color.c0.rop = ROPB_MONO;
    if (dc->depth_buf)
      GrBlot3Cpp(dc, x, y, dc->db_z, dc->brush);
    else
      GrBlotCpp(dc, x, y, dc->brush);
    dc->color = old_color;
  } else if (GrClipPoint(dc, x, y))
    GrPlot0Cpp(dc, x, y);
  return true;
}

extern "C" int64_t GrPeekCpp(CDC *dc, int64_t x, int64_t y)
{
  if (!dc)
    dc = gr.dc;
  return GrClipPoint(dc, x, y) ? GrPeek0Cpp(dc, x, y) : -1;
}

static int64_t GrFillCircleRows(CDC *dc, int64_t cx, int64_t cy, int64_t z,
                                int64_t diameter, bool vertical, int64_t i,
                                int64_t end)
{
  const int64_t r = diameter >> 1;
  const uint64_t rr = uint64_t(r + 1) * (r + 1);
  const int64_t center = vertical ? cy : cx, across = vertical ? cx : cy;
  int64_t res = 0;
  for (; i < end; i++) {
    int64_t lo, hi, axis = uint64_t(across) + i - r;
    if (diameter < GR_PEN_BRUSHES_NUM) {
      lo = uint64_t(center) + gr.circle_lo[diameter][i];
      hi = uint64_t(center) + gr.circle_hi[diameter][i];
    } else {
      uint64_t dy = i - r;
      auto root = F64SqrtBits(F64FromI64(rr - dy * dy));
      auto mid = F64FromI64(center);
      lo = F64ToI64(F64AddBits(mid, root ^ (1ULL << 63)), 0x0C00);
      hi = F64ToI64(F64AddBits(mid, root), 0x0C00);
    }
    res += vertical ? GrVLineCpp(dc, axis, lo, hi, z, z)
                    : GrHLineCpp(dc, lo, hi, axis, z, z);
  }
  return res;
}

static int64_t GrFillSemiCircle(CDC *dc, int64_t cx, int64_t cy, int64_t z,
                                int64_t diameter, bool vertical, bool upper)
{
  const auto end =
      int64_t(uint64_t(diameter) + (diameter >= GR_PEN_BRUSHES_NUM));
  const auto r = diameter >> 1;
  return GrFillCircleRows(dc, cx, cy, z, diameter, vertical, upper ? 0 : r + 1,
                          upper ? r : end);
}

extern "C" int64_t GrFillCircleCpp(CDC *dc, int64_t cx, int64_t cy, int64_t z,
                                   int64_t diameter)
{
  if (diameter < 1)
    return 0;
  const auto end =
      int64_t(uint64_t(diameter) + (diameter >= GR_PEN_BRUSHES_NUM));
  return GrFillCircleRows(dc, cx, cy, z, diameter, false, 0, end);
}

extern "C" bool GrPlot3BCpp(CDC *dc, int64_t x, int64_t y, int64_t z)
{
  if (!dc)
    dc = gr.dc;
  CD3I64 p{x, y, z};
  const auto was_transform = dc->flags & DCF_TRANSFORMATION;
  if (was_transform) {
    DCTransformPoint(*dc, p);
    dc->flags &= ~DCF_TRANSFORMATION;
  }
  const auto was_symmetry = dc->flags & DCF_SYMMETRY;
  if (was_symmetry) {
    auto mirror = p;
    DCReflectCpp(dc, &mirror.x, &mirror.y, &mirror.z);
    dc->flags &= ~DCF_SYMMETRY;
    dc->db_z = mirror.z;
    GrPlot1(dc, mirror.x, mirror.y);
  }
  if (!was_symmetry || !(dc->flags & DCF_JUST_MIRROR)) {
    dc->db_z = p.z;
    GrPlot1(dc, p.x, p.y);
  }
  dc->flags |= was_transform | was_symmetry;
  return true;
}

extern "C" bool GrPlot3Cpp(CDC *dc, int64_t x, int64_t y, int64_t z)
{
  if (!dc)
    dc = gr.dc;
  const auto old_color = dc->color;
  CD3I64 p{x, y, z};
  const auto was_transform = dc->flags & DCF_TRANSFORMATION;
  if (was_transform) {
    DCTransformPoint(*dc, p);
    dc->flags &= ~DCF_TRANSFORMATION;
  }
  const auto was_symmetry = dc->flags & DCF_SYMMETRY;
  if (was_symmetry) {
    auto mirror = p;
    DCReflectCpp(dc, &mirror.x, &mirror.y, &mirror.z);
    dc->flags &= ~DCF_SYMMETRY;
    GrPlot3Cpp(dc, mirror.x, mirror.y, mirror.z);
  }
  if (!was_symmetry || !(dc->flags & DCF_JUST_MIRROR)) {
    x = p.x;
    y = p.y;
    dc->db_z = p.z;
    const auto w = dc->thick >> 1;
    if (dc->brush || w <= 0)
      GrPlot1(dc, x, y);
    else if (dc->thick < int32_t(GR_PEN_BRUSHES_NUM)) {
      const auto thick = dc->thick;
      if (dc->color.c0.rop != ROPB_COLLISION)
        dc->color.c0.rop = ROPB_MONO;
      const int64_t left = uint64_t(x) - w, top = uint64_t(y) - w;
      auto blot = [&](CDC *pen) {
        return dc->depth_buf ? GrBlot3Cpp(dc, left, top, p.z, pen)
                             : GrBlotCpp(dc, left, top, pen);
      };
      bool record;
      if (dc->color.c1.rop & ROPBF_DITHER) {
        dc->color.c1.rop = dc->color.c0.rop;
        const bool odd = (left ^ top) & 1;
        blot(odd ? gr.odd_pen_brushes[thick] : gr.even_pen_brushes[thick]);
        dc->color.c0 = dc->color.c1;
        record =
            blot(odd ? gr.even_pen_brushes[thick] : gr.odd_pen_brushes[thick]);
      } else if (dc->color.c0.rop == ROPB_COLLISION)
        record = dc->color.c0.color != dc->bkcolor.c0.color &&
                 dc->color.c0.color != TRANSPARENT &&
                 blot(gr.collision_pen_brushes[thick]);
      else
        record = blot(gr.pen_brushes[thick]);
      if (record) {
        if (dc->flags & DCF_SCRN_BITMAP)
          GrWinCoords(dc->win_task, x, y);
        if (dc->flags & DCF_LOCATE_NEAREST)
          GrNearest(dc, x, y);
        if (dc->flags & DCF_RECORD_EXTENTS) {
          const int64_t hi = w - !(thick & 1);
          GrExtents(dc, uint64_t(x) - w, uint64_t(y) - w, uint64_t(x) + hi,
                    uint64_t(y) + hi);
        }
      }
    } else
      GrFillCircleCpp(dc, x, y, p.z, dc->thick);
  }
  dc->color = old_color;
  dc->flags |= was_transform | was_symmetry;
  return true;
}

static bool GrLinePlot0(CDC *dc, int64_t x, int64_t y, int64_t z)
{
  if (GrPixelVisible(dc, x, y, true)) {
    dc->db_z = z;
    GrPlot0Cpp(dc, x, y);
  }
  return true;
}

static bool GrLinePlot(CDC *dc, int64_t x, int64_t y, int64_t z)
{
  dc->db_z = z;
  return GrPlotCpp(dc, x, y);
}

extern "C" uint64_t GrLineCpp(CDC *dc, int64_t x1, int64_t y1, int64_t x2,
                              int64_t y2, int64_t step, int64_t start)
{
  if (!dc)
    dc = gr.dc;
  auto *db = dc->depth_buf;
  dc->depth_buf = nullptr;
  bool res;
  if (step == 1 && !start && !dc->brush)
    res = DCClipLine(dc, &x1, &y1, &x2, &y2) &&
          Line(dc, x1, y1, 0, x2, y2, 0, GrLinePlot0, step, start);
  else
    res = Line(dc, x1, y1, 0, x2, y2, 0, GrLinePlot, step, start);
  dc->depth_buf = db;
  return res;
}

static int64_t GrLineFat3(CDC *dc, const CD3I64 &p1, const CD3I64 &p2,
                          int64_t width, int64_t start)
{
  if (width <= 0)
    return 0;
  int64_t dx = uint64_t(p2.x) - p1.x, dy = uint64_t(p2.y) - p1.y,
          dz = uint64_t(p2.z) - p1.z;
  const auto adx = AbsI64(dx), ady = AbsI64(dy), adz = AbsI64(dz);
  const bool vertical = adx >= ady;
  const auto planar = vertical ? adx : ady;
  const bool z_axis = adz > planar;
  const auto distance = z_axis ? adz : planar;
  if (distance > INT64_MAX)
    return 0;
  const auto d = int64_t(distance);
  if (d) {
    dx = !z_axis && vertical ? (dx < 0 ? -GR_SCALE : GR_SCALE)
                             : GrFixed32(dx) / d;
    dy = !z_axis && !vertical ? (dy < 0 ? -GR_SCALE : GR_SCALE)
                              : GrFixed32(dy) / d;
    dz = z_axis ? (dz < 0 ? -GR_SCALE : GR_SCALE) : GrFixed32(dz) / d;
  }

  const auto skip = start > 0 ? uint64_t(start) : 0;
  uint64_t x = GrFixed32(p1.x) + skip * dx, y = GrFixed32(p1.y) + skip * dy,
           z = GrFixed32(p1.z) + skip * dz;
  auto cap = [&](bool upper) {
    return GrFillSemiCircle(dc, int32_t(x >> 32), int32_t(y >> 32),
                            int32_t(z >> 32), width, vertical, upper);
  };
  if (start >= d)
    return GrFillCircleCpp(dc, int32_t(x >> 32), int32_t(y >> 32),
                           int32_t(z >> 32), width);

  int64_t res = 0;
  if (width == 1) {
    for (int64_t i = start; i <= d; i++) {
      dc->db_z = int32_t(z >> 32);
      res += GrPlot1(dc, int32_t(x >> 32), int32_t(y >> 32));
      x += dx;
      y += dy;
      z += dz;
    }
    return res;
  }

  auto length = F64SqrtBits(F64FromI64(adx * adx + ady * ady));
  auto span = F64ToI64(
      F64DivBits(F64MulBits(F64FromI64(width), length), F64FromI64(planar)),
      0x0C00);
  const int64_t lo = span >> 1, hi = int64_t(uint64_t(span) - 1) >> 1;
  const bool forward = (vertical ? dx : dy) >= 0;
  res += cap(forward);
  for (int64_t i = start; i <= d; i++) {
    const int64_t ix = int32_t(x >> 32), iy = int32_t(y >> 32),
                  iz = int32_t(z >> 32);
    res += vertical ? GrVLineCpp(dc, ix, iy - lo, iy + hi, iz, iz)
                    : GrHLineCpp(dc, ix - lo, ix + hi, iy, iz, iz);
    x += dx;
    y += dy;
    z += dz;
  }
  x -= dx;
  y -= dy;
  z -= dz;
  return res + cap(!forward);
}

static bool GrLine3Pass(CDC *dc, CD3I64 p1, CD3I64 p2, int64_t step,
                        int64_t start)
{
  if (step != 1 || dc->brush)
    return Line(dc, p1.x, p1.y, p1.z, p2.x, p2.y, p2.z, GrPlot3Cpp, step,
                start);
  if (!start && dc->thick < 2 && !dc->depth_buf)
    return DCClipLine(dc, &p1.x, &p1.y, &p2.x, &p2.y) &&
           Line(dc, p1.x, p1.y, 0, p2.x, p2.y, 0, GrLinePlot0, step, start);
  return GrLineFat3(dc, p1, p2, dc->thick, start) != 0;
}

extern "C" bool GrLine3Cpp(CDC *dc, int64_t x1, int64_t y1, int64_t z1,
                           int64_t x2, int64_t y2, int64_t z2, int64_t step,
                           int64_t start)
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
    res = GrLine3Pass(dc, m1, m2, step, start);
  }
  if (!was_symmetry || !(dc->flags & DCF_JUST_MIRROR))
    res |= GrLine3Pass(dc, p1, p2, step, start);
  dc->flags |= was_transform | was_symmetry;
  return res;
}

static int64_t GrBlot3Distance(const CD3I64 &delta)
{
  auto abs_i64 = [](int64_t value) {
    return value < 0 ? -uint64_t(value) : uint64_t(value);
  };
  auto x = abs_i64(delta.x), y = abs_i64(delta.y), z = abs_i64(delta.z);
  auto d = x > y ? x : y;
  if (d < z)
    d = z;
  return d <= INT64_MAX / 2 ? int64_t(d) : 0;
}

static bool GrBlot3Pass(CDC &dc, CDC &img, const CD3I64 &p1, const CD3I64 &p2,
                        const CD3I64 &p3, CColorROPU32 old_color)
{
  CD3I64 dp1{int64_t(uint64_t(p2.x) - p1.x), int64_t(uint64_t(p2.y) - p1.y),
             int64_t(uint64_t(p2.z) - p1.z)};
  CD3I64 dp2{int64_t(uint64_t(p3.x) - p1.x), int64_t(uint64_t(p3.y) - p1.y),
             int64_t(uint64_t(p3.z) - p1.z)};
  auto d1 = GrBlot3Distance(dp1), d2 = GrBlot3Distance(dp2);
  if (!d1 || !d2)
    return false;
  // TempleOS doubles both sampling axes when either projected length changes.
  if (d1 != img.width || d2 != img.height) {
    d1 *= 2;
    d2 *= 2;
  }
  auto fixed = [](int64_t value) { return int64_t(uint64_t(value) << 32); };
  dp1 = {fixed(dp1.x) / d1, fixed(dp1.y) / d1, fixed(dp1.z) / d1};
  dp2 = {fixed(dp2.x) / d2, fixed(dp2.y) / d2, fixed(dp2.z) / d2};
  auto dw = fixed(img.width) / d1, dh = fixed(img.height) / d2;
  dc.brush = nullptr;
  // The previous destination pixel carries across columns.
  bool first = true;
  int64_t last_x = 0, last_y = 0;
  uint64_t x = 0, fx = fixed(p1.x), fy = fixed(p1.y), fz = fixed(p1.z);
  for (int64_t j = 0; j <= d1; j++) {
    uint64_t y = 0, x2 = fx, y2 = fy, z2 = fz;
    for (int64_t i = 0; i <= d2; i++) {
      int64_t nx = int32_t(x2 >> 32), ny = int32_t(y2 >> 32);
      if (first || nx != last_x || ny != last_y) {
        auto color = GrPeekCpp(&img, int32_t(x >> 32), int32_t(y >> 32));
        const bool mono = dc.color.c0.rop == ROPB_MONO;
        if (color >= 0 && (mono ? color != 0 : color != TRANSPARENT)) {
          if (mono) {
            dc.color = old_color;
            dc.color.c1.rop &= ~ROPBF_DITHER;
          } else {
            dc.color = {};
            dc.color.c0.rop = old_color.c0.rop;
            dc.color.c0.color = color;
          }
          if (dc.depth_buf) {
            dc.db_z = int32_t(z2 >> 32);
            GrPlot1(&dc, nx, ny);
          } else
            GrPlotCpp(&dc, nx, ny);
        }
      }
      first = false;
      last_x = nx;
      last_y = ny;
      x2 += dp2.x;
      y2 += dp2.y;
      z2 += dp2.z;
      y += dh;
    }
    fx += dp1.x;
    fy += dp1.y;
    fz += dp1.z;
    x += dw;
  }
  return true;
}

extern "C" int64_t GrBlot3Cpp(CDC *dc, int64_t x1, int64_t y1, int64_t z1,
                              CDC *img)
{
  if (!dc)
    dc = gr.dc;
  auto old_color = dc->color;
  auto *old_brush = dc->brush;
  int64_t res = 0;
  if (dc->depth_buf || (dc->flags & (DCF_TRANSFORMATION | DCF_SYMMETRY))) {
    CD3I64 p1{x1, y1, z1}, p2{int64_t(uint64_t(x1) + img->width), y1, z1},
        p3{x1, int64_t(uint64_t(y1) + img->height), z1};
    if (dc->flags & DCF_TRANSFORMATION) {
      DCTransformPoint(*dc, p1);
      DCTransformPoint(*dc, p2);
      DCTransformPoint(*dc, p3);
    }
    if (dc->flags & DCF_SYMMETRY) {
      auto a = p1, b = p2, c = p3;
      DCReflectCpp(dc, &a.x, &a.y, &a.z);
      DCReflectCpp(dc, &b.x, &b.y, &b.z);
      DCReflectCpp(dc, &c.x, &c.y, &c.z);
      res = GrBlot3Pass(*dc, *img, a, b, c, old_color);
    }
    if (!(dc->flags & DCF_SYMMETRY) || !(dc->flags & DCF_JUST_MIRROR))
      res |= GrBlot3Pass(*dc, *img, p1, p2, p3, old_color);
  } else
    res = GrBlotCpp(dc, x1, y1, img);
  dc->color = old_color;
  dc->brush = old_brush;
  return res;
}

struct CGrTriPoint
{
  int64_t along, across, z;
};

struct CGrTriEdge
{
  uint64_t across, z;
  int64_t d_across = 0, dz = 0;

  void Step(const CGrTriPoint &from, const CGrTriPoint &to, int64_t count)
  {
    d_across =
        count ? int64_t(uint64_t(to.across - from.across) << 32) / count : 0;
    dz = count ? int64_t(uint64_t(to.z - from.z) << 32) / count : 0;
  }

  void Advance(int64_t count)
  {
    across += uint64_t(d_across) * count;
    z += uint64_t(dz) * count;
  }
};

extern "C" int64_t GrFillTri0Cpp(CDC *dc, const CD3I32 *p1, const CD3I32 *p2,
                                 const CD3I32 *p4)
{
  if (!dc)
    dc = gr.dc;
  auto abs = [](int64_t n) { return n < 0 ? -n : n; };
  bool vertical = abs(int64_t(p1->y) - p2->y) + abs(int64_t(p1->y) - p4->y) >
                  abs(int64_t(p1->x) - p2->x) + abs(int64_t(p1->x) - p4->x);
  auto point = [vertical](const CD3I32 &p) -> CGrTriPoint {
    return vertical ? CGrTriPoint{p.x, p.y, p.z} : CGrTriPoint{p.y, p.x, p.z};
  };
  auto a = point(*p1), b = point(*p2), c = point(*p4);
  auto swap = [](CGrTriPoint &p, CGrTriPoint &q) {
    auto temp = p;
    p = q;
    q = temp;
  };
  if (c.across < b.across)
    swap(c, b);
  if (b.across < a.across)
    swap(b, a);
  if (c.along < b.along)
    swap(c, b);

  int64_t min = 0, max = vertical ? dc->width : dc->height;
  if (dc->flags & DCF_SCRN_BITMAP) {
    const auto &win = *dc->win_task;
    int64_t origin = vertical ? uint64_t(win.scroll_x) + win.pix_left
                              : uint64_t(win.scroll_y) + win.pix_top;
    int64_t end = vertical ? win.pix_right : win.pix_bottom;
    if (max > end)
      max = end;
    min -= uint64_t(origin);
    max -= uint64_t(origin);
  }

  CGrTriEdge e1{}, e2{};
  int64_t scan = 0, res = 0;
  auto start = [&](const CGrTriPoint &p) {
    scan = p.along;
    e2 = e1 = {uint64_t(p.across) << 32, uint64_t(p.z) << 32};
  };
  auto advance = [&](int64_t count, int64_t direction) {
    scan += count * direction;
    e1.Advance(count);
    e2.Advance(count);
  };
  auto sweep = [&](int64_t count, int64_t direction) {
    for (int64_t i = 0; i < count; i++) {
      int64_t v1 = int32_t(e1.across >> 32), v2 = int32_t(e2.across >> 32);
      int64_t z1 = int32_t(e1.z >> 32), z2 = int32_t(e2.z >> 32);
      res += vertical ? GrVLineCpp(dc, scan, v1, v2, z1, z2)
                      : GrHLineCpp(dc, v1, v2, scan, z1, z2);
      advance(1, direction);
    }
  };
  auto clip = [&](int64_t &count, int64_t direction, bool first_half) {
    int64_t skip = direction > 0 ? uint64_t(scan) + count - max + 1
                                 : uint64_t(min) - scan + count;
    if (skip > count)
      return false;
    if (skip > 0)
      count -= skip;
    skip = direction > 0 ? uint64_t(min) - scan : uint64_t(scan) - max + 1;
    if (skip > count) {
      if (!first_half)
        return false;
      skip = count;
    }
    if (skip > 0) {
      count -= skip;
      advance(skip, direction);
    }
    return true;
  };

  if (c.along < a.along || b.along > a.along) {
    int64_t direction = 1;
    if (c.along < a.along) {
      direction = -1;
      swap(b, c);
    }
    int64_t count = (b.along - a.along) * direction;
    start(a);
    e1.Step(a, c, (c.along - a.along) * direction);
    e2.Step(a, b, count);
    if (!clip(count, direction, true))
      return res;
    sweep(count, direction);

    count = (c.along - b.along) * direction;
    if (count) {
      e2.Step(b, c, count);
      if (!clip(count, direction, false))
        return res;
    }
    sweep(count + 1, direction);
  } else {
    // The two opposing sweeps can both draw the row through vertex a.
    if (int64_t count = a.along - b.along) {
      start(b);
      e1.Step(b, a, count);
      e2.Step(b, c, c.along - b.along);
      if (scan < min) {
        int64_t skip = uint64_t(min) - scan;
        if (skip > count)
          skip = count;
        count -= skip;
        advance(skip, 1);
      }
      int64_t available = uint64_t(max) - scan;
      if (available > 0)
        sweep(count < available ? count + 1 : available, 1);
    }
    if (int64_t count = c.along - a.along) {
      start(c);
      e1.Step(c, a, count);
      e2.Step(c, b, c.along - b.along);
      if (clip(count, -1, false))
        sweep(count + 1, -1);
    }
  }
  return res;
}
