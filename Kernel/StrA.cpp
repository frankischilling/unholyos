#include "KernelB.hpp"

static char *StrPrintHex(char *dst, uint64_t num, size_t width)
{
  char *end = dst + width;
  for (char *p = end; p != dst; num >>= 4)
    *--p = "0123456789ABCDEF"[num & 15];
  return end;
}

extern "C" void PutHexCpp(uint64_t num, int64_t width)
{
  char buf[17];
  if (width < 0)
    return;
  *StrPrintHex(buf, num, width > 16 ? 16 : width) = 0;
  PutS(buf);
}

uint32_t char_bmp_alpha[16]{
    0,          0,          0x87FFFFFF, 0x07FFFFFE,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
};
uint32_t char_bmp_alpha_numeric[16]{
    0,          0x03FF0000, 0x87FFFFFF, 0x07FFFFFE,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
};
uint32_t char_bmp_alpha_numeric_no_at[16]{
    0,          0x03FF0000, 0x87FFFFFE, 0x07FFFFFE,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
};
uint32_t char_bmp_word[16]{
    0,          0x03FF0080, 0x87FFFFFE, 0x07FFFFFE,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
};
uint32_t char_bmp_filename[16]{
    0,          0x03FF73FB, 0xEFFFFFFF, 0x6FFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
};
uint32_t char_bmp_dec_numeric[16]{0, 0x03FF0000};
uint32_t char_bmp_hex_numeric[16]{0, 0x03FF0000, 0x7E, 0x7E};
uint32_t char_bmp_white_space[16]{0x80002600, 1};
uint32_t char_bmp_non_eol_white_space[16]{0x80000200, 1};
uint32_t char_bmp_zero_cr_nl_cursor[16]{0x00002421};
uint32_t char_bmp_zero_tab_cr_nl_cursor[16]{0x00002621};
uint32_t char_bmp_zero_tab_cr_nl_cursor_dollar[16]{0x00002621, 0x10};
uint32_t char_bmp_macro[16]{
    0x80002600, 0xFFFFFFDF, 0xFFFFFFFF, 0x7FFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
};
uint32_t char_bmp_printable[16]{
    0x80002600, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
};
uint32_t char_bmp_displayable[16]{
    0x80000000, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
};
uint32_t char_bmp_safe_dollar[16]{
    0x80000000, 0xFFFFFFEF, 0xFFFFFFFF, 0x7FFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
};
uint32_t char_bmp_non_eol[16]{
    0xFFFFDBFE, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
};

static uint8_t ToUpper(uint8_t ch)
{
  return ch >= 'a' && ch <= 'z' ? ch - ('a' - 'A') : ch;
}

extern "C" int64_t ToUpperCpp(uint8_t ch)
{
  return ToUpper(ch);
}

extern "C" void StrCpyCpp(char *dst, const char *src)
{
  if (!dst)
    return;
  if (!src)
    src = "";
  while ((*dst++ = *src++))
    ;
}

extern "C" int64_t StrCmpCpp(const uint8_t *a, const uint8_t *b)
{
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return (*a > *b) - (*a < *b);
}

extern "C" const char *StrIMatchCpp(const char *needle, const char *haystack)
{
  if (!needle || !haystack)
    return nullptr;
  if (!*needle)
    return haystack;
  for (; *haystack; haystack++) {
    size_t i = 0;
    while (needle[i] && ToUpper(needle[i]) == ToUpper(haystack[i]))
      i++;
    if (!needle[i])
      return haystack;
  }
  return nullptr;
}

extern "C" int64_t StrOccCpp(const uint8_t *src, int64_t ch)
{
  if (!src)
    return 0;
  int64_t count = 0;
  for (; *src; src++)
    count += *src == ch;
  return count;
}

extern "C" int64_t LstMatchCpp(const uint8_t *needle, const uint8_t *list,
                               uint64_t flags)
{
  constexpr uint64_t LMF_IGNORE_CASE = 1, LMF_EXACT = 2;
  int64_t sub = 0, result = -1;
  if (!list)
    return result;
  while (*list) {
    if (*list == '@') {
      sub--;
      list++;
    }
    const auto *p = needle;
    if (flags & LMF_IGNORE_CASE) {
      while (*p && ToUpper(*p) == ToUpper(*list)) {
        p++;
        list++;
      }
    } else {
      while (*p && *p == *list) {
        p++;
        list++;
      }
    }
    if (!*p) {
      if (!*list)
        return sub;
      if (!(flags & LMF_EXACT))
        result = result == -1 ? sub : -2;
    }
    while (*list)
      list++;
    list++;
    sub++;
  }
  return result;
}

const char *LstSub(int64_t index, const char *list)
{
  if (!list)
    return nullptr;
  while (*list && index > 0) {
    list += StrLen(list) + 1;
    if (*list == '@')
      list++;
    else
      index--;
  }
  return !index && *list ? list : nullptr;
}

extern "C" int64_t Spaces2TabsCpp(char *dst, const char *src)
{
  const char *src2;
  int64_t chged = 0, space_cnt, space_cnt2, col = 0;
  if (*src)
    for (;;) {
      src2 = src;
      while (*src2 == ' ')
        src2++;
      space_cnt = src2 - src;
      while (col + space_cnt >= 8) {
        space_cnt2 = 8 - col;
        if (space_cnt2 == 1)
          *dst++ = ' ';
        else {
          *dst++ = '\t';
          chged += space_cnt2 - 1;
        }
        space_cnt -= space_cnt2;
        col = 0;
      }
      if (*src2 == '\t') {
        if (space_cnt == 1 && col == 7)
          *dst++ = ' ';
        else
          chged += space_cnt;
        *dst++ = '\t';
        col = 0;
      } else {
        while (space_cnt--) {
          *dst++ = ' ';
          if (++col == 8)
            col = 0;
        }
        if (*src2) {
          *dst++ = *src2;
          if (++col == 8)
            col = 0;
        } else
          break;
      }
      src = ++src2;
    }
  *dst = 0;
  return chged;
}

static bool StrCharBit(const uint32_t *bitmap, uint8_t ch)
{
  return (bitmap[ch >> 5] >> (ch & 31)) & 1;
}

extern "C" char *StrUtilCpp(char *_src, int64_t flags)
{
  char *src = _src, *dst = _src;
  uint8_t ch;
  if (flags & SUF_REM_LEADING)
    while (StrCharBit(char_bmp_white_space, *src))
      src++;
  while ((ch = *src++))
    if (StrCharBit(char_bmp_white_space, ch)) {
      if (!(flags & SUF_REM_SPACES)) {
        if (flags & SUF_SINGLE_SPACE) {
          *dst++ = ' ';
          while ((ch = *src++) && StrCharBit(char_bmp_white_space, ch))
            ;
          src--;
        } else
          *dst++ = ch;
      }
    } else if (!(flags & SUF_REM_CTRL_CHARS) || ch >= CH_SHIFT_SPACE)
      *dst++ = ch;
  *dst = 0;

  if (flags & SUF_REM_TRAILING)
    while (dst != _src && (!*dst || StrCharBit(char_bmp_white_space, *dst)))
      *dst-- = 0;
  if (flags & SUF_TO_UPPER)
    for (dst = _src; *dst; dst++) {
      ch = *dst;
      if (ch >= 'a' && ch <= 'z')
        *dst = ch - 0x20;
    }
  if (flags & SUF_TO_LOWER)
    for (dst = _src; *dst; dst++) {
      ch = *dst;
      if (ch >= 'A' && ch <= 'Z')
        *dst = ch + 0x20;
    }
  if (flags & SUF_SAFE_DOLLAR)
    for (dst = _src; *dst; dst++)
      if (!StrCharBit(char_bmp_safe_dollar, *dst))
        *dst = '.';
  if (flags & SUF_S2T)
    Spaces2TabsCpp(_src, _src);
  return _src;
}

extern "C" char *StrFirstOccCpp(char *src, const uint8_t *marker)
{
  uint8_t ch;
  while ((ch = *src++) && !StrOccCpp(marker, ch))
    ;
  return ch ? src - 1 : nullptr;
}

extern "C" char *StrFirstRemCpp(char *src, const uint8_t *marker, char *dst)
{
  uint8_t ch;
  char *ptr = src, *res = dst;
  if (dst) {
    while ((ch = *ptr++) && !StrOccCpp(marker, ch))
      *dst++ = ch;
    *dst = 0;
  } else
    while ((ch = *ptr++) && !StrOccCpp(marker, ch))
      ;
  if (ch)
    StrCpyCpp(src, ptr);
  else
    *src = 0;
  return res;
}

extern "C" char *StrLastOccCpp(char *src, const uint8_t *marker)
{
  uint8_t ch;
  char *res = nullptr;
  while ((ch = *src++))
    if (StrOccCpp(marker, ch))
      res = src - 1;
  return res;
}

extern "C" char *StrLastRemCpp(char *src, const uint8_t *marker, char *dst)
{
  char *ptr = StrLastOccCpp(src, marker);
  if (ptr) {
    if (dst)
      StrCpyCpp(dst, ptr + 1);
    *ptr = 0;
  } else {
    if (dst)
      StrCpyCpp(dst, src);
    *src = 0;
  }
  return dst;
}

extern "C" const char *StrFindCpp(const char *needle, const char *haystack_str,
                                  int64_t flags)
{
  bool cont;
  const char *saved_haystack_str = haystack_str;
  size_t plen = StrLen(needle);
  do {
    cont = false;
    if (flags & SFF_IGNORE_CASE)
      haystack_str = StrIMatchCpp(needle, haystack_str);
    else
      haystack_str = StrMatchCpp(needle, haystack_str);
    if (haystack_str && (flags & SFF_WHOLE_LABELS_BEFORE) &&
        haystack_str != saved_haystack_str &&
        StrCharBit(char_bmp_alpha_numeric, *(haystack_str - 1))) {
      haystack_str++;
      if (*haystack_str)
        cont = true;
      else
        haystack_str = nullptr;
    }
    if (haystack_str && (flags & SFF_WHOLE_LABELS_AFTER) &&
        StrCharBit(char_bmp_alpha_numeric, *(haystack_str + plen))) {
      haystack_str++;
      if (*haystack_str)
        cont = true;
      else
        haystack_str = nullptr;
    }
  } while (cont);
  return haystack_str;
}
