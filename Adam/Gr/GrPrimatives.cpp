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
