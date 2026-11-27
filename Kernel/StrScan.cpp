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

extern "C" uint64_t Str2F64Cpp(const char *src, const char **end)
{
  if (!src) {
    if (end)
      *end = nullptr;
    return 0;
  }
  while (char_bmp_white_space[uint8_t(*src) / 32] &
         (uint32_t{1} << (uint8_t(*src) % 32)))
    src++;
  bool neg = *src == '-';
  if (neg)
    src++;
  uint64_t result;
  if (src[0] == 'i' && src[1] == 'n' && src[2] == 'f') {
    result = 0x7FF0000000000000;
    src += 3;
  } else if (uint8_t(*src) == 0xEC) {
    result = 0x7FF0000000000000;
    src++;
  } else {
    uint64_t i = 0;
    int64_t k = 0, j = 0;
    while ('0' <= *src && *src <= '9')
      i = i * 10 + *src++ - '0';
    if (*src == '.') {
      src++;
      while ('0' <= *src && *src <= '9') {
        i = i * 10 + *src++ - '0';
        if (k < 1000000)
          k++;
      }
    }
    if (*src == 'e' || *src == 'E') {
      src++;
      bool neg_e = *src == '-';
      if (neg_e)
        src++;
      while ('0' <= *src && *src <= '9') {
        if (j < 1000000)
          j = j * 10 + *src - '0';
        src++;
      }
      if (neg_e)
        j = -j;
    }
    result = F64MulBits(F64FromI64(int64_t(i)), Pow10I64Cpp(j - k));
  }
  if (end)
    *end = src;
  return result ^ (neg ? uint64_t{1} << 63 : 0);
}

extern "C" uint64_t Str2DateCpp(const char *_src)
{
  CDate res{};
  CDateStruct ds, ds_now;
  char *src = MStrUtilCpp(_src, SUF_REM_SPACES | SUF_TO_UPPER,
                          0x3FF0000000000000ULL),
       *v = StrNew(src, CurrentTask()->data_heap), *ptr = src;
  bool start_mon = false, end_mon = false, start_year = false, end_year = false;
  const char *end;
  if (!v)
    RaiseHolyC(0x6D654D74754FULL);

  MemSet(&ds, 0, sizeof(CDateStruct));
  if (ptr[0] == 'S' && ptr[1] == 'M' && ptr[2] == '(') {
    ptr += 3;
    start_mon = true;
  } else if (ptr[0] == 'E' && ptr[1] == 'M' && ptr[2] == '(') {
    ptr += 3;
    end_mon = true;
  } else if (ptr[0] == 'S' && ptr[1] == 'Y' && ptr[2] == '(') {
    ptr += 3;
    start_year = true;
  } else if (ptr[0] == 'E' && ptr[1] == 'Y' && ptr[2] == '(') {
    ptr += 3;
    end_year = true;
  }
  if (*ptr == '*') {
    ptr++;
    if (*ptr == '+' || *ptr == '-') {
      res.date = Str2I64Cpp(ptr, 10, &end);
      ptr = const_cast<char *>(end);
    }
    res = DateFromBits(DateBits(res) + Now());
    res = DateFromBits(DateBits(res) + DateBits(local_time_offset));
  } else {
    StrFirstRemCpp(ptr, reinterpret_cast<const uint8_t *>("/"), v);
    ds.mon = Str2I64Cpp(v, 10, nullptr);
    if (StrFirstOccCpp(ptr, reinterpret_cast<const uint8_t *>("/"))) {
      StrFirstRemCpp(ptr, reinterpret_cast<const uint8_t *>("/"), v);
      ds.day_of_mon = Str2I64Cpp(v, 10, nullptr);
      ds.year = Str2I64Cpp(ptr, 10, &end);
      ptr = const_cast<char *>(end);
      if (ds.year < 100)
        ds.year += 2000;
    } else {
      ds.day_of_mon = Str2I64Cpp(ptr, 10, &end);
      ptr = const_cast<char *>(end);
      auto now = Now();
      Date2Struct(ds_now, DateFromBits(now + DateBits(local_time_offset)));
      ds.year = ds_now.year;
    }
    res = Struct2Date(ds);
  }
  if (*ptr == ')')
    ptr++;

  if (start_mon)
    res.date = FirstDayOfMon(res.date);
  else if (end_mon)
    res.date = LastDayOfMon(res.date);
  else if (start_year)
    res.date = FirstDayOfYear(res.date);
  else if (end_year)
    res.date = LastDayOfYear(res.date);

  if (*ptr == '+' || *ptr == '-')
    res.date = uint32_t(res.date) + uint32_t(Str2I64Cpp(ptr, 10, nullptr));
  Free(src);
  Free(v);
  return DateBits(res) - DateBits(local_time_offset);
}

extern "C" char *StrScanCpp(char *src, const char *fmt, int64_t argc,
                            const uint64_t *argv)
{
  char *buf, *ptr, **pptr;
  [[maybe_unused]] bool left_justify = false;
  int64_t ch, cur_arg = 0, i, len;
  [[maybe_unused]] int64_t dec_len;
  uint64_t value;
  void *out;
  constexpr uint64_t scan_error = 0x6E616353; // Scan
  if (!fmt)
    RaiseHolyC(scan_error);
  while ((ch = uint8_t(*fmt++))) {
    if (ch == '%') {
      if (*fmt == '%') {
        src++;
        fmt++;
      } else {
        if (*fmt == '-') {
          left_justify = true;
          fmt++;
        } else
          left_justify = false;
        len = 0;
        while ('0' <= *fmt && *fmt <= '9')
          len = int64_t(uint64_t(len) * 10 + (*fmt++ - '0'));
        if (*fmt == '*') {
          fmt++;
          if (cur_arg >= argc)
            RaiseHolyC(scan_error);
          len = int64_t(argv[cur_arg++]);
        }
        ch = uint8_t(*fmt++);
        if (ch && !len) {
          ptr = src;
          while (*ptr && *ptr != *fmt)
            ptr++;
          len = ptr - src;
        } else {
          if (ch == '.') {
            dec_len = 0;
            while ('0' <= *fmt && *fmt <= '9')
              dec_len = int64_t(uint64_t(dec_len) * 10 + (*fmt++ - '0'));
            if (*fmt == '*') {
              fmt++;
              if (cur_arg >= argc)
                RaiseHolyC(scan_error);
              dec_len = int64_t(argv[cur_arg++]);
            }
            ch = uint8_t(*fmt++);
          }
        }
        buf = reinterpret_cast<char *>(
            MAlloc(uint64_t(len) + 1, CurrentTask()->data_heap));
        if (!buf)
          RaiseHolyC(0x6D654D74754FULL); // OutMem from the reference MAlloc.
        for (i = 0; i < len; i++)
          buf[i] = *src++;
        buf[i] = 0;
        switch (ch) {
        case 's':
          if (cur_arg >= argc)
            RaiseHolyC(scan_error);
          pptr = reinterpret_cast<char **>(argv[cur_arg++]);
          MemCpy(&ptr, pptr, sizeof(ptr));
          MemCpy(ptr, buf, StrLen(buf) + 1);
          break;
        case 'c':
          if (cur_arg >= argc)
            RaiseHolyC(scan_error);
          ptr = reinterpret_cast<char *>(argv[cur_arg++]);
          *ptr = *buf;
          break;
        case 'C':
          if (cur_arg >= argc)
            RaiseHolyC(scan_error);
          ptr = reinterpret_cast<char *>(argv[cur_arg++]);
          *ptr = 'a' <= *buf && *buf <= 'z' ? *buf - ('a' - 'A') : *buf;
          break;
        case 'z':
          if (cur_arg + 1 >= argc)
            RaiseHolyC(scan_error);
          out = reinterpret_cast<void *>(argv[cur_arg++]);
          value = LstMatchCpp(
              reinterpret_cast<uint8_t *>(buf),
              reinterpret_cast<const uint8_t *>(argv[cur_arg++]), 0);
          MemCpy(out, &value, sizeof(value));
          break;
        case 'Z':
          if (cur_arg + 1 >= argc)
            RaiseHolyC(scan_error);
          out = reinterpret_cast<void *>(argv[cur_arg++]);
          value = DefineMatchCpp(
              reinterpret_cast<uint8_t *>(buf),
              reinterpret_cast<const char *>(argv[cur_arg++]), 0);
          MemCpy(out, &value, sizeof(value));
          break;
        case 'd':
        case 'X':
        case 'b':
          if (cur_arg >= argc)
            RaiseHolyC(scan_error);
          out = reinterpret_cast<void *>(argv[cur_arg++]);
          value = Str2I64Cpp(buf, ch == 'd' ? 10 : ch == 'X' ? 16 : 2, nullptr);
          MemCpy(out, &value, sizeof(value));
          break;
        case 'e':
        case 'f':
        case 'g':
        case 'n':
          if (cur_arg >= argc)
            RaiseHolyC(scan_error);
          out = reinterpret_cast<void *>(argv[cur_arg++]);
          value = Str2F64Cpp(buf, nullptr);
          MemCpy(out, &value, sizeof(value));
          break;
        case 'D':
          if (cur_arg >= argc)
            RaiseHolyC(scan_error);
          out = reinterpret_cast<void *>(argv[cur_arg++]);
          value = Str2DateCpp(buf);
          MemCpy(out, &value, sizeof(value));
          break;
        }
        Free(buf);
      }
    } else
      src++;
  }
  return src;
}
