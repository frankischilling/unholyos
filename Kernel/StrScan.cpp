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

static int64_t MonthStartDate(int64_t year,unsigned mon)
{
  constexpr uint16_t mon_start_days[]{0,31,59,90,120,151,181,212,243,273,304,334};
  auto start=YearStartDate(year);
  return start+mon_start_days[mon-1]+(mon>2 && YearStartDate(year+1)-start==366);
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
  const char *ptr=src;
  enum { NONE,SM,EM,SY,EY } mode=NONE;
  if (StrLen(ptr)>=3 && ptr[2]=='(') {
    if (ptr[0]=='S' && ptr[1]=='M') mode=SM;
    if (ptr[0]=='E' && ptr[1]=='M') mode=EM;
    if (ptr[0]=='S' && ptr[1]=='Y') mode=SY;
    if (ptr[0]=='E' && ptr[1]=='Y') mode=EY;
    if (mode!=NONE) ptr+=3;
  }
  uint64_t res=0;
  if (*ptr=='*') {
    ptr++;
    res=Now();
    if (*ptr=='+' || *ptr=='-')
      res+=uint64_t(Str2I64Cpp(ptr,10,&ptr))<<32;
  } else {
    auto mon=Str2I64Cpp(ptr,10,&ptr);
    if (mon<1 || mon>12 || *ptr!='/') {
      Free(src);
      return 0;
    }
    auto day=Str2I64Cpp(ptr+1,10,&ptr);
    int64_t year;
    if (*ptr=='/') {
      year=Str2I64Cpp(ptr+1,10,&ptr);
      if (year<100) year+=2000;
    } else {
      uint64_t now=Now();
      CDate date;
      MemCpy(&date,&now,sizeof(date));
      CDateStruct ds;
      Date2Struct(ds,date);
      year=ds.year;
    }
    // CDate stores its day in a signed 32-bit field.
    if (year<-5870000 || year>5870000) {
      Free(src);
      return 0;
    }
    res=(uint64_t(MonthStartDate(year,mon))+uint64_t(day)-1)<<32;
  }
  if (*ptr==')') ptr++;
  if (mode!=NONE) {
    CDate date;
    MemCpy(&date,&res,sizeof(date));
    CDateStruct ds;
    Date2Struct(ds,date);
    int64_t day;
    switch (mode) {
      case SM: day=MonthStartDate(ds.year,ds.mon); break;
      case EM: day=(ds.mon==12 ? YearStartDate(ds.year+1) : MonthStartDate(ds.year,ds.mon+1))-1; break;
      case SY: day=YearStartDate(ds.year); break;
      default: day=YearStartDate(ds.year+1)-1; break;
    }
    res=(uint64_t(day)<<32)|date.time;
  }
  if (*ptr=='+' || *ptr=='-')
    res+=uint64_t(Str2I64Cpp(ptr,10,nullptr))<<32;
  Free(src);
  return res;
}
