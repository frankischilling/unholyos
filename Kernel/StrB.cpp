#include "KernelB.hpp"

extern "C" int64_t GetCharCpp(int64_t *scan_code, uint8_t echo,
                              uint8_t raw_cursor);

extern "C" uint64_t YorNCpp()
{
  PutS("(y or n)? ");
  for (;;) {
    auto ch = GetCharCpp(nullptr, false, false);
    if (ch == 'Y' || ch == 'y') {
      PutS("$PT$YES$FG$\n");
      return true;
    }
    if (ch == 'N' || ch == 'n') {
      PutS("$PT$NO$FG$\n");
      return false;
    }
  }
}
