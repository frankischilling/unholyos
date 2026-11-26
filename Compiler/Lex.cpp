#include "../Adam/DolDoc/DolDoc.hpp"
#include "CompilerA.hpp"

extern "C" uint64_t I64MulF64Bits(int64_t value, uint64_t scale);
extern "C" void PrintWarnCpp(const char *fmt, int64_t argc,
                             const uint64_t *argv);

constexpr size_t STR_LEN = 144;
constexpr int64_t LEX_RESCAN = -2;
constexpr uint64_t OUT_MEM = 0x6D654D74754FULL; // OutMem
static const char lex_zeros[] = "";

void LexSourceInit(CCmpCtrl &cc, CLexFile &file, const char *src, int64_t flags,
                   const char *filename, CHashTable *table)
{
  MemSet(&cc, 0, sizeof(cc));
  MemSet(&file, 0, sizeof(file));
  cc.flags = flags;
  cc.char_bmp_alpha_numeric = flags & CCF_KEEP_AT_SIGN
                                  ? char_bmp_alpha_numeric_no_at
                                  : char_bmp_alpha_numeric;
  cc.lex_include_stk = &file;
  QueInit(cc.next_stream_blk, cc.last_stream_blk);
  cc.opts = OPTF_WARN_UNUSED_VAR | OPTF_WARN_HEADER_MISMATCH;
  cc.htc.hash_mask = HTG_TYPE_MASK - HTT_IMPORT_SYS_SYM;
  cc.htc.hash_table_lst = cc.htc.define_hash_table = cc.htc.local_hash_table =
      cc.htc.glbl_hash_table = table;
  file.full_name = filename ? filename : blkdev.tmp_filename;
  cc.cur_buf_ptr = file.buf = file.buf_ptr = file.line_start = src;
  file.line_num = 1;
  file.depth = -1;
}

static void LexFilePop(CCmpCtrl &cc)
{
  auto *file = cc.lex_include_stk;
  cc.lex_include_stk = file->next;
  Free(const_cast<char *>(file->full_name));
  if (file->doc)
    DocDelCpp(file->doc);
  else
    Free(const_cast<char *>(file->buf));
  Free(file);
}

static CLexFile *LexFilePush(CCmpCtrl &cc, char *name)
{
  if (!name)
    return nullptr;
  auto *file = reinterpret_cast<CLexFile *>(
      CAlloc(sizeof(CLexFile), CurrentTask()->data_heap));
  if (!file)
    return nullptr;
  auto *parent = cc.lex_include_stk;
  parent->buf_ptr = cc.cur_buf_ptr;
  parent->last_U16 = (cc.flags & CCF_USE_LAST_U16) ? cc.last_U16 : 0;
  cc.flags &= ~CCF_USE_LAST_U16;
  file->next = parent;
  file->full_name = name;
  file->line_num = 1;
  file->depth = parent->depth + 1;
  cc.lex_include_stk = file;
  return file;
}

bool LexIncludeStr(CCmpCtrl &cc, char *name, char *src, int64_t flags)
{
  auto *file = LexFilePush(cc, name);
  if (!file) {
    Free(name);
    Free(src);
    return false;
  }
  file->flags = flags;
  file->buf = file->buf_ptr = file->line_start = cc.cur_buf_ptr = src;
  return true;
}

bool LexAttachDoc(CCmpCtrl &cc, CDoc &doc, CLexFile *root, CDocEntryBase *entry,
                  uint32_t col)
{
  auto *file = root;
  if (!file) {
    auto *name = StrNew(doc.filename.name, CurrentTask()->data_heap);
    file = LexFilePush(cc, name);
    if (!file) {
      Free(name);
      return false;
    }
  } else {
    file->full_name = doc.filename.name;
    cc.flags &= ~CCF_USE_LAST_U16;
  }
  file->doc = &doc;
  file->buf = nullptr;
  file->last_U16 = 0;
  file->flags = 0;
  file->buf_ptr = file->line_start = lex_zeros;
  file->line_num = 1;
  if (!entry)
    entry = doc.head.next;
  while (entry != &doc.head && entry->Type() != DOCT_TEXT &&
         entry->Type() != DOCT_TAB && entry->Type() != DOCT_INS_BIN &&
         entry->Type() != DOCT_INS_BIN_SIZE) {
    entry = entry->next;
    col = entry->min_col;
  }
  if (entry != &doc.head) {
    file->flags = LFSF_DOC;
    file->line_num = int64_t{entry->y} + 1;
    file->line_start = reinterpret_cast<const char *>(entry);
    file->cur_entry = entry->last;
    if (entry->Type() == DOCT_TEXT && entry->tag) {
      file->cur_entry = entry;
      size_t offset = col < entry->min_col ? entry->min_col : col;
      if (offset > entry->max_col)
        offset = entry->max_col;
      size_t bytes = StrLen(entry->tag);
      file->buf_ptr = entry->tag + (offset < bytes ? offset : bytes);
    }
  }
  cc.cur_buf_ptr = file->buf_ptr;
  return true;
}

void LexSourceDel(CCmpCtrl &cc)
{
  while (cc.lex_include_stk && cc.lex_include_stk->next)
    LexFilePop(cc);
  Free(const_cast<char *>(cc.cur_str));
  Free(const_cast<char *>(cc.cur_help_idx));
  Free(const_cast<char *>(cc.dollar_buf));
  Free(cc.pmt_buf);
  cc.pmt_buf = nullptr;
  cc.dollar_buf = nullptr;
  cc.cur_help_idx = nullptr;
  cc.cur_str = nullptr;
  cc.cur_str_len = 0;
  cc.cur_buf_ptr = nullptr;
  cc.lex_include_stk = nullptr;
}

static bool LexDollar(CCmpCtrl &cc, CLexFile &file, CDocEntryBase &entry)
{
  if (!(cc.flags & CCF_IN_QUOTES))
    return false;
  auto *plain = Doc2PlainTextCpp(file.doc, &entry);
  if (!plain)
    RaiseHolyC(OUT_MEM);
  uint64_t arg = reinterpret_cast<uintptr_t>(plain);
  auto *dollar = MStrPrintCpp("$%$Q$", 1, &arg);
  Free(plain);
  if (!dollar)
    RaiseHolyC(OUT_MEM);
  Free(const_cast<char *>(cc.dollar_buf));
  cc.dollar_buf = dollar;
  cc.dollar_cnt = 2;
  file.line_num = int64_t{entry.y} + 1;
  cc.cur_buf_ptr = file.buf_ptr = cc.dollar_buf;
  return true;
}

static int64_t LexDocChar(CCmpCtrl &cc, CLexFile &file)
{
  auto &head = file.doc->head;
  constexpr uint64_t text_flags =
      DOCG_PLAIN_ENTRY | DOCEF_DEFINE | DOCEF_TAG_CB;
  for (auto *base = file.cur_entry->next; base != &head; base = base->next) {
    file.cur_entry = base;
    uint8_t type = base->Type();
    if (type == DOCT_MARKER || type == DOCT_CURSOR)
      continue;
    if (type == DOCT_TEXT) {
      if ((base->de_flags & ~text_flags) && LexDollar(cc, file, *base))
        return LEX_RESCAN;
      if (base->tag && *base->tag) {
        cc.cur_buf_ptr = file.buf_ptr = base->tag;
        return LEX_RESCAN;
      }
      continue;
    }
    if (type != DOCT_NEW_LINE && type != DOCT_TAB && type != DOCT_INS_BIN &&
        type != DOCT_INS_BIN_SIZE && LexDollar(cc, file, *base))
      return LEX_RESCAN;

    cc.cur_buf_ptr = file.buf_ptr = lex_zeros;
    auto *entry = reinterpret_cast<CDocEntry *>(base);
    switch (type) {
    case DOCT_NEW_LINE:
      file.line_start = reinterpret_cast<const char *>(base->next);
      file.line_num = int64_t{base->y} + 2;
      return '\n';
    case DOCT_TAB:
      file.line_num = int64_t{base->y} + 1;
      return '\t';
    case DOCT_INS_BIN:
      Free(const_cast<char *>(cc.cur_str));
      cc.cur_str = nullptr;
      cc.cur_str_len = 0;
      if (auto *bin = entry->bin_data) {
        auto *copy = MAlloc(bin->size, CurrentTask()->data_heap);
        if (!copy)
          RaiseHolyC(OUT_MEM);
        if (bin->data)
          MemCpy(copy, bin->data, bin->size);
        cc.cur_str = reinterpret_cast<char *>(copy);
        cc.cur_str_len = bin->size;
      }
      file.line_num = int64_t{base->y} + 1;
      return TK_INS_BIN;
    case DOCT_INS_BIN_SIZE:
      cc.cur_i64 = entry->bin_data ? entry->bin_data->size : 0;
      file.line_num = int64_t{base->y} + 1;
      return TK_INS_BIN_SIZE;
    case DOCT_SHIFTED_Y:
      file.line_num = int64_t{base->y} + 1;
      if (entry->attr < 0)
        return TK_SUPERSCRIPT;
      return entry->attr > 0 ? TK_SUBSCRIPT : TK_NORMALSCRIPT;
    default:
      break;
    }
  }
  file.cur_entry = head.last;
  return TK_EOF;
}

int64_t LexGetChar(CCmpCtrl &cc)
{
  if (cc.flags & CCF_USE_LAST_U16) {
    cc.flags &= ~CCF_USE_LAST_U16;
    return cc.last_U16;
  }
  while (true) {
    auto *file = cc.lex_include_stk;
    uint8_t ch = cc.cur_buf_ptr ? static_cast<uint8_t>(*cc.cur_buf_ptr) : 0;
    if (!ch) {
      if (file->flags & LFSF_DOC) {
        int64_t doc_char = LexDocChar(cc, *file);
        if (doc_char == LEX_RESCAN)
          continue;
        if (doc_char != TK_EOF)
          return cc.last_U16 = doc_char;
      }
      if (!file->next) {
        if (!(cc.flags & CCF_PMT))
          return cc.last_U16 = TK_EOF;
        Free(cc.pmt_buf);
        cc.pmt_buf = CmdLinePmt();
        if (!cc.pmt_buf) {
          cc.flags &= ~CCF_PMT;
          cc.cur_buf_ptr = file->buf_ptr = file->buf = file->line_start = nullptr;
          return cc.last_U16 = TK_EOF;
        }
        cc.cur_buf_ptr = file->buf_ptr = file->buf = file->line_start = cc.pmt_buf;
        continue;
      }
      LexFilePop(cc);
      file = cc.lex_include_stk;
      cc.cur_buf_ptr = file->buf_ptr;
      if (file->last_U16)
        return cc.last_U16 = file->last_U16;
      continue;
    }
    ++cc.cur_buf_ptr;
    if (ch == CH_CURSOR)
      continue;
    if (ch == '\n' && !(file->flags & LFSF_DOC)) {
      ++file->line_num;
      file->line_start = cc.cur_buf_ptr;
    } else if (ch == CH_SHIFT_SPACE) {
      ch = ' ';
    }
    return cc.last_U16 = ch;
  }
}

static int64_t LexEnd(CCmpCtrl &cc, int64_t token)
{
  // The lexer exposes the next character and its line before returning.
  LexGetChar(cc);
  cc.flags |= CCF_USE_LAST_U16;
  return cc.token = token;
}

static bool DecDigit(int64_t ch)
{
  return '0' <= ch && ch <= '9';
}

static int64_t HexDigit(int64_t ch)
{
  if (DecDigit(ch))
    return ch - '0';
  if ('A' <= ch && ch <= 'F')
    return ch - 'A' + 10;
  if ('a' <= ch && ch <= 'f')
    return ch - 'a' + 10;
  return -1;
}

static int64_t LexInteger(CCmpCtrl &cc, uint64_t value)
{
  cc.cur_i64 = static_cast<int64_t>(value);
  cc.flags |= CCF_USE_LAST_U16;
  return LexEnd(cc, TK_I64);
}

static int64_t LexFloat(CCmpCtrl &cc, uint64_t value, int64_t ch)
{
  uint64_t places = 0, exponent = 0;
  for (; DecDigit(ch); ch = LexGetChar(cc)) {
    value = value * 10 + (ch - '0');
    ++places;
  }
  if (ch == 'e' || ch == 'E') {
    ch = LexGetChar(cc);
    bool negative = ch == '-';
    if (negative)
      ch = LexGetChar(cc);
    for (; DecDigit(ch); ch = LexGetChar(cc))
      exponent = exponent * 10 + (ch - '0');
    if (negative)
      exponent = 0 - exponent;
  }
  cc.cur_f64 =
      I64MulF64Bits(static_cast<int64_t>(value),
                    Pow10I64Cpp(static_cast<int64_t>(exponent - places)));
  cc.flags |= CCF_USE_LAST_U16;
  return LexEnd(cc, TK_F64);
}

static int64_t LexNumber(CCmpCtrl &cc, int64_t ch)
{
  uint64_t value = ch - '0';
  ch = LexGetChar(cc);
  if (ch == 'X' || ch == 'x') {
    for (int64_t digit; (digit = HexDigit(LexGetChar(cc))) >= 0;)
      value = (value << 4) + digit;
    return LexInteger(cc, value);
  }
  if (ch == 'B' || ch == 'b') {
    while ((ch = LexGetChar(cc)) == '0' || ch == '1')
      value = (value << 1) + (ch - '0');
    return LexInteger(cc, value);
  }
  while (DecDigit(ch)) {
    value = value * 10 + (ch - '0');
    ch = LexGetChar(cc);
  }
  if (ch == '.') {
    ch = LexGetChar(cc);
    if (ch == '.') {
      cc.flags |= CCF_LAST_WAS_DOT;
      return LexInteger(cc, value);
    }
    return LexFloat(cc, value, ch);
  }
  if (ch == 'e' || ch == 'E')
    return LexFloat(cc, value, ch);
  return LexInteger(cc, value);
}

static uint8_t LexEscape(CCmpCtrl &cc)
{
  int64_t ch = LexGetChar(cc);
  switch (ch) {
  case '0':
    return 0;
  case '\'':
  case '`':
  case '"':
  case '\\':
    return ch;
  case 'd':
    return '$';
  case 'n':
    return '\n';
  case 'r':
    return '\r';
  case 't':
    return '\t';
  case 'x':
  case 'X': {
    uint8_t value = 0;
    for (int i = 0; i < 2; ++i) {
      int64_t digit = HexDigit(LexGetChar(cc));
      if (digit < 0) {
        cc.flags |= CCF_USE_LAST_U16;
        break;
      }
      value = (value << 4) + digit;
    }
    return value;
  }
  default:
    cc.flags |= CCF_USE_LAST_U16;
    return '\\';
  }
}

static int64_t LexCharConst(CCmpCtrl &cc)
{
  uint64_t value = 0;
  int64_t ch = LexGetChar(cc);
  for (int i = 0; i < 8 && ch && ch != '\''; ++i) {
    uint8_t byte = ch;
    if (ch == '\\')
      byte = LexEscape(cc);
    else if (ch == '$' && LexGetChar(cc) != '$')
      cc.flags |= CCF_USE_LAST_U16;
    value |= uint64_t{byte} << (i * 8);
    ch = LexGetChar(cc);
  }
  if (ch && ch != '\'')
    return cc.token = TK_ERROR;
  cc.cur_i64 = static_cast<int64_t>(value);
  return LexEnd(cc, TK_CHAR_CONST);
}

class CLexBuffer
{
public:
  CLexBuffer() = default;
  CLexBuffer(const CLexBuffer &) = delete;
  CLexBuffer &operator=(const CLexBuffer &) = delete;

  ~CLexBuffer()
  {
    Free(data);
  }

  bool Append(const char *bytes, size_t count)
  {
    if (!count)
      return true;
    if (count > SIZE_MAX - size)
      return false;
    if (size + count > capacity) {
      size_t next = capacity ? capacity : STR_LEN;
      while (next < size + count) {
        if (next > SIZE_MAX / 2)
          return false;
        next *= 2;
      }
      auto *larger =
          reinterpret_cast<char *>(MAlloc(next, CurrentTask()->data_heap));
      if (!larger)
        return false;
      MemCpy(larger, data, size);
      Free(data);
      data = larger;
      capacity = next;
    }
    MemCpy(data + size, bytes, count);
    size += count;
    return true;
  }

  bool Put(char ch)
  {
    return Append(&ch, 1);
  }

  size_t Size() const
  {
    return size;
  }

  char *TakeString()
  {
    if (size != capacity) {
      auto *result =
          reinterpret_cast<char *>(MAlloc(size, CurrentTask()->data_heap));
      if (result)
        MemCpy(result, data, size);
      return result;
    }
    auto *result = data;
    data = nullptr;
    size = capacity = 0;
    return result;
  }

private:
  char *data = nullptr;
  size_t size = 0, capacity = 0;
};

static int64_t LexString(CCmpCtrl &cc)
{
  cc.flags |= CCF_IN_QUOTES;
  CLexBuffer text;
  int64_t ch;
  while ((ch = LexGetChar(cc)) && ch != '"') {
    if (ch == '\\')
      ch = LexEscape(cc);
    else if (ch == '$') {
      if (cc.dollar_cnt)
        --cc.dollar_cnt;
      else if (LexGetChar(cc) != '$') {
        cc.dollar_cnt = 1;
        cc.flags |= CCF_USE_LAST_U16;
      }
    }
    if (!text.Put(static_cast<char>(ch))) {
      cc.flags &= ~CCF_IN_QUOTES;
      return cc.token = TK_ERROR;
    }
  }
  cc.flags &= ~CCF_IN_QUOTES;
  if (!text.Put(0))
    return cc.token = TK_ERROR;
  auto size = text.Size();
  auto *str = text.TakeString();
  if (!str)
    return cc.token = TK_ERROR;
  Free(const_cast<char *>(cc.cur_str));
  cc.cur_str_len = size;
  cc.cur_str = str;
  return LexEnd(cc, TK_STR);
}

char *LexExtStr(CCmpCtrl &cc, size_t &size)
{
  auto *first = const_cast<char *>(cc.cur_str);
  size = cc.cur_str_len;
  cc.cur_str = nullptr;
  if (Lex(cc) == TK_ERROR) {
    Free(first);
    return nullptr;
  }
  if (cc.token != TK_STR)
    return first;

  CLexBuffer text;
  bool copied = text.Append(first, size - 1);
  Free(first);
  if (!copied)
    return nullptr;
  while (cc.token == TK_STR) {
    if (!text.Append(cc.cur_str, cc.cur_str_len - 1))
      return nullptr;
    Free(const_cast<char *>(cc.cur_str));
    cc.cur_str = nullptr;
    if (Lex(cc) == TK_ERROR)
      return nullptr;
  }
  if (!text.Put(0))
    return nullptr;
  size = text.Size();
  return text.TakeString();
}

static bool LexBit(const uint32_t *map, int64_t ch)
{
  return map[ch >> 5] & (uint32_t{1} << (ch & 31));
}

static int64_t LexScriptChar(int64_t ch)
{
  switch (ch) {
  case TK_SUPERSCRIPT:
    return '>';
  case TK_SUBSCRIPT:
    return '<';
  case TK_NORMALSCRIPT:
    return '=';
  default:
    return 0;
  }
}

static int64_t LexIdent(CCmpCtrl &cc, int64_t ch, bool raw)
{
  char buf[STR_LEN];
  size_t size = 0;
  do {
    if (size == sizeof(buf) - 1)
      return cc.token = TK_ERROR;
    int64_t script = LexScriptChar(ch);
    buf[size++] = static_cast<char>(script ? script : ch);
    ch = LexGetChar(cc);
  } while (LexBit(cc.char_bmp_alpha_numeric, ch) || LexScriptChar(ch));
  if (ch)
    cc.flags |= CCF_USE_LAST_U16;
  buf[size++] = 0;
  auto *heap = CurrentTask()->data_heap;
  if (!raw) {
    auto *locals = cc.htc.local_var_lst;
    cc.local_var_entry = locals ? MemberFind(buf, &locals->cls) : nullptr;
    auto *entry = cc.local_var_entry
                      ? nullptr
                      : HashFind(buf, cc.htc.hash_table_lst, cc.htc.hash_mask);
    if (entry && (entry->type & HTT_DEFINE_STR) &&
        !(cc.flags & CCF_NO_DEFINES)) {
      auto *define = reinterpret_cast<const CHashDefineStr *>(entry);
      auto *src = StrNew(define->data, heap);
      if (!src || !LexIncludeStr(cc, StrNew(blkdev.tmp_filename, heap), src,
                                 LFSF_DEFINE))
        return cc.token = TK_ERROR;
      return LEX_RESCAN;
    }
    cc.hash_entry = entry;
  }
  auto *name = StrNew(buf, heap);
  if (!name)
    return cc.token = TK_ERROR;
  Free(const_cast<char *>(cc.cur_str));
  cc.cur_str = name;
  cc.cur_str_len = size;
  return LexEnd(cc, TK_IDENT);
}

static int64_t LexDot(CCmpCtrl &cc)
{
  if (cc.flags & CCF_KEEP_DOT)
    return LexEnd(cc, '.');
  if (cc.flags & CCF_LAST_WAS_DOT) {
    cc.flags &= ~CCF_LAST_WAS_DOT;
  } else {
    int64_t ch = LexGetChar(cc);
    if (DecDigit(ch))
      return LexFloat(cc, 0, ch);
    if (ch != '.') {
      cc.flags |= CCF_USE_LAST_U16;
      return LexEnd(cc, '.');
    }
  }
  if (LexGetChar(cc) == '.')
    return LexEnd(cc, TK_ELLIPSIS);
  cc.flags |= CCF_USE_LAST_U16;
  return LexEnd(cc, TK_DOT_DOT);
}

struct CLexOp
{
  char second;
  int16_t token;
};

struct CLexOps
{
  CLexOp by_char[128][3];
};

static constexpr auto dual_tokens = [] {
  CLexOps t{};
  t.by_char['!'][0] = {'=', TK_NOT_EQU};
  t.by_char['&'][0] = {'&', TK_AND_AND};
  t.by_char['&'][1] = {'=', TK_AND_EQU};
  t.by_char['*'][0] = {'=', TK_MUL_EQU};
  t.by_char['+'][0] = {'+', TK_PLUS_PLUS};
  t.by_char['+'][1] = {'=', TK_ADD_EQU};
  t.by_char['-'][0] = {'>', TK_DEREFERENCE};
  t.by_char['-'][1] = {'-', TK_MINUS_MINUS};
  t.by_char['-'][2] = {'=', TK_SUB_EQU};
  t.by_char['/'][0] = {'=', TK_DIV_EQU};
  t.by_char[':'][0] = {':', TK_DBL_COLON};
  t.by_char['<'][0] = {'=', TK_LESS_EQU};
  t.by_char['<'][1] = {'<', TK_SHL};
  t.by_char['='][0] = {'=', TK_EQU_EQU};
  t.by_char['>'][0] = {'=', TK_GREATER_EQU};
  t.by_char['>'][1] = {'>', TK_SHR};
  t.by_char['^'][0] = {'^', TK_XOR_XOR};
  t.by_char['^'][1] = {'=', TK_XOR_EQU};
  t.by_char['|'][0] = {'|', TK_OR_OR};
  t.by_char['|'][1] = {'=', TK_OR_EQU};
  t.by_char['%'][0] = {'=', TK_MOD_EQU};
  return t;
}();

static bool LexBlockComment(CCmpCtrl &cc)
{
  size_t depth = 1;
  while (int64_t ch = LexGetChar(cc)) {
    while (ch == '*' || ch == '/') {
      const int64_t last = ch;
      ch = LexGetChar(cc);
      if (last == '/' && ch == '*') {
        ++depth;
        break;
      }
      if (last == '*' && ch == '/') {
        if (!--depth)
          return true;
        break;
      }
    }
  }
  return false;
}

static int64_t LexToken(CCmpCtrl &cc, bool raw)
{
  while (int64_t ch = LexGetChar(cc)) {
    if (LexScriptChar(ch) || (LexBit(char_bmp_alpha, ch) &&
                              (ch != '@' || !(cc.flags & CCF_KEEP_AT_SIGN)))) {
      int64_t token = LexIdent(cc, ch, raw);
      if (token != LEX_RESCAN)
        return token;
      continue;
    }
    if (DecDigit(ch))
      return LexNumber(cc, ch);
    switch (ch) {
    case '\n':
      if (cc.flags & CCF_KEEP_NEW_LINES)
        return LexEnd(cc, ch);
      continue;
    case TK_INS_BIN:
    case TK_INS_BIN_SIZE:
      return LexEnd(cc, ch);
    case '"':
      return LexString(cc);
    case '#':
      if (raw || (cc.flags & CCF_KEEP_SIGN_NUM))
        return LexEnd(cc, ch);
      return cc.token = ch;
    case '\'':
      if (!(cc.flags & CCF_NO_CHAR_CONST))
        return LexCharConst(cc);
      continue;
    case '.':
      return LexDot(cc);
    case '$': {
      int64_t next = LexGetChar(cc);
      if (next == '$')
        return LexEnd(cc, ch);
      if (!next) {
        cc.flags |= CCF_USE_LAST_U16;
        return LexEnd(cc, ch);
      }
      do
        next = LexGetChar(cc);
      while (next && next != '$');
      if (!next)
        return LexEnd(cc, TK_EOF);
      continue;
    }
    case '/': {
      int64_t next = LexGetChar(cc);
      if (next == '*') {
        if (!LexBlockComment(cc))
          return cc.token = TK_EOF;
        continue;
      }
      if (next == '/') {
        do
          next = LexGetChar(cc);
        while (LexBit(char_bmp_non_eol, next));
        if (cc.flags & CCF_KEEP_NEW_LINES)
          return LexEnd(cc, '\n');
        continue;
      }
      cc.flags |= CCF_USE_LAST_U16;
      break;
    }
    default:
      break;
    }
    if (ch < '!' || ch > '~' || ch == '\\')
      continue;
    const auto &ops = dual_tokens.by_char[ch];
    if (ops[0].token) {
      const int64_t next = LexGetChar(cc);
      for (const auto &op : ops) {
        if (op.token && op.second == next) {
          int64_t token = op.token;
          if (token == TK_SHL || token == TK_SHR) {
            if (LexGetChar(cc) == '=')
              token = token == TK_SHL ? TK_SHL_EQU : TK_SHR_EQU;
            else
              cc.flags |= CCF_USE_LAST_U16;
          }
          return LexEnd(cc, token);
        }
      }
      cc.flags |= CCF_USE_LAST_U16;
    }
    return LexEnd(cc, ch);
  }
  return cc.token = TK_EOF;
}

int64_t LexRaw(CCmpCtrl &cc)
{
  cc.last_line_num = cc.lex_include_stk->line_num;
  return LexToken(cc, true);
}

static int64_t PrsKeyWord(const CCmpCtrl &cc)
{
  if (cc.token != TK_IDENT || !cc.hash_entry ||
      !(cc.hash_entry->type & HTT_KEYWORD))
    return -1;
  return reinterpret_cast<const CHashGeneric *>(cc.hash_entry)->user_data0;
}

static int64_t LexNoDefines(CCmpCtrl &cc)
{
  cc.flags |= CCF_NO_DEFINES;
  int64_t token = Lex(cc);
  cc.flags &= ~CCF_NO_DEFINES;
  return token;
}

static bool LexKeepDefine(CCmpCtrl &cc, CLexBuffer &text)
{
  int64_t ch;
  do
    ch = LexGetChar(cc);
  while (LexBit(char_bmp_non_eol_white_space, ch));
  bool quoted = false;
  // The first byte of each continued line is handled before scanning its body.
  do {
    if (ch == '\\') {
      ch = LexGetChar(cc);
      if (!ch)
        return text.Put('\\') && text.Put(0);
      if (ch == '\r') {
        if (LexGetChar(cc) != '\n')
          cc.flags |= CCF_USE_LAST_U16;
      } else if (ch != '\n' && (!text.Put('\\') || !text.Put(ch))) {
        return false;
      }
    } else if (ch && ch != '\n') {
      if (ch == '"')
        quoted = !quoted;
      if (!text.Put(ch))
        return false;
    } else {
      break;
    }

    while ((ch = LexGetChar(cc))) {
      if (ch == '/') {
        ch = LexGetChar(cc);
        if (ch == '/' && !quoted) {
          do
            ch = LexGetChar(cc);
          while (LexBit(char_bmp_non_eol, ch));
          break;
        }
        if (!text.Put('/'))
          return false;
        cc.flags |= CCF_USE_LAST_U16;
      } else if (ch == '\\') {
        ch = LexGetChar(cc);
        if (ch == '"') {
          if (!text.Put('\\') || !text.Put('"'))
            return false;
        } else if (ch) {
          cc.flags |= CCF_USE_LAST_U16;
          ch = '\\';
          break;
        } else {
          break;
        }
      } else if (LexBit(char_bmp_non_eol, ch)) {
        if (ch == '"')
          quoted = !quoted;
        if (!text.Put(ch))
          return false;
      } else {
        break;
      }
    }
  } while (ch == '\\');
  return text.Put(0);
}

static bool LexDefine(CCmpCtrl &cc)
{
  if (LexNoDefines(cc) != TK_IDENT)
    return cc.token != TK_ERROR;
  if (!cc.htc.define_hash_table)
    return false;
  auto *entry = reinterpret_cast<CHashDefineStr *>(
      CAlloc(sizeof(CHashDefineStr), CurrentTask()->data_heap));
  if (!entry)
    return false;
  entry->sym.hash.str = cc.cur_str;
  cc.cur_str = nullptr;
  entry->sym.hash.type = HTT_DEFINE_STR;
  HashSrcFileSetCpp(&cc, &entry->sym, 0);
  CLexBuffer text;
  if (!LexKeepDefine(cc, text) || !(entry->data = text.TakeString())) {
    HashDel(&entry->sym.hash);
    return false;
  }
  entry->cnt = -1;
  HashAdd(&entry->sym.hash, cc.htc.define_hash_table);
  return true;
}

static int64_t LexSkipCondition(CCmpCtrl &cc, bool stop_at_else,
                                size_t depth = 1)
{
  while (int64_t ch = LexGetChar(cc)) {
    if (ch != '#')
      continue;
    if (Lex(cc) <= TK_EOF)
      return cc.token;
    switch (PrsKeyWord(cc)) {
    case KW_IF:
    case KW_IFDEF:
    case KW_IFNDEF:
    case KW_IFAOT:
    case KW_IFJIT:
      ++depth;
      break;
    case KW_ENDIF:
      if (!--depth)
        return TK_IDENT;
      break;
    case KW_ELSE:
      if (stop_at_else && depth == 1)
        return TK_IDENT;
      break;
    }
  }
  return LexEnd(cc, TK_EOF);
}

static constexpr int64_t if_keywords[]{KW_IF,    KW_IFDEF, KW_IFNDEF, KW_IFAOT,
                                       KW_IFJIT, KW_ENDIF, KW_ELSE};

static int64_t LexIfToken(int64_t keyword)
{
  // The reference returns TK_IFAOT for #ifjit inside an expression.
  if (keyword == KW_IFJIT)
    return TK_IFAOT;
  for (size_t i = 0; i < sizeof(if_keywords) / sizeof(*if_keywords); ++i)
    if (keyword == if_keywords[i])
      return TK_IF + i;
  return 0;
}

static char *LexFileName(const char *file, const char *ext)
{
  auto *extended = ExtDftCpp(file, ext);
  auto *abs = extended ? FileNameAbs(extended, *CurrentTask()) : nullptr;
  Free(extended);
  return abs;
}

static bool LexIncludeFile(CCmpCtrl &cc, char *name)
{
  if (!(SYS_RUN_LEVEL & RLF_DOC)) {
    auto *src = name ? FileReadCpp(name, nullptr, nullptr) : nullptr;
    return LexIncludeStr(cc, name, reinterpret_cast<char *>(src), 0);
  }
  auto *doc = name ? DocNewCpp(name, nullptr) : nullptr;
  if (doc) {
    size_t size;
    auto *src = FileReadCpp(name, &size, nullptr);
    doc->flags |= DOCF_DBL_DOLLARS;
    DocLoadCpp(doc, src, size);
    Free(src);
  }
  Free(name);
  if (!doc)
    return false;
  if (LexAttachDoc(cc, *doc))
    return true;
  DocDelCpp(doc);
  return false;
}

static bool LexHelpIndex(CCmpCtrl &cc)
{
  CLexBuffer text;
  while (cc.token == TK_STR) {
    if (!text.Append(cc.cur_str, cc.cur_str_len - 1))
      return false;
    Free(const_cast<char *>(cc.cur_str));
    cc.cur_str = nullptr;
    if (LexGetChar(cc) != '\\') {
      cc.flags |= CCF_USE_LAST_U16;
      break;
    }
    if (Lex(cc) == TK_ERROR)
      return false;
  }
  if (!text.Put(0))
    return false;
  auto *index = text.TakeString();
  if (!index)
    return false;
  Free(const_cast<char *>(cc.cur_help_idx));
  cc.cur_help_idx = index;
  return true;
}

static bool LexHelpFile(CCmpCtrl &cc)
{
  if (!cc.htc.glbl_hash_table)
    return false;
  auto *entry = reinterpret_cast<CHashSrcSym *>(
      CAlloc(sizeof(CHashSrcSym), CurrentTask()->data_heap));
  if (!entry)
    return false;
  entry->hash.str = LexFileName(cc.cur_str, "DD.Z");
  if (!entry->hash.str) {
    Free(entry);
    return false;
  }
  entry->hash.type = HTT_HELP_FILE | HTF_PUBLIC;
  HashSrcFileSetCpp(&cc, entry, 0);
  HashAdd(&entry->hash, cc.htc.glbl_hash_table);
  return true;
}

int64_t Lex(CCmpCtrl &cc)
{
  cc.last_line_num = cc.lex_include_stk->line_num;
  while (true) {
    int64_t token = LexToken(cc, false);
    if (token != '#' || (cc.flags & CCF_KEEP_SIGN_NUM))
      return token;

    if (Lex(cc) != TK_IDENT)
      return cc.token <= TK_EOF ? cc.token : LexEnd(cc, cc.token);

    int64_t keyword = PrsKeyWord(cc);
    if (cc.flags & CCF_IN_IF)
      if (int64_t conditional = LexIfToken(keyword))
        return LexEnd(cc, conditional);
  lex_directive:
    switch (keyword) {
    case KW_INCLUDE:
    case KW_HELP_FILE:
    case KW_HELP_INDEX:
      if (Lex(cc) != TK_STR)
        return cc.token <= TK_EOF ? cc.token : LexEnd(cc, cc.token);
      if (keyword == KW_INCLUDE) {
        if (!LexIncludeFile(cc, LexFileName(cc.cur_str, "HC.Z")))
          return cc.token = TK_ERROR;
      } else if (keyword == KW_HELP_FILE ? !LexHelpFile(cc)
                                         : !LexHelpIndex(cc)) {
        return cc.token = TK_ERROR;
      }
      break;
    case KW_DEFINE:
      if (!LexDefine(cc))
        return cc.token = TK_ERROR;
      break;
    case KW_IFDEF:
    case KW_IFNDEF:
      if (LexNoDefines(cc) != TK_IDENT)
        return cc.token <= TK_EOF ? cc.token : LexEnd(cc, cc.token);
      if ((cc.hash_entry != nullptr) == (keyword == KW_IFDEF))
        break;
      [[fallthrough]];
    case KW_ELSE:
      token = LexSkipCondition(cc, keyword != KW_ELSE);
      if (token <= TK_EOF)
        return cc.token = token;
      break;
    case KW_IFAOT:
    case KW_IFJIT:
      if (bool(cc.flags & CCF_AOT_COMPILE) != (keyword == KW_IFAOT)) {
        token = LexSkipCondition(cc, true);
        if (token <= TK_EOF)
          return cc.token = token;
      }
      break;
    case KW_ENDIF:
      break;
    case KW_IF: {
      cc.flags |= CCF_IN_IF;
      token = Lex(cc);
      uint64_t condition = 0;
      bool parsed = token > TK_EOF && LexExpression(cc, condition);
      cc.flags &= ~CCF_IN_IF;
      if (!parsed)
        return cc.token = token == TK_EOF ? TK_EOF : TK_ERROR;

      token = cc.token;
      bool conditional = TK_IF <= token && token <= TK_IFJIT;
      if (condition) {
        if (conditional || token == TK_ELSE) {
          keyword = if_keywords[token - TK_IF];
          goto lex_directive;
        }
        if (token != TK_ENDIF)
          return token <= TK_EOF ? token : LexEnd(cc, token);
      } else if (token != TK_ELSE && token != TK_ENDIF) {
        token = LexSkipCondition(cc, true, conditional ? 2 : 1);
        if (token <= TK_EOF)
          return cc.token = token;
      }
      break;
    }
    case KW_ASSERT: {
      if (Lex(cc) <= TK_EOF)
        return cc.token;
      uint64_t condition;
      if (!LexExpression(cc, condition))
        return cc.token = TK_ERROR;
      if (!condition) {
        if (cc.aot)
          cc.aot->warning_cnt++;
        const auto *file = cc.lex_include_stk;
        uint64_t args[]{reinterpret_cast<uintptr_t>(file->full_name),
                        static_cast<uint64_t>(file->line_num)};
        PrintWarnCpp("Assert Failed %s,%d\n", 2, args);
      }
      return cc.token <= TK_EOF ? cc.token : LexEnd(cc, cc.token);
    }
    case KW_EXE:
      if (Lex(cc) != '{' || !PrsStreamBlk(cc))
        return cc.token = TK_ERROR;
      break;
    default:
      return LexEnd(cc, cc.token);
    }
  }
}
