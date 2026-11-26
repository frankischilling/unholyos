#include "../../Kernel/KernelB.hpp"
#include "GrExt.hpp"
#include "GrGlbls.hpp"

extern "C" void TextPrintCpp(CTask *task,int64_t x,int64_t y,int64_t attr,
                              const char *fmt,const uint64_t *args)
{
  auto *buf=StrPrintJoin(nullptr,fmt,args[0],args+1);
  if (buf) {
    TextLenStrCpp(task,x,y,StrLen(buf),uint32_t(attr)<<8,buf);
    Free(buf);
  }
}

extern "C" void TextBorderCpp(CTask *task, int64_t l, int64_t r, int64_t t,
                              int64_t b, uint32_t attr, uint8_t solid)
{
  if (!task)
    task = CurrentTask();
  attr <<= 8;
  auto put = [&](int64_t x, int64_t y, unsigned index) {
    TextCharCpp(task,false,x,y,attr|text.border_chars[index+(solid!=0)]);
  };
  const int64_t left = uint64_t(l) - 1, right = uint64_t(r) + 1;
  const int64_t top = uint64_t(t) - 1, bottom = uint64_t(b) + 1;
  put(left, top, 6);
  put(right, top, 8);
  put(left, bottom, 10);
  put(right, bottom, 12);

  // Clip the spans without moving a border onto the edge of the screen.
  const auto x_offset =
      static_cast<__int128>(task->win_left) + (task->scroll_x >> 3);
  const auto y_offset =
      static_cast<__int128>(task->win_top) + (task->scroll_y >> 3);
  for (auto x = x_offset + l < 0 ? -x_offset : l;
       x <= r && x + x_offset < TEXT_COLS; x++) {
    put(x, top, 2);
    put(x, bottom, 2);
  }
  for (auto y = y_offset + t < 0 ? -y_offset : t;
       y <= b && y + y_offset < TEXT_ROWS; y++) {
    put(left, y, 4);
    put(right, y, 4);
  }
}

extern "C" void TextRectCpp(int64_t l, int64_t r, int64_t t, int64_t b,
                            uint32_t d)
{
  if (l < 0)
    l = 0;
  if (r >= TEXT_COLS)
    r = TEXT_COLS - 1;
  if (t < 0)
    t = 0;
  if (b >= TEXT_ROWS)
    b = TEXT_ROWS - 1;
  for (int64_t y = t; y <= b; y++)
    for (int64_t x = l; x <= r; x++)
      gr.text_base[y * TEXT_COLS + x] = d;
}
