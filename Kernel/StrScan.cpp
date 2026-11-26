#include "KernelB.hpp"

extern "C" int64_t Str2I64Cpp(const char *text, int64_t radix, const char **end)
{
  if (!text || radix < 2 || radix > 36) {
    if (end)
      *end = text;
    return 0;
  }
  auto upper = [](uint8_t ch) {
    return ch >= 'a' && ch <= 'z' ? ch - ('a' - 'A') : ch;
  };
  while (char_bmp_white_space[uint8_t(*text) / 32] &
         (uint32_t{1} << (uint8_t(*text) % 32)))
    text++;
  bool neg = false;
  while (*text == '+' || *text == '-') {
    neg ^= *text == '-';
    text++;
  }
  if (*text == '0') {
    text++;
    int ch = upper(*text);
    if (ch >= 'B' && (radix <= 10 || ch > 'A' + radix - 11)) {
      switch (ch) {
      case 'B':
        radix = 2;
        text++;
        break;
      case 'D':
        radix = 10;
        text++;
        break;
      case 'X':
        radix = 16;
        text++;
        break;
      }
    }
  }
  uint64_t res = 0;
  for (; *text; text++) {
    int ch = upper(*text), digit;
    if ('0' <= ch && ch <= '9')
      digit = ch - '0';
    else if ('A' <= ch && ch <= 'Z')
      digit = ch - 'A' + 10;
    else
      break;
    if (digit >= radix)
      break;
    res = res * radix + digit;
  }
  if (end)
    *end = text;
  return static_cast<int64_t>(neg ? 0 - res : res);
}

extern "C" uint64_t Str2DateCpp(const char *_src)
{
  if (!_src)
    return 0;
  auto *src=StrNew(_src,CurrentTask()->data_heap);
  if (!src)
    return 0;
  char *dst=src;
  for (auto *s=src;*s;s++) {
    uint8_t ch=*s;
    if (!(char_bmp_white_space[ch/32]&(uint32_t{1}<<(ch%32))))
      *dst++=ch>='a' && ch<='z' ? ch-32 : ch;
  }
  *dst=0;
  char *ptr=src;
  enum { NONE,SM,EM,SY,EY } mode=NONE;
  if (StrLen(ptr)>=3 && ptr[2]=='(') {
    if (ptr[0]=='S' && ptr[1]=='M') mode=SM;
    if (ptr[0]=='E' && ptr[1]=='M') mode=EM;
    if (ptr[0]=='S' && ptr[1]=='Y') mode=SY;
    if (ptr[0]=='E' && ptr[1]=='Y') mode=EY;
    if (mode!=NONE) ptr+=3;
  }
  uint64_t res=0;
  auto offset=DateBits(local_time_offset);
  if (*ptr=='*') {
    ptr++;
    res=Now()+offset;
    if (*ptr=='+' || *ptr=='-') {
      const char *end;
      res+=uint64_t(Str2I64Cpp(ptr,10,&end))<<32;
      ptr=const_cast<char *>(end);
    }
  } else {
    // Split the slash-separated fields before converting their numeric prefixes.
    auto *sep=ptr;
    while (*sep && *sep!='/')
      sep++;
    bool slash=*sep!=0;
    *sep=0;
    CDateStruct ds{};
    ds.mon=Str2I64Cpp(ptr,10,nullptr);
    if (!ds.mon || ds.mon>12) {
      Free(src);
      return 0;
    }
    ptr=sep+slash;
    sep=ptr;
    while (*sep && *sep!='/')
      sep++;
    const char *end;
    if (*sep) {
      *sep=0;
      ds.day_of_mon=Str2I64Cpp(ptr,10,nullptr);
      ds.year=Str2I64Cpp(sep+1,10,&end);
      if (ds.year<100) ds.year+=2000;
    } else {
      ds.day_of_mon=Str2I64Cpp(ptr,10,&end);
      CDateStruct ds_now;
      Date2Struct(ds_now,DateFromBits(Now()+offset));
      ds.year=ds_now.year;
    }
    ptr=const_cast<char *>(end);
    res=DateBits(Struct2Date(ds));
  }
  if (*ptr==')') ptr++;
  if (mode!=NONE) {
    auto date=DateFromBits(res);
    switch (mode) {
      case SM: date.date=FirstDayOfMon(date.date); break;
      case EM: date.date=LastDayOfMon(date.date); break;
      case SY: date.date=FirstDayOfYear(date.date); break;
      default: date.date=LastDayOfYear(date.date); break;
    }
    res=DateBits(date);
  }
  if (*ptr=='+' || *ptr=='-')
    res+=uint64_t(Str2I64Cpp(ptr,10,nullptr))<<32;
  Free(src);
  return res-offset;
}
