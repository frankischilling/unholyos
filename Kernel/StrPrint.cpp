#include "KernelB.hpp"

extern "C" uint8_t *FileReadCpp(const char *path, size_t *size, uint64_t *attr);
extern "C" uint8_t *DocSaveNoCursorCpp(CDoc *doc);
extern "C" uint64_t F64MulBits(uint64_t a, uint64_t b);
extern "C" uint64_t F64DivBits(uint64_t a, uint64_t b);
extern "C" uint64_t F64AddBits(uint64_t a, uint64_t b);
extern "C" uint64_t F64Log10Bits(uint64_t bits);
extern "C" int64_t F64ToI64(uint64_t value, uint16_t rounding);
extern "C" uint64_t F64FromI64(int64_t value);

enum : unsigned
{
  PRTF_PAD_ZERO = 1 << 0,
  PRTF_LEFT_JUSTIFY = 1 << 1,
  PRTF_TRUNCATE = 1 << 2,
  PRTF_COMMA = 1 << 3,
  PRTF_DOLLAR = 1 << 4,
  PRTF_SLASH = 1 << 5,
  PRTF_QUESTION = 1 << 6,
  PRTF_AUX_FMT_NUM = 1 << 7,
  PRTF_DECIMAL = 1 << 8,
  PRTF_NEG_AUX_FMT_NUM = 1 << 9,
};

struct CPrintFmt
{
  int64_t width, precision, aux;
  unsigned flags;
};

struct CPrintArgs
{
  const uint64_t *argv;
  int64_t count;
  uint64_t error;
  const char *undefined = nullptr;

  uint64_t Take()
  {
    if (!argv || count <= 0) {
      error = STR_PRINT_ERROR;
      return 0;
    }
    count--;
    return *argv++;
  }
};

class CPrintOut
{
public:
  explicit CPrintOut(char *dst)
      : data(dst), heap(dst ? nullptr : CurrentTask()->data_heap),
        capacity(dst ? SIZE_MAX : 0)
  {
  }

  void Put(char ch)
  {
    if (Reserve(1))
      data[length++] = ch;
  }

  void Put(const char *src, size_t size)
  {
    if (Reserve(size) && size) {
      MemCpy(data + length, src, size);
      length += size;
    }
  }

  void Fill(char ch, size_t count)
  {
    if (Reserve(count) && count) {
      MemSet(data + length, ch, count);
      length += count;
    }
  }

  char *Finish()
  {
    Put(0);
    if (!ok) {
      if (heap)
        Free(data);
      return nullptr;
    }
    return data;
  }

  bool Good() const
  {
    return ok;
  }

private:
  bool Reserve(size_t count)
  {
    if (!ok)
      return false;
    if (count <= capacity - length)
      return true;
    if (count > SIZE_MAX - length || !heap)
      return ok = false;
    size_t need = length + count;
    size_t grow = capacity <= (SIZE_MAX - 1) / 2 ? capacity * 2 + 1 : need;
    if (grow < 144)
      grow = 144;
    if (grow < need)
      grow = need;
    auto *next = reinterpret_cast<char *>(MAlloc(grow, heap));
    if (!next)
      return ok = false;
    if (length)
      MemCpy(next, data, length);
    Free(data);
    data = next;
    capacity = MSize(next);
    return true;
  }

  char *data;
  CHeapCtrl *heap;
  size_t length = 0, capacity;
  bool ok = true;
};

static void OutStr(CPrintOut &out, const char *text, CPrintFmt fmt)
{
  size_t len = text ? StrLen(text) : 0, width = fmt.width > 0 ? fmt.width : 0;
  if ((fmt.flags & PRTF_TRUNCATE) && len > width)
    len = width;
  size_t pad = width > len ? width - len : 0;
  if (!(fmt.flags & PRTF_LEFT_JUSTIFY))
    out.Fill(' ', pad);
  out.Put(text, len);
  if (fmt.flags & PRTF_LEFT_JUSTIFY)
    out.Fill(' ', pad);
}

static int64_t PrintNumber(const char *&fmt)
{
  int64_t n = 0;
  while (*fmt >= '0' && *fmt <= '9') {
    int digit = *fmt++ - '0';
    n = n > (INT64_MAX - digit) / 10 ? INT64_MAX : 10 * n + digit;
  }
  return n;
}

static int64_t PrintWidth(const char *&fmt, CPrintArgs &args)
{
  int64_t n = PrintNumber(fmt);
  if (*fmt == '*') {
    fmt++;
    return static_cast<int64_t>(args.Take());
  }
  return n;
}

static CPrintFmt PrintFlags(const char *&fmt, CPrintArgs &args)
{
  CPrintFmt spec{};
  if (*fmt == '-') {
    spec.flags |= PRTF_LEFT_JUSTIFY;
    fmt++;
  }
  if (*fmt == '0') {
    spec.flags |= PRTF_PAD_ZERO;
    fmt++;
  }
  spec.width = PrintWidth(fmt, args);
  if (*fmt == '.') {
    fmt++;
    spec.precision = PrintWidth(fmt, args);
    spec.flags |= PRTF_DECIMAL;
  }
  for (;;) {
    switch (*fmt) {
    case '$':
      spec.flags |= PRTF_DOLLAR;
      break;
    case '/':
      spec.flags |= PRTF_SLASH;
      break;
    case ',':
      spec.flags |= PRTF_COMMA;
      break;
    case 't':
      spec.flags |= PRTF_TRUNCATE;
      break;
    case 'l':
      break;
    case 'h': {
      fmt++;
      spec.flags |= PRTF_AUX_FMT_NUM;
      if (*fmt == '?') {
        fmt++;
        spec.flags |= PRTF_QUESTION;
      } else if (*fmt == '*') {
        fmt++;
        spec.aux = args.Take();
      } else {
        if (*fmt == '-') {
          fmt++;
          spec.flags |= PRTF_NEG_AUX_FMT_NUM;
        }
        int64_t n = PrintNumber(fmt);
        spec.aux = spec.flags & PRTF_NEG_AUX_FMT_NUM ? -n : n;
      }
      continue;
    }
    default:
      return spec;
    }
    fmt++;
  }
}

constexpr size_t TMP_BUF_LEN = 256, TMP_NUM_LEN = TMP_BUF_LEN - 8;

struct CPrintDigits
{
  char reverse[TMP_BUF_LEN];
  size_t len = 0;
  unsigned group = 0, left = 0;

  void Put(char ch)
  {
    reverse[len++] = ch;
  }
};

static char Digit(unsigned value, bool upper)
{
  return value < 10 ? char('0' + value)
                    : char((upper ? 'A' : 'a') + value - 10);
}

static void ReverseDigits(CPrintDigits &n, uint64_t value, unsigned base,
                          unsigned group, bool upper = true)
{
  n.group = n.left = group;
  do {
    n.Put(Digit(value % base, upper));
    value /= base;
    if (!value)
      break;
    if (group && !--n.left) {
      n.Put(',');
      n.left = group;
    }
  } while (n.len < TMP_NUM_LEN);
}

static void OutNumber(CPrintOut &out, CPrintDigits &n, const CPrintFmt &fmt,
                      bool negative)
{
  size_t width = fmt.width > 0 ? fmt.width : 0, sign = negative;
  if ((fmt.flags & PRTF_TRUNCATE) && n.len + sign > width)
    n.len = width > sign ? width - sign : 0;
  size_t pad = width > n.len + sign ? width - n.len - sign : 0;
  if (fmt.flags & PRTF_PAD_ZERO) {
    if (negative)
      out.Put('-');
    if (n.group) {
      unsigned left =
          (pad % (n.group + 1) + n.group - n.left + 1) % (n.group + 1) + 1;
      while (pad && out.Good()) {
        if (!--left) {
          out.Put(',');
          left = n.group;
          if (!--pad)
            break;
        }
        out.Put('0');
        pad--;
      }
    } else {
      out.Fill('0', pad);
    }
  } else {
    out.Fill(' ', pad);
    if (negative)
      out.Put('-');
  }
  while (n.len)
    out.Put(n.reverse[--n.len]);
}

static void OutInteger(CPrintOut &out, uint64_t value, const CPrintFmt &fmt,
                       unsigned base, bool upper = true, bool negative = false)
{
  CPrintDigits n;
  ReverseDigits(n, value, base,
                fmt.flags & PRTF_COMMA ? (base == 10 ? 3 : 4) : 0, upper);
  OutNumber(out, n, fmt, negative);
}

static void OutQuote(CPrintOut &out, const char *text, unsigned flags)
{
  for (; text && *text; text++) {
    unsigned char ch = *text;
    switch (ch) {
    case '$':
      out.Put(flags & PRTF_DOLLAR ? "\\d" : "$$", 2);
      break;
    case '%':
      out.Put('%');
      if (flags & PRTF_SLASH)
        out.Put('%');
      break;
    case '\n':
      out.Put("\\n", 2);
      break;
    case '\r':
      out.Put("\\r", 2);
      break;
    case '\t':
      out.Put("\\t", 2);
      break;
    case '"':
    case '\\':
      out.Put('\\');
      out.Put(ch);
      break;
    default:
      if (ch < 0x1F || ch == 0x7F) {
        out.Put("\\x", 2);
        out.Put(Digit(ch >> 4, true));
        out.Put(Digit(ch & 15, true));
      } else {
        out.Put(ch);
      }
      break;
    }
  }
}

static int HexDigit(unsigned char ch)
{
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  ch |= 32;
  return ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : -1;
}

static void OutUnquote(CPrintOut &out, const char *text, unsigned flags)
{
  while (text && *text) {
    char ch = *text++;
    if (ch == '\\') {
      switch (*text) {
      case '0':
        ch = 0;
        break;
      case '\'':
      case '`':
      case '"':
      case '\\':
        ch = *text;
        break;
      case 'd':
        ch = '$';
        break;
      case 'n':
        ch = '\n';
        break;
      case 'r':
        ch = '\r';
        break;
      case 't':
        ch = '\t';
        break;
      case 'x':
      case 'X': {
        ch = 0;
        text++;
        for (int i = 0; i < 2; i++) {
          int digit = HexDigit(*text);
          if (digit < 0)
            break;
          ch = static_cast<unsigned char>(ch) * 16 + digit;
          text++;
        }
        out.Put(ch);
        continue;
      }
      default:
        out.Put(ch);
        continue;
      }
      text++;
    } else if (*text == ch &&
               (ch == '$' || (ch == '%' && (flags & PRTF_SLASH)))) {
      text++;
    }
    out.Put(ch);
  }
}

static void OutDateTime(CPrintOut &out, uint64_t value, bool date)
{
  const CPrintFmt two{2, 0, 0, PRTF_PAD_ZERO};
  CDateStruct ds;
  Date2Struct(ds, DateFromBits(value + DateBits(local_time_offset)));
  int64_t first, second, third;
  char sep;
  if (date) {
    first = ds.mon;
    second = ds.day_of_mon;
    third = ds.year % 100;
    sep = '/';
  } else {
    first = ds.hour;
    second = ds.min;
    third = ds.sec;
    sep = ':';
  }
  OutInteger(out, first, two, 10);
  out.Put(sep);
  OutInteger(out, second, two, 10);
  out.Put(sep);
  OutInteger(out, third < 0 ? -third : third, two, 10, true, third < 0);
}

static void OutFunSeg(CPrintOut &out, uint64_t addr, const CPrintFmt &fmt)
{
  uint64_t offset;
  auto *sym = FunSegFind(addr, offset);
  bool short_name = fmt.flags & PRTF_COMMA;
  if (!sym) {
    if (short_name)
      out.Put('.');
    else
      OutInteger(out, addr,
                 {fmt.flags & PRTF_TRUNCATE ? fmt.width : 0, 0, 0,
                  fmt.flags & PRTF_TRUNCATE},
                 16);
    return;
  }
  int64_t width = (fmt.flags & PRTF_TRUNCATE) ? fmt.width : 0;
  if (!short_name)
    width = width > 7 ? width - 7 : 0;
  bool is_fun = sym->type & HTT_FUN;
  size_t length = StrLen(sym->str) + is_fun;
  if (width) {
    size_t columns = width > 0 ? width : 0;
    if (length > columns)
      length = columns;
    else if (!(fmt.flags & PRTF_LEFT_JUSTIFY))
      out.Fill(' ', columns - length);
  }
  if (is_fun && length) {
    out.Put('&');
    length--;
  }
  out.Put(sym->str, length);
  if (!short_name) {
    out.Put("+0x", 3);
    OutInteger(out, offset, {4, 0, 0, PRTF_PAD_ZERO}, 16);
  }
}

constexpr uint64_t F64_SIGN = 0x8000000000000000, F64_INF = 0x7FF0000000000000;
constexpr uint64_t F64_ONE = 0x3FF0000000000000, F64_TWO = 0x4000000000000000,
                   F64_TEN = 0x4024000000000000,
                   F64_HUNDRED = 0x4059000000000000;
constexpr uint16_t F64_NEAREST = 0, F64_FLOOR = 0x0400, F64_TRUNCATE = 0x0C00;

static int64_t Log10I64(uint64_t bits, uint16_t rounding)
{
  return bits & ~F64_SIGN ? F64ToI64(F64Log10Bits(bits), rounding) : 0;
}

static char FixedDigit(uint64_t &value, int64_t &zeroes)
{
  if (zeroes) {
    zeroes--;
    return '0';
  }
  char ch = '0' + value % 10;
  value /= 10;
  return ch;
}

static void ReverseFixed(CPrintDigits &n, uint64_t bits, const CPrintFmt &fmt)
{
  int64_t places = fmt.precision < 0 ? 0 : fmt.precision;
  bool comma = fmt.flags & PRTF_COMMA;
  int64_t exponent = Log10I64(bits, F64_TRUNCATE);
  int64_t scale = places - (comma ? places / 4 : 0);
  int64_t zeroes = exponent > 17 - scale ? exponent - 17 + scale : 0;
  if (scale || zeroes)
    bits = F64MulBits(bits, Pow10I64Cpp(scale - zeroes));
  uint64_t value = F64ToI64(bits, F64_NEAREST);
  int group = places & 3;
  for (int64_t i = places; i-- > 0 && n.len < TMP_NUM_LEN;) {
    if (comma && i > 2 && !group--) {
      n.Put(',');
      group = 2;
      if (!--i)
        break;
    }
    n.Put(FixedDigit(value, zeroes));
  }
  if (places)
    n.Put('.');
  group = 3;
  do {
    n.Put(FixedDigit(value, zeroes));
    if (!value)
      break;
    if (comma && !--group) {
      n.Put(',');
      group = 3;
    }
  } while (n.len < TMP_NUM_LEN);
}

static void ReverseExponent(CPrintDigits &digits, int64_t exponent)
{
  uint64_t value = exponent < 0 ? 0 - uint64_t(exponent) : exponent;
  for (int i = 0; i < 4; i++) {
    digits.Put(Digit(value % 10, true));
    value /= 10;
    if (!value)
      break;
  }
  if (exponent < 0)
    digits.Put('-');
  digits.Put('e');
}

static uint64_t RoundForWidth(uint64_t bits, int64_t exponent)
{
  return F64AddBits(bits, F64DivBits(Pow10I64Cpp(exponent), F64_TWO));
}

static void OutFloat(CPrintOut &out, uint64_t bits, CPrintFmt fmt, char mode,
                     bool negative)
{
  CPrintDigits n;
  if (mode != 'f') {
    if (!fmt.width)
      fmt.width = 12;
    if (mode != 'd')
      fmt.flags |= PRTF_TRUNCATE;
  }
  if ((bits & ~F64_SIGN) >= F64_INF) {
    n.Put(static_cast<char>(0xEC));
    fmt.flags &= ~PRTF_PAD_ZERO;
    OutNumber(out, n, fmt, negative);
    return;
  }
  int64_t width = fmt.width > 0 ? fmt.width : 0;
  if (width > 1000)
    width = 1000;
  if (fmt.precision > 1000)
    fmt.precision = 1000;
  else if (fmt.precision < -1000)
    fmt.precision = -1000;
  int64_t exponent = Log10I64(bits, F64_FLOOR);
  if (mode == 'e' || (mode == 'g' && (exponent >= width - 1 - fmt.precision ||
                                      exponent < 1 - fmt.precision))) {
    bits = F64DivBits(bits, Pow10I64Cpp(exponent));
    for (int i = 0; i < 2; i++) {
      n.len = 0;
      ReverseExponent(n, exponent);
      fmt.precision = width - static_cast<int64_t>(n.len) - 2 - negative;
      if (!bits)
        break;
      uint64_t rounded = RoundForWidth(bits, -fmt.precision);
      if (rounded < F64_ONE) {
        bits = F64MulBits(bits, F64_TEN);
        exponent--;
      } else if (rounded >= F64_TEN) {
        bits = F64DivBits(bits, F64_TEN);
        exponent++;
      } else {
        break;
      }
      n.len = 0;
    }
  } else if (mode == 'n' || mode == 'd') {
    if (exponent < 0) {
      exponent++;
      exponent = static_cast<int64_t>(uint64_t(exponent - exponent % 3) - 3);
    } else {
      exponent -= exponent % 3;
    }
    bits = F64DivBits(bits, Pow10I64Cpp(exponent));
    bool si =
        (fmt.flags & PRTF_AUX_FMT_NUM) && exponent >= -24 && exponent <= 24 &&
        ((fmt.flags & PRTF_QUESTION) || (fmt.aux >= -26 && fmt.aux <= 26));
    int64_t shift = 0;
    if (si) {
      constexpr char positive[] = " KMGTPEZY", negative_si[] = " m\xE6npfazy";
      int64_t index = exponent / 3;
      if (!(fmt.flags & PRTF_QUESTION)) {
        shift = exponent - fmt.aux;
        index = fmt.aux / 3;
        bits = F64MulBits(bits, Pow10I64Cpp(shift));
      }
      if (index || fmt.width)
        n.Put(index < 0 ? negative_si[-index] : positive[index]);
    } else {
      ReverseExponent(n, exponent);
    }
    if (si ? !(fmt.flags & PRTF_DECIMAL) : !fmt.precision) {
      fmt.precision = width - static_cast<int64_t>(n.len) - 2 - negative;
      if (shift > 0)
        fmt.precision -= fmt.flags & PRTF_COMMA ? 4 * shift / 3 : shift;
      uint64_t rounded = RoundForWidth(bits, 1 - fmt.precision);
      if (rounded >= F64_TEN)
        fmt.precision -= rounded >= F64_HUNDRED ? 2 : 1;
    }
    if (fmt.flags & PRTF_COMMA) {
      if (fmt.width && fmt.precision > 0 && !(fmt.precision & 3))
        n.Put(',');
      fmt.precision -= fmt.precision / 4;
    }
  }
  ReverseFixed(n, bits, fmt);
  OutNumber(out, n, fmt, negative);
}

static void OutChars(CPrintOut &out, uint64_t value, CPrintFmt fmt, bool upper)
{
  char text[9];
  for (int i = 0; i < 8; i++) {
    unsigned char ch = value >> (8 * i);
    text[i] = upper && ch >= 'a' && ch <= 'z' ? ch - 'a' + 'A' : ch;
  }
  text[8] = 0;
  int64_t repeat = fmt.flags & PRTF_AUX_FMT_NUM ? fmt.aux : 1;
  if (!*text && fmt.width <= 0)
    return;
  for (int64_t i = 0; i < repeat && out.Good(); i++)
    OutStr(out, text, fmt);
}

static void OutFormatted(CPrintOut &out, char kind, CPrintFmt fmt,
                         CPrintArgs &args, bool buffer)
{
  if (kind == '%') {
    out.Put('%');
    return;
  }
  const char *text = nullptr;
  char *allocated = nullptr;
  switch (kind) {
  case 's':
    text = reinterpret_cast<const char *>(args.Take());
    break;
  case 'S':
  case 'Z': {
    int64_t index = kind == 'Z' ? args.Take() : 0;
    auto *name = reinterpret_cast<const char *>(args.Take());
    bool defined;
    text = kind == 'Z' ? DefineSub(index, name, defined)
                       : DefineText(name, defined);
    if (!defined) {
      args.error = UNDEF_DEFINE_ERROR;
      args.undefined = name;
    }
    break;
  }
  case 'z': {
    int64_t index = args.Take();
    text = LstSub(index, reinterpret_cast<const char *>(args.Take()));
    break;
  }
  case 'F': {
    uint64_t source = args.Take();
    if (source) {
      auto *data = fmt.flags & PRTF_DOLLAR
                       ? DocSaveNoCursorCpp(reinterpret_cast<CDoc *>(source))
                       : FileReadCpp(reinterpret_cast<const char *>(source),
                                     nullptr, nullptr);
      allocated = reinterpret_cast<char *>(data);
    }
    text = allocated;
    break;
  }
  case 'p':
  case 'P': {
    uint64_t addr = args.Take();
    CPrintOut symbol(nullptr);
    bool link = kind == 'P' && (buffer || !IsRawCpp());
    if (link)
      symbol.Put("$LK,\"", 5);
    OutFunSeg(symbol, addr, fmt);
    if (link) {
      symbol.Put("\",A=\"AD:0x", 10);
      OutInteger(symbol, addr, {}, 16);
      symbol.Put("\"$", 2);
    }
    allocated = symbol.Finish();
    text = allocated;
    break;
  }
  case 'q':
  case 'Q': {
    auto *src = reinterpret_cast<const char *>(args.Take());
    CPrintOut quoted(nullptr);
    if (kind == 'Q')
      OutQuote(quoted, src, fmt.flags);
    else
      OutUnquote(quoted, src, fmt.flags);
    allocated = quoted.Finish();
    text = allocated;
    break;
  }
  case 'D':
  case 'T': {
    char date[11];
    CPrintOut value(date);
    OutDateTime(value, args.Take(), kind == 'D');
    OutStr(out, value.Finish(), fmt);
    return;
  }
  case 'c':
  case 'C':
    OutChars(out, args.Take(), fmt, kind == 'C');
    return;
  case 'X':
  case 'x':
  case 'B':
  case 'b':
    OutInteger(out, args.Take(), fmt, kind == 'X' || kind == 'x' ? 16 : 2,
               kind != 'x');
    return;
  case 'u':
  case 'd': {
    uint64_t value = args.Take();
    bool negative = kind == 'd' && static_cast<int64_t>(value) < 0;
    if (negative)
      value = 0 - value;
    if (fmt.flags & PRTF_AUX_FMT_NUM)
      OutFloat(out, F64FromI64(static_cast<int64_t>(value)), fmt, 'd',
               negative);
    else
      OutInteger(out, value, fmt, 10, true, negative);
    return;
  }
  case 'e':
  case 'f':
  case 'g':
  case 'n': {
    uint64_t value = args.Take(), magnitude = value & ~F64_SIGN;
    OutFloat(out, magnitude, fmt, kind,
             (value & F64_SIGN) && magnitude && magnitude <= F64_INF);
    return;
  }
  default:
    return;
  }
  OutStr(out, text, fmt);
  Free(allocated);
}

extern "C" char *StrPrintJoin(char *dst, const char *fmt, int64_t argc,
                              const uint64_t *argv)
{
  if (!fmt)
    RaiseHolyC(STR_PRINT_ERROR);
  CPrintOut out(dst);
  CPrintArgs args{argv, argc, 0};
  while (*fmt && !args.error && out.Good()) {
    char ch = *fmt++;
    if (ch != '%') {
      out.Put(ch);
      continue;
    }
    auto spec = PrintFlags(fmt, args);
    if (!*fmt)
      break;
    if (!args.error)
      OutFormatted(out, *fmt++, spec, args, dst != nullptr);
  }
  auto *result = out.Finish();
  if (args.error || !result) {
    if (!dst)
      Free(result);
    if (args.undefined)
      UndefinedDefine(args.undefined);
    RaiseHolyC(STR_PRINT_ERROR);
  }
  return result;
}

extern "C" char *CatPrintCpp(char *dst, const char *fmt, int64_t argc,
                             const uint64_t *argv)
{
  StrPrintJoin(dst + StrLen(dst), fmt, argc, argv);
  return dst;
}

extern "C" char *MStrPrintCpp(const char *fmt, int64_t argc,
                              const uint64_t *argv)
{
  auto *buf = StrPrintJoin(nullptr, fmt, argc, argv);
  auto *result = StrNew(buf, CurrentTask()->data_heap);
  Free(buf);
  return result;
}

extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv)
{
  auto *buf = StrPrintJoin(nullptr, fmt, argc, argv);
  PutS(buf);
  Free(buf);
}

static void PrintDiagnostic(const char *label, const uintptr_t *frame,
                            const char *fmt, int64_t argc, const uint64_t *argv)
{
  auto *buf = StrPrintJoin(nullptr, fmt, argc, argv);
  const auto &task = *CurrentTask();
  if (!(task.display_flags & DISPLAYF_SILENT)) {
    uint64_t addrs[]{reinterpret_cast<uintptr_t>(Caller(frame, 0, task)),
                     reinterpret_cast<uintptr_t>(Caller(frame, 2, task)),
                     reinterpret_cast<uintptr_t>(Caller(frame, 3, task)),
                     reinterpret_cast<uintptr_t>(Caller(frame, 4, task))};
    auto *prefix = StrPrintJoin(nullptr, "%,p %,p %,p %,p ", 4, addrs);
    PutS(prefix);
    PutS(label);
    PutS(buf);
    Free(prefix);
  }
  Free(buf);
}

extern "C" void PrintErrCpp(const char *fmt, int64_t argc, const uint64_t *argv,
                            const uintptr_t *frame)
{
  PrintDiagnostic("ERROR: ", frame, fmt, argc, argv);
}

extern "C" void PrintWarnCpp(const char *fmt, int64_t argc,
                             const uint64_t *argv, const uintptr_t *frame)
{
  PrintDiagnostic("WARNING: ", frame, fmt, argc, argv);
}
