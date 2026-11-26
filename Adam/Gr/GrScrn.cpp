#include "../../Kernel/KernelB.hpp"
#include "GrExt.hpp"
#include "GrGlbls.hpp"

constexpr int64_t FONT_WIDTH = 8, FONT_HEIGHT = 8;
constexpr uint32_t ATTRF_BLINK = 0x10000000, ATTRF_INVERT = 0x20000000,
                   ATTRF_SEL = 0x40000000, ATTRF_UNDERLINE = 0x80000000;

static void GrFixZoomScale()
{
  if (gr.scrn_zoom < 1)
    gr.scrn_zoom = 1;
  if (gr.scrn_zoom > GR_SCRN_ZOOM_MAX)
    gr.scrn_zoom = GR_SCRN_ZOOM_MAX;
  if (gr.scrn_zoom == 1) {
    gr.sx = gr.sy = 0;
    return;
  }
  auto xmax = GR_WIDTH - GR_WIDTH / gr.scrn_zoom;
  auto ymax = GR_HEIGHT - GR_HEIGHT / gr.scrn_zoom;
  gr.sx = (gr.sx < 0 ? 0 : gr.sx > xmax ? xmax : gr.sx) & ~7LL;
  gr.sy = gr.sy < 0 ? 0 : gr.sy > ymax ? ymax : gr.sy;
}

extern "C" void GrScaleZoomCpp(uint64_t scale)
{
  if (!gr.zoomed_dc)
    return;
  auto old = F64FromI64(gr.scrn_zoom);
  gr.scrn_zoom = F64ToI64(F64MulBits(old, scale), 0x0C00);
  GrFixZoomScale();

  auto ratio = F64DivBits(old, F64FromI64(gr.scrn_zoom));
  ms.scale.x = F64MulBits(ms.scale.x, ratio);
  ms.scale.y = F64MulBits(ms.scale.y, ratio);
  ms.scale.z = F64MulBits(ms.scale.z, ratio);
  auto offset = [ratio](int64_t pos, int64_t old_offset) {
    constexpr uint64_t sign = uint64_t{1} << 63;
    auto delta = F64MulBits(F64FromI64(uint64_t(pos) - old_offset), ratio);
    return F64ToI64(F64AddBits(F64FromI64(pos), delta ^ sign), 0x0C00);
  };
  ms.offset.x = offset(ms.pos.x, ms.offset.x);
  ms.offset.y = offset(ms.pos.y, ms.offset.y);
  ms.offset.z = offset(ms.pos.z, ms.offset.z);
  gr.sx = ms.pos.x - (gr.zoomed_dc->width >> 1) / gr.scrn_zoom;
  gr.sy = ms.pos.y - (gr.zoomed_dc->height >> 1) / gr.scrn_zoom;
  GrFixZoomScale();
}

static void GrZoomInScrn()
{
  GrFixZoomScale();
  auto zoom = gr.scrn_zoom;
  auto stride = gr.zoomed_dc->width_internal >> 3;
  auto cols = (gr.zoomed_dc->width >> 3) / zoom;
  auto rows = gr.zoomed_dc->height / zoom;
  auto *src = gr.dc1->body + (gr.sx >> 3) + gr.sy * stride;
  auto *dst = gr.zoomed_dc->body;
  const auto *table = gr.scrn_zoom_tables[zoom];
  for (unsigned plane = 0; plane < 4; plane++) {
    for (int64_t row = 0; row < rows; row++) {
      for (int64_t repeat = 0; repeat < zoom; repeat++) {
        for (int64_t col = 0; col < cols; col++) {
          auto pixel = src[col];
          for (int64_t i = 0; i < zoom; i++)
            *dst++ = table[pixel + i * 256];
        }
        auto pad = stride - cols * zoom;
        MemSet(dst, 0, pad);
        dst += pad;
      }
      src += stride;
    }
    auto pad = (gr.zoomed_dc->height % zoom) * stride;
    MemSet(dst, 0, pad);
    dst += pad;
    src += (gr.zoomed_dc->height - rows) * stride;
  }
}

static uint8_t TextAttr(uint32_t ch, bool blink)
{
  uint8_t attr = ch >> 8;
  if (ch & ATTRF_SEL)
    attr ^= 0xFF;
  if (ch & ATTRF_INVERT)
    attr = (attr << 4) | (attr >> 4);
  if ((ch & ATTRF_BLINK) && blink)
    attr = (attr << 4) | (attr >> 4);
  return attr;
}

struct CTextView
{
  int64_t cols, rows, src_x, src_y, dst_x, dst_y;
};

static CTextView GrTextView()
{
  if (gr.pan_text_x < -7)
    gr.pan_text_x = -7;
  if (gr.pan_text_x > 7)
    gr.pan_text_x = 7;
  if (gr.pan_text_y < -7)
    gr.pan_text_y = -7;
  if (gr.pan_text_y > 7)
    gr.pan_text_y = 7;
  return {TEXT_COLS - (gr.pan_text_x || gr.hide_col),
          TEXT_ROWS - (gr.pan_text_y || gr.hide_row),
          gr.pan_text_x < 0,
          gr.pan_text_y < 0,
          gr.pan_text_x < 0 ? FONT_WIDTH : 0,
          gr.pan_text_y < 0 ? FONT_HEIGHT : 0};
}

static void GrUpdateTextBG(const CTextView &view, bool blink)
{
  auto stride = gr.dc2->width_internal;
  auto *body = gr.dc2->body;
  if (view.cols != TEXT_COLS || view.rows != TEXT_ROWS)
    MemSet(body, 0, stride * gr.dc2->height);
  for (int64_t row = 0; row < view.rows; row++) {
    auto *src = gr.text_base + (view.src_y + row) * TEXT_COLS + view.src_x;
    auto *dst = body +
                (row * FONT_HEIGHT + view.dst_y + gr.pan_text_y) * stride +
                view.dst_x + gr.pan_text_x;
    for (int64_t col = 0; col < view.cols; col++, dst += FONT_WIDTH) {
      const uint64_t bg =
          0x0101010101010101ULL * (TextAttr(src[col], blink) >> 4);
      for (int64_t y = 0; y < FONT_HEIGHT; y++)
        __builtin_memcpy(dst + y * stride, &bg, sizeof(bg));
    }
  }
}

static int64_t GrTextDelta(int64_t bits)
{
  auto x = bits & 31;
  auto y = (bits >> 5) & 31;
  if (x & 16)
    x -= 32;
  if (y & 16)
    y -= 32;
  return y * gr.dc2->width_internal + x;
}

static void GrUpdateTextFG(const CTextView &view, bool blink)
{
  auto stride = gr.dc2->width_internal;
  auto end = stride * gr.dc2->height - 7 * stride - 8;
  for (int64_t row = 0; row < view.rows; row++) {
    const auto *src =
        gr.text_base + (view.src_y + row) * TEXT_COLS + view.src_x;
    for (int64_t col = 0; col < view.cols; col++) {
      auto ch = src[col];
      auto bits = sys_font_std[static_cast<uint8_t>(ch)];
      if (ch & ATTRF_UNDERLINE)
        bits |= 0xFF00000000000000ULL;
      if (!bits)
        continue;
      auto offset = (row * FONT_HEIGHT + view.dst_y) * stride +
                    col * FONT_WIDTH + view.dst_x;
      auto shift = ((ch >> 16) & 0x3FF) + gr.pan_text_x + 32 * gr.pan_text_y;
      if (shift) {
        offset += GrTextDelta(shift);
        if (offset < 0 || offset >= end)
          continue;
      }
      auto color = TextAttr(ch, blink) & 15;
      auto *dst = gr.dc2->body + offset;
      for (int64_t y = 0; y < FONT_HEIGHT; y++, bits >>= 8, dst += stride)
        for (int64_t x = 0; x < FONT_WIDTH; x++)
          if ((bits >> x) & 1)
            dst[x] = color;
    }
  }
}

static void DCBlotColor8(CDC *dc, const CDC *img)
{
  size_t len = img->width_internal * img->height;
  for (size_t i = 0; i < len; i++) {
    auto color = img->body[i];
    if (color != TRANSPARENT)
      dc->body[i] = color;
  }
}

static bool GrTakeVGAFlush()
{
  auto &flags = sys_semas[SEMA_FLUSH_VGA_IMAGE].flags;
  return __atomic_fetch_and(&flags, ~uint64_t{1}, __ATOMIC_RELAXED) & 1;
}

static void GrUpdateTextModeText(bool blink)
{
  auto *dst =
      reinterpret_cast<volatile uint16_t *>(dev.uncached_alias + 0xB8000);
  const bool flush = GrTakeVGAFlush();
  const auto cursor =
      WinMsCursorVisible() ? ms.pos_text.x + ms.pos_text.y * TEXT_COLS : -1;
  for (int64_t i = 0; i < TEXT_COLS * 25; i++) {
    auto ch = gr.text_base[i];
    if (i == cursor)
      ch ^= 0x7F00;
    uint16_t cell =
        (ch & 0xFF) | (static_cast<uint16_t>(TextAttr(ch, blink) & 0x7F) << 8);
    if (flush || gr.vga_text_cache[i] != cell)
      dst[i] = gr.vga_text_cache[i] = cell;
  }
}

static void GrUpdateVGAGraphics()
{
  const uint8_t *src = gr.dc1->body;
  if (gr.scrn_zoom != 1) {
    GrZoomInScrn();
    src = gr.zoomed_dc->body;
  }
  auto *cache = gr.scrn_image->body;
  bool flush = GrTakeVGAFlush();
  if (flush) {
    OutU8(0x3CE, 1);
    OutU8(0x3CF, 0);
    OutU8(0x3CE, 3);
    OutU8(0x3CF, 0);
    OutU8(0x3CE, 8);
    OutU8(0x3CF, 0xFF);
  }
  auto update = flush ? GrUpdateLine64FlushCache : GrUpdateLine64;
  auto words = gr.zoomed_dc->width_internal >> 6;
  for (unsigned plane = 1; plane < 0x10; plane <<= 1) {
    OutU8(0x3C4, 2);
    OutU8(0x3C5, plane);
    auto *vga =
        reinterpret_cast<volatile uint8_t *>(dev.uncached_alias + 0xA0000);
    for (int64_t row = 0; row < gr.zoomed_dc->height; row++)
      update(&vga, &src, words, &cache);
  }
}

void GrUpdateScrn()
{
  const bool blink =
      (__atomic_load_n(&cnts.jiffies, __ATOMIC_RELAXED) * 5 / JIFFY_FREQ) & 1;
  const bool graphics = SYS_RUN_LEVEL & RLF_VGA;
  if (graphics) {
    auto view = GrTextView();
    GrUpdateTextBG(view, blink);
    GrUpdateTextFG(view, blink);
    DCBlotColor8(gr.dc2, gr.dc);
  }

  if (sys_winmgr_task && gr.fp_final_scrn_update) {
    if (auto *dc = DCAliasCpp(gr.dc2, CurrentTask())) {
      dc->flags |= DCF_ON_TOP;
      bool drawn = CallHolyC1Checked(gr.fp_final_scrn_update,
                                     reinterpret_cast<uintptr_t>(dc));
      DCDelCpp(dc);
      if (!drawn)
        return;
    }
  }

  if (!graphics) {
    GrUpdateTextModeText(blink);
    return;
  }
  DCBlotColor4(gr.dc1->body, gr.dc2->body, gr.dc_cache->body,
               gr.dc2->height * gr.dc2->width_internal >> 3);
  GrUpdateVGAGraphics();
}
