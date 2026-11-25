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
