#pragma once

#include "../../Kernel/KernelA.hpp"

extern "C" bool TextCharCpp(CTask *task, uint8_t allow_border, int64_t x,
                             int64_t y, uint32_t d);
extern "C" bool TextLenStrCpp(CTask *task, int64_t x, int64_t y, int64_t len,
                               uint32_t attr, const char *s);
extern "C" bool TextLenAttrStrCpp(CTask *task,int64_t x,int64_t y,int64_t len,
                                  const uint32_t *attr);
extern "C" bool TextLenAttrCpp(CTask *task, int64_t x, int64_t y, int64_t len,
                                uint32_t attr);
extern "C" void TextRectCpp(int64_t l, int64_t r, int64_t t, int64_t b, uint32_t d);
extern "C" void TextBorderCpp(CTask *task, int64_t l, int64_t r, int64_t t,
                                int64_t b, uint32_t attr, uint8_t solid);

bool GrPixelVisible(const CDC *dc, int64_t x, int64_t y, bool skip_top_task);
extern "C" uint64_t IsPixCovered0Cpp(CTask *task, int64_t x, int64_t y);
extern "C" uint64_t GrPlot0Cpp(CDC *dc, int64_t x, int64_t y);
extern "C" uint64_t GrPlotCpp(CDC *dc, int64_t x, int64_t y);
bool GrPlot1(CDC *dc, int64_t x, int64_t y);
void GrNearest(CDC *dc, int64_t x, int64_t y);
void GrExtents(CDC *dc, int64_t left, int64_t top, int64_t right,
               int64_t bottom);
extern "C" bool GrPlot3BCpp(CDC *dc, int64_t x, int64_t y, int64_t z);
extern "C" bool GrPlot3Cpp(CDC *dc, int64_t x, int64_t y, int64_t z);
extern "C" int64_t GrFillCircleCpp(CDC *dc, int64_t x, int64_t y, int64_t z,
                                   int64_t diameter);
extern "C" int64_t GrPeekCpp(CDC *dc, int64_t x, int64_t y);
int64_t GrRayLenMinus(CDC *dc, int64_t x, int64_t y);
int64_t GrRayLen(CDC *dc, int64_t *x1, int64_t y, int64_t z, int32_t *db);
extern "C" int64_t GrFloodFillCpp(CDC *dc, int64_t x, int64_t y,
                                  bool not_color, int64_t z, int32_t *db);
extern "C" int64_t GrFloodFill3Cpp(CDC *dc, int64_t x, int64_t y, int64_t z,
                                   bool not_color);
extern "C" int64_t GrBlot3Cpp(CDC *dc, int64_t x, int64_t y, int64_t z,
                              CDC *img);
extern "C" int64_t *Mat4x4IdentEquCpp(int64_t *r);
void DCTransformPoint(CDC &dc, CD3I64 &p);
extern "C" void DCThickScaleCpp(CDC *dc);
extern "C" void DCReflectCpp(CDC *dc, int64_t *x, int64_t *y, int64_t *z);
extern "C" uint64_t DCSymmetry3SetCpp(CDC *dc, int64_t x1, int64_t y1,
                                      int64_t z1, int64_t x2, int64_t y2,
                                      int64_t z2, int64_t x3, int64_t y3,
                                      int64_t z3);
extern "C" int64_t GrPeek0Cpp(CDC *dc, int64_t x, int64_t y);
extern "C" int64_t GrBlotCpp(CDC *dc, int64_t x, int64_t y, CDC *img);
uint8_t *GrBitMap4ToBitMap8(uint8_t *dst,const uint8_t *src,int64_t src_size,
                            int64_t bkcolor);
extern "C" CDC *DCScrnCaptureCpp(uint8_t include_zoom,CTask *task);
extern "C" CDC *GRReadCpp(const char *filename,CTask *task);
extern "C" int64_t GRWriteCpp(const char *filename,CDC *dc,int64_t dcsf_flags);
extern "C" int64_t GrRectCpp(CDC *dc, int64_t x, int64_t y, int64_t w,
                             int64_t h);
extern "C" int64_t GrHLineCpp(CDC *dc, int64_t x1, int64_t x2, int64_t y,
                              int64_t z1, int64_t z2);
extern "C" int64_t GrVLineCpp(CDC *dc, int64_t x, int64_t y1, int64_t y2,
                              int64_t z1, int64_t z2);
extern "C" int64_t GrFillTri0Cpp(CDC *dc, const CD3I32 *p1, const CD3I32 *p2,
                                 const CD3I32 *p4);
extern "C" int64_t GrFillPoly3Cpp(CDC *dc, int64_t n, const CD3I32 *poly);
extern "C" int64_t GrRect3Cpp(CDC *dc, int64_t x, int64_t y, int64_t z,
                              int64_t w, int64_t h);
inline constexpr uint64_t GR_ARROW_WIDTH = 0x4006000000000000; // 2.75
extern "C" bool GrArrow3Cpp(CDC *dc, int64_t x1, int64_t y1, int64_t z1,
                            int64_t x2, int64_t y2, int64_t z2,
                            uint64_t w = GR_ARROW_WIDTH, int64_t step = 1,
                            int64_t start = 0);
extern "C" uint64_t ClipLineCpp(int64_t *_x1, int64_t *_y1, int64_t *_x2,
                                int64_t *_y2, int64_t left, int64_t top,
                                int64_t right, int64_t bottom);
extern "C" void GrUpdateLine64(volatile uint8_t **vga, const uint8_t **src,
                               size_t words, uint8_t **cache);
extern "C" void GrUpdateLine64FlushCache(volatile uint8_t **vga,
                                         const uint8_t **src, size_t words,
                                         uint8_t **cache);
extern "C" void DCBlotColor4(uint8_t *dst, const uint8_t *src, uint8_t *cache,
                             size_t groups);
extern "C" uint64_t GrLineCpp(CDC *dc, int64_t x1, int64_t y1, int64_t x2,
                              int64_t y2, int64_t step, int64_t start);
extern "C" bool GrLine3Cpp(CDC *dc, int64_t x1, int64_t y1, int64_t z1,
                           int64_t x2, int64_t y2, int64_t z2, int64_t step,
                           int64_t start);

extern "C" int64_t GrPrintCpp(CDC *dc, int64_t x, int64_t y, const char *fmt,
                              int64_t argc, const uint64_t *argv);

inline uint64_t AbsI64(int64_t n)
{
  return n < 0 ? -static_cast<uint64_t>(n) : n;
}

inline int64_t GrFixed32(int64_t n)
{
  return static_cast<uint64_t>(n) << 32;
}

using GrLinePlotFn = bool (*)(CDC *dc, int64_t x, int64_t y, int64_t z);
bool Line(CDC *dc, int64_t x1, int64_t y1, int64_t z1, int64_t x2, int64_t y2,
          int64_t z2, GrLinePlotFn fp_plot, int64_t step, int64_t start);
bool Circle(CDC *dc, int64_t cx, int64_t cy, int64_t cz, int64_t radius,
            GrLinePlotFn fp_plot, int64_t step, uint64_t start, uint64_t length);
bool Ellipse(CDC *dc, int64_t cx, int64_t cy, int64_t cz, int64_t xr,
             int64_t yr, GrLinePlotFn fp_plot, uint64_t rotation, int64_t step,
             uint64_t start, uint64_t length);
bool RegPoly(CDC *dc, int64_t cx, int64_t cy, int64_t cz, int64_t xr,
             int64_t yr, int64_t sides, GrLinePlotFn fp_plot, uint64_t rotation,
             int64_t step, uint64_t start, uint64_t length);
bool Bezier2(CDC *dc, const CD3I32 *ctrl, GrLinePlotFn fp_plot, bool first = true);
bool Bezier3(CDC *dc, const CD3I32 *ctrl, GrLinePlotFn fp_plot, bool first = true);
bool BSpline2(CDC *dc, const CD3I32 *ctrl, int64_t cnt, GrLinePlotFn fp_plot,
              bool closed);
bool BSpline3(CDC *dc, const CD3I32 *ctrl, int64_t cnt, GrLinePlotFn fp_plot,
              bool closed);
