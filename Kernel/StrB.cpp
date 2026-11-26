#include "KernelB.hpp"

extern "C" int64_t GetCharCpp(int64_t *scan_code, uint8_t echo,
                              uint8_t raw_cursor);

extern "C" int64_t PressAKeyCpp()
{
  PutS("$BK,1$PRESS A KEY$BK,0$\n");
  return GetCharCpp(nullptr, false, false);
}

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

extern "C" int64_t LstMatchCpp(const uint8_t *needle, const uint8_t *list,
                               uint64_t flags);

extern "C" void ScanFlagsCpp(uint8_t *dst, const uint8_t *list,
                              const char *src)
{
  if (!src || !dst || !list)
    return;
  auto *buf = MAlloc(StrLen(src) + 1, CurrentTask()->data_heap);
  if (!buf)
    return;
  while (*src) {
    while (*src && *src != '+' && *src != '-')
      src++;
    if (!*src)
      break;
    bool set = *src++ == '+';
    if (!*src)
      break;
    auto *ptr = buf;
    while (*src && *src != '+' && *src != '-' && *src != ' ' &&
           *src != CH_SHIFT_SPACE)
      *ptr++ = *src++;
    *ptr = 0;
    auto i = LstMatchCpp(buf, list, 0);
    if (i < 0) {
      Free(buf);
      RaiseHolyC(0x67616C466E616353ULL); // ScanFlag
    }
    auto bit = uint8_t{1} << (i & 7);
    if (set)
      dst[i >> 3] |= bit;
    else
      dst[i >> 3] &= ~bit;
  }
  Free(buf);
}
