#include "../../Kernel/KernelB.hpp"
#include "GrExt.hpp"
#include "GrGlbls.hpp"

void GrNearest(CDC *dc, int64_t x, int64_t y)
{
  uint64_t dx = static_cast<uint64_t>(x) - dc->cur_x;
  uint64_t dy = static_cast<uint64_t>(y) - dc->cur_y;
  int64_t dist = dx * dx + dy * dy;
  if (dist <= dc->nearest_dist)
    dc->nearest_dist = dist;
}

void GrExtents(CDC *dc, int64_t left, int64_t top, int64_t right,
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

static void GrWritePixel(CDC &dc, uint8_t &dst, CColorROPU16 color)
{
  switch (color.rop) {
  case ROPB_EQU:
  case ROPB_MONO:
    dst = color.color;
    break;
  case ROPB_COLLISION:
    if (dst != TRANSPARENT && dst != dc.bkcolor.c0.color)
      dc.collision_cnt++;
    break;
  case ROPB_XOR:
    dst ^= color.color;
    break;
  }
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

  GrWritePixel(*dc, dc->body[d], color.c0);
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

extern "C" CDC *DCExtCpp(CDC *dc,int64_t x1,int64_t y1,int64_t x2,int64_t y2,
                          CTask *task)
{
  if (!dc) dc=gr.dc;
  if (x1>x2) {int64_t i=x1; x1=x2; x2=i;}
  if (y1>y2) {int64_t i=y1; y1=y2; y2=i;}
  if (dc->flags&DCF_SCRN_BITMAP) {
    CTask *win_task=dc->win_task;
    uint64_t x=uint64_t(win_task->pix_left)+win_task->scroll_x;
    uint64_t y=uint64_t(win_task->pix_top)+win_task->scroll_y;
    x1+=x; x2+=x;
    y1+=y; y2+=y;
  }
  CDC *res=DCNewCpp(uint64_t(x2)-x1+1,uint64_t(y2)-y1+1,task,false);
  if (res) {
    DCFillCpp(res,TRANSPARENT);
    GrBlotCpp(res,-uint64_t(x1),-uint64_t(y1),dc);
  }
  return res;
}

static bool GrDiffRow(const uint8_t *base,const uint8_t *update,int64_t width)
{
  for (int64_t i=0;i<width;i++)
    if (base[i]!=update[i])
      return true;
  return false;
}

extern "C" CDC *DCDiffCpp(CDC *base,CDC *update)
{
  if (!base || !update || !base->body || !update->body ||
      base->width!=update->width || base->height!=update->height ||
      base->width_internal!=update->width_internal ||
      update->width<=0 || update->height<=0)
    return nullptr;
  int64_t x1=0,y1=0,x2=update->width-1,y2=update->height-1;
  const uint8_t *ptr_base=base->body,*ptr_update=update->body;
  // The reference's vertical scans advance by the logical width, not stride.
  while (y1<=y2) {
    if (GrDiffRow(ptr_base,ptr_update,update->width))
      break;
    ptr_base+=update->width;
    ptr_update+=update->width;
    y1++;
  }
  if (y1>y2)
    return nullptr;
  ptr_base=base->body+static_cast<int64_t>(base->width_internal)*base->height;
  ptr_update=update->body+static_cast<int64_t>(update->width_internal)*update->height;
  while (y1<y2) {
    ptr_base-=update->width;
    ptr_update-=update->width;
    if (GrDiffRow(ptr_base,ptr_update,update->width))
      break;
    y2--;
  }
  while (x1<x2) {
    for (int64_t i=y1;i<=y2;i++)
      if (GrPeek0Cpp(base,x1,i)!=GrPeek0Cpp(update,x1,i))
        goto df_x2;
    x1++;
  }
df_x2:
  while (x1<x2) {
    for (int64_t i=y1;i<=y2;i++)
      if (GrPeek0Cpp(base,x2,i)!=GrPeek0Cpp(update,x2,i))
        goto df_done;
    x2--;
  }
df_done:
  CDC *res=DCExtCpp(update,x1,y1,x2,y2,nullptr);
  if (res) {
    res->x0=x1;
    res->y0=y1;
  }
  return res;
}

extern "C" int64_t GrPutCharCpp(CDC *dc, int64_t x, int64_t y, uint8_t ch)
{
  if (!dc)
    dc = gr.dc;
  constexpr int64_t FONT_WIDTH = 8, FONT_HEIGHT = 8;
  CGrClip c;
  if (!GrClipArea(dc, x, y, FONT_WIDTH, FONT_HEIGHT, false, c))
    return 0;
  if ((dc->flags & DCF_DONT_DRAW) || !dc->body)
    return 1;

  auto color = dc->color.c0;
  const auto *font=reinterpret_cast<const uint8_t *>(text.font);
  const auto glyph = FONT_HEIGHT * ch;
  if (color.rop == ROPB_COLLISION) {
    for (auto j = c.h1; j < c.h2; j++) {
      auto *dst = dc->body + (c.y + j) * dc->width_internal + c.x + c.w1;
      auto row = font[glyph + j];
      for (auto i = c.w1; i < c.w2; i++, dst++)
        if (row & (1U << i))
          GrWritePixel(*dc, *dst, color);
    }
    return 1;
  }
  if (color.rop == ROPB_XOR) {
    if (!color.color)
      return 1;
    color.color = COLOR_MONO;
  }

  const auto left = c.x + c.w1;
  const auto shift = (-uint64_t(c.x)) & 7;
  const auto leading_pixels = 8 - (left & 7);
  auto trailing_pixels = (c.x + c.w2) & 7;
  uint8_t leading_mask = 0xFF >> leading_pixels;
  const uint8_t trailing_mask = 0xFF << trailing_pixels;
  if (leading_pixels + trailing_pixels > c.w2 - c.w1) {
    leading_mask |= trailing_mask;
    trailing_pixels = 0;
  }
  // The leading cell reads from the preceding font byte when the glyph
  // is unaligned, even when left clipping has advanced into the next cell.
  const int64_t src = glyph - bool(shift);
  auto font_byte = [&](int64_t i) {
    return uint64_t(i) < sizeof(sys_font_std) ? font[i] : uint8_t{0};
  };
  auto draw_cell = [&](int64_t cell, int64_t y1, int64_t row, uint8_t mask) {
    if (!GrPixelVisible(dc, cell, y1, false))
      return;
    auto bits = (uint16_t(font_byte(row + 1)) << 8 | font_byte(row)) >> shift;
    bits &= uint8_t(~mask);
    auto *dst = dc->body + y1 * dc->width_internal + (cell & ~7LL);
    for (int i = 0; i < 8; i++)
      if (bits & (1U << i))
        GrWritePixel(*dc, dst[i], color);
  };
  for (auto j = c.h1; j < c.h2; j++) {
    draw_cell(left, c.y + j, src + j, leading_mask);
    if (trailing_pixels)
      draw_cell(left + 8, c.y + j, src + j + 1, trailing_mask);
  }
  return 1;
}

static int64_t GrPutS(CDC *dc, int64_t x, int64_t y, const char *s)
{
  constexpr int64_t FONT_WIDTH = 8, FONT_HEIGHT = 8;
  int64_t x0 = x, sx = 0, sy = 0, res = 0;
  while (s && *s) {
    if (*s == '\n') {
      x = x0;
      y = uint64_t(y) + FONT_HEIGHT;
      s++;
    } else if (*s == '\t') {
      constexpr uint64_t tab = 8 * FONT_WIDTH;
      x = uint64_t(x0) +
          ((uint64_t(x) - x0 + FONT_WIDTH + tab - 1) & ~(tab - 1));
      s++;
    } else if (s[0] == '$' && s[1] == 'S' && (s[2] == 'X' || s[2] == 'Y') &&
               s[3] == ',' && s[4] && s[5] && (s[4] != '-' || s[6])) {
      auto &offset = s[2] == 'X' ? sx : sy;
      bool negative = s[4] == '-';
      offset = negative ? '0' - uint8_t(s[5]) : uint8_t(s[4]) - '0';
      s += 6 + negative;
    } else {
      res += GrPutCharCpp(dc, uint64_t(x) + sx, uint64_t(y) + sy, *s++);
      x = uint64_t(x) + FONT_WIDTH;
    }
  }
  return res;
}

extern "C" int64_t GrPrintCpp(CDC *dc, int64_t x, int64_t y, const char *fmt,
                              int64_t argc, const uint64_t *argv)
{
  auto *buf = StrPrintJoin(nullptr, fmt, argc, argv);
  auto res = GrPutS(dc, x, y, buf);
  Free(buf);
  return res;
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

static int64_t GrSpan(CDC &dc, int64_t first, int64_t last, int64_t across,
                      int64_t z1, int64_t z2, bool vertical)
{
  const bool screen = dc.flags & DCF_SCRN_BITMAP;
  auto *task = dc.win_task;
  if (screen) {
    int64_t scroll = vertical ? task->scroll_y : task->scroll_x;
    first += uint64_t(scroll);
    last += uint64_t(scroll);
    across += uint64_t(vertical ? task->scroll_x : task->scroll_y);
    z1 += uint64_t(task->scroll_z);
    z2 += uint64_t(task->scroll_z);
  }
  if (last < first) {
    auto temp = first;
    first = last;
    last = temp;
    temp = z1;
    z1 = z2;
    z2 = temp;
  }
  if (dc.flags & DCF_RECORD_EXTENTS) {
    if (vertical)
      GrExtents(&dc, across, first, across, last);
    else
      GrExtents(&dc, first, across, last, across);
  }
  if (across < 0 || last < 0)
    return 0;

  int64_t dist = uint64_t(last) - first;
  if (dist < 0)
    return 0;
  int64_t dz = dist ? int64_t((uint64_t(z2) - z1) << 32) / dist : 0;
  uint64_t z = uint64_t(z1) << 32;
  // Advance from the unclipped start in window coordinates.
  if (first < 0) {
    z -= uint64_t(first) * dz;
    first = 0;
  }
  int64_t limit = (vertical ? dc.height : dc.width) - 1;
  int64_t across_limit = (vertical ? dc.width : dc.height) - 1;
  if (screen) {
    int64_t origin = vertical ? task->pix_top : task->pix_left;
    first += uint64_t(origin);
    last += uint64_t(origin);
    across += uint64_t(vertical ? task->pix_left : task->pix_top);
    int64_t edge = vertical ? task->pix_bottom : task->pix_right;
    if (edge < limit)
      limit = edge;
    edge = vertical ? task->pix_right : task->pix_bottom;
    if (edge < across_limit)
      across_limit = edge;
  }
  if (across < 0 || across > across_limit || first > limit || last < 0)
    return 0;
  if (first < 0) {
    z -= uint64_t(first) * dz;
    first = 0;
  }
  if (last > limit)
    last = limit;
  if (first > last)
    return 0;
  if (dc.flags & DCF_LOCATE_NEAREST) {
    int64_t cursor = vertical ? dc.cur_y : dc.cur_x;
    int64_t nearest = cursor < first ? first : cursor > last ? last : cursor;
    GrNearest(&dc, vertical ? across : nearest, vertical ? nearest : across);
  }
  if ((dc.flags & DCF_DONT_DRAW) || !dc.body)
    return 0;

  const auto color = dc.color;
  const bool probability = color.c1.rop & ROPBF_PROBABILITY_DITHER;
  const bool dither = color.c1.rop & ROPBF_DITHER;
  bool alternate = probability ? RandU16Cpp() < dc.dither_probability_u16
                               : dither && ((first ^ across) & 1);
  auto visible = [&](int64_t along) {
    return !screen || (dc.flags & DCF_ON_TOP) ||
           task->next_task == sys_winmgr_task ||
           !IsPixCovered0Cpp(task, vertical ? across : along,
                             vertical ? along : across);
  };
  bool char_clear = visible(first);
  int64_t d = vertical ? first * dc.width_internal + across
                       : across * dc.width_internal + first;
  int64_t stride = vertical ? dc.width_internal : 1;
  int64_t res = 0;
  for (int64_t along = first; along <= last; along++, d += stride, z += dz) {
    if (along != first && !(along & 7))
      char_clear = visible(along);
    int32_t depth = z >> 32;
    if (char_clear && depth >= 0 && depth <= dc.depth_buf[d]) {
      dc.depth_buf[d] = depth;
      GrWritePixel(dc, dc.body[d],
                   {alternate ? color.c1.color : color.c0.color, color.c0.rop});
      res++;
    }
    // The reference samples after every position, even a rejected pixel.
    alternate = probability ? RandU16Cpp() < dc.dither_probability_u16
                            : dither && !alternate;
  }
  return res;
}

extern "C" int64_t GrHLineCpp(CDC *dc, int64_t x1, int64_t x2, int64_t y,
                              int64_t z1, int64_t z2)
{
  if (!dc)
    dc = gr.dc;
  if (dc->depth_buf)
    return GrSpan(*dc, x1, x2, y, z1, z2, false);
  return x1 <= x2 ? GrRectCpp(dc, x1, y, uint64_t(x2) - x1 + 1, 1)
                  : GrRectCpp(dc, x2, y, uint64_t(x1) - x2 + 1, 1);
}

extern "C" int64_t GrVLineCpp(CDC *dc, int64_t x, int64_t y1, int64_t y2,
                              int64_t z1, int64_t z2)
{
  if (!dc)
    dc = gr.dc;
  if (dc->depth_buf)
    return GrSpan(*dc, y1, y2, x, z1, z2, true);
  return y1 <= y2 ? GrRectCpp(dc, x, y1, 1, uint64_t(y2) - y1 + 1)
                  : GrRectCpp(dc, x, y2, 1, uint64_t(y1) - y2 + 1);
}
