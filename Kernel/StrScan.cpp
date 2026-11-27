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
  // HolyC I64 accumulators wrap in both the significand and exponent loops.
  uint64_t i, j, k, d;
  uint8_t ch;
  bool neg = false, neg_e = false;
  auto scale = [](uint64_t value, uint64_t exponent) {
    uint64_t power = Pow10I64Cpp(static_cast<int64_t>(exponent)), result;
    // Str2F64 keeps the signed integer exact in x87 until the final F64 store.
    asm volatile("fildq %1; fmull %2; fstpl %0"
                 : "=m"(result)
                 : "m"(value), "m"(power));
    return result;
  };

  ch = *src++;
  while (char_bmp_white_space[ch / 32] & (uint32_t{1} << (ch % 32)))
    ch = *src++;
  if (ch == '-') {
    neg = true;
    ch = *src++;
  }
  if (src[-1] == 'i' && src[0] == 'n' && src[1] == 'f') {
    d = 0x7FF0000000000000;
    src += 3;
    goto a2f_end;
  }
  if (uint8_t(*src) == 0xEC) {
    d = 0x7FF0000000000000;
    src++;
    goto a2f_end;
  }
  i = 0;
  while (true) {
    if (char_bmp_dec_numeric[ch / 32] & (uint32_t{1} << (ch % 32)))
      i = i * 10 + ch - '0';
    else {
      if (ch == '.' || ch == 'e' || ch == 'E')
        break;
      d = F64FromI64(static_cast<int64_t>(i));
      goto a2f_end;
    }
    ch = *src++;
  }
  if (ch == '.')
    ch = *src++;
  k = 0;
  while (true) {
    if (char_bmp_dec_numeric[ch / 32] & (uint32_t{1} << (ch % 32))) {
      i = i * 10 + ch - '0';
      k++;
    } else {
      if (ch == 'e' || ch == 'E')
        break;
      d = scale(i, 0 - k);
      goto a2f_end;
    }
    ch = *src++;
  }
  ch = *src++;
  if (ch == '-') {
    neg_e = true;
    ch = *src++;
  }
  j = 0;
  while (true) {
    if (char_bmp_dec_numeric[ch / 32] & (uint32_t{1} << (ch % 32)))
      j = j * 10 + ch - '0';
    else {
      if (neg_e)
        d = scale(i, 0 - j - k);
      else
        d = scale(i, j - k);
      goto a2f_end;
    }
    ch = *src++;
  }
a2f_end:
  if (end)
    *end = src - 1;
  return d ^ (neg ? uint64_t{1} << 63 : 0);
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
