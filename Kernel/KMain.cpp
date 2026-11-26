#include "KernelB.hpp"
#include "../Adam/Gr/GrGlbls.hpp"
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
  scrncast.snd_head.next = scrncast.snd_head.last = &scrncast.snd_head;

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
  if (!AsmHashLoad()) {
    Free(src);
    return;
  }
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

static void BootGuestFileReport(const CDrv &ram, const char *path)
{
  CRedSeaDirEntry entry;
  size_t prefix;
  if (!RedSeaFileLookup(ram, path, false, false, entry, prefix))
    return;
  SerialPutS("UnholyOS: guest file ");
  SerialPutS(path);
  SerialPutS(", address ");
  SerialPutHex(reinterpret_cast<uintptr_t>(ram.bd->RAM_dsk) +
               entry.clus * BLK_SIZE);
  SerialPutS(", size ");
  SerialPutHex(entry.size);
  SerialPutS("\r\n");
}

static void BootRasterReport(const CDrv &dv)
{
  if (!BootFileProgramReport(dv, "/RasterProbe.HC").okay)
    return;
  if (auto *ram = MountedDrive('B'))
    BootGuestFileReport(*ram, "/RasterProbe.GR.Z");
  SerialPutS("UnholyOS: raster context ");
  SerialPutHex(reinterpret_cast<uintptr_t>(gr.dc->body));
  SerialPutS("\r\n");
  BootFileProgramReport(dv, "/Lines.HC");
}

static void BootGridReport(const CDrv &dv)
{
  auto draw = gr.fp_draw_ms;
  auto grid = ms_grid;
  if (!BootFileProgramReport(dv, "/Grid.HC").okay || gr.fp_draw_ms != draw)
    return;
  auto *before = reinterpret_cast<const uint8_t *>(&grid);
  auto *after = reinterpret_cast<const uint8_t *>(&ms_grid);
  for (size_t i = 0; i < sizeof(grid); ++i)
    if (before[i] != after[i])
      return;
  SerialPutS("UnholyOS: Grid restored the cursor and mouse grid\r\n");
}

static void BootFunctionPtrReport(const CDrv &dvd, CTask &task)
{
  if (!BootFileProgramReport(dvd, "/FunctionPtrProbe.HC").okay)
    return;
  auto *ram = MountedDrive('B');
  if (!ram)
    return;
  auto compiled =
      ExePutS("Cmp(\"::/FunctionPtrProbe.HC\",,\"B:/FunctionPtrProbe.BIN.Z\");",
              nullptr, 0);
  if (!compiled.okay || compiled.value)
    return;
  constexpr const char *path = "/FunctionPtrProbe.BIN.Z";
  SerialPutS("UnholyOS: executing /FunctionPtrProbe.BIN.Z\r\n");
  auto *mod = Load(*ram, path, task, LDF_JUST_LOAD);
  if (!mod || !LoadPass2(*mod))
    return;
  SerialPutS("UnholyOS: finished /FunctionPtrProbe.BIN.Z, checks "
             "0x0000000000000001\r\n");
  BootGuestFileReport(*ram, path);
}

static void BootISOReport(const CDrv &dv)
{
  auto run = BootFileProgramReport(dv, "/RedSeaISOProbe.HC");
  if (auto *ram = MountedDrive('B'); ram && run.okay) {
    BootGuestFileReport(*ram, "/GuestBuild.ISO.C");
    BootGuestFileReport(*ram, "/GuestData.ISO.C");
  }
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

static void BootCmpReport(const CDrv &dvd, CTask &task)
{
  auto *ram = MountedDrive('B');
  if (!ram || !BootFileProgramReport(dvd, "/CmpProbeRunner.HC").okay)
    return;
  auto *file = HashFind("CmpProbeResult", task.hash_table, HTG_TYPE_MASK);
  auto *rejected = HashFind("CmpStaleExport", task.hash_table, HTG_TYPE_MASK);
  SerialPutS(file || rejected ? "UnholyOS: early AOT symbols\r\n"
                             : "UnholyOS: AOT symbols isolated\r\n");
  size_t size;
  auto *map = FileRead(*ram, "/CmpOutput.MAP.Z", size, task.data_heap);
  if (map) {
    SerialPutS("UnholyOS: AOT source map\r\n");
    SerialPutS(reinterpret_cast<const char *>(map));
    Free(map);
  }
  BootProbeReport(*ram, task, "/CmpOutput.BIN.Z", "CmpProbeResult",
                  "UnholyOS: AOT result ");
  SerialPutS("UnholyOS: executing /CmpPrint.BIN.Z\r\n");
  auto *print = Load(*ram, "/CmpPrint.BIN.Z", task, LDF_JUST_LOAD);
  if (print && LoadPass2(*print))
    SerialPutS("UnholyOS: finished /CmpPrint.BIN.Z, checks 0x0000000000000001\r\n");
  BootGuestFileReport(*ram, "/CmpOutput.BIN.Z");
}

static void BootImportReport(const CDrv &dvd, CTask &task)
{
  auto *ram = MountedDrive('B');
  if (!ram || !BootFileProgramReport(dvd, "/ImportBuild.HC").okay)
    return;
  auto *caller = Load(*ram, "/ImportCaller.BIN.Z", task, LDF_JUST_LOAD);
  if (!caller || LoadPass2(*caller))
    return;
  if (!Load(*ram, "/ImportProvider.BIN.Z", task) || !LoadPass2(*caller))
    return;
  auto *entry = HashFind("NativeLinkDateResult", task.hash_table,
                          HTT_EXPORT_SYS_SYM);
  uint64_t date;
  MemCpy(&date, SYS_COMPILE_TIME, sizeof(date));
  if (entry && date == *reinterpret_cast<const uint64_t *>(
                           reinterpret_cast<const CHashExport *>(entry)->val))
    SerialPutS("UnholyOS: AOT system alias matched\r\n");
  auto declarations = ExePutS("extern I64 NativeLateFn(I64 value);"
                              "extern I64 NativeLateData;", nullptr, 0);
  if (!declarations.okay)
    return;
  auto *late = Load(*ram, "/ImportLate.BIN.Z", task, LDF_JUST_LOAD);
  if (!late || LoadPass2(*late) ||
      !BootFileProgramReport(dvd, "/ImportExtern.HC").okay ||
      !LoadPass2(*late))
    return;
  auto missing = ExePutS("extern I64 MissingNativeExtern(I64 n);"
                         "MissingNativeExtern(1);", nullptr, 0);
  auto illegal = ExePutS("import I64 BadNativeImport;", nullptr, 0);
  if (!missing.okay && missing.exception == 0x7478456665646E55 &&
      !illegal.okay && !HashFind("BadNativeImport", task.hash_table, HTG_TYPE_MASK))
    SerialPutS("UnholyOS: extern and import rejection matched\r\n");
  BootGuestFileReport(*ram, "/ImportCaller.BIN.Z");
  BootGuestFileReport(*ram, "/ImportProvider.BIN.Z");
}

static void BootAsmReport(const CDrv &dvd, CTask &task)
{
  auto *ram = MountedDrive('B');
  if (!ram || !BootFileProgramReport(dvd, "/AsmBuild.HC").okay)
    return;
  BootFileProgramReport(dvd, "/DivByHand.HC");
  BootFileProgramReport(dvd, "/AsmHelloWorld.HC");
  BootFileProgramReport(dvd, "/MulByHand.HC");
  BootFileProgramReport(dvd, "/PutDec.HC");
  BootFileProgramReport(dvd, "/AsmAndC3.HC");
  BootFileProgramReport(dvd, "/BuzzFizz.HC");
  BootFileProgramReport(dvd, "/InlineAsmProbe.HC");
  BootFileProgramReport(dvd, "/GotoProbe.HC");
  BootFileProgramReport(dvd, "/OpcodeProbe.HC");
  auto goto_missing = ExePutS("U0 GotoMissing() { goto missing; }", nullptr, 0);
  auto goto_duplicate = ExePutS(
      "U0 GotoDuplicate() { duplicate: asm { duplicate: NOP } }", nullptr, 0);
  if (!goto_missing.okay && !goto_duplicate.okay &&
      !HashFind("GotoMissing", task.hash_table, HTT_FUN) &&
      !HashFind("GotoDuplicate", task.hash_table, HTT_FUN))
    SerialPutS("UnholyOS: goto rejected missing and duplicate labels\r\n");
  auto rejected = ExePutS("asm { _ASM_REJECTED:: LOOP @@0 }", nullptr, 0);
  auto inline_rejected = ExePutS(
      "U0 InlineAsmRejected() { asm { _INLINE_ASM_REJECTED:: LOOP @@0 } }",
      nullptr, 0);
  if (!rejected.okay && !inline_rejected.okay &&
      !HashFind("_ASM_REJECTED", task.hash_table, HTT_EXPORT_SYS_SYM) &&
      !HashFind("_INLINE_ASM_REJECTED", task.hash_table, HTT_EXPORT_SYS_SYM) &&
      !HashFind("InlineAsmRejected", task.hash_table, HTT_FUN))
    SerialPutS("UnholyOS: asm rejected unresolved local\r\n");
  const char *paths[]{"/AsmProbe.BIN.Z", "/DivByHand.BIN.Z",
                      "/InlineAsmProbe.BIN.Z", "/GotoProbe.BIN.Z",
                      "/OpcodeProbe.BIN.Z"};
  for (const char *path : paths) {
    SerialPutS("UnholyOS: executing ");
    SerialPutS(path);
    SerialPutS("\r\n");
    auto *module = Load(*ram, path, task, LDF_JUST_LOAD);
    if (!module || !LoadPass2(*module))
      return;
    SerialPutS("UnholyOS: finished ");
    SerialPutS(path);
    SerialPutS(", checks 0x0000000000000001\r\n");
    BootGuestFileReport(*ram, path);
  }

  constexpr uint8_t mode16[]{0xB8,0x34,0x12,0x66,0xBA,0xEF,0xCD,0xAB,0x89,
                            0xCD,0x10,0xC3};
  constexpr uint8_t mode32[]{0x66,0xB8,0x78,0x56,0xBA,0x78,0x56,0x34,0x12,0xC3};
  auto *a = reinterpret_cast<const CHashExport *>(
      HashFind("_ASM_MODE16", task.hash_table, HTT_EXPORT_SYS_SYM));
  auto *b = reinterpret_cast<const CHashExport *>(
      HashFind("_ASM_MODE32", task.hash_table, HTT_EXPORT_SYS_SYM));
  auto *c = reinterpret_cast<const CHashExport *>(
      HashFind("_ASM_WORK", task.hash_table, HTT_EXPORT_SYS_SYM));
  auto matches = [](const CHashExport *s, const uint8_t *bytes, size_t count) {
    if (!s)
      return false;
    auto *code = reinterpret_cast<const uint8_t *>(s->val);
    for (size_t i = 0; i < count; i++)
      if (code[i] != bytes[i])
        return false;
    return true;
  };
  if (c && !(c->val & 15) && matches(a, mode16, sizeof(mode16)) &&
      matches(b, mode32, sizeof(mode32)))
    SerialPutS("UnholyOS: asm opcode modes and alignment matched\r\n");
  constexpr uint8_t table16[]{0x66, 0x90, 0x9C, 0x99, 0x67, 0x8B, 0x03, 0xC3};
  constexpr uint8_t table32[]{0x66, 0x90, 0x66, 0x9C, 0x66,
                              0x99, 0x66, 0x8B, 0x03, 0xC3};
  auto *r16 = reinterpret_cast<const CHashExport *>(
      HashFind("_OPCODE_16", task.hash_table, HTT_EXPORT_SYS_SYM));
  auto *r32 = reinterpret_cast<const CHashExport *>(
      HashFind("_OPCODE_32", task.hash_table, HTT_EXPORT_SYS_SYM));
  constexpr uint8_t table_seg[]{0x65, 0x4A, 0x8B, 0x04, 0xC2,
                                0x64, 0x8B, 0x04, 0x8A, 0xC3};
  auto *seg = reinterpret_cast<const CHashExport *>(
      HashFind("_OPCODE_SEG", task.hash_table, HTT_EXPORT_SYS_SYM));
  if (matches(r16, table16, sizeof(table16)) &&
      matches(r32, table32, sizeof(table32)) &&
      matches(seg, table_seg, sizeof(table_seg)))
    SerialPutS("UnholyOS: opcode table mode bytes matched\r\n");
}

static bool BootAsmBranchCheck(const CTask &task)
{
  constexpr struct
  {
    const char *name;
    size_t offset, size;
    uint8_t bytes[5];
    uint64_t result;
  } checks[]{
      {"_BRANCH_LITERAL", 0, 4, {0xEB, 2, 0xCC, 0xCC}, 41},
      {"_BRANCH_FORWARD", 0, 2, {0xEB, 0x7F}, 42},
      {"_BRANCH_BACKWARD", 128, 2, {0xEB, 0x80}, 43},
      {"_BRANCH_DWORD", 0, 5, {0xE9, 0, 1, 0, 0}, 44},
      {"_BRANCH_COND", 2, 4, {0x74, 2, 0xCC, 0xCC}, 45},
      {"_BRANCH_WORD", 0, 3, {0xE9, 0, 1}, 0},
  };
  for (const auto &check : checks) {
    auto *symbol = reinterpret_cast<const CHashExport *>(
        HashFind(check.name, task.hash_table, HTT_EXPORT_SYS_SYM));
    if (!symbol)
      return false;
    auto *code = reinterpret_cast<const uint8_t *>(symbol->val);
    for (size_t i = 0; i < check.size; i++)
      if (code[check.offset + i] != check.bytes[i])
        return false;
    uint64_t value;
    if (check.result &&
        (!CallHolyC0Checked(code, &value) || value != check.result))
      return false;
  }
  return true;
}

static void BootAsmBranchReport(const CDrv &dvd, CTask &task)
{
  auto *ram = MountedDrive('B');
  if (!ram || !BootFileProgramReport(dvd, "/AsmBranchProbe.HC").okay ||
      !BootAsmBranchCheck(task))
    return;
  SerialPutS("UnholyOS: JIT branch values and bytes matched\r\n");

  auto forward = ExePutS("asm { _BRANCH_FAR_FORWARD:: JMP @@end "
                         "DU8 128 DUP (0); @@end: RET }",
                         nullptr, 0);
  auto backward = ExePutS("asm { _BRANCH_FAR_BACKWARD:: @@back: "
                          "DU8 127 DUP (0); JMP @@back }",
                          nullptr, 0);
  if (!forward.okay && !backward.okay &&
      !HashFind("_BRANCH_FAR_FORWARD", task.hash_table, HTT_EXPORT_SYS_SYM) &&
      !HashFind("_BRANCH_FAR_BACKWARD", task.hash_table, HTT_EXPORT_SYS_SYM))
    SerialPutS("UnholyOS: short branches rejected out-of-range targets\r\n");

  constexpr const char *path = "/AsmBranchProbe.BIN.Z";
  auto *mod = Load(*ram, path, task, LDF_JUST_LOAD);
  if (mod && LoadPass2(*mod) && BootAsmBranchCheck(task)) {
    SerialPutS("UnholyOS: AOT branch values and bytes matched\r\n");
    BootGuestFileReport(*ram, path);
  }

  constexpr const char *imports = "/AsmBranchImport.BIN.Z";
  mod = Load(*ram, imports, task, LDF_JUST_LOAD | LDF_SILENT);
  if (!mod)
    return;
  auto *entry = reinterpret_cast<const CHashExport *>(
      HashFind("_BRANCH_IMPORT", task.hash_table, HTT_EXPORT_SYS_SYM));
  if (!entry)
    return;
  auto target = ExePutS("asm { ORG _BRANCH_IMPORT_BODY _BR_PATCH:: "
                        "MOV RAX,46 RET }",
                        nullptr, 0);
  constexpr uint8_t bytes[]{0xEB, 8, 0xE9, 5, 0, 0xE9, 0, 0, 0, 0};
  auto *code = reinterpret_cast<const uint8_t *>(entry->val);
  for (size_t i = 0; i < sizeof(bytes); i++)
    if (code[i] != bytes[i])
      return;
  uint64_t value;
  if (target.okay && LoadPass2(*mod) && CallHolyC0Checked(code, &value) &&
      value == 46) {
    SerialPutS("UnholyOS: short and near branch imports matched\r\n");
    BootGuestFileReport(*ram, imports);
  }
}

static void BootSchedulerReport(const CDrv &dvd)
{
  constexpr char source[] = "/SchedulerProbe.HC";
  constexpr char path[] = "/SchedulerProbe.BIN.Z";
  if (!BootFileProgramReport(dvd, source).okay)
    return;
  auto *ram = MountedDrive('B');
  if (!ram || !ExePutS(
      "Cmp(\"::/SchedulerProbe.HC\",,\"B:/SchedulerProbe.BIN.Z\");", nullptr, 0).okay)
    return;
  SerialPutS("UnholyOS: executing /SchedulerProbe.BIN.Z\r\n");
  bool okay = Load(*ram, path, *adam_task) != nullptr;
  SerialPutS("UnholyOS: finished /SchedulerProbe.BIN.Z, checks ");
  SerialPutHex(okay);
  SerialPutS("\r\n");
  if (!okay)
    return;
  SleepCpp(2 * DYING_JIFFIES + 10);
  _YIELD();
  auto *cpu = CurrentCPU();
  if (cpu->next_dying == reinterpret_cast<CTaskDying *>(&cpu->next_dying) &&
      adam_task->next_task == sys_winmgr_task &&
      adam_task->last_task == sys_winmgr_task &&
      sys_winmgr_task->next_task == adam_task &&
      sys_winmgr_task->last_task == adam_task)
    SerialPutS("UnholyOS: dead tasks reaped\r\n");
  BootGuestFileReport(*ram, path);
}

static void BootJobReport(const CDrv &dvd)
{
  constexpr char source[] = "/JobProbe.HC", path[] = "/JobProbe.BIN.Z";
  if (!BootFileProgramReport(dvd, source).okay)
    return;
  auto *ram = MountedDrive('B');
  if (!ram || !ExePutS("Cmp(\"::/JobProbe.HC\",,\"B:/JobProbe.BIN.Z\");",
                       nullptr, 0).okay)
    return;
  SerialPutS("UnholyOS: executing /JobProbe.BIN.Z\r\n");
  bool okay = Load(*ram, path, *adam_task) != nullptr;
  SerialPutS("UnholyOS: finished /JobProbe.BIN.Z, checks ");
  SerialPutHex(okay);
  SerialPutS("\r\n");
  if (okay)
    BootGuestFileReport(*ram, path);
}

static void BootKeyboardReport(const CDrv &dvd)
{
  auto old_debug = DbgModeCpp(false);
  BootFileProgramReport(dvd, "/KeyboardProbe.HC");
  auto level = SYS_RUN_LEVEL;
  SYS_RUN_LEVEL |= RLF_DOC;
  BootFileProgramReport(dvd, "/MsgLoop.HC");
  SYS_RUN_LEVEL = level;
  DbgModeCpp(old_debug);
}

static void BootSoundReport(const CDrv &dvd, CTask &task)
{
  auto start = __atomic_load_n(&cnts.jiffies, __ATOMIC_RELAXED);
  auto freq = cnts.HPET_freq;
  auto hpet_start = freq ? HPET() : 0;
  if (!BootFileProgramReport(dvd, "/SoundProbe.HC").okay)
    return;
  auto elapsed = __atomic_load_n(&cnts.jiffies, __ATOMIC_RELAXED) - start;
  bool timed = elapsed >= 700 && (!freq || HPET() - hpet_start >= freq * 13 / 10);
  if (timed && !(InU8(0x61) & 3) && !scrncast.ona)
    SerialPutS("UnholyOS: beeps completed and stopped\r\n");

  SndCpp(60);
  bool enabled = (InU8(0x61) & 3) == 3 && scrncast.ona == 60;
  auto &snd_lock = sys_semas[SEMA_SND].flags;
  __atomic_fetch_or(&snd_lock, 1, __ATOMIC_ACQUIRE);
  SndCpp(0);
  bool contended = (InU8(0x61) & 3) == 3 && scrncast.ona == 60;
  SndRstCpp();
  bool reset = !(InU8(0x61) & 3) && !scrncast.ona && !(snd_lock & 1);
  auto initial_mute = MuteCpp(1);
  SndCpp(60);
  bool muted = !(InU8(0x61) & 3) && !scrncast.ona && IsMuteCpp();
  auto restored = MuteCpp(initial_mute);
  if (enabled && contended && reset && muted && restored &&
      IsMuteCpp() == initial_mute)
    SerialPutS("UnholyOS: speaker gate, mute and reset matched\r\n");

  auto *ram = MountedDrive('B');
  if (!ram)
    return;
  auto compiled = ExePutS(
      "Cmp(\"::/SoundRecord.HC\",,\"B:/SoundRecord.BIN.Z\");", nullptr, 0);
  if (!compiled.okay || compiled.value)
    return;
  auto *mod = Load(*ram, "/SoundRecord.BIN.Z", task, LDF_JUST_LOAD);
  if (!mod)
    return;
  auto old_debug = DbgModeCpp(false);
  bool okay = LoadPass2(*mod);
  DbgModeCpp(old_debug);
  auto &head = scrncast.snd_head;
  constexpr int8_t notes[]{0, 60, 60, 0};
  unsigned count = 0;
  uint64_t last = head.tS;
  while (head.next != &head) {
    auto *entry = head.next;
    okay = okay && count < sizeof(notes) && entry->ona == notes[count] &&
           entry->tS >= last;
    count++;
    last = entry->tS;
    QueRem(*entry);
    Free(entry);
  }
  auto *fmt = scrncast.print_fmt;
  if (okay && count == sizeof(notes) && !scrncast.record &&
      scrncast.just_audio && fmt &&
      !StrCmpCpp(reinterpret_cast<const uint8_t *>(fmt),
                 reinterpret_cast<const uint8_t *>("B:/Tmp/%X.GR"))) {
    SerialPutS("UnholyOS: sound event queue and default format matched\r\n");
    BootGuestFileReport(*ram, "/SoundRecord.BIN.Z");
  }
  Free(fmt);
  scrncast.print_fmt = nullptr;
}

static void BootAsmListingReport(const CDrv &dvd, CTask &task)
{
  auto *ram = MountedDrive('B');
  if (!ram)
    return;
  constexpr const char *path = "/AsmListingProbe.HC";
  size_t size;
  auto *src = FileRead(dvd, path, size, task.data_heap);
  if (!src)
    return;
  SerialPutS("UnholyOS: executing /AsmListingProbe.HC\r\n");
  auto raw = ExePutS(reinterpret_cast<const char *>(src), path, 0);
  Free(src);
  if (!raw.okay)
    return;
  SerialPutS(
      "UnholyOS: finished /AsmListingProbe.HC, checks 0x0000000000000001\r\n");
  auto called = ExePutS("Call(_ASM_LIST_VALUES);", nullptr, 0);
  if (!called.okay || called.value != 42)
    return;

  SerialPutS("UnholyOS: executing listing Cmp\r\n");
  auto run_level = SYS_RUN_LEVEL;
  SYS_RUN_LEVEL |= RLF_DOC;
  auto built =
      ExePutS("Cmp(\"::/AsmListingProbe.HC\",,\"B:/AsmListingProbe.BIN.Z\");",
              nullptr, 0);
  SYS_RUN_LEVEL = run_level;
  if (!built.okay || built.value)
    return;
  SerialPutS("UnholyOS: finished listing Cmp\r\n");
  auto *module = Load(*ram, "/AsmListingProbe.BIN.Z", task, LDF_JUST_LOAD);
  if (!module || !LoadPass2(*module))
    return;
  called = ExePutS("Call(_ASM_LIST_VALUES);", nullptr, 0);
  if (called.okay && called.value == 42) {
    SerialPutS("UnholyOS: listed module returned 42\r\n");
    BootGuestFileReport(*ram, "/AsmListingProbe.BIN.Z");
  }
}

static void BootFixedAsmReport(const CDrv &dvd, CTask &task)
{
  auto *ram = MountedDrive('B');
  constexpr uint8_t payload[]{0xB8, 42, 0, 0, 0, 0xC3};
  if (!ram ||
      !RedSeaFileWrite(*ram, "/FixedPayload.BIN", payload, sizeof(payload), 0,
                       0) ||
      !BootFileProgramReport(dvd, "/FixedAsm.HC").okay)
    return;

  auto symbol = [&](const char *name) {
    return reinterpret_cast<const CHashExport *>(
        HashFind(name, task.hash_table, HTT_EXPORT_SYS_SYM));
  };
  auto missing = ExePutS("asm { _FIXED_ASM_MISSING::"
                         " BINFILE \"B:/FixedMissing\"; RET }",
                         nullptr, 0);
  auto malformed = ExePutS("asm { _FIXED_ASM_BAD_FILE::"
                           " BINFILE \"B:/FixedPayload\" }",
                           nullptr, 0);
  auto *empty = symbol("_FIXED_ASM_MISSING");
  if (missing.okay && !malformed.okay && !symbol("_FIXED_ASM_BAD_FILE") &&
      empty && *reinterpret_cast<const uint8_t *>(empty->val) == 0xC3)
    SerialPutS("UnholyOS: BINFILE checked missing file and terminator\r\n");

  auto *jit = symbol("_FIXED_ASM_BYTES");
  if (!jit)
    return;
  auto rejected = ExePutS("asm { ORG fixed_asm_bytes _FIXED_ASM_REJECTED::"
                          " DU8 1; JMP @@missing }",
                          nullptr, 0);
  auto misplaced = ExePutS("asm { NOP ORG fixed_asm_bytes }", nullptr, 0);
  auto duplicate =
      ExePutS("asm { ORG fixed_asm_bytes ORG fixed_asm_bytes }", nullptr, 0);
  auto in_fun =
      ExePutS("U0 FixedAsmInFun() { asm { ORG fixed_asm_bytes } }", nullptr, 0);
  if (!rejected.okay && !misplaced.okay && !duplicate.okay && !in_fun.okay &&
      !symbol("_FIXED_ASM_REJECTED") &&
      *reinterpret_cast<const uint8_t *>(jit->val) == payload[0])
    SerialPutS("UnholyOS: invalid ORG left the destination intact\r\n");

  auto *dest = CAlloc(4096, task.code_heap);
  if (!dest)
    return;
  const uint64_t address = reinterpret_cast<uintptr_t>(dest);
  auto *src = MStrPrintCpp(
      "#define FIXED_IMAGE_ORG 0x%X\n#include \"::/FixedImage.HC\"\n", 1,
      &address);
  bool wrote = src && RedSeaFileWrite(*ram, "/FixedImageBuild.HC",
                                      reinterpret_cast<const uint8_t *>(src),
                                      StrLen(src), 0, 0);
  Free(src);
  if (!wrote)
    return;
  auto built = ExePutS("Cmp(\"B:/FixedImageBuild.HC\",,\"B:/FixedImage.BIN\");",
                       nullptr, 0);
  if (!built.okay || built.value)
    return;

  size_t file_size;
  auto *file = FileRead(*ram, "/FixedImage.BIN", file_size, task.data_heap);
  if (file && file_size >= sizeof(CBinFile)) {
    reinterpret_cast<CBinFile *>(file)->patch_table_offset = file_size;
    if (RedSeaFileWrite(*ram, "/FixedBad.BIN", file, file_size, 0, 0) &&
        !Load(*ram, "/FixedBad.BIN", task, LDF_JUST_LOAD) &&
        !symbol("_FIXED_IMAGE") && !*reinterpret_cast<uint64_t *>(dest))
      SerialPutS("UnholyOS: rejected malformed fixed image before copying\r\n");
  }
  Free(file);
  SerialPutS("UnholyOS: executing /FixedImage.BIN\r\n");
  auto *mod = Load(*ram, "/FixedImage.BIN", task, LDF_JUST_LOAD);
  auto *entry = symbol("_FIXED_IMAGE");
  auto *ptr = symbol("_FIXED_PTR");
  auto *body = symbol("_FIXED_BODY");
  uint64_t target = 0;
  if (ptr)
    MemCpy(&target, reinterpret_cast<const uint8_t *>(ptr->val),
           sizeof(target));
  auto &bfh = *reinterpret_cast<const CBinFile *>(dest);
  uint64_t result = 0;
  if (mod && bfh.org == address && bfh.patch_table_offset == sizeof(bfh) + 16 &&
      dest[sizeof(bfh)] == 0xEB && dest[sizeof(bfh) + 1] == 8 && entry &&
      entry->val == address + sizeof(bfh) && body && target == body->val &&
      LoadPass2(*mod) &&
      CallHolyC0Checked(reinterpret_cast<const uint8_t *>(entry->val),
                        &result) &&
      result == 42) {
    SerialPutS("fixed load 42\n");
    SerialPutS(
        "UnholyOS: finished /FixedImage.BIN, checks 0x0000000000000001\r\n");
    BootGuestFileReport(*ram, "/FixedImage.BIN");
  }
}

static void BootSwitchReport(const CDrv &dvd, CTask &task)
{
  auto *ram = MountedDrive('B');
  if (!ram || !BootFileProgramReport(dvd, "/SwitchBuild.HC").okay)
    return;
  BootFileProgramReport(dvd, "/NullCase.HC");
  BootFileProgramReport(dvd, "/SubSwitch.HC");
  BootFileProgramReport(dvd, "/SwitchProbe.HC");
  auto dup = ExePutS(
      "U0 DuplicateCase() { switch(2) { case 1...3: break; case 2: break; } }",
      nullptr, 0);
  if (!dup.okay && !HashFind("DuplicateCase", task.hash_table, HTT_FUN))
    SerialPutS("UnholyOS: switch rejected duplicate case\r\n");

  constexpr const char *path = "/SwitchProbe.BIN.Z";
  SerialPutS("UnholyOS: executing /SwitchProbe.BIN.Z\r\n");
  auto *module = Load(*ram, path, task, LDF_JUST_LOAD);
  if (!module || !LoadPass2(*module))
    return;
  SerialPutS(
      "UnholyOS: finished /SwitchProbe.BIN.Z, checks 0x0000000000000001\r\n");
  BootGuestFileReport(*ram, path);
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
  if (!Core0TaskInit()) {
    SerialPutS("UnholyOS: Could not initialize the idle task\r\n");
    return;
  }
  MsInit();
  if (!KbdInit())
    SerialPutS("UnholyOS: Could not initialize the PS/2 keyboard\r\n");
  if (MsHardDrvrInstall())
    KbdMsService();
  else
    SerialPutS("UnholyOS: Could not initialize the PS/2 mouse\r\n");
  if (!DefineInitColors() || !GrPaletteInit()) {
    SerialPutS("UnholyOS: Could not register color names\r\n");
    return;
  }
  if (!GrInit2()) {
    SerialPutS("UnholyOS: Could not allocate graphics device contexts\r\n");
    return;
  }
  BootPutS("UnholyOS kernel entered in 64-bit mode");
  if (!WinMgrInit()) {
    SerialPutS("UnholyOS: Could not start the window manager\r\n");
    return;
  }

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
    BootProbeReport(*adam_task->cur_dv, *adam_task, "/FileWriteProbe.BIN",
                    "FileWriteProbeResult", "UnholyOS: write probe ");
    BootCmpReport(*adam_task->cur_dv, *adam_task);
    BootImportReport(*adam_task->cur_dv, *adam_task);
    BootAsmReport(*adam_task->cur_dv, *adam_task);
    BootAsmBranchReport(*adam_task->cur_dv, *adam_task);
    BootAsmListingReport(*adam_task->cur_dv, *adam_task);
    BootSchedulerReport(*adam_task->cur_dv);
    BootJobReport(*adam_task->cur_dv);
    BootKeyboardReport(*adam_task->cur_dv);
    BootFileProgramReport(*adam_task->cur_dv, "/MouseProbe.HC");
    if (BootFileProgramReport(*adam_task->cur_dv, "/DCProbe.HC").okay)
      if (auto *ram = MountedDrive('B'))
        BootGuestFileReport(*ram, "/DCProbe.GR.Z");
    BootRasterReport(*adam_task->cur_dv);
    BootFileProgramReport(*adam_task->cur_dv, "/CursorProbe.HC");
    BootFileProgramReport(*adam_task->cur_dv, "/CursorExcept.HC");
    BootFunctionPtrReport(*adam_task->cur_dv, *adam_task);
    BootGridReport(*adam_task->cur_dv);
    BootFileProgramReport(*adam_task->cur_dv, "/DocFrame.HC");
    BootFileProgramReport(*adam_task->cur_dv, "/DocButton.HC");
    if (BootFileProgramReport(*adam_task->cur_dv, "/DocCallback.HC").okay) {
      auto thrown = ExePutS("DocCBThrowTest;", nullptr, 0);
      auto cleanup = ExePutS("DocCBThrowCheck;", nullptr, 0);
      SerialPutS("UnholyOS: document callback propagation ");
      SerialPutHex(!thrown.okay && thrown.exception == 0x4243636F44 &&
                   cleanup.okay && cleanup.value == 1);
      SerialPutS("\r\n");
    }
    BootFileProgramReport(*adam_task->cur_dv, "/ScreenProbe.HC");
    BootFileProgramReport(*adam_task->cur_dv, "/PaletteProbe.HC");
    BootSoundReport(*adam_task->cur_dv, *adam_task);
    BootFileProgramReport(*adam_task->cur_dv, "/AsmListingScope.HC");
    BootFixedAsmReport(*adam_task->cur_dv, *adam_task);
    BootSwitchReport(*adam_task->cur_dv, *adam_task);
    BootISOReport(*adam_task->cur_dv);
  } else {
    SerialPutS("UnholyOS: Could not mount the boot DVD\r\n");
  }
  SerialPutS("UnholyOS: kernel entered in 64-bit mode\r\n");
}
