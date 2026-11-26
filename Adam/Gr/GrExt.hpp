#pragma once

#include "../../Kernel/KernelA.hpp"

bool GrPixelVisible(const CDC *dc, int64_t x, int64_t y, bool skip_top_task);
extern "C" uint64_t IsPixCovered0Cpp(CTask *task, int64_t x, int64_t y);
extern "C" uint64_t GrPlot0Cpp(CDC *dc, int64_t x, int64_t y);
extern "C" int64_t GrPeek0Cpp(CDC *dc, int64_t x, int64_t y);
extern "C" int64_t GrBlotCpp(CDC *dc, int64_t x, int64_t y, CDC *img);
extern "C" int64_t GrRectCpp(CDC *dc, int64_t x, int64_t y, int64_t w,
                             int64_t h);
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
using GrLinePlotFn = bool (*)(CDC *dc, int64_t x, int64_t y, int64_t z);
bool Line(CDC *dc, int64_t x1, int64_t y1, int64_t z1, int64_t x2, int64_t y2,
          int64_t z2, GrLinePlotFn fp_plot, int64_t step, int64_t start);
