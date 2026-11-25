#include "../Kernel/KernelB.hpp"
#include "CompilerA.hpp"

static constexpr auto type_mask = HTT_INTERNAL_TYPE | HTT_CLASS;

static CHashClass *FindType(const char *name, CHashTable *table)
{
  return reinterpret_cast<CHashClass *>(HashFind(name, table, type_mask));
}

static bool CmpRuntimeData(CHashTable *table)
{
  auto *heap = CurrentTask()->data_heap;
  auto *cls = PrsClassNew();
  if (!cls)
    return false;
  cls->sym.hash.type |= HTF_PUBLIC;
  cls->sym.hash.str = StrNew("CCntsGlbls", heap);
  cls->size = offsetof(CCntsGlbls, time_stamp_calibrated) + 1;
  if (!cls->sym.hash.str) {
    HashDel(&cls->sym.hash);
    return false;
  }
  HashAdd(&cls->sym.hash, table);

  auto *i64 = FindType("I64", table);
  auto *boolean = FindType("Bool", table);
  constexpr const char *members[]{
      "jiffies",
      "timer",
      "time_stamp_freq",
      "time_stamp_kHz_freq",
      "time_stamp_freq_initial",
      "HPET_freq",
      "HPET_kHz_freq",
      "HPET_initial",
      "time_stamp_calibrated",
  };
  static_assert(offsetof(CCntsGlbls, time_stamp_freq) == 16 &&
                offsetof(CCntsGlbls, time_stamp_calibrated) == 64);
  for (unsigned i = 0; i < sizeof(members) / sizeof(*members); ++i) {
    auto *name = StrNew(members[i], heap);
    if (!name || !MemberAdd(*cls, name, *(i == 8 ? boolean : i64), 8 * i)) {
      Free(name);
      return false;
    }
  }

  auto *var =
      reinterpret_cast<CHashGlblVar *>(CAlloc(sizeof(CHashGlblVar), heap));
  if (!var)
    return false;
  var->sym.hash.type = HTT_GLBL_VAR | HTF_PUBLIC;
  var->sym.hash.str = StrNew("cnts", heap);
  var->sym.import_name = StrNew("cnts", heap);
  if (!var->sym.hash.str || !var->sym.import_name) {
    HashDel(&var->sym.hash);
    return false;
  }
  var->size = cls->size;
  var->flags = GVF_IMPORT | GVF_ALIAS;
  var->var_class = cls;
  var->dim.cnt = var->dim.total_cnt = 1;
  var->data_addr = reinterpret_cast<uint8_t *>(&cnts);
  HashAdd(&var->sym.hash, table);
  return true;
}

static bool CmpRuntimeHeaders(CHashTable *table)
{
  auto *void_type = FindType("U0", table);
  auto *u64_type = FindType("U64", table);
  auto *i64_type = FindType("I64", table);
  auto *string_type = FindType("U8", table) + 1;
  auto *task = CurrentTask();
  struct
  {
    const char *name;
    CHashClass *result;
    uint16_t flags;
    struct
    {
      const char *name;
      CHashClass *type;
      bool default_zero = false;
    } args[4];
  } headers[]{
      {"Print", void_type, FF_DOT_DOT_DOT, {{"fmt", string_type}}},
      {"PutHex", void_type, FF_RET1, {{"num", u64_type}, {"width", i64_type}}},
      {"PutChars", void_type, FF_RET1, {{"ch", u64_type}}},
      {"PutS", void_type, FF_RET1, {{"st", string_type}}},
      {"Call", i64_type, FF_RET1, {{"addr", u64_type}}},
      {"StreamPrint", void_type, FF_DOT_DOT_DOT, {{"fmt", string_type}}},
      {"StreamExePrint", i64_type, FF_DOT_DOT_DOT, {{"fmt", string_type}}},
      {"RedSeaISO",
       i64_type,
       FF_RET1,
       {{"iso_filename", string_type, true},
        {"src_dir", string_type},
        {"stage2_filename", string_type, true}}},
      {"Cmp",
       i64_type,
       FF_RET1,
       {{"filename", string_type},
        {"map_name", string_type, true},
        {"out_name", string_type, true},
        {"mapfile_drv_let", u64_type, true}}},
      {"ExeFile",
       i64_type,
       FF_RET1,
       {{"name", string_type}, {"ccf_flags", i64_type, true}}},
      {"ExePutS",
       i64_type,
       FF_RET1,
       {{"buf", string_type},
        {"filename", string_type, true},
        {"ccf_flags", i64_type, true},
        {"htc", string_type, true}}},
  };
  for (const auto &header : headers) {
    auto *symbol = reinterpret_cast<CHashExport *>(
        HashFind(header.name, task->hash_table, HTT_EXPORT_SYS_SYM));
    if (!symbol)
      continue;
    auto *fun =
        reinterpret_cast<CHashFun *>(CAlloc(sizeof(CHashFun), task->data_heap));
    if (!fun)
      return false;
    fun->cls.sym.hash.type = HTT_FUN | HTF_PUBLIC;
    fun->cls.sym.hash.str = StrNew(header.name, task->data_heap);
    if (!fun->cls.sym.hash.str) {
      HashDel(&fun->cls.sym.hash);
      return false;
    }
    for (const auto &arg : header.args) {
      if (!arg.name)
        break;
      auto *name = StrNew(arg.name, task->data_heap);
      auto *member =
          name ? MemberAdd(fun->cls, name, *arg.type, 16 + 8 * fun->arg_cnt)
               : nullptr;
      if (!member) {
        Free(name);
        HashDel(&fun->cls.sym.hash);
        return false;
      }
      if (arg.default_zero)
        member->flags |= MLF_DFT_AVAILABLE;
      ++fun->arg_cnt;
    }
    fun->cls.flags = FF__EXTERN | header.flags;
    fun->return_class = header.result;
    fun->exe_addr = reinterpret_cast<uint8_t *>(symbol->val);
    HashAdd(&fun->cls.sym.hash, table);
  }
  return true;
}

static bool CmpIntMembers(CHashTable *table, CHashClass &cls)
{
  static constexpr struct
  {
    const char *name, *type;
    unsigned size;
  } views[]{{"i8", "I8i", 1},  {"u8", "U8i", 1},  {"i16", "I16", 2},
            {"u16", "U16", 2}, {"i32", "I32", 4}, {"u32", "U32", 4}};
  auto *heap = CurrentTask()->data_heap;
  for (const auto &view : views) {
    if (view.size >= cls.size)
      break;
    auto *type = FindType(view.type, table);
    auto *name = StrNew(view.name, heap);
    auto *member = name ? MemberAdd(cls, name, *type, 0) : nullptr;
    if (!member) {
      Free(name);
      return false;
    }
    auto count = cls.size / view.size;
    auto *dim = reinterpret_cast<CArrayDim *>(CAlloc(sizeof(CArrayDim), heap));
    if (!dim)
      return false;
    dim->cnt = count;
    dim->total_cnt = 1;
    member->dim = {dim, 0, count};
    member->size = cls.size;
  }
  return true;
}

static bool CmpFillHash(CHashTable *table)
{
  static constexpr const char *keywords[]{
      "include",    "define",  "union",     "catch",   "class",
      "try",        "if",      "else",      "for",     "while",
      "extern",     "_extern", "return",    "sizeof",  "_intern",
      "do",         "asm",     "goto",      "exe",     "break",
      "switch",     "start",   "end",       "case",    "default",
      "public",     "offset",  "import",    "_import", "ifdef",
      "ifndef",     "ifaot",   "ifjit",     "endif",   "assert",
      "reg",        "noreg",   "lastclass", "no_warn", "help_index",
      "help_file",  "static",  "lock",      "defined", "interrupt",
      "haserrcode", "argpop",  "noargpop",
  };
  static_assert(sizeof(keywords) / sizeof(*keywords) == KW_NOARGPOP + 1);
  auto *heap = CurrentTask()->data_heap;
  for (uint64_t i = 0; i < sizeof(keywords) / sizeof(*keywords); ++i) {
    auto *entry =
        reinterpret_cast<CHashGeneric *>(CAlloc(sizeof(CHashGeneric), heap));
    auto *name = entry ? StrNew(keywords[i], heap) : nullptr;
    if (!name) {
      Free(entry);
      return false;
    }
    entry->hash.str = name;
    entry->hash.type = HTT_KEYWORD;
    entry->user_data0 = i;
    HashAdd(&entry->hash, table);
  }
  static constexpr struct
  {
    const char *name;
    CRawType type;
    bool members = false;
  } types[]{{"U0", RT_U0},         {"U0i", RT_U0},        {"I8", RT_I8},
            {"I8i", RT_I8},        {"Bool", RT_I8},       {"U8", RT_U8},
            {"U8i", RT_U8},        {"I16i", RT_I16},      {"U16i", RT_U16},
            {"I32i", RT_I32},      {"U32i", RT_U32},      {"I64i", RT_I64},
            {"U64i", RT_U64},      {"I16", RT_I16, true}, {"U16", RT_U16, true},
            {"I32", RT_I32, true}, {"U32", RT_U32, true}, {"I64", RT_I64, true},
            {"U64", RT_U64, true}, {"F64", RT_F64},       {"F64i", RT_F64}};
  for (const auto &type : types) {
    auto *entry = PrsClassNew();
    auto *name = entry ? StrNew(type.name, heap) : nullptr;
    if (!name) {
      Free(entry);
      return false;
    }
    entry->sym.hash.str = name;
    entry->raw_type = type.type;
    entry->size = TypeSize(type.type);
    if (!type.members) {
      entry->sym.hash.type = HTT_INTERNAL_TYPE;
      entry->flags = CF_INTERNAL_TYPE;
    }
    HashAdd(&entry->sym.hash, table);
  }
  for (const auto &type : types) {
    if (type.members) {
      char name[6];
      auto len = StrLen(type.name);
      MemCpy(name, type.name, len);
      name[len] = 'i';
      name[len + 1] = 0;
      auto *cls = FindType(type.name, table);
      cls->fwd_class = FindType(name, table);
      if (!CmpIntMembers(table, *cls))
        return false;
    }
  }
  constexpr struct {
    const char *name;
    uint64_t value;
  } constants[]{{"SF_RBP",0},{"SF_RIP",8},{"SF_ARG1",16},{"SF_ARG2",24},
                {"SF_ARG3",32},{"SF_ARG4",40},{"SF_ARG5",48},{"SF_ARG6",56},
                {"SF_ARG7",64},{"SF_ARG8",72},{"CH_SPACE",' '}};
  for (const auto &v : constants) {
    auto *entry = reinterpret_cast<CHashExport *>(CAlloc(sizeof(CHashExport), heap));
    auto *name = entry ? StrNew(v.name, heap) : nullptr;
    if (!name) {
      Free(entry);
      return false;
    }
    entry->sym.hash.str = name;
    entry->sym.hash.type = HTT_EXPORT_SYS_SYM | HTF_IMM;
    entry->val = v.value;
    HashAdd(&entry->sym.hash, table);
  }
  constexpr struct {
    const char *name, *data;
  } defines[]{
      {"PUSH_C_REGS","PUSH RAX PUSH RCX PUSH RDX PUSH RBX PUSH R8 PUSH R9"},
      {"POP_C_REGS","POP R9 POP R8 POP RBX POP RDX POP RCX POP RAX"},
  };
  for (const auto &v : defines) {
    auto *entry = reinterpret_cast<CHashDefineStr *>(
        CAlloc(sizeof(CHashDefineStr),heap));
    if (!entry)
      return false;
    entry->sym.hash.str = StrNew(v.name,heap);
    entry->sym.hash.type = HTT_DEFINE_STR;
    entry->data = StrNew(v.data,heap);
    entry->cnt = -1;
    if (!entry->sym.hash.str || !entry->data) {
      HashDel(&entry->sym.hash);
      return false;
    }
    HashAdd(&entry->sym.hash,table);
  }
  return CmpRuntimeData(table) && CmpRuntimeHeaders(table);
}

CHashTable *CmpAsmHashNew()
{
  auto *table = HashTableNew(1024, CurrentTask()->data_heap);
  if (!table)
    return nullptr;
  if (CmpFillHash(table))
    return table;
  HashTableDelCpp(table);
  return nullptr;
}

bool CmpAsmHashLoad(CHashTable *table)
{
  if (FindType("I64i", table))
    return true;
  auto *asm_hash = CmpAsmHashNew();
  if (!asm_hash)
    return false;
  // Keep typed kernel headers with the task's symbols and internal
  // types and keywords in the compiler table.
  for (int64_t i = 0; i <= asm_hash->mask; ++i) {
    auto **link = &asm_hash->body[i];
    while (auto *entry = *link) {
      if (entry->type & (HTT_FUN | HTT_GLBL_VAR)) {
        *link = entry->next;
        HashAdd(entry, table);
      } else
        link = &entry->next;
    }
  }
  asm_hash->next = table->next;
  table->next = asm_hash;
  return true;
}
