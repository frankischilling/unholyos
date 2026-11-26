#include "../../Kernel/KernelB.hpp"
#include "GrExt.hpp"
#include "GrGlbls.hpp"

static void GrNearest(CDC *dc, int64_t x, int64_t y)
{
  uint64_t dx = static_cast<uint64_t>(x) - dc->cur_x;
  uint64_t dy = static_cast<uint64_t>(y) - dc->cur_y;
  int64_t dist = dx * dx + dy * dy;
  if (dist <= dc->nearest_dist)
    dc->nearest_dist = dist;
}

static void GrExtents(CDC *dc, int64_t left, int64_t top, int64_t right,
                      int64_t bottom)
{
  if (left < dc->min_x)
    dc->min_x = left;
  if (right > dc->max_x)
    dc->max_x = right;
  if (top < dc->min_y)
    dc->min_y = top;
  if (bottom > dc->max_y)
    dc->max_y = bottom;
}

extern "C" uint64_t GrPlot0Cpp(CDC *dc, int64_t x, int64_t y)
{
  if (!dc)
    dc = gr.dc;
  CColorROPU32 color = dc->color;
  if (dc->flags & DCF_LOCATE_NEAREST)
    GrNearest(dc, x, y);
  if (dc->flags & DCF_RECORD_EXTENTS)
    GrExtents(dc, x, y, x, y);
  if ((dc->flags & DCF_DONT_DRAW) || !dc->body)
    return true;

  auto d = dc->width_internal * y + x;
  if (dc->depth_buf) {
    auto &z = dc->depth_buf[d];
    if (dc->db_z < 0 || dc->db_z > z)
      return true;
    z = dc->db_z;
  }

  if ((color.c1.rop & ROPBF_PROBABILITY_DITHER)
          ? RandU16Cpp() < dc->dither_probability_u16
          : (color.c1.rop & ROPBF_DITHER) && ((x ^ y) & 1))
    color.c0.color = color.c1.color;

  auto &dst = dc->body[d];
  switch (color.c0.rop) {
  case ROPB_EQU:
  case ROPB_MONO:
    dst = color.c0.color;
    break;
  case ROPB_COLLISION:
    if (dst != TRANSPARENT && dst != dc->bkcolor.c0.color)
      dc->collision_cnt++;
    break;
  case ROPB_XOR:
    dst ^= color.c0.color;
    break;
  }
  return true;
}

extern "C" int64_t GrPeek0Cpp(CDC *dc, int64_t x, int64_t y)
{
  if (!dc)
    dc = gr.dc;
  return dc->body ? dc->body[dc->width_internal * y + x] : -1;
}

struct CGrClip
{
  int64_t x, y, w1, h1, w2, h2;
};

static bool GrClipArea(CDC *dc, int64_t x, int64_t y, int64_t w, int64_t h,
                       bool rect, CGrClip &out)
{
  auto right = static_cast<int64_t>(dc->width);
  auto bottom = static_cast<int64_t>(dc->height);
  const bool screen = dc->flags & DCF_SCRN_BITMAP;
  auto *win_task = dc->win_task;
  if (screen) {
    x = static_cast<uint64_t>(x) + win_task->scroll_x;
    y = static_cast<uint64_t>(y) + win_task->scroll_y;
  }
  // Source clipping starts in window coordinates, before applying the origin.
  __int128 w1 = x < 0 ? -static_cast<__int128>(x) : 0;
  __int128 h1 = y < 0 ? -static_cast<__int128>(y) : 0;
  if (screen) {
    x = static_cast<uint64_t>(x) + win_task->pix_left;
    y = static_cast<uint64_t>(y) + win_task->pix_top;
    if (win_task->pix_right < right - 1)
      right = win_task->pix_right + 1;
    if (win_task->pix_bottom < bottom - 1)
      bottom = win_task->pix_bottom + 1;
    if (static_cast<__int128>(x) + w1 < 0)
      w1 = -static_cast<__int128>(x);
    if (static_cast<__int128>(y) + h1 < 0)
      h1 = -static_cast<__int128>(y);
  }
  if (dc->flags & DCF_LOCATE_NEAREST) {
    if (rect && x <= dc->cur_x && static_cast<__int128>(x) + w >= dc->cur_x &&
        y <= dc->cur_y && static_cast<__int128>(y) + h >= dc->cur_y) {
      if (dc->nearest_dist >= 0)
        dc->nearest_dist = 0;
    } else
      GrNearest(dc, static_cast<int64_t>(static_cast<uint64_t>(x) + (w >> 1)),
                static_cast<int64_t>(static_cast<uint64_t>(y) + (h >> 1)));
  }
  __int128 w2 = static_cast<__int128>(right) - x;
  __int128 h2 = static_cast<__int128>(bottom) - y;
  if (w2 > w)
    w2 = w;
  if (h2 > h)
    h2 = h;
  if (w1 >= w2 || h1 >= h2)
    return false;
  out = {x,
         y,
         static_cast<int64_t>(w1),
         static_cast<int64_t>(h1),
         static_cast<int64_t>(w2),
         static_cast<int64_t>(h2)};
  if (dc->flags & DCF_RECORD_EXTENTS)
    GrExtents(dc, x + out.w1, y + out.h1, x + out.w2 - 1, y + out.h2 - 1);
  return true;
}

extern "C" int64_t GrBlotCpp(CDC *dc, int64_t x, int64_t y, CDC *img)
{
  if (!dc)
    dc = gr.dc;
  CGrClip c;
  if (!GrClipArea(dc, x, y, img->width, img->height, false, c))
    return 0;
  if ((dc->flags & DCF_DONT_DRAW) || !dc->body || !img->body)
    return 1;

  const auto old_color = dc->color;
  auto *db = dc->depth_buf;
  dc->depth_buf = nullptr;
  dc->color.c1.rop &= ~ROPBF_DITHER;
  const auto color = dc->color.c0;
  const bool opaque = img->flags & DCF_NO_TRANSPARENTS;
  for (int64_t j = c.h1; j < c.h2; j++) {
    const auto *src = img->body + j * img->width_internal + c.w1;
    auto *dst = dc->body + (j + c.y) * dc->width_internal + c.x + c.w1;
    const int64_t row = j + c.y;
    for (int64_t i = c.w1; i < c.w2;) {
      const int64_t col = i + c.x;
      if (color.rop == ROPB_COLLISION) {
        if (*dst != TRANSPARENT && *dst != dc->bkcolor.c0.color &&
            *src != TRANSPARENT)
          dc->collision_cnt++;
      } else if (opaque) {
        // The fast path reads a destination-aligned word before writing it.
        // Keep that sampling order when two contexts share the same body.
        auto count = 8 - (col & 7);
        if (count > c.w2 - i)
          count = c.w2 - i;
        if (GrPixelVisible(dc, col, row, false)) {
          uint8_t pixels[8];
          MemCpy(pixels, src, count);
          for (int64_t k = 0; k < count; k++) {
            if (color.rop == ROPB_EQU)
              dst[k] = pixels[k];
            else if (color.rop == ROPB_XOR)
              dst[k] ^= pixels[k];
            else if (color.rop == ROPB_MONO)
              dst[k] = pixels[k] & color.color;
          }
        }
        src += count;
        dst += count;
        i += count;
        continue;
      } else if (GrPixelVisible(dc, col, row, false)) {
        if (color.rop == ROPB_MONO) {
          if (*src)
            GrPlot0Cpp(dc, col, row);
        } else if ((color.rop == ROPB_EQU || color.rop == ROPB_XOR) &&
                   *src != TRANSPARENT) {
          dc->color.c0.color = *src;
          GrPlot0Cpp(dc, col, row);
        }
      }
      src++;
      dst++;
      i++;
    }
  }
  dc->depth_buf = db;
  dc->color = old_color;
  return 1;
}

extern "C" int64_t GrRectCpp(CDC *dc, int64_t x, int64_t y, int64_t w,
                             int64_t h)
{
  if (!dc)
    dc = gr.dc;
  CGrClip c;
  if (!GrClipArea(dc, x, y, w, h, true, c))
    return 0;
  if ((dc->flags & DCF_DONT_DRAW) || !dc->body)
    return 1;

  const auto color = dc->color;
  int64_t res = 0;
  if (color.c0.rop == ROPB_COLLISION) {
    for (int64_t j = c.h1; j < c.h2; j++) {
      const auto *dst = dc->body + (j + c.y) * dc->width_internal + c.x + c.w1;
      for (int64_t i = c.w1; i < c.w2; i++, dst++)
        if (*dst != TRANSPARENT && *dst != dc->bkcolor.c0.color)
          res++;
    }
    dc->collision_cnt += res;
    return res;
  }
  if (color.c0.rop != ROPB_EQU && color.c0.rop != ROPB_XOR &&
      color.c0.rop != ROPB_MONO)
    return 0;

  const bool probability = color.c1.rop & ROPBF_PROBABILITY_DITHER;
  const bool dither = !probability && (color.c1.rop & ROPBF_DITHER);
  bool alternate = probability && RandU16Cpp() < dc->dither_probability_u16;
  for (int64_t j = c.h1; j < c.h2; j++) {
    const auto row = j + c.y;
    auto *dst = dc->body + row * dc->width_internal + c.x + c.w1;
    for (int64_t i = c.w1; i < c.w2; i++, dst++) {
      const auto col = i + c.x;
      if (!GrPixelVisible(dc, col, row, false))
        continue;
      auto ink = (dither ? ((col ^ row) & 1) != 0 : alternate) ? color.c1.color
                                                               : color.c0.color;
      if (color.c0.rop == ROPB_XOR)
        *dst ^= ink;
      else
        *dst = ink;
      res++;
    }
    // The reference consumes a random value even after the last row.
    alternate = probability && RandU16Cpp() < dc->dither_probability_u16;
  }
  return res;
}
