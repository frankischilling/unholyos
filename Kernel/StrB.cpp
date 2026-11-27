#include "KernelB.hpp"

extern "C" char *Tabs2SpacesCpp(const char *src)
{
  int64_t ch, i, j, l = (StrLen(src) << 1) + 2, col = 0;
  char *dst = reinterpret_cast<char *>(MAlloc(l, CurrentTask()->data_heap)), *tmp;
  if (!dst)
    RaiseHolyC(0x6D654D74754FULL);
  while ((ch = uint8_t(*src++))) {
    if (ch == '\t') {
      j = (col + 8) & ~7;
      for (i = col; i < j; i++) {
        dst[i] = ' ';
        if (i >= l - 2) {
          tmp = reinterpret_cast<char *>(MAlloc(l << 1, CurrentTask()->data_heap));
          if (!tmp)
            RaiseHolyC(0x6D654D74754FULL);
          MemCpy(tmp, dst, i + 1);
          Free(dst);
          l <<= 1;
          dst = tmp;
        }
      }
      col = j;
    } else {
      dst[col] = ch;
      if (col >= l - 2) {
        tmp = reinterpret_cast<char *>(MAlloc(l << 1, CurrentTask()->data_heap));
        if (!tmp)
          RaiseHolyC(0x6D654D74754FULL);
        MemCpy(tmp, dst, col + 1);
        Free(dst);
        l <<= 1;
        dst = tmp;
      }
      col++;
    }
  }
  dst[col] = 0;
  return dst;
}

extern "C" char *ScaleIndentCpp(const char *src, uint64_t indent_scale_factor)
{
  int64_t ch, i, col = 0;
  while ((ch = uint8_t(*src++))) {
    if (ch == '\t')
      col = (col + 8) & -8;
    else if (ch == ' ')
      col++;
    else
      break;
  }
  src--;
  col = F64ToI64(F64MulBits(indent_scale_factor, F64FromI64(col)), 0);
  auto *dst = reinterpret_cast<char *>(
      MAlloc(StrLen(src) + col / 8 + (col & 7) + 1, CurrentTask()->data_heap));
  if (!dst)
    RaiseHolyC(0x6D654D74754FULL);
  char *dst2 = dst;
  for (i = col / 8; i > 0; i--)
    *dst2++ = '\t';
  for (i = col & 7; i > 0; i--)
    *dst2++ = ' ';
  StrCpyCpp(dst2, src);
  return dst;
}

extern "C" char *MStrUtilCpp(const char *src, int64_t flags,
                             uint64_t indent_scale_factor)
{
  char *dst = StrNew(src, CurrentTask()->data_heap), *dst2, *tmp;
  if (!dst)
    RaiseHolyC(0x6D654D74754FULL);
  StrUtilCpp(dst, flags);
  if (flags & SUF_T2S) {
    tmp = Tabs2SpacesCpp(dst);
    Free(dst);
    dst = tmp;
  }
  if (flags & SUF_SCALE_INDENT)
    dst2 = ScaleIndentCpp(dst, indent_scale_factor);
  else
    dst2 = StrNew(dst, CurrentTask()->data_heap);
  if (!dst2)
    RaiseHolyC(0x6D654D74754FULL);
  Free(dst);
  return dst2;
}

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
