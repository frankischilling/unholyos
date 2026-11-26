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
  if (dc->brush) {
    const auto old_color = dc->color;
    if (dc->color.c0.rop != ROPB_COLLISION)
      dc->color.c0.rop = ROPB_MONO;
    GrBlotCpp(dc, x, y, dc->brush);
    dc->color = old_color;
  } else if (GrClipPoint(dc, x, y))
    GrPlot0Cpp(dc, x, y);
  dc->depth_buf = db;
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
