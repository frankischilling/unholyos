#pragma once

#include "../../Kernel/KernelA.hpp"

constexpr unsigned GR_PEN_BRUSHES_NUM = 64, GR_SCRN_ZOOM_MAX = 8;

struct [[gnu::packed]] CGrGlbls
{
  int64_t *to_8_bits, *to_8_colors;
  CDC *scrn_image, *dc, *dc1, *dc2, *dc_cache, *zoomed_dc;
  uint32_t *text_base;
  uint16_t *win_z_buf;
  CHashTable *sprite_hash;
  uint16_t *win_uncovered_bitmap;
  int64_t highest_uncovered;
  uint16_t *vga_text_cache;
  int64_t pan_text_x, pan_text_y;
  uintptr_t fp_final_scrn_update, fp_wall_paper, fp_draw_ms, fp_draw_grab_ms;
  uint8_t *empty_sprite;
  CDC *pen_brushes[GR_PEN_BRUSHES_NUM],
      *collision_pen_brushes[GR_PEN_BRUSHES_NUM],
      *even_pen_brushes[GR_PEN_BRUSHES_NUM],
      *odd_pen_brushes[GR_PEN_BRUSHES_NUM];
  int8_t circle_lo[GR_PEN_BRUSHES_NUM][GR_PEN_BRUSHES_NUM],
      circle_hi[GR_PEN_BRUSHES_NUM][GR_PEN_BRUSHES_NUM];
  uint8_t *scrn_zoom_tables[GR_SCRN_ZOOM_MAX + 1];
  int64_t scrn_zoom, sx, sy;
  bool continuous_scroll, hide_row, hide_col;
};

static_assert(sizeof(CGrGlbls) == 10507 && offsetof(CGrGlbls, dc) == 24 &&
              offsetof(CGrGlbls, text_base) == 64 &&
              offsetof(CGrGlbls, win_z_buf) == 72 &&
              offsetof(CGrGlbls, win_uncovered_bitmap) == 88 &&
              offsetof(CGrGlbls, fp_final_scrn_update) == 128 &&
              offsetof(CGrGlbls, fp_draw_ms) == 144 &&
              offsetof(CGrGlbls, fp_draw_grab_ms) == 152 &&
              offsetof(CGrGlbls, pen_brushes) == 168 &&
              offsetof(CGrGlbls, collision_pen_brushes) == 680 &&
              offsetof(CGrGlbls, even_pen_brushes) == 1192 &&
              offsetof(CGrGlbls, odd_pen_brushes) == 1704 &&
              offsetof(CGrGlbls, circle_lo) == 2216 &&
              offsetof(CGrGlbls, circle_hi) == 6312 &&
              offsetof(CGrGlbls, scrn_zoom) == 10480);

extern CGrGlbls gr;
