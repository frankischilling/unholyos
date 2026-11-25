#include "KernelB.hpp"
#include "../Adam/DolDoc/DolDoc.hpp"
#include "../Compiler/CompilerA.hpp"

static void TimersInit()
{
  OutU8(0x43, 0x34);
  OutU8(0x40, SYS_TIMER0_PERIOD & 255);
  OutU8(0x40, SYS_TIMER0_PERIOD >> 8);

  if (PCIReadU16(0, 31, 0, 0) == 0x8086) {
    uintptr_t rcba = PCIReadU32(0, 31, 0, 0xF0) & ~0x3FFFU;
    auto *hpet_config = reinterpret_cast<volatile uint32_t *>(rcba + 0x3404);
    *hpet_config = (*hpet_config & 3) | 0x80;
  }

  uintptr_t alias = dev.uncached_alias;
  uint64_t capabilities =
      *reinterpret_cast<volatile uint64_t *>(alias + HPET_GCAP_ID);
  uint64_t period = capabilities >> 32;
  if (period > 100000 && period < 1000000000) {
    cnts.HPET_freq = 1000000000000000ULL / period;
    cnts.HPET_kHz_freq = 1000000000000ULL / period;
    auto *config = reinterpret_cast<volatile uint64_t *>(alias + HPET_GEN_CONF);
    *config = *config | 1;
    cnts.HPET_initial = HPET();
  } else {
    cnts.HPET_freq = 0;
    cnts.HPET_kHz_freq = 0;
    cnts.HPET_initial = 0;
  }
}

static bool SysGlblsInit()
{
  uint32_t eax = 1, ebx, ecx = 0, edx;
  asm volatile("cpuid" : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx));
  sys_cache_line_width = ((ebx >> 8) & 255) * 8;
  sys_focus_task = adam_task;
  DbgModeCpp(true);
  QueInit(sys_macro_head.next, sys_macro_head.last);

  auto *hc = adam_task->data_heap;
  rev_bits_table = MAlloc(256, hc);
  set_bits_table = MAlloc(256, hc);
  if (!rev_bits_table || !set_bits_table)
    return false;
  for (unsigned i = 0; i < 256; i++) {
    unsigned rev = 0, count = 0;
    for (unsigned j = 0; j < 8; j++) {
      rev |= ((i >> (7 - j)) & 1) << j;
      count += (i >> j) & 1;
    }
    rev_bits_table[i] = rev;
    set_bits_table[i] = count;
  }

  constexpr unsigned EXT_EXTS_NUM = 5;
  ext = reinterpret_cast<CExt *>(MAlloc(EXT_EXTS_NUM * sizeof(*ext), hc));
  if (!ext)
    return false;
  MemSet(ext, 0, EXT_EXTS_NUM * sizeof(*ext));

  constexpr int limit = 308, bias = limit + 1;
  pow10_I64 = reinterpret_cast<uint64_t *>(
      MAlloc((2 * limit + 2) * sizeof(*pow10_I64), hc));
  if (!pow10_I64)
    return false;
  pow10_I64[0] = 0;
  for (int i = -limit; i <= limit; i++)
    pow10_I64[i + bias] = Pow10Bits(i);

  QueInit(dev.pci_head.next, dev.pci_head.last);
  dev.mem64_ptr = MEM_MAPPED_SPACE;
  return true;
}

static uint64_t BootFileHash(uint64_t hash, const uint8_t *buf, size_t bytes)
{
  for (size_t i = 0; i < bytes; i++)
    hash = (hash ^ buf[i]) * 1099511628211ULL;
  return hash;
}

static void BootDVDReport(const CDrv &dv)
{
  CRedSeaDirEntry boot, kernel;
  if (!RedSeaFileFind(dv, dv.root_clus, "0000Boot", true, boot) ||
      !RedSeaFileFind(dv, boot.clus, "0000Kernel.BIN.C", false, kernel)) {
    SerialPutS("UnholyOS: Kernel file not found on boot DVD\r\n");
    return;
  }

  uint8_t buf[4 * DVD_BLK_SIZE];
  uint64_t hash = 14695981039346656037ULL;
  for (uint64_t pos = 0; pos < kernel.size;) {
    uint64_t left = kernel.size - pos;
    size_t bytes = left < sizeof(buf) ? left : sizeof(buf);
    if (!RedSeaFileRead(dv, kernel, pos, buf, bytes)) {
      SerialPutS("UnholyOS: Could not read the kernel file from boot DVD\r\n");
      return;
    }
    hash = BootFileHash(hash, buf, bytes);
    pos += bytes;
  }

  SerialPutS("UnholyOS: RedSea drive ");
  SerialPutHex(reinterpret_cast<uintptr_t>(&dv));
  SerialPutS(", port ");
  SerialPutHex(dv.bd->base0);
  SerialPutS(", unit ");
  SerialPutHex(dv.bd->unit);
  SerialPutS(", root ");
  SerialPutHex(dv.root_clus);
  SerialPutS(", kernel cluster ");
  SerialPutHex(kernel.clus);
  SerialPutS(", size ");
  SerialPutHex(kernel.size);
  SerialPutS(", hash ");
  SerialPutHex(hash);
  SerialPutS("\r\n");
}

static void BootStartupReport(const CDrv &dv)
{
  size_t size;
  auto *src = FileRead(dv, "/StartOS.HC", size, adam_task->data_heap);
  if (!src)
    return;
  SerialPutS("UnholyOS: StartOS.HC size ");
  SerialPutHex(size);
  SerialPutS(", hash ");
  SerialPutHex(BootFileHash(14695981039346656037ULL, src, size));
  SerialPutS("\r\n");
  Free(src);
}

static uint64_t BootLexHash(const char *src, int64_t flags,
                            CHashTable *table = nullptr,
                            const char *filename = nullptr, CDoc *doc = nullptr)
{
  CCmpCtrl cc;
  CLexFile file;
  LexSourceInit(cc, file, src, flags, filename, table);
  if (doc && !LexAttachDoc(cc, *doc, &file))
    return 0;
  uint64_t hash = 14695981039346656037ULL;
  int64_t token;
  do {
    token = table ? Lex(cc) : LexRaw(cc);
    if (token == TK_ERROR) {
      hash = 0;
      break;
    }
    auto *current = cc.lex_include_stk;
    const int64_t position[]{token, cc.last_line_num, current->line_num,
                             cc.last_U16};
    hash = BootFileHash(hash, reinterpret_cast<const uint8_t *>(position),
                        sizeof(position));
    if (table || doc) {
      const int64_t level[]{current->depth, current->flags};
      hash = BootFileHash(hash, reinterpret_cast<const uint8_t *>(level),
                          sizeof(level));
    }
    if (token == TK_INS_BIN) {
      hash = BootFileHash(hash, reinterpret_cast<const uint8_t *>(&cc.cur_str_len),
                          sizeof(cc.cur_str_len));
      if (cc.cur_str)
        hash = BootFileHash(hash, reinterpret_cast<const uint8_t *>(cc.cur_str),
                            cc.cur_str_len);
    } else if (token == TK_IDENT || token == TK_STR)
      hash = BootFileHash(hash, reinterpret_cast<const uint8_t *>(cc.cur_str),
                          cc.cur_str_len);
    else if (token == TK_I64 || token == TK_CHAR_CONST || token == TK_INS_BIN_SIZE)
      hash = BootFileHash(hash, reinterpret_cast<const uint8_t *>(&cc.cur_i64),
                          sizeof(cc.cur_i64));
    else if (token == TK_F64)
      hash = BootFileHash(hash, reinterpret_cast<const uint8_t *>(&cc.cur_f64),
                          sizeof(cc.cur_f64));
  } while (token != TK_EOF);
  LexSourceDel(cc);
  return hash;
}

static void BootLexReport(const CDrv &dv)
{
  size_t size;
  auto *src = FileRead(dv, "/LexProbe.HC", size, adam_task->data_heap);
  if (!src)
    return;
  constexpr char flags_source[] = "@A .7 '..' #\\\n// Comment\r\nP\x1F"
                                  "Q\x05" "9 \x80" "_\n/* Unterminated";
  SerialPutS("UnholyOS: source lexer ");
  SerialPutHex(BootLexHash(reinterpret_cast<const char *>(src), 0));
  SerialPutS(", lexer flags ");
  SerialPutHex(BootLexHash(flags_source,
                          CCF_KEEP_NEW_LINES | CCF_KEEP_DOT | CCF_KEEP_AT_SIGN |
                              CCF_NO_CHAR_CONST));
  SerialPutS("\r\n");
  Free(src);
}

static void BootDirectiveReport(const CDrv &dv)
{
  size_t size;
  auto *heap = adam_task->data_heap;
  auto *src = FileRead(dv, "/LexDirectives.HC", size, heap);
  if (!src)
    return;
  auto *name = FileNameAbs("/LexDirectives.HC", *adam_task);
  auto *nested = FileNameAbs("/LexNested.HH", *adam_task);
  auto *guide = FileNameAbs("LexGuide.DD.Z", *adam_task);
  uint64_t nested_arg = reinterpret_cast<uintptr_t>(nested);
  auto *link = nested ? MStrPrintCpp("FL:%s,1", 1, &nested_arg) : nullptr;
  SerialPutS("UnholyOS: source directives");
  const auto used = heap->used_u8s;
  constexpr int64_t modes[]{0, CCF_AOT_COMPILE};
  bool symbols = name && guide && link;
  for (int64_t flags : modes) {
    uint64_t value = 0;
    if (auto *keywords = CmpAsmHashNew()) {
      auto *table = HashTableNew(128, heap);
      if (table) {
        table->next = keywords;
        value = BootLexHash(reinterpret_cast<const char *>(src), flags, table,
                            name);
        auto *base = reinterpret_cast<CHashDefineStr *>(
            HashFind("LEX_BASE", table, HTT_DEFINE_STR));
        auto *prior = reinterpret_cast<CHashDefineStr *>(
            HashFind("LEX_BASE", table, HTT_DEFINE_STR, 2));
        auto *inner = reinterpret_cast<CHashDefineStr *>(
            HashFind("LEX_INNER", table, HTT_DEFINE_STR));
        auto *help = guide ? reinterpret_cast<CHashSrcSym *>(
                                HashFind(guide, table, HTT_HELP_FILE))
                           : nullptr;
        auto eq = [](const char *s, const char *expected) {
          return s && expected &&
                 !StrCmpCpp(reinterpret_cast<const uint8_t *>(s),
                            reinterpret_cast<const uint8_t *>(expected));
        };
        symbols = symbols && base && eq(base->data, "21") &&
                  base->cnt == -1 && prior && eq(prior->data, "40") &&
                  inner && eq(inner->sym.src_link, link) &&
                  !HashFind("LEX_BAD", table, HTT_DEFINE_STR) && help &&
                  (help->hash.type & HTF_PUBLIC) && eq(help->idx, "Lexer/Probe");
        CCmpCtrl cc;
        CLexFile file;
        LexSourceInit(cc, file, "BEFORE LEX_BASE", flags, name, table);
        symbols = symbols && Lex(cc) == TK_IDENT;
        const char *text = cc.cur_str;
        symbols = symbols && Lex(cc) == TK_I64 && cc.cur_i64 == 21 &&
                  cc.cur_str == text;
        LexSourceDel(cc);
        HashTableDelCpp(table);
      }
      HashTableDelCpp(keywords);
    }
    symbols = symbols && value;
    SerialPutS(" ");
    SerialPutHex(value);
  }
  SerialPutS(", heap ");
  SerialPutHex(heap->used_u8s == used);
  SerialPutS(", symbols ");
  SerialPutHex(symbols);
  SerialPutS("\r\n");
  Free(link);
  Free(guide);
  Free(nested);
  Free(name);
  Free(src);
}

static void BootDocLexReport(const CDrv &dv)
{
  CRedSeaDirEntry entry;
  if (!RedSeaFileFind(dv, dv.root_clus, "TextDemo.HC", false, entry))
    return;
  const auto used = adam_task->data_heap->used_u8s;
  uint64_t doc_hash = 0, text_hash = 0, sprite_hash = 0;
  constexpr char doc_text[] =
      "\n$FG,4$\tAL$SY,-3$pha$SY,0$Tag\tBe$SY,2$ta;\n"
      "$AN,\"Marker\"$Alpha\t\"A$FG,12$B$FG$ C$$D\";\n";
  if (auto *doc = DocNewCpp(nullptr, adam_task)) {
    doc->flags |= DOCF_DBL_DOLLARS;
    DocLoadCpp(doc, reinterpret_cast<const uint8_t *>(doc_text),
               sizeof(doc_text) - 1);
    doc_hash = BootLexHash(nullptr, CCF_KEEP_NEW_LINES, nullptr, nullptr, doc);
    DocDelCpp(doc);
  }
  if (auto *table = CmpAsmHashNew()) {
    auto run_level = SYS_RUN_LEVEL;
    SYS_RUN_LEVEL |= RLF_DOC;
    text_hash = BootLexHash("#include \"/TextDemo.HC\"\n", 0, table);
    sprite_hash = BootLexHash("#define GR_WIDTH 640\n#define GR_HEIGHT 480\n"
                              "#include \"/SpritePlot.HC\"\n",
                              0, table);
    SYS_RUN_LEVEL = run_level;
    HashTableDelCpp(table);
  }
  SerialPutS("UnholyOS: DolDoc source ");
  SerialPutHex(doc_hash);
  SerialPutS(", text ");
  SerialPutHex(text_hash);
  SerialPutS(", sprite ");
  SerialPutHex(sprite_hash);
  SerialPutS(", heap ");
  SerialPutHex(used == adam_task->data_heap->used_u8s);
  SerialPutS("\r\n");
}

static void BootExpressionReport(const CDrv &dv)
{
  auto *heap = adam_task->data_heap;
  size_t size;
  auto *src = FileRead(dv, "/ExpressionProbe.HC", size, heap);
  if (!src)
    return;
  const auto data_used = heap->used_u8s;
  const auto code_used = adam_task->code_heap->used_u8s;
  uint64_t hash = 0;
  uint64_t count = 0;
  bool rejected = false;
  if (auto *table = CmpAsmHashNew()) {
    constexpr const char *names[]{"expr_i", "expr_u", "expr_f", "expr_divisor"};
    constexpr const char *types[]{"I64i", "U64i", "F64", "I64i"};
    constexpr uint64_t signed_initial = static_cast<uint64_t>(-17);
    uint64_t values[]{signed_initial, 0x8000000000000003, 0x3ff4000000000000, 1};
    CHashGlblVar vars[4];
    MemSet(vars, 0, sizeof(vars));
    for (unsigned i = 0; i < 4; ++i) {
      vars[i].sym.hash.str = names[i];
      vars[i].sym.hash.type = HTT_GLBL_VAR;
      vars[i].var_class = reinterpret_cast<CHashClass *>(
          HashFind(types[i], table, HTT_INTERNAL_TYPE));
      vars[i].size = sizeof(values[i]);
      vars[i].data_addr = reinterpret_cast<uint8_t *>(&values[i]);
      HashAdd(&vars[i].sym.hash, table);
    }
    CCmpCtrl cc;
    CLexFile file;
    LexSourceInit(cc, file, reinterpret_cast<const char *>(src), 0,
                  "/ExpressionProbe.HC", table);
    hash = 14695981039346656037ULL;
    while (Lex(cc) != TK_EOF) {
      CRawType type;
      auto *machine = LexExpression2Bin(cc, type);
      if (!machine || cc.token != ';') {
        Free(machine);
        hash = 0;
        break;
      }
      uint64_t first = CallHolyC0(machine);
      values[0] = 28;
      uint64_t second = CallHolyC0(machine);
      values[0] = signed_initial;
      Free(machine);
      uint64_t entry[]{type, first, second};
      hash = BootFileHash(hash, reinterpret_cast<const uint8_t *>(entry),
                          sizeof(entry));
      ++count;
    }
    LexSourceDel(cc);
    LexSourceInit(cc, file, "1 + ;", 0, "/ExpressionProbe.HC", table);
    Lex(cc);
    CRawType type;
    auto *invalid = LexExpression2Bin(cc, type);
    rejected = !invalid;
    Free(invalid);
    LexSourceDel(cc);
    for (auto &var : vars)
      HashRemove(&var.sym.hash, table);
    HashTableDelCpp(table);
  }
  SerialPutS("UnholyOS: native expressions ");
  SerialPutHex(count);
  SerialPutS(", hash ");
  SerialPutHex(hash);
  SerialPutS(", heap ");
  SerialPutHex(heap->used_u8s == data_used &&
               adam_task->code_heap->used_u8s == code_used);
  SerialPutS(", rejected ");
  SerialPutHex(rejected);
  SerialPutS("\r\n");
  Free(src);
}

static void BootCompilerTableDel(CHashTable *table)
{
  for (int64_t i = 0; i <= table->mask; ++i)
    for (auto *entry = table->body[i]; entry; entry = entry->next)
      if (entry->type & HTT_FUN) {
        auto *fun = reinterpret_cast<CHashFun *>(entry);
        if (!(fun->cls.flags & FF__EXTERN)) {
          Free(fun->exe_addr);
          for (auto *member = fun->cls.member_lst_and_root; member;
               member = member->next)
            if (member->flags & MLF_STATIC)
              Free(member->static_data);
        }
      }
  HashTableDelCpp(table);
}

static void BootFunctionReport(const CDrv &dv)
{
  auto *heap = adam_task->data_heap;
  size_t size;
  auto *src = FileRead(dv, "/FunctionProbe.HC", size, heap);
  if (!src)
    return;
  const auto data_used = heap->used_u8s;
  const auto code_used = adam_task->code_heap->used_u8s;
  uint64_t hash = 0, count = 0;
  bool command = false, abi = false, rejected = false;
  if (auto *table = CmpAsmHashNew()) {
    auto *last_fun = adam_task->last_fun;
    auto answer = adam_task->answer;
    auto answer_type = adam_task->answer_type;
    auto new_answer = adam_task->new_answer;
    CCmpCtrl cc;
    CLexFile file;
    LexSourceInit(cc, file, reinterpret_cast<const char *>(src), 0,
                  "/FunctionProbe.HC", table);
    hash = 14695981039346656037ULL;
    for (Lex(cc); cc.token != TK_EOF;) {
      uint8_t *machine;
      CRawType type;
      if (!LexStmt2Bin(cc, machine, type)) {
        hash = 0;
        break;
      }
      if (machine) {
        uint64_t entry[]{type, CallHolyC0(machine)};
        hash = BootFileHash(hash, reinterpret_cast<const uint8_t *>(entry),
                            sizeof(entry));
        Free(machine);
        ++count;
      }
    }
    LexSourceDel(cc);
    uint64_t result;
    LexSourceInit(cc, file,
                  "CallPack(2); if (CallSeries(3)==22) call_result=42; "
                  "call_result;",
                  0, nullptr, table);
    Lex(cc);
    command = ExeCmdLine(cc, result) && result == 42 &&
              adam_task->answer == 42 && adam_task->answer_type == RT_I64;
    LexSourceDel(cc);
    auto *target = reinterpret_cast<CHashFun *>(
        HashFind("CallPack", table, HTT_FUN));
    auto *shift = reinterpret_cast<CHashFun *>(
        HashFind("CallShift", table, HTT_FUN));
    auto *diff = reinterpret_cast<CHashFun *>(
        HashFind("CallDifference", table, HTT_FUN));
    auto *sum = reinterpret_cast<CHashFun *>(HashFind("CallSum", table, HTT_FUN));
    abi = target && (target->cls.sym.hash.type & HTF_PUBLIC) &&
          FunPopsArgs(*target) && shift && FunPopsArgs(*shift) &&
          diff && !FunPopsArgs(*diff) && sum && FunPopsArgs(*sum) &&
          CallHolyC2(reinterpret_cast<uintptr_t>(sum->exe_addr), 19, 23) == 42;

    LexSourceInit(cc, file, "I64 Invalid(I64 x) { return x + ; }",
                  0, nullptr, table);
    Lex(cc);
    rejected = !ExeCmdLine(cc, result) &&
               !HashFind("Invalid", table, HTT_FUN);
    LexSourceDel(cc);
    LexSourceInit(cc, file, "I64 Invalid(I64 x, y) { return x; }",
                  0, nullptr, table);
    Lex(cc);
    rejected = rejected && !ExeCmdLine(cc, result) &&
               !HashFind("Invalid", table, HTT_FUN);
    LexSourceDel(cc);
    LexSourceInit(cc, file,
                  "I64 Invalid(I64 x) { I64 a=0; if (x) { break; } return a; }",
                  0, nullptr, table);
    Lex(cc);
    rejected = rejected && !ExeCmdLine(cc, result) &&
               !HashFind("Invalid", table, HTT_FUN);
    LexSourceDel(cc);
    adam_task->last_fun = last_fun;
    adam_task->answer = answer;
    adam_task->answer_type = answer_type;
    adam_task->new_answer = new_answer;
    BootCompilerTableDel(table);
  }
  SerialPutS("UnholyOS: native functions ");
  SerialPutHex(count);
  SerialPutS(", hash ");
  SerialPutHex(hash);
  SerialPutS(", checks ");
  SerialPutHex(command | (abi << 1) | (rejected << 2) |
               ((heap->used_u8s == data_used &&
                 adam_task->code_heap->used_u8s == code_used) << 3));
  SerialPutS("\r\n");
  Free(src);
}

static void BootPrintReport(const CDrv &dv, const char *path)
{
  auto *heap = adam_task->data_heap;
  size_t size;
  auto *src = FileRead(dv, path, size, heap);
  if (!src)
    return;
  const auto data_used = heap->used_u8s;
  const auto code_used = adam_task->code_heap->used_u8s;
  auto *old_fun = adam_task->last_fun;
  bool compiled = false;
  SerialPutS("UnholyOS: executing ");
  SerialPutS(path);
  SerialPutS("\r\n");
  if (auto *table = CmpAsmHashNew()) {
    CCmpCtrl cc;
    CLexFile file;
    LexSourceInit(cc, file, reinterpret_cast<const char *>(src), 0, path,
                  table);
    compiled = true;
    Lex(cc);
    while (cc.token != TK_EOF) {
      uint8_t *machine;
      CRawType type;
      if (!LexStmt2Bin(cc, machine, type)) {
        compiled = false;
        break;
      }
      if (machine) {
        CallHolyC0(machine);
        // These fixtures send their strings to output during the call.
        Free(machine);
      }
    }
    LexSourceDel(cc);
    adam_task->last_fun = old_fun;
    BootCompilerTableDel(table);
  }
  SerialPutS("UnholyOS: finished ");
  SerialPutS(path);
  SerialPutS(", checks ");
  SerialPutHex(compiled | ((heap->used_u8s == data_used &&
                            adam_task->code_heap->used_u8s == code_used) << 1));
  SerialPutS("\r\n");
  Free(src);
}

static CExeResult BootFileProgramReport(const CDrv &dv, const char *path)
{
  CRedSeaDirEntry entry;
  if (!RedSeaFileFind(dv, dv.root_clus, path + 1, false, entry))
    return {};
  SerialPutS("UnholyOS: executing ");
  SerialPutS(path);
  SerialPutS("\r\n");
  auto run = ExeFile(path);
  SerialPutS("UnholyOS: finished ");
  SerialPutS(path);
  SerialPutS(", checks ");
  SerialPutHex(run.okay);
  SerialPutS("\r\n");
  return run;
}

static void BootStreamReport(const CDrv &dv)
{
  auto run = BootFileProgramReport(dv, "/StreamProbe.HC");
  if (!run.okay)
    return;
  SerialPutS("UnholyOS: source file result ");
  SerialPutHex(run.value);
  SerialPutS("\r\n");
  auto format = ExePutS(
      "I64 stream_bad_fmt, stream_bad_source;"
      "stream_bad_fmt=\"%d\";"
      "stream_bad_source=\"#exe { StreamPrint(stream_bad_fmt); };\";",
      nullptr, 0);
  auto *cc_head = reinterpret_cast<CCmpCtrl *>(&adam_task->next_cc);
  auto *except_head = reinterpret_cast<CExcept *>(&adam_task->next_except);
  auto data_used = adam_task->data_heap->used_u8s;
  auto code_used = adam_task->code_heap->used_u8s;
  auto invalid = ExePutS("#exe { 1 + ; };", nullptr, 0);
  auto thrown = ExePutS("#exe { StreamPrint(stream_bad_fmt); };", nullptr, 0);
  auto nested = ExePutS("ExePutS(stream_bad_source);", nullptr, 0);
  auto recovery = ExePutS("StreamOuter(3)+stream_result;", nullptr, 0);
  bool clean = adam_task->last_cc == cc_head &&
               adam_task->next_cc == cc_head &&
               adam_task->last_except == except_head &&
               adam_task->next_except == except_head &&
               adam_task->data_heap->used_u8s == data_used &&
               adam_task->code_heap->used_u8s == code_used;
  SerialPutS("UnholyOS: stream checks ");
  SerialPutHex((run.okay && format.okay && run.value == 164) |
               ((!invalid.okay && !invalid.exception) << 1) |
               ((!thrown.okay && thrown.exception == STR_PRINT_ERROR &&
                 !nested.okay && nested.exception == STR_PRINT_ERROR) << 2) |
               ((recovery.okay && recovery.value == 164 && clean) << 3));
  SerialPutS("\r\n");
}

static void BootMemoryReport(const CDrv &dv)
{
  if (!BootFileProgramReport(dv, "/MemoryProbe.HC").okay)
    return;
  auto flag = sys_var_init_flag, value = sys_var_init_val;
  sys_var_init_flag = 1;
  sys_var_init_val = 0xA5;
  auto fill = ExePutS("U16 probe_filled[2]; probe_filled[0]+probe_filled[1];",
                      nullptr, 0);
  sys_var_init_flag = flag;
  sys_var_init_val = value;
  auto call = ExePutS("PtrNarrow(255);", nullptr, 0);
  bool call_type = call.okay && call.value == 255 &&
                   adam_task->answer_type == RT_I64;
  auto byte = ExePutS("probe_byte;", nullptr, 0);
  bool byte_type = byte.okay && byte.value == UINT64_MAX &&
                   adam_task->answer_type == RT_I64;
  auto data_used = adam_task->data_heap->used_u8s;
  auto code_used = adam_task->code_heap->used_u8s;
  auto global = ExePutS("I32 probe_bad[2][-1];", nullptr, 0);
  auto local = ExePutS("I64 ProbeBad() { U16 buffer[3][-1]; return 0; }",
                       nullptr, 0);
  auto *table = adam_task->hash_table;
  bool rejected = !global.okay && !local.okay &&
                  !HashFind("probe_bad", table, HTT_GLBL_VAR) &&
                  !HashFind("ProbeBad", table, HTT_FUN);
  bool clean = adam_task->data_heap->used_u8s == data_used &&
               adam_task->code_heap->used_u8s == code_used;
  SerialPutS("UnholyOS: memory type checks ");
  SerialPutHex((fill.okay && fill.value == 2 * 0xA5A5) |
               ((call_type && byte_type) << 1) | (rejected << 2) | (clean << 3));
  SerialPutS("\r\n");
}

static void BootAggregateReport(const CDrv &dv)
{
  if (!BootFileProgramReport(dv, "/AggregateProbe.HC").okay)
    return;
  auto flag = sys_var_init_flag, value = sys_var_init_val;
  sys_var_init_flag = 1;
  sys_var_init_val = 0xA5;
  auto init = ExePutS("class AggBaseFill {U16 base;};"
                       "class AggLeafFill:AggBaseFill {U8 leaf;};"
                       "AggLeafFill agg_leaf={7}; agg_leaf.base*256+agg_leaf.leaf;",
                       nullptr, 0);
  sys_var_init_flag = flag;
  sys_var_init_val = value;
  auto fwd = ExePutS("extern class AggPartial;", nullptr, 0);
  auto *table = adam_task->hash_table;
  auto *pending = HashFind("AggPartial", table, HTT_CLASS);
  auto data_used = adam_task->data_heap->used_u8s;
  auto code_used = adam_task->code_heap->used_u8s;
  auto malformed = ExePutS("class AggPartial {U8 first; I16 bad[-1];};",
                           nullptr, 0);
  auto omitted = ExePutS("U16 agg_bad_array[]={1};", nullptr, 0);
  auto stat = ExePutS("I64 AggBadStatic() { static I16 a[2]={1}; return 0; }",
                       nullptr, 0);
  auto *still_pending = reinterpret_cast<CHashClass *>(
      HashFind("AggPartial", table, HTT_CLASS));
  bool clean = adam_task->data_heap->used_u8s == data_used &&
               adam_task->code_heap->used_u8s == code_used &&
               !HashFind("agg_bad_array", table, HTT_GLBL_VAR) &&
               !HashFind("AggBadStatic", table, HTT_FUN);
  bool pending_ok = fwd.okay && !malformed.okay && still_pending &&
                    &still_pending->sym.hash == pending &&
                    (still_pending->flags & CF_EXTERN) && !still_pending->size;
  auto retry = ExePutS("class AggPartial {U16 ready;}; sizeof(AggPartial);",
                        nullptr, 0);
  bool reused = HashFind("AggPartial", table, HTT_CLASS) == pending;
  SerialPutS("UnholyOS: aggregate checks ");
  SerialPutHex((init.okay && init.value == 0xA507A5) | (pending_ok << 1) |
               ((!omitted.okay && !stat.okay && clean) << 2) |
               ((retry.okay && retry.value == 2 && reused) << 3));
  SerialPutS("\r\n");
}

static void BootCompoundReport(const CDrv &dv)
{
  if (!BootFileProgramReport(dv, "/CompoundProbe.HC").okay)
    return;
  auto data_used = adam_task->data_heap->used_u8s;
  auto code_used = adam_task->code_heap->used_u8s;
  auto invalid = ExePutS("I64 CompoundBad() { 2+=3; return 0; }", nullptr, 0);
  auto clean = adam_task->data_heap->used_u8s == data_used &&
               adam_task->code_heap->used_u8s == code_used;
  auto *symbol = HashFind("CompoundBad", adam_task->hash_table, HTT_FUN);
  SerialPutS("UnholyOS: compound checks ");
  SerialPutHex(!invalid.okay && !symbol && clean);
  SerialPutS("\r\n");
}

static void BootProbeReport(const CDrv &dv, CTask &task, const char *path,
                            const char *symbol, const char *label)
{
  auto *probe = Load(dv, path, task, LDF_JUST_LOAD);
  if (!probe || !LoadPass2(*probe))
    return;
  auto *entry = HashFind(symbol, task.hash_table, HTT_EXPORT_SYS_SYM);
  if (!entry)
    return;
  uint64_t result;
  MemCpy(&result,
         reinterpret_cast<const void *>(
             reinterpret_cast<CHashExport *>(entry)->val),
         sizeof(result));
  SerialPutS(label);
  SerialPutHex(result);
  SerialPutS("\r\n");
}

static void BootModuleReport(const CDrv &dv, CTask &task)
{
  auto *caller = Load(dv, "/LoadCaller.BIN", task, LDF_JUST_LOAD);
  if (!caller || LoadPass2(*caller))
    return;
  if (!Load(dv, "/LoadProbe", task) || !LoadPass2(*caller))
    return;
  const char *names[]{"LoadProbeValue", "LoadCallerValue", "LoadProbeAddress",
                      "LoadProbe",      "LoadProbeDate",   "LoadProbeCopyEnd",
                      "LoadProbeHeap"};
  uint64_t value[sizeof(names) / sizeof(names[0])];
  for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
    auto *entry = HashFind(names[i], task.hash_table, HTT_EXPORT_SYS_SYM);
    if (!entry)
      return;
    value[i] = reinterpret_cast<CHashExport *>(entry)->val;
  }
  uint64_t probe, called, address, date, end, expected_date, heap_value;
  MemCpy(&probe, reinterpret_cast<const void *>(value[0]), sizeof(probe));
  MemCpy(&called, reinterpret_cast<const void *>(value[1]), sizeof(called));
  MemCpy(&address, reinterpret_cast<const void *>(value[2]), sizeof(address));
  MemCpy(&date, reinterpret_cast<const void *>(value[4]), sizeof(date));
  MemCpy(&end, reinterpret_cast<const void *>(value[5]), sizeof(end));
  MemCpy(&heap_value, reinterpret_cast<const void *>(value[6]),
         sizeof(heap_value));
  MemCpy(&expected_date, SYS_COMPILE_TIME, sizeof(expected_date));
  auto *registered = HashFind("/LoadProbe", task.hash_table, HTT_MODULE);
  if (!registered)
    return;
  uintptr_t header = reinterpret_cast<CHashGeneric *>(registered)->user_data0;
  SerialPutS("UnholyOS: TOSB probe ");
  SerialPutHex(probe);
  SerialPutS(", caller ");
  SerialPutHex(called);
  SerialPutS(", relocated ");
  SerialPutHex(address == value[3]);
  SerialPutS(", aligned ");
  SerialPutHex(((header + BIN_HEADER_SIZE) & 127) == 0);
  SerialPutS(", kernel copy ");
  SerialPutHex(date == expected_date && end == value[4] + sizeof(date));
  SerialPutS(", heap value ");
  SerialPutHex(heap_value);
  SerialPutS(", heap address ");
  SerialPutHex(value[6]);
  SerialPutS("\r\n");
}

extern "C" void KMainCpp()
{
  OutU8(0x61, InU8(0x61) & ~3);
  asm volatile("movq %%fs:0,%0" : "=r"(adam_task));
  if (!BlkPoolsInit(adam_task)) {
    SerialPutS("UnholyOS: Could not initialize memory pools\r\n");
    return;
  }
  if (!SysGlblsInit()) {
    SerialPutS("UnholyOS: Could not initialize kernel globals\r\n");
    return;
  }
  if (!Mem32DevInit(adam_task->data_heap)) {
    SerialPutS("UnholyOS: Could not initialize device memory ranges\r\n");
    return;
  }
  UncachedAliasAlloc();
  if (!IntInit1()) {
    SerialPutS("UnholyOS: Could not initialize the interrupt table\r\n");
    return;
  }
  TimersInit();
  IntsInit();
  if (!IntInit2()) {
    SerialPutS("UnholyOS: Could not initialize fault handlers\r\n");
    return;
  }
  SYS_RUN_LEVEL |= RLF_INTERRUPTS;
  asm volatile("sti" : : : "memory");
  TimeCal();
  if (!LoadKernel(*adam_task)) {
    SerialPutS("UnholyOS: Could not register kernel symbols\r\n");
    return;
  }
  if (!BlkDevGlblsInit()) {
    SerialPutS("UnholyOS: Could not allocate drive tables\r\n");
    return;
  }
  adam_task->cur_dv = BlkDevsInitAll();
  BootPutS("UnholyOS kernel entered in 64-bit mode");

  SerialPutS("UnholyOS: DVD LBA ");
  SerialPutHex(SYS_BOOT_BLK);
  SerialPutS(", source ");
  SerialPutHex(SYS_BOOT_SRC & 0xFFFF);
  SerialPutS(", shift ");
  SerialPutHex(SYS_BOOT_SRC >> 16);
  SerialPutS(", E820 entries ");
  SerialPutHex(SYS_E820_COUNT);
  SerialPutS(", physical space ");
  SerialPutHex(MEM_PHYSICAL_SPACE);
  SerialPutS(", mapped space ");
  SerialPutHex(MEM_MAPPED_SPACE);
  SerialPutS(", PML4 ");
  SerialPutHex(MEM_PML4);
  SerialPutS(", heap base ");
  SerialPutHex(MEM_HEAP_BASE);
  SerialPutS(", run level ");
  SerialPutHex(SYS_RUN_LEVEL);
  SerialPutS(", heap limit ");
  SerialPutHex(MEM_HEAP_LIMIT);
  SerialPutS(", data pool ");
  SerialPutHex(reinterpret_cast<uintptr_t>(SYS_DATA_BP));
  SerialPutS(", uncached alias ");
  SerialPutHex(dev.uncached_alias);
  SerialPutS(", device ranges ");
  SerialPutHex(reinterpret_cast<uintptr_t>(&dev.mem32_head));
  SerialPutS(", cache line ");
  SerialPutHex(sys_cache_line_width);
  SerialPutS(", focus ");
  SerialPutHex(reinterpret_cast<uintptr_t>(sys_focus_task));
  SerialPutS(", macro queue ");
  SerialPutHex(reinterpret_cast<uintptr_t>(&sys_macro_head));
  SerialPutS(", reverse bits ");
  SerialPutHex(reinterpret_cast<uintptr_t>(rev_bits_table));
  SerialPutS(", set bits ");
  SerialPutHex(reinterpret_cast<uintptr_t>(set_bits_table));
  SerialPutS(", ext slots ");
  SerialPutHex(reinterpret_cast<uintptr_t>(ext));
  SerialPutS(", pow10 ");
  SerialPutHex(reinterpret_cast<uintptr_t>(pow10_I64));
  SerialPutS(", IDT ");
  SerialPutHex(reinterpret_cast<uintptr_t>(dev.idt));
  SerialPutS(", counters ");
  SerialPutHex(reinterpret_cast<uintptr_t>(&cnts));
  SerialPutS(", timer read ");
  SerialPutHex(SysTimerRead());
  SerialPutS(", PCI directory ");
  SerialPutHex(SYS_PCIBIOS_SERVICE_DIR);
  SerialPutS(", PCI service ");
  SerialPutHex(SYS_PCI_SERVICES);
  SerialPutS(", PCI host ");
  SerialPutHex(PCIReadU32(0, 0, 0, 0));
  SerialPutS(", HPET read ");
  SerialPutHex(cnts.HPET_freq ? HPET() : 0);
  SerialPutS("\r\n");
  if (adam_task->cur_dv) {
    BootDVDReport(*adam_task->cur_dv);
    BootStartupReport(*adam_task->cur_dv);
    BootLexReport(*adam_task->cur_dv);
    BootDirectiveReport(*adam_task->cur_dv);
    BootDocLexReport(*adam_task->cur_dv);
    BootExpressionReport(*adam_task->cur_dv);
    BootFunctionReport(*adam_task->cur_dv);
    BootFileProgramReport(*adam_task->cur_dv, "/Print.HC");
    BootPrintReport(*adam_task->cur_dv, "/PrintSourceProbe.HC");
    BootStreamReport(*adam_task->cur_dv);
    BootMemoryReport(*adam_task->cur_dv);
    BootFileProgramReport(*adam_task->cur_dv, "/SubIntAccess.HC");
    BootAggregateReport(*adam_task->cur_dv);
    BootCompoundReport(*adam_task->cur_dv);
    BootModuleReport(*adam_task->cur_dv, *adam_task);
    BootProbeReport(*adam_task->cur_dv, *adam_task, "/RuntimeProbe.BIN",
                    "RuntimeProbeResult", "UnholyOS: runtime probe ");
    BootProbeReport(*adam_task->cur_dv, *adam_task, "/FileReadProbe.BIN",
                    "FileReadProbeResult", "UnholyOS: file probe ");
    BootProbeReport(*adam_task->cur_dv, *adam_task, "/FileWriteProbe.BIN",
                    "FileWriteProbeResult", "UnholyOS: write probe ");
    BootProbeReport(*adam_task->cur_dv, *adam_task, "/ExceptProbe.BIN",
                    "ExceptProbeResult", "UnholyOS: exception probe ");
    BootProbeReport(*adam_task->cur_dv, *adam_task, "/FormatProbe.BIN",
                    "FormatProbeResult", "UnholyOS: format probe ");
    auto *hash_caller =
        Load(*adam_task->cur_dv, "/HashCaller.BIN", *adam_task, LDF_JUST_LOAD);
    if (hash_caller && !LoadPass2(*hash_caller)) {
      BootProbeReport(*adam_task->cur_dv, *adam_task, "/HashProbe.BIN",
                      "HashProbeResult", "UnholyOS: hash probe ");
      if (LoadPass2(*hash_caller))
        SerialPutS("UnholyOS: typed imports resolved\r\n");
    }
    BootProbeReport(*adam_task->cur_dv, *adam_task, "/DocProbe.BIN",
                    "DocProbeResult", "UnholyOS: document probe ");
    BootProbeReport(*adam_task->cur_dv, *adam_task, "/CompilerRuntimeProbe.BIN",
                    "CompilerRuntimeProbeResult",
                    "UnholyOS: compiler runtime probe ");
  } else {
    SerialPutS("UnholyOS: Could not mount the boot DVD\r\n");
  }
  SerialPutS("UnholyOS: kernel entered in 64-bit mode\r\n");
}
