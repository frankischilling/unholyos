#include "DolDoc.hpp"

static constexpr const char *commands[] = {
    "TX", "CR", "SR", "TB", "PB", "CU", "MK", "PT", "CL", "PL", "LM",
    "RM", "HD", "FO", "ID", "FG", "BG", "FD", "BD", "WW", "HL", "BK",
    "IV", "UL", "SX", "SY", "CM", "AN", "LK", "BT", "DA", "CB", "LS",
    "MA", "MU", "HX", "TR", "SP", "IB", "IS", "SO", "HC", "ER"};
static constexpr const char *flag_names[] = {
    "T",   "LEN", "A",  "D",   "HTML", "LE",  "LM",  "RE", "RM",  "BI", "BP",
    "RT",  "SX",  "SY", "SCX", "U",    "LC",  "LIS", "RC", "RIS", "LX", "CX",
    "RX",  "TY",  "CY", "BY",  "HL",   "WW",  "BK",  "IV", "SEL", "UL", "TC",
    "PRY", "MRX", "WR", "L",   "X",    "Q",   "FST", "B",  "S",   "BD", "C",
    "CA",  "RD",  "UD", "P",   "M",    "TRM", "Z",   "H",  "TR",  "LS", "SK",
    "PU",  "SIF", "FS", "NC",  "DD",   "DL",  "DRT"};
static constexpr const char *colors[] = {
    "BLACK", "BLUE",     "GREEN",  "CYAN",   "RED",     "PURPLE",
    "BROWN", "LTGRAY",   "DKGRAY", "LTBLUE", "LTGREEN", "LTCYAN",
    "LTRED", "LTPURPLE", "YELLOW", "WHITE"};
static constexpr const char *raw_types[] = {
    "",    "",    "I0",  "U0",  "I8",  "U8",   "I16", "U16",
    "I32", "U32", "I64", "U64", "F32", "UF32", "F64", "UF64"};
static_assert(sizeof(commands) / sizeof(*commands) == DOCT_TYPES_NUM);
static_assert(sizeof(flag_names) / sizeof(*flag_names) == 62);

constexpr uint64_t DOCG_ARGS = 0xFFFF;
constexpr uint64_t DOCG_TRANSIENT = DOCG_BL_IV_UL | (1ull << 30) |
                                    (1ull << 26) | (1ull << 27) | DOCEF_SKIP |
                                    DOCEF_FILTER_SKIP;
constexpr uint32_t DOCET_UNDERLINE = 1u << 31;

static uint64_t DefaultFlags(unsigned type)
{
  constexpr uint64_t deref = 1ull << 47, term = 1ull << 49,
                     collapsible = 1ull << 44, escape = 1ull << 37;
  switch (type) {
  case DOCT_TEXT:
  case DOCT_ANCHOR:
  case DOCT_SPRITE:
  case DOCT_INS_BIN:
  case DOCT_INS_BIN_SIZE:
  case DOCT_SONG:
  case DOCT_HTML_CODE:
    return DOCEF_TAG;
  case DOCT_DATA:
    return DOCEF_TAG | deref | term | DOCEF_DFT_LEN | DOCEF_DFT_RAW_TYPE;
  case DOCT_CURSOR_MOVEMENT:
    return DOCEF_LEFT_EXP | DOCEF_RIGHT_EXP;
  case DOCT_LINK:
    return DOCEF_TAG | (1ull << 36);
  case DOCT_MACRO:
  case DOCT_MENU_VAL:
    return DOCEF_TAG | escape;
  case DOCT_BTTN:
    return DOCEF_TAG | escape | (1ull << 40);
  case DOCT_CHECK_BOX:
    return DOCEF_TAG | deref | collapsible | DOCEF_DFT_RAW_TYPE;
  case DOCT_LST:
    return DOCEF_TAG | deref | DOCEF_LST | DOCEF_DFT_RAW_TYPE;
  case DOCT_HEX_ED:
    return deref | (1ull << 50);
  case DOCT_TREE:
    return DOCEF_TAG | DOCEF_TREE | collapsible | (1ull << 43);
  default:
    return 0;
  }
}

static uint32_t DefaultTypeFlags(unsigned type)
{
  return type == DOCT_LINK || type == DOCT_MACRO || type == DOCT_MENU_VAL ||
                 type == DOCT_TREE
             ? DOCET_UNDERLINE
             : 0;
}

static bool HasAttribute(unsigned type)
{
  return type >= DOCT_PAGE_LEN && type <= DOCT_SHIFTED_Y;
}

static int Hex(unsigned char ch)
{
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  ch |= 32;
  return ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : -1;
}

class CDollarCmd
{
public:
  CDollarCmd(const char *text, const char *end) : ptr(text), end(end)
  {
  }

  char Peek()
  {
    while (ptr < end &&
           (*ptr == ' ' || *ptr == '\t' || *ptr == '\r' || *ptr == '\n'))
      ptr++;
    return ptr < end ? *ptr : 0;
  }

  bool Eat(char ch)
  {
    if (Peek() != ch)
      return false;
    ptr++;
    return true;
  }

  void Commas()
  {
    while (Eat(','))
      ;
  }

  template <size_t N> int Match(const char *const (&table)[N])
  {
    Peek();
    const char *start = ptr, *tail = ptr;
    while (tail < end && Ident(*tail))
      tail++;
    if (tail == start)
      return -1;
    size_t len = tail - start;
    for (size_t i = 0; i < N; i++) {
      const char *key = table[i];
      if (StrLen(key) != len)
        continue;
      size_t j = 0;
      while (j < len && key[j] == start[j])
        j++;
      if (j == len) {
        ptr = tail;
        return i;
      }
    }
    return -1;
  }

  bool Number(int64_t &result)
  {
    bool neg = Eat('-');
    if (!neg)
      Eat('+');
    if (Eat('(')) {
      if (!Number(result) || !Eat(')'))
        return false;
      if (neg)
        result = static_cast<int64_t>(0 - uint64_t(result));
      return true;
    }
    constexpr const char *boolean[] = {"OFF", "ON", "FALSE", "TRUE"};
    int symbol = Match(boolean);
    if (symbol >= 0) {
      result = symbol & 1;
    } else if ((symbol = Match(colors)) >= 0) {
      result = symbol;
    } else {
      unsigned base = 10;
      if (end - ptr >= 2 && ptr[0] == '0') {
        if (ptr[1] == 'x' || ptr[1] == 'X')
          base = 16;
        if (ptr[1] == 'b' || ptr[1] == 'B')
          base = 2;
        if (base != 10)
          ptr += 2;
      }
      bool found = false;
      uint64_t value = 0;
      for (; ptr < end; ptr++) {
        int digit = Hex(*ptr);
        if (digit < 0 || unsigned(digit) >= base)
          break;
        value = value * base + digit;
        found = true;
      }
      if (!found)
        return false;
      result = static_cast<int64_t>(value);
    }
    if (neg)
      result = static_cast<int64_t>(0 - uint64_t(result));
    return true;
  }

  char *String(CHeapCtrl *hc, const char *prefix = "")
  {
    if (!Eat('"'))
      return nullptr;
    size_t pre = StrLen(prefix);
    auto *temp = reinterpret_cast<char *>(MAlloc(pre + (end - ptr) + 1, hc));
    if (!temp)
      return nullptr;
    MemCpy(temp, prefix, pre);
    size_t len = pre;
    do {
      bool closed = false;
      while (ptr < end) {
        char ch = *ptr++;
        if (ch == '"') {
          closed = true;
          break;
        }
        if (ch == '\\' && ptr < end) {
          ch = *ptr++;
          switch (ch) {
          case '0':
            ch = 0;
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
            unsigned byte = 0;
            for (unsigned i = 0; i < 2 && ptr < end; i++) {
              int digit = Hex(*ptr);
              if (digit < 0)
                break;
              byte = byte * 16 + digit;
              ptr++;
            }
            ch = byte;
            break;
          }
          default:
            break;
          }
        }
        temp[len++] = ch;
      }
      if (!closed) {
        Free(temp);
        return nullptr;
      }
    } while (Eat('"'));
    temp[len] = 0;
    auto *str = StrNew(temp, hc);
    Free(temp);
    return str;
  }

private:
  static bool Ident(unsigned char ch)
  {
    return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= 'a' && ch <= 'z') || ch == '_';
  }

  const char *ptr, *end;
};

static void ApplyFlag(CDocEntry &entry, unsigned index, bool value)
{
  uint64_t bit = 1ull << index;
  if (value)
    entry.base.de_flags |= bit;
  else
    entry.base.de_flags &= ~bit;
  if (index >= 28 && index <= 31) {
    if (value)
      entry.base.type |= bit;
    else
      entry.base.type &= ~static_cast<uint32_t>(bit);
  }
}

static bool ParseArg(CDollarCmd &cmd, CDocEntry &entry, unsigned index,
                     CHeapCtrl *hc)
{
  auto &base = entry.base;
  char **string = nullptr;
  switch (index) {
  case 0:
    string = &base.tag;
    break;
  case 2:
    string = &entry.aux_str;
    break;
  case 3:
    string = &entry.define_str;
    break;
  case 4:
    string = &entry.html_link;
    break;
  case 6:
    string = &entry.left_macro;
    break;
  case 8:
    string = &entry.right_macro;
    break;
  case 10:
    string = &entry.bin_ptr_link;
    break;
  default:
    break;
  }
  if (string) {
    const char *prefix = "";
    if (index == 0) {
      if (base.Type() == DOCT_CHECK_BOX)
        prefix = "[X] ";
      else if (base.de_flags & DOCEF_TREE)
        prefix = "+] ";
      if (*prefix)
        base.min_col = 1;
    }
    if (index == 0 && base.tag)
      return true;
    *string = cmd.String(hc, prefix);
    if (!*string)
      return false;
    if ((index == 6 || index == 8) && base.Type() == DOCT_MACRO && !base.tag)
      base.tag = StrNew(*string, hc);
    if (index == 3 && (base.de_flags & DOCEF_LST) && !base.tag)
      base.tag = StrNew("[]", hc);
    return true;
  }
  if (index == 11) {
    int type = cmd.Match(raw_types);
    if (type < 0)
      return false;
    entry.raw_type = type;
    base.de_flags &= ~DOCEF_DFT_RAW_TYPE;
    return true;
  }
  int64_t value;
  if (!cmd.Number(value))
    return false;
  switch (index) {
  case 1:
    entry.len = value;
    base.de_flags &= ~DOCEF_DFT_LEN;
    break;
  case 5:
    entry.attr = value;
    break;
  case 7:
    entry.right_exp = value;
    break;
  case 9:
    entry.bin_num = value;
    break;
  case 12:
    base.type |= (uint32_t(value) & 31) << 16;
    break;
  case 13:
    base.type |= (uint32_t(value) & 31) << 21;
    break;
  case 14:
    entry.scroll_len = value;
    break;
  case 15:
    base.user_data = value;
    break;
  default:
    return false;
  }
  return true;
}

static bool ParseCommand(CDollarCmd &cmd, CDocEntry &entry, CHeapCtrl *heap)
{
  int type = cmd.Match(commands), color = -1;
  if (type < 0) {
    color = cmd.Match(colors);
    if (color < 0)
      return false;
    type = DOCT_FOREGROUND;
  }
  if (type == DOCT_ERROR)
    return false;
  entry.base.type = type | DefaultTypeFlags(type);
  entry.base.de_flags = DefaultFlags(type);
  entry.raw_type = type == DOCT_CHECK_BOX ? 4 : 10;
  entry.len = 64;
  while (cmd.Peek() == '+' || cmd.Peek() == '-') {
    bool set = cmd.Eat('+');
    if (!set)
      cmd.Eat('-');
    int idx = cmd.Match(flag_names);
    if (idx < 0)
      return false;
    ApplyFlag(entry, idx, set);
  }
  if (HasAttribute(type)) {
    cmd.Commas();
    entry.attr = color >= 0 ? color : DOC_DFT;
    if (cmd.Peek() && !cmd.Number(entry.attr))
      return false;
  } else if (type == DOCT_HEX_ED) {
    cmd.Commas();
    int64_t length;
    if (!cmd.Number(length))
      return false;
    entry.len = length;
    cmd.Commas();
    if (!cmd.Number(entry.hex_ed_width))
      return false;
  }
  uint64_t processed = 0;
  for (;;) {
    cmd.Commas();
    int idx = cmd.Match(flag_names);
    if (idx >= 0)
      ApplyFlag(entry, idx, true);
    uint64_t remaining = ~processed & entry.base.de_flags & DOCG_ARGS;
    if (!remaining)
      return true;
    if (!cmd.Eat('='))
      idx = __builtin_ctzll(remaining);
    if (idx < 0 || idx >= 16 || (processed & (1ull << idx)) ||
        !ParseArg(cmd, entry, idx, heap))
      return false;
    processed |= 1ull << idx;
  }
}

CDocEntry *PrsDollarCmd(CDoc &doc, const char *text, const char *end)
{
  auto *heap = MemTaskHeap(doc.mem_task);
  auto *entry = reinterpret_cast<CDocEntry *>(CAlloc(sizeof(CDocEntry), heap));
  if (entry) {
    CDollarCmd cmd(text, end);
    if (!ParseCommand(cmd, *entry, heap))
      DocEntryError(*entry);
  }
  return entry;
}

void CDocOut::Put(const void *bytes, size_t size)
{
  if (dst && size)
    MemCpy(dst + len, bytes, size);
  len += size;
}

void CDocOut::Put(const char *text)
{
  if (text)
    Put(text, StrLen(text));
}

void CDocOut::Put(char ch)
{
  Put(&ch, 1);
}

void CDocOut::Number(int64_t value, bool hex)
{
  char temp[24];
  uint64_t bits = value;
  Put(StrPrintJoin(temp, hex ? "0x%X" : "%d", 1, &bits));
}

void CDocOut::Quote(const char *text)
{
  constexpr char hex[] = "0123456789ABCDEF";
  Put('"');
  for (; text && *text; text++) {
    unsigned char ch = *text;
    switch (ch) {
    case '$':
      Put("\\d");
      break;
    case '\n':
      Put("\\n");
      break;
    case '\r':
      Put("\\r");
      break;
    case '\t':
      Put("\\t");
      break;
    case '"':
    case '\\':
      Put('\\');
      Put(ch);
      break;
    default:
      if (ch < 0x1F || ch == 0x7F) {
        Put("\\x");
        Put(hex[ch >> 4]);
        Put(hex[ch & 15]);
      } else {
        Put(ch);
      }
      break;
    }
  }
  Put('"');
}

static const char *EntryString(const CDocEntryBase &base, unsigned index)
{
  if (index == 0) {
    const auto *tag = base.tag;
    size_t skip = base.Type() == DOCT_CHECK_BOX ? 4
                  : base.de_flags & DOCEF_TREE  ? 3
                                                : 0;
    return tag && StrLen(tag) >= skip ? tag + skip : "";
  }
  const auto &entry = reinterpret_cast<const CDocEntry &>(base);
  switch (index) {
  case 2:
    return entry.aux_str;
  case 3:
    return entry.define_str;
  case 4:
    return entry.html_link;
  case 6:
    return entry.left_macro;
  case 8:
    return entry.right_macro;
  case 10:
    return entry.bin_ptr_link;
  default:
    return nullptr;
  }
}

static int64_t EntryNumber(const CDocEntryBase &base, unsigned index)
{
  if (index == 15)
    return base.user_data;
  const auto &entry = reinterpret_cast<const CDocEntry &>(base);
  switch (index) {
  case 1:
    return entry.len;
  case 5:
    return entry.attr;
  case 7:
    return entry.right_exp;
  case 9:
    return entry.bin_num;
  case 12:
    return (base.type >> 16) & 31;
  case 13:
    return (base.type >> 21) & 31;
  case 14:
    return entry.scroll_len;
  default:
    return 0;
  }
}

void Doc2PlainText(CDocOut &out, const CDoc &doc, const CDocEntryBase &base)
{
  auto type = base.Type();
  if (type >= DOCT_ERROR) {
    out.Put(commands[DOCT_ERROR]);
    return;
  }
  bool named = doc.flags & DOCF_COLOR_NAMES;
  const auto &entry = reinterpret_cast<const CDocEntry &>(base);
  int64_t attr = HasAttribute(type) ? entry.attr : DOC_DFT;
  if (type == DOCT_FOREGROUND && named && 0 <= attr && attr < 16) {
    out.Put(colors[attr]);
    attr = DOC_DFT;
  } else {
    out.Put(commands[type]);
  }
  uint64_t flags = base.de_flags & ~DOCG_TRANSIENT;
  uint64_t defaults = DefaultFlags(type);
  uint64_t changed = flags ^ defaults;
  for (unsigned i = 0; i < sizeof(flag_names) / sizeof(*flag_names); i++) {
    uint64_t bit = 1ull << i;
    if ((changed & bit) && (!(flags & bit) || !(DOCG_ARGS & bit))) {
      out.Put(flags & bit ? '+' : '-');
      out.Put(flag_names[i]);
    }
  }
  uint32_t shifted = (base.type ^ DefaultTypeFlags(type)) & DOCG_BL_IV_UL;
  for (unsigned i = 28; i < 32; i++) {
    if (shifted & (1u << i)) {
      out.Put(base.type & (1u << i) ? '+' : '-');
      out.Put(flag_names[i]);
    }
  }
  if (type == DOCT_HEX_ED) {
    out.Put(',');
    out.Number(entry.len);
    out.Put(',');
    out.Number(entry.hex_ed_width);
  } else if (attr != DOC_DFT) {
    out.Put(',');
    if (named && type >= DOCT_FOREGROUND && type <= DOCT_DFT_BACKGROUND &&
        0 <= attr && attr < 16)
      out.Put(colors[attr]);
    else
      out.Number(attr);
  }
  uint64_t args = flags & DOCG_ARGS;
  while (args) {
    unsigned i = __builtin_ctzll(args);
    args &= args - 1;
    const char *str = EntryString(base, i);
    if (i == 0 && (type == DOCT_DATA || (!str && type == DOCT_MACRO)))
      continue;
    if (i == 11 && (entry.raw_type == (type == DOCT_CHECK_BOX ? 4 : 10) ||
                    entry.raw_type >= sizeof(raw_types) / sizeof(*raw_types)))
      continue;
    out.Put(',');
    if (i || !(defaults & DOCEF_TAG)) {
      out.Put(flag_names[i]);
      out.Put('=');
    }
    if (i == 0 || i == 2 || i == 3 || i == 4 || i == 6 || i == 8 || i == 10)
      out.Quote(str);
    else if (i == 11)
      out.Put(raw_types[entry.raw_type]);
    else
      out.Number(EntryNumber(base, i), i == 15);
  }
}

extern "C" char *Doc2PlainTextCpp(CDoc *doc, CDocEntryBase *entry)
{
  if (!doc || !entry || doc->doc_signature != DOC_SIGNATURE_VAL)
    return nullptr;
  CDocLock lock(*doc);
  CDocOut measure;
  Doc2PlainText(measure, *doc, *entry);
  auto *str = MAlloc(measure.len + 1, MemTaskHeap(doc->mem_task));
  if (!str)
    return nullptr;
  CDocOut out(str);
  Doc2PlainText(out, *doc, *entry);
  out.Put(char{0});
  return reinterpret_cast<char *>(str);
}
