#include "../Kernel/KernelB.hpp"
#include "../Adam/Gr/GrExt.hpp"
#include "../Adam/Gr/GrGlbls.hpp"
#include "../Adam/Menu.hpp"
#include "CompilerA.hpp"

extern "C" int64_t LstMatchCpp(const uint8_t *needle, const uint8_t *list,
                               uint64_t flags);
static CHashTable *asm_hash;
static uint64_t asm_hash_lock;

static int64_t AsmLstMatch(const char *name, const char *lst)
{
  constexpr uint64_t LMF_EXACT = 2;
  return LstMatchCpp(reinterpret_cast<const uint8_t *>(name),
                     reinterpret_cast<const uint8_t *>(lst), LMF_EXACT);
}

static bool AsmPrsInsFlags(CCmpCtrl &cc, CInst &ins)
{
  static constexpr char modifiers[] = "NO\0CB\0CW\0CD\0CP\0IB\0IW\0ID\0";
  for (;;) {
    switch (cc.token) {
    case TK_IDENT: {
      auto modifier = AsmLstMatch(cc.cur_str, modifiers);
      if (modifier < 0)
        return true;
      ins.opcode_modifier = modifier;
      break;
    }
    case TK_I64:
      if (cc.cur_i64 == 16)
        ins.flags |= IEF_OP_SIZE16;
      else if (cc.cur_i64 == 32)
        ins.flags |= IEF_OP_SIZE32;
      else
        return true;
      break;
    case '+':
      ins.flags |= IEF_PLUS_OPCODE;
      [[fallthrough]];
    case '/':
      if (LexRaw(cc) == TK_I64 && uint64_t(cc.cur_i64) < 8)
        ins.slash_val = cc.cur_i64;
      else if (cc.token == TK_IDENT && cc.cur_str[1] == 0 &&
               (cc.cur_str[0] == 'I' || cc.cur_str[0] == 'R'))
        ins.slash_val = cc.cur_str[0] == 'R' ? SV_R_REG : SV_I_REG;
      else
        return false;
      break;
    case '!':
      ins.flags |= IEF_DONT_SWITCH_MODES;
      break;
    case '&':
      ins.flags |= IEF_DFT;
      break;
    case '%':
      ins.flags |= IEF_NOT_IN_64_BIT;
      break;
    case '=':
      ins.flags |= IEF_48_REX;
      break;
    case '`':
      ins.flags |= IEF_REX_ONLY_R8_R15;
      break;
    case '^':
      ins.flags |= IEF_REX_XOR_LIKE;
      break;
    case '*':
      ins.flags |= IEF_STI_LIKE;
      break;
    case '$':
      ins.flags |= IEF_ENDING_ZERO;
      break;
    default:
      return cc.token > TK_EOF;
    }
    LexRaw(cc);
  }
}

static bool AsmPrsArgType(CCmpCtrl &cc, uint8_t &arg, uint8_t &size)
{
  if (cc.token != TK_IDENT)
    return true;
  static constexpr char types[] =
      "NONE\0REL8\0REL16\0REL32\0IMM8\0IMM16\0IMM32\0IMM64\0"
      "UIMM8\0UIMM16\0UIMM32\0UIMM64\0R8\0R16\0R32\0R64\0"
      "RM8\0RM16\0RM32\0RM64\0M8\0M16\0M32\0M64\0"
      "M1632\0M16N32\0M16N16\0M32N32\0MOFFS8\0MOFFS16\0MOFFS32\0MOFFS64\0"
      "AL\0AX\0EAX\0RAX\0CL\0DX\0 \0SREG\0SS\0DS\0ES\0FS\0GS\0CS\0"
      "ST0\0STI\0MM\0MM32\0MM64\0XMM\0XMM32\0XMM64\0XMM128\0XMM0\0";
  auto type = AsmLstMatch(cc.cur_str, types);
  if (type < 0)
    return false;
  arg = type;
  constexpr uint64_t masks[]{0x1110111112, 0x2220222224, 0x0440444448,
                             0x0880888880};
  for (unsigned i = 0; i < 4; i++)
    if (masks[i] & (uint64_t{1} << type))
      size = 8U << i;
  return LexRaw(cc) > TK_EOF;
}

static bool AsmOpcodeAdd(CHashTable *table, CHeapCtrl *heap, const char *name,
                         const CInst *ins, unsigned count, unsigned flags)
{
  if (HashFind(name, table, HTT_OPCODE))
    return false;
  const size_t head = offsetof(CHashOpcode, ins);
  auto *op = reinterpret_cast<CHashOpcode *>(
      CAlloc(head + count * sizeof(*ins), heap));
  if (!op)
    return false;
  op->hash.str = StrNew(name, heap);
  if (!op->hash.str) {
    Free(op);
    return false;
  }
  op->hash.type = HTT_OPCODE;
  op->inst_entry_cnt = count;
  op->oc_flags = flags;
  MemCpy(reinterpret_cast<uint8_t *>(op) + head, ins, count * sizeof(*ins));
  HashAdd(&op->hash, table);
  return true;
}

static bool AsmPrsOpcode(CCmpCtrl &cc, CHashTable *table, CHeapCtrl *heap,
                         const char *name)
{
  CInst entries[32];
  MemSet(entries, 0, sizeof(entries));
  unsigned count = 0;
  if (LexRaw(cc) == ',')
    LexRaw(cc); // OpCodes.DD puts a comma after REP_OUTSB.
  while (cc.token == TK_I64) {
    if (count == sizeof(entries) / sizeof(*entries))
      return false;
    auto &ins = entries[count];
    ins.ins_entry_num = count++;
    ins.slash_val = SV_NONE;
    do {
      if (ins.opcode_cnt == sizeof(ins.opcode) || uint64_t(cc.cur_i64) > 0xFF)
        return false;
      ins.opcode[ins.opcode_cnt++] = cc.cur_i64;
    } while (LexRaw(cc) == TK_I64);
    if (cc.token == ',') {
      LexRaw(cc);
      if (!AsmPrsInsFlags(cc, ins) || !AsmPrsArgType(cc, ins.arg1, ins.size1) ||
          !AsmPrsArgType(cc, ins.arg2, ins.size2))
        return false;
    } else if (cc.token != ';')
      return false;
    ins.uasm_slash_val = (ins.flags & IEF_STI_LIKE) && ins.slash_val != SV_I_REG
                             ? uint8_t{SV_STI_LIKE}
                             : ins.slash_val;
  }
  if (!count || !AsmOpcodeAdd(table, heap, name, entries, count, 0))
    return false;
  if (cc.token == ':')
    while (LexRaw(cc) == TK_IDENT)
      if (!AsmOpcodeAdd(table, heap, cc.cur_str, entries, count, OCF_ALIAS))
        return false;
  return cc.token == ';';
}

static bool AsmPrsHashEntry(CCmpCtrl &cc, CHashTable *table, CHeapCtrl *heap)
{
  static constexpr char kinds[] = "NONE\0R8\0R16\0R32\0R64\0SEG\0FSTK\0MM\0XMM\0OPCODE"
                           "\0KEYWORD\0ASM_KEYWORD\0";
  constexpr int opcode = REGT_XMM + 1, keyword = opcode + 1;
  if (cc.token != TK_IDENT)
    return false;
  auto kind = AsmLstMatch(cc.cur_str, kinds);
  if (kind < REGT_R8 || LexRaw(cc) != TK_IDENT)
    return false;
  auto *name = const_cast<char *>(cc.cur_str);
  cc.cur_str = nullptr;
  if (kind == opcode) {
    bool okay = AsmPrsOpcode(cc, table, heap, name);
    Free(name);
    return okay;
  }
  if (LexRaw(cc) != TK_I64 || cc.cur_i64 < 0 ||
      (kind <= REGT_XMM && cc.cur_i64 > 255)) {
    Free(name);
    return false;
  }
  auto val = cc.cur_i64;
  uint32_t type = kind <= REGT_XMM  ? HTT_REG
                  : kind == keyword ? HTT_KEYWORD
                                    : HTT_ASM_KEYWORD;
  if (HashFind(name, table, type) || LexRaw(cc) != ';') {
    Free(name);
    return false;
  }
  auto *entry = reinterpret_cast<CHash *>(
      CAlloc(type == HTT_REG ? sizeof(CHashReg) : sizeof(CHashGeneric), heap));
  if (entry) {
    entry->str = StrNew(name, heap);
    if (!entry->str) {
      Free(entry);
      entry = nullptr;
    }
  }
  Free(name);
  if (!entry)
    return false;
  entry->type = type;
  if (type == HTT_REG) {
    auto *reg = reinterpret_cast<CHashReg *>(entry);
    reg->reg_num = val;
    reg->reg_type = kind;
  } else
    reinterpret_cast<CHashGeneric *>(entry)->user_data0 = val;
  HashAdd(entry, table);
  return true;
}

bool AsmHashLoad()
{
  CMemLock lock(asm_hash_lock);
  if (asm_hash)
    return true;
  auto *source = FileReadCpp("::/Compiler/OpCodes.DD", nullptr, nullptr);
  if (!source)
    return false;
  auto *heap = adam_task->data_heap;
  auto *table = HashTableNew(1024, heap);
  if (!table) {
    Free(source);
    return false;
  }
  CCmpCtrl cc;
  CLexFile file;
  LexSourceInit(cc, file, reinterpret_cast<const char *>(source), 0,
                "::/Compiler/OpCodes.DD");
  bool okay = true;
  while (LexRaw(cc) > TK_EOF)
    if (!AsmPrsHashEntry(cc, table, heap)) {
      okay = false;
      break;
    }
  okay = okay && cc.token == TK_EOF;
  LexSourceDel(cc);
  Free(source);
  if (okay)
    okay=UAsmHashLoad(*table,heap);
  if (okay)
    asm_hash = table;
  else
    HashTableDelCpp(table);
  return okay;
}

const CHash *AsmHashFind(const char *name, uint32_t mask)
{
  CMemLock lock(asm_hash_lock);
  return HashFind(name, asm_hash, mask);
}

static constexpr auto type_mask = HTT_INTERNAL_TYPE | HTT_CLASS;

static CHashClass *FindType(const char *name, CHashTable *table)
{
  return reinterpret_cast<CHashClass *>(HashFind(name, table, type_mask));
}

static bool CmpKernelGlobal(CHashTable *table, const char *name, CHashClass *cls,
                            void *addr, size_t count = 0)
{
  auto *heap = CurrentTask()->data_heap;
  auto *var =
      reinterpret_cast<CHashGlblVar *>(CAlloc(sizeof(CHashGlblVar), heap));
  if (!var)
    return false;
  var->sym.hash.type = HTT_GLBL_VAR | HTF_PUBLIC;
  var->sym.hash.str = StrNew(name, heap);
  var->sym.import_name = StrNew(name, heap);
  if (!var->sym.hash.str || !var->sym.import_name) {
    HashDel(&var->sym.hash);
    return false;
  }
  var->size = cls->size;
  var->flags = GVF_IMPORT | GVF_ALIAS;
  var->var_class = cls;
  var->dim.cnt = 1;
  var->dim.total_cnt = count ? count : 1;
  if (count) {
    auto *axis = reinterpret_cast<CArrayDim *>(CAlloc(sizeof(CArrayDim),heap));
    if (!axis) {
      HashDel(&var->sym.hash);
      return false;
    }
    axis->cnt = count;
    axis->total_cnt = 1;
    var->dim.next = axis;
    var->size = cls->size * count;
  }
  var->data_addr = reinterpret_cast<uint8_t *>(addr);
  HashAdd(&var->sym.hash, table);
  return true;
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

  return CmpKernelGlobal(table, "cnts", cls, &cnts);
}

static CHashClass *CmpKernelClass(CHashTable *table, const char *name, size_t size)
{
  auto *cls = PrsClassNew();
  if (!cls)
    return nullptr;
  cls->sym.hash.type |= HTF_PUBLIC;
  cls->sym.hash.str = StrNew(name, CurrentTask()->data_heap);
  if (!cls->sym.hash.str) {
    HashDel(&cls->sym.hash);
    return nullptr;
  }
  cls->size = size;
  HashAdd(&cls->sym.hash, table);
  return cls;
}

static bool CmpMemberData(CHashTable *table)
{
  auto *hash = CmpKernelClass(table, "CHash", sizeof(CHash));
  auto *sym = CmpKernelClass(table, "CHashSrcSym", sizeof(CHashSrcSym));
  auto *define = CmpKernelClass(table, "CHashDefineStr", sizeof(CHashDefineStr));
  auto *ht = CmpKernelClass(table, "CHashTable", sizeof(CHashTable));
  auto *dim = CmpKernelClass(table, "CArrayDim", sizeof(CArrayDim));
  auto *meta = CmpKernelClass(table, "CMemberLstMeta", sizeof(CMemberLstMeta));
  auto *member = CmpKernelClass(table, "CMemberLst", sizeof(CMemberLst));
  auto *cls = CmpKernelClass(table, "CHashClass", sizeof(CHashClass));
  auto *fun = CmpKernelClass(table, "CHashFun", sizeof(CHashFun));
  if (!hash || !sym || !define || !ht || !dim || !meta || !member || !cls || !fun)
    return false;
  sym->base_class = hash;
  define->base_class = sym;
  cls->base_class = sym;
  fun->base_class = cls;
  auto *heap = CurrentTask()->data_heap;
  auto *i64 = FindType("I64", table);
  auto *u8 = FindType("U8", table);
  auto *u32 = FindType("U32", table);
  auto *u16 = FindType("U16", table);
  auto *i8 = FindType("I8", table);
  auto *ptr = u8 + 1;
  const struct
  {
    const char *name;
    CHashClass *record, *type;
    size_t offset;
  } members[]{
      {"next", hash, hash + 1, offsetof(CHash, next)},
      {"str", hash, ptr, offsetof(CHash, str)},
      {"type", hash, u32, offsetof(CHash, type)},
      {"use_cnt", hash, u32, offsetof(CHash, use_cnt)},
      {"src_link", sym, ptr, offsetof(CHashSrcSym, src_link)},
      {"idx", sym, ptr, offsetof(CHashSrcSym, idx)},
      {"dbg_info", sym, ptr, offsetof(CHashSrcSym, dbg_info)},
      {"import_name", sym, ptr, offsetof(CHashSrcSym, import_name)},
      {"ie_lst", sym, ptr, offsetof(CHashSrcSym, ie_lst)},
      {"data", define, ptr, offsetof(CHashDefineStr, data)},
      {"sub_idx", define, u8 + 2, offsetof(CHashDefineStr, sub_idx)},
      {"cnt", define, i64, offsetof(CHashDefineStr, cnt)},
      {"next", ht, ht + 1, offsetof(CHashTable, next)},
      {"mask", ht, i64, offsetof(CHashTable, mask)},
      {"locked_flags", ht, i64, offsetof(CHashTable, locked_flags)},
      {"body", ht, hash + 2, offsetof(CHashTable, body)},
      {"next", dim, dim + 1, offsetof(CArrayDim, next)},
      {"cnt", dim, i64, offsetof(CArrayDim, cnt)},
      {"total_cnt", dim, i64, offsetof(CArrayDim, total_cnt)},
      {"next", meta, meta + 1, offsetof(CMemberLstMeta, next)},
      {"str", meta, ptr, offsetof(CMemberLstMeta, str)},
      {"flags", meta, i64, offsetof(CMemberLstMeta, flags)},
      {"user_data", meta, i64, offsetof(CMemberLstMeta, user_data)},
      {"next", member, member + 1, offsetof(CMemberLst, next)},
      {"left", member, member + 1, offsetof(CMemberLst, left)},
      {"right", member, member + 1, offsetof(CMemberLst, right)},
      {"left_class_base", member, member + 1, offsetof(CMemberLst, left_class_base)},
      {"right_class_base", member, member + 1, offsetof(CMemberLst, right_class_base)},
      {"str", member, ptr, offsetof(CMemberLst, str)},
      {"member_class", member, cls + 1, offsetof(CMemberLst, member_class)},
      {"member_class_base", member, cls + 1, offsetof(CMemberLst, member_class_base)},
      {"meta", member, meta + 1, offsetof(CMemberLst, meta)},
      {"use_cnt", member, u32, offsetof(CMemberLst, use_cnt)},
      {"flags", member, u16, offsetof(CMemberLst, flags)},
      {"reg", member, i8, offsetof(CMemberLst, reg)},
      {"pad", member, i8, offsetof(CMemberLst, pad)},
      {"offset", member, i64, offsetof(CMemberLst, offset)},
      {"size", member, i64, offsetof(CMemberLst, size)},
      {"dim", member, dim, offsetof(CMemberLst, dim)},
      {"static_data", member, ptr, offsetof(CMemberLst, static_data)},
      {"static_data_rip", member, i64, offsetof(CMemberLst, static_data_rip)},
      {"dft_val", member, i64, offsetof(CMemberLst, dft_val)},
      {"fun_ptr", member, fun + 1, offsetof(CMemberLst, fun_ptr)},
      {"size", cls, i64, offsetof(CHashClass, size)},
      {"neg_offset", cls, i64, offsetof(CHashClass, neg_offset)},
      {"member_cnt", cls, u32, offsetof(CHashClass, member_cnt)},
      {"ptr_stars_cnt", cls, u8, offsetof(CHashClass, ptr_stars_cnt)},
      {"raw_type", cls, u8, offsetof(CHashClass, raw_type)},
      {"flags", cls, u16, offsetof(CHashClass, flags)},
      {"member_lst_and_root", cls, member + 1, offsetof(CHashClass, member_lst_and_root)},
      {"member_class_base_root", cls, member + 1, offsetof(CHashClass, member_class_base_root)},
      {"last_in_member_lst", cls, member + 1, offsetof(CHashClass, last_in_member_lst)},
      {"base_class", cls, cls + 1, offsetof(CHashClass, base_class)},
      {"fwd_class", cls, cls + 1, offsetof(CHashClass, fwd_class)},
      {"return_class", fun, cls + 1, offsetof(CHashFun, return_class)},
      {"arg_cnt", fun, u32, offsetof(CHashFun, arg_cnt)},
      {"pad", fun, u32, offsetof(CHashFun, pad)},
      {"used_reg_mask", fun, u32, offsetof(CHashFun, used_reg_mask)},
      {"clobbered_reg_mask", fun, u32, offsetof(CHashFun, clobbered_reg_mask)},
      {"exe_addr", fun, ptr, offsetof(CHashFun, exe_addr)},
      {"ext_lst", fun, ptr, offsetof(CHashFun, ext_lst)},
  };
  for (const auto &field : members) {
    auto *name = StrNew(field.name, heap);
    if (!name || !MemberAdd(*field.record, name, *field.type, field.offset)) {
      Free(name);
      return false;
    }
  }
  return true;
}

static bool CmpJobData(CHashTable *table, CHashClass *task)
{
  auto *job = CmpKernelClass(table, "CJob", sizeof(CJob));
  auto *ctrl = CmpKernelClass(table, "CJobCtrl", sizeof(CJobCtrl));
  if (!job || !ctrl)
    return false;
  auto *heap = CurrentTask()->data_heap;
  auto *i64 = FindType("I64", table);
  auto *u8 = FindType("U8", table);
  const struct
  {
    const char *name;
    CHashClass *record, *type;
    size_t offset;
  } members[]{
      {"next", job, job + 1, offsetof(CJob, next)},
      {"last", job, job + 1, offsetof(CJob, last)},
      {"ctrl", job, ctrl + 1, offsetof(CJob, ctrl)},
      {"job_code", job, i64, offsetof(CJob, job_code)},
      {"flags", job, i64, offsetof(CJob, flags)},
      {"msg_code", job, i64, offsetof(CJob, msg_code)},
      {"fun_arg", job, u8 + 1, offsetof(CJob, fun_arg)},
      {"aux_str", job, u8 + 1, offsetof(CJob, aux_str)},
      {"aux1", job, i64, offsetof(CJob, aux1)},
      {"aux2", job, i64, offsetof(CJob, aux2)},
      {"res", job, i64, offsetof(CJob, res)},
      {"spawned_task", job, task + 1, offsetof(CJob, spawned_task)},
      {"master_task", job, task + 1, offsetof(CJob, master_task)},
      {"next_waiting", ctrl, job + 1, offsetof(CJobCtrl, next_waiting)},
      {"last_waiting", ctrl, job + 1, offsetof(CJobCtrl, last_waiting)},
      {"next_done", ctrl, job + 1, offsetof(CJobCtrl, next_done)},
      {"last_done", ctrl, job + 1, offsetof(CJobCtrl, last_done)},
      {"flags", ctrl, i64, offsetof(CJobCtrl, flags)},
      {"srv_ctrl", task, ctrl, offsetof(CTask, srv_ctrl)},
  };
  for (const auto &member : members) {
    auto *name = StrNew(member.name, heap);
    if (!name || !MemberAdd(*member.record, name, *member.type, member.offset)) {
      Free(name);
      return false;
    }
  }
  return true;
}

static bool CmpKernelCallback(CHashTable *table, CHashClass &record,
                              const char *member, size_t offset,
                              CHashClass &result, unsigned argc,
                              const char *const *names,
                              CHashClass *const *types)
{
  auto *heap = CurrentTask()->data_heap;
  CPrsDeclarator decl;
  decl.name = StrNew(member, heap);
  decl.fun_ptr = PrsFunPtrNew(result, 1, FF_RET1);
  if (!decl.name || !decl.fun_ptr)
    return false;
  auto *storage = FindType("U0", table) + 1;
  auto *entry = MemberAdd(record, decl.name, *storage, offset);
  if (!entry)
    return false;
  auto &signature = *FunPtrBase(decl.fun_ptr);
  decl.Bind(*entry);
  for (unsigned i = 0; i < argc; i++) {
    auto *name = StrNew(names[i], heap);
    if (!name || !MemberAdd(signature.cls, name, *types[i], 16 + 8 * i)) {
      Free(name);
      return false;
    }
    ++signature.arg_cnt;
  }
  return true;
}

static bool CmpDocData(CHashTable *table, CHashClass &task)
{
  static_assert(offsetof(CDocEntry,define_str)==128 &&
                offsetof(CDocEntry,my_fmt_data)==160);
  auto *doc = CmpKernelClass(table, "CDoc", sizeof(CDoc));
  auto *entry = CmpKernelClass(table, "CDocEntry", sizeof(CDocEntry));
  auto *bin = CmpKernelClass(table, "CDocBin", sizeof(CDocBin));
  if (!doc || !entry || !bin)
    return false;
  auto *i32 = FindType("I32", table);
  auto *i64 = FindType("I64", table);
  auto *u64 = FindType("U64", table);
  auto *u8ptr = FindType("U8", table) + 1;
  auto *heap = CurrentTask()->data_heap;
  const struct
  {
    const char *name;
    CHashClass *record, *type;
    size_t offset;
  } members[]{
      {"flags", doc, u64, offsetof(CDoc, flags)},
      {"locked_flags", doc, u64, offsetof(CDoc, locked_flags)},
      {"cur_entry", doc, entry + 1, offsetof(CDoc, cur_entry)},
      {"cur_col", doc, i32, offsetof(CDoc, cur_col)},
      {"top_line_num", doc, i32, offsetof(CDoc, top_line_num)},
      {"line_start_col", doc, i32, offsetof(CDoc, line_start_col)},
      {"win_task", doc, &task + 1, offsetof(CDoc, win_task)},
      {"mem_task", doc, &task + 1, offsetof(CDoc, mem_task)},
      {"owning_task", doc, &task + 1, offsetof(CDoc, owning_task)},
      {"cmd_U8", doc, i64, offsetof(CDoc, cmd_U8)},
      {"updates_cnt", doc, i64, offsetof(CDoc, updates_cnt)},
      {"max_entries", doc, i64, offsetof(CDoc, max_entries)},
      {"doc_signature", doc, FindType("U32", table),
       offsetof(CDoc, doc_signature)},
      {"user_put_data", doc, u8ptr, offsetof(CDoc, user_put_data)},
      {"bin_head", doc, bin, offsetof(CDoc, bin_head)},
      {"de_flags", entry, u64,
       offsetof(CDocEntry, base) + offsetof(CDocEntryBase, de_flags)},
      {"tag", entry, u8ptr,
       offsetof(CDocEntry, base) + offsetof(CDocEntryBase, tag)},
      {"last", entry, entry + 1,
       offsetof(CDocEntry, base) + offsetof(CDocEntryBase, last)},
      {"next", entry, entry + 1, offsetof(CDocEntry, base) + offsetof(CDocEntryBase, next)},
      {"type_u8", entry, FindType("U8", table), offsetof(CDocEntry, base) + offsetof(CDocEntryBase, type)},
      {"min_col", entry, FindType("U32", table), offsetof(CDocEntry, base) + offsetof(CDocEntryBase, min_col)},
      {"max_col", entry, FindType("U32", table), offsetof(CDocEntry, base) + offsetof(CDocEntryBase, max_col)},
      {"aux_str", entry, u8ptr, offsetof(CDocEntry, aux_str)},
      {"define_str", entry, u8ptr, offsetof(CDocEntry, define_str)},
      {"my_fmt_data", entry, u8ptr, offsetof(CDocEntry, my_fmt_data)},
      {"raw_type", entry, FindType("U8", table), offsetof(CDocEntry, raw_type)},
      {"len", entry, i32, offsetof(CDocEntry, len)},
      {"data", entry, u8ptr, offsetof(CDocEntry, data)},
      {"left_exp", entry, i64, offsetof(CDocEntry, left_exp)},
      {"right_exp", entry, i64, offsetof(CDocEntry, right_exp)},
      {"bin_ptr_link", entry, u8ptr, offsetof(CDocEntry, bin_ptr_link)},
      {"bin_num", entry, i32, offsetof(CDocEntry, bin_num)},
      {"bin_data", entry, bin + 1, offsetof(CDocEntry, bin_data)},
      {"next", bin, bin + 1, offsetof(CDocBin, next)},
      {"last", bin, bin + 1, offsetof(CDocBin, last)},
      {"tmp_use_cnt", bin, i32, offsetof(CDocBin, tmp_use_cnt)},
      {"renum_num", bin, i32, offsetof(CDocBin, renum_num)},
      {"tag", bin, u8ptr, offsetof(CDocBin, tag)},
      {"start", bin, FindType("U0", table), offsetof(CDocBin, num)},
      {"num", bin, FindType("U32", table), offsetof(CDocBin, num)},
      {"flags", bin, FindType("U32", table), offsetof(CDocBin, flags)},
      {"size", bin, FindType("U32", table), offsetof(CDocBin, size)},
      {"use_cnt", bin, FindType("U32", table), offsetof(CDocBin, use_cnt)},
      {"end", bin, FindType("U0", table), offsetof(CDocBin, data)},
      {"data", bin, u8ptr, offsetof(CDocBin, data)},
  };
  for (const auto &member : members) {
    auto *name = StrNew(member.name, heap);
    if (!name ||
        !MemberAdd(*member.record, name, *member.type, member.offset)) {
      Free(name);
      return false;
    }
  }

  const char *args[]{"doc", "doc_e", "mem_task"};
  CHashClass *types[]{doc + 1, entry + 1, &task + 1};
  const char *key_args[]{"doc", "data", "ch", "sc"};
  CHashClass *key_types[]{doc + 1, u8ptr, i64, i64};
  return CmpKernelCallback(table, *entry, "left_cb",
                           offsetof(CDocEntry, left_cb), *i64, 2, args,
                           types) &&
         CmpKernelCallback(table, *entry, "right_cb",
                           offsetof(CDocEntry, right_cb), *i64, 2, args,
                           types) &&
         CmpKernelCallback(table, *entry, "tag_cb", offsetof(CDocEntry, tag_cb),
                           *u8ptr, 3, args, types) &&
         CmpKernelCallback(table, *doc, "left_click_link",
                           offsetof(CDoc, left_click_link), *i64, 2, args,
                           types) &&
         CmpKernelCallback(table, *doc, "right_click_link",
                           offsetof(CDoc, right_click_link), *i64, 2, args,
                           types) &&
         CmpKernelCallback(table, *doc, "user_put_key",
                           offsetof(CDoc, user_put_key),
                           *FindType("Bool", table), 4, key_args, key_types);
}

static bool CmpQueData(CHashTable *table)
{
  auto *que=CmpKernelClass(table,"CQue",sizeof(CQue));
  if (!que)
    return false;
  auto *heap=CurrentTask()->data_heap;
  const char *fields[]{"next","last"};
  for (const char *field : fields) {
    auto *name=StrNew(field,heap);
    size_t offset=*field=='n' ? offsetof(CQue,next) : offsetof(CQue,last);
    if (!name || !MemberAdd(*que,name,*(que+1),offset)) {
      Free(name);
      return false;
    }
  }
  return true;
}

static bool CmpFifoData(CHashTable *table)
{
  auto *u8 = CmpKernelClass(table, "CFifoU8", sizeof(CFifoU8));
  auto *i64 = CmpKernelClass(table, "CFifoI64", sizeof(CFifoI64));
  if (!u8 || !i64)
    return false;
  auto *heap = CurrentTask()->data_heap;
  auto *integer = FindType("I64", table);
  const struct {
    const char *name;
    CHashClass *record, *type;
    size_t offset;
  } members[]{
      {"buf", u8, FindType("U8", table) + 1, offsetof(CFifoU8, buf)},
      {"mask", u8, integer, offsetof(CFifoU8, mask)},
      {"in_ptr", u8, integer, offsetof(CFifoU8, in_ptr)},
      {"out_ptr", u8, integer, offsetof(CFifoU8, out_ptr)},
      {"buf", i64, integer + 1, offsetof(CFifoI64, buf)},
      {"mask", i64, integer, offsetof(CFifoI64, mask)},
      {"in_ptr", i64, integer, offsetof(CFifoI64, in_ptr)},
      {"out_ptr", i64, integer, offsetof(CFifoI64, out_ptr)},
  };
  for (const auto &member : members) {
    auto *name = StrNew(member.name, heap);
    if (!name || !MemberAdd(*member.record, name, *member.type, member.offset)) {
      Free(name);
      return false;
    }
  }
  return true;
}

static bool CmpDateData(CHashTable *table)
{
  auto *date = CmpKernelClass(table, "CDate", sizeof(CDate));
  auto *ds = CmpKernelClass(table, "CDateStruct", sizeof(CDateStruct));
  if (!date || !ds)
    return false;
  date->raw_type = RT_I64;
  date->fwd_class = FindType("I64i", table);
  auto *heap = CurrentTask()->data_heap;
  auto *time = StrNew("time", heap);
  if (!time ||
      !MemberAdd(*date, time, *FindType("U32", table), offsetof(CDate, time))) {
    Free(time);
    return false;
  }
  auto *day = StrNew("date", heap);
  if (!day ||
      !MemberAdd(*date, day, *FindType("I32", table), offsetof(CDate, date))) {
    Free(day);
    return false;
  }
  const struct {
    const char *name;
    size_t offset;
  } fields[]{
      {"sec10000", offsetof(CDateStruct, sec10000)},
      {"sec100", offsetof(CDateStruct, sec100)},
      {"sec", offsetof(CDateStruct, sec)},
      {"min", offsetof(CDateStruct, min)},
      {"hour", offsetof(CDateStruct, hour)},
      {"day_of_week", offsetof(CDateStruct, day_of_week)},
      {"day_of_mon", offsetof(CDateStruct, day_of_mon)},
      {"mon", offsetof(CDateStruct, mon)},
      {"year", offsetof(CDateStruct, year)},
  };
  for (const auto &field : fields) {
    auto *name = StrNew(field.name, heap);
    auto *type = FindType(field.offset == offsetof(CDateStruct, year) ? "I32" : "U8", table);
    if (!name || !MemberAdd(*ds, name, *type, field.offset)) {
      Free(name);
      return false;
    }
  }
  auto *u16 = FindType("U16", table);
  return CmpKernelGlobal(table, "local_time_offset", date, &local_time_offset) &&
         CmpKernelGlobal(table, "mon_start_days1", u16, mon_start_days1, 12) &&
         CmpKernelGlobal(table, "mon_start_days2", u16, mon_start_days2, 12);
}

static bool CmpBinData(CHashTable *table)
{
  static_assert(sizeof(CBinFile) == 32 && offsetof(CBinFile, jmp) == 0 &&
                offsetof(CBinFile, module_align_bits) == 2 &&
                offsetof(CBinFile, reserved) == 3 &&
                offsetof(CBinFile, bin_signature) == 4 &&
                offsetof(CBinFile, org) == 8 &&
                offsetof(CBinFile, patch_table_offset) == 16 &&
                offsetof(CBinFile, file_size) == 24);
  auto *bin = CmpKernelClass(table, "CBinFile", sizeof(CBinFile));
  if (!bin)
    return false;
  auto *heap = CurrentTask()->data_heap;
  const struct
  {
    const char *name, *type;
    size_t offset;
  } members[]{
      {"jmp", "U16", offsetof(CBinFile, jmp)},
      {"module_align_bits", "U8", offsetof(CBinFile, module_align_bits)},
      {"reserved", "U8", offsetof(CBinFile, reserved)},
      {"bin_signature", "U32", offsetof(CBinFile, bin_signature)},
      {"org", "I64", offsetof(CBinFile, org)},
      {"patch_table_offset", "I64", offsetof(CBinFile, patch_table_offset)},
      {"file_size", "I64", offsetof(CBinFile, file_size)},
  };
  for (const auto &member : members) {
    auto *name = StrNew(member.name, heap);
    if (!name ||
        !MemberAdd(*bin, name, *FindType(member.type, table), member.offset)) {
      Free(name);
      return false;
    }
  }
  return true;
}

static bool CmpDirData(CHashTable *table)
{
  auto *entry = CmpKernelClass(table, "CDirEntry", sizeof(CDirEntry));
  auto *context = CmpKernelClass(table, "CDirContext", sizeof(CDirContext));
  auto *drive = CmpKernelClass(table, "CDrv", sizeof(CDrv));
  auto *bd = CmpKernelClass(table, "CBlkDev", sizeof(CBlkDev));
  auto *file = CmpKernelClass(table, "CFile", sizeof(CFile));
  auto *glbls = CmpKernelClass(table, "CBlkDevGlbls", sizeof(CBlkDevGlbls));
  auto *cache = CmpKernelClass(table, "CCacheBlk", sizeof(CCacheBlk));
  auto *free_lst = CmpKernelClass(table,"CFreeLst",sizeof(CFreeLst));
  if (!cache || !free_lst) return false;
  if (!entry || !context || !drive || !bd || !file || !glbls)
    return false;
  auto *heap = CurrentTask()->data_heap;
  auto *i64 = FindType("I64", table);
  auto *u8 = FindType("U8", table);
  const struct
  {
    const char *name;
    CHashClass *record, *type;
    size_t offset;
      size_t count = 0;
  } members[]{
      {"next", entry, entry + 1, offsetof(CDirEntry, next)},
      {"parent", entry, entry + 1, offsetof(CDirEntry, parent)},
      {"sub", entry, entry + 1, offsetof(CDirEntry, sub)},
      {"full_name", entry, u8 + 1, offsetof(CDirEntry, full_name)},
      {"user_data", entry, i64, offsetof(CDirEntry, user_data)},
      {"user_data2", entry, i64, offsetof(CDirEntry, user_data2)},
      {"start", entry, FindType("U0", table), offsetof(CDirEntry, attr)},
      {"attr", entry, FindType("U16", table), offsetof(CDirEntry, attr)},
      {"name", entry, u8, offsetof(CDirEntry, name), CDIR_FILENAME_LEN},
      {"clus", entry, i64, offsetof(CDirEntry, clus)},
      {"size", entry, i64, offsetof(CDirEntry, size)},
      {"datetime", entry, FindType("CDate", table), offsetof(CDirEntry, datetime)},
      {"old_dv", context, drive + 1, offsetof(CDirContext, old_dv)},
      {"dv", context, drive + 1, offsetof(CDirContext, dv)},
      {"old_dir", context, u8 + 1, offsetof(CDirContext, old_dir)},
      {"mask", context, u8 + 1, offsetof(CDirContext, mask)},
      {"locked_flags", drive, i64, offsetof(CDrv, locked_flags)},
      {"dv_signature", drive, FindType("U32", table), offsetof(CDrv, dv_signature)},
      {"drv_let", drive, u8, offsetof(CDrv, drv_let)},
      {"pad", drive, u8, offsetof(CDrv, pad)},
      {"fs_type", drive, FindType("U16", table), offsetof(CDrv, fs_type)},
      {"drv_offset", drive, i64, offsetof(CDrv, drv_offset)},
      {"size", drive, i64, offsetof(CDrv, size)},
      {"prt_num", drive, i64, offsetof(CDrv, prt_num)},
      {"file_system_info_sect", drive, i64, offsetof(CDrv, file_system_info_sect)},
      {"fat1", drive, i64, offsetof(CDrv, fat1)},
      {"fat2", drive, i64, offsetof(CDrv, fat2)},
      {"root_clus", drive, i64, offsetof(CDrv, root_clus)},
      {"data_area", drive, i64, offsetof(CDrv, data_area)},
      {"spc", drive, i64, offsetof(CDrv, spc)},
      {"fat32_local_time_offset", drive, FindType("CDate", table), offsetof(CDrv, fat32_local_time_offset)},
      {"owning_task", drive, FindType("CTask", table) + 1, offsetof(CDrv, owning_task)},
      {"bd", drive, bd + 1, offsetof(CDrv, bd)},
      {"fat_blk_dirty", drive, i64, offsetof(CDrv, fat_blk_dirty)},
      {"cur_fat_blk_num", drive, i64, offsetof(CDrv, cur_fat_blk_num)},
      {"cur_fat_blk", drive, FindType("U32", table) + 1, offsetof(CDrv, cur_fat_blk)},
      {"next_free", drive, free_lst+1, offsetof(CDrv,next_free)},
      {"last_free", drive, free_lst+1, offsetof(CDrv,last_free)},
      {"next", free_lst, free_lst+1, offsetof(CFreeLst,next)},
      {"last", free_lst, free_lst+1, offsetof(CFreeLst,last)},
      {"start", free_lst, i64, offsetof(CFreeLst,start)},
      {"size", free_lst, i64, offsetof(CFreeLst,size)},
      {"lock_fwding", bd, bd + 1, offsetof(CBlkDev, lock_fwding)},
      {"locked_flags", bd, i64, offsetof(CBlkDev, locked_flags)},
      {"bd_signature", bd, FindType("U32", table), offsetof(CBlkDev, bd_signature)},
      {"type", bd, FindType("U32", table), offsetof(CBlkDev, type)},
      {"flags", bd, FindType("U32", table), offsetof(CBlkDev, flags)},
      {"first_drv_let", bd, u8, offsetof(CBlkDev, first_drv_let)},
      {"unit", bd, u8, offsetof(CBlkDev, unit)},
      {"pad", bd, u8, offsetof(CBlkDev, pad), 2},
      {"base0", bd, FindType("U32", table), offsetof(CBlkDev, base0)},
      {"base1", bd, FindType("U32", table), offsetof(CBlkDev, base1)},
      {"blk_size", bd, FindType("U32", table), offsetof(CBlkDev, blk_size)},
      {"drv_offset", bd, i64, offsetof(CBlkDev, drv_offset)},
      {"init_root_dir_blks", bd, i64, offsetof(CBlkDev, init_root_dir_blks)},
      {"max_blk", bd, i64, offsetof(CBlkDev, max_blk)},
      {"dev_id_record", bd, FindType("U16", table) + 1, offsetof(CBlkDev, dev_id_record)},
      {"RAM_dsk", bd, u8 + 1, offsetof(CBlkDev, RAM_dsk)},
      {"file_dsk_name", bd, u8 + 1, offsetof(CBlkDev, file_dsk_name)},
      {"file_dsk", bd, file + 1, offsetof(CBlkDev, file_dsk)},
      {"owning_task", bd, FindType("CTask", table) + 1, offsetof(CBlkDev, owning_task)},
      {"last_time", bd, FindType("F64", table), offsetof(CBlkDev, last_time)},
      {"max_reads", bd, FindType("U32", table), offsetof(CBlkDev, max_reads)},
      {"max_writes", bd, FindType("U32", table), offsetof(CBlkDev, max_writes)},
      {"flags", file, i64, offsetof(CFile, flags)},
      {"de", file, entry, offsetof(CFile, de)},
      {"dv", file, drive + 1, offsetof(CFile, dv)},
      {"fblk_num", file, i64, offsetof(CFile, fblk_num)},
      {"clus", file, i64, offsetof(CFile, clus)},
      {"file_clus_num", file, i64, offsetof(CFile, file_clus_num)},
      {"max_blk", file, i64, offsetof(CFile, max_blk)},
      {"clus_buf", file, u8 + 1, offsetof(CFile, clus_buf)},
      {"next_lru", cache, cache + 1, offsetof(CCacheBlk, next_lru)},
      {"last_lru", cache, cache + 1, offsetof(CCacheBlk, last_lru)},
      {"next_hash", cache, cache + 1, offsetof(CCacheBlk, next_hash)},
      {"last_hash", cache, cache + 1, offsetof(CCacheBlk, last_hash)},
      {"dv", cache, drive + 1, offsetof(CCacheBlk, dv)},
      {"blk", cache, i64, offsetof(CCacheBlk, blk)},
      {"body", cache, u8, offsetof(CCacheBlk, body), BLK_SIZE},
      {"blkdevs", glbls, bd + 1, offsetof(CBlkDevGlbls, blkdevs)},
      {"dft_iso_filename", glbls, u8 + 1, offsetof(CBlkDevGlbls, dft_iso_filename)},
      {"dft_iso_c_filename", glbls, u8 + 1, offsetof(CBlkDevGlbls, dft_iso_c_filename)},
      {"tmp_filename", glbls, u8 + 1, offsetof(CBlkDevGlbls, tmp_filename)},
      {"home_dir", glbls, u8 + 1, offsetof(CBlkDevGlbls, home_dir)},
      {"cache_base", glbls, cache + 1, offsetof(CBlkDevGlbls, cache_base)},
      {"cache_ctrl", glbls, cache + 1, offsetof(CBlkDevGlbls, cache_ctrl)},
      {"cache_hash_table", glbls, cache + 2, offsetof(CBlkDevGlbls, cache_hash_table)},
      {"cache_size", glbls, i64, offsetof(CBlkDevGlbls, cache_size)},
      {"read_cnt", glbls, i64, offsetof(CBlkDevGlbls, read_cnt)},
      {"write_cnt", glbls, i64, offsetof(CBlkDevGlbls, write_cnt)},
      {"drvs", glbls, drive + 1, offsetof(CBlkDevGlbls, drvs)},
      {"let_to_drv", glbls, drive + 1, offsetof(CBlkDevGlbls, let_to_drv), 32},
      {"mount_ide_auto_cnt", glbls, i64, offsetof(CBlkDevGlbls, mount_ide_auto_cnt)},
      {"ins_base0", glbls, i64, offsetof(CBlkDevGlbls, ins_base0)},
      {"ins_base1", glbls, i64, offsetof(CBlkDevGlbls, ins_base1)},
      {"boot_drv_let", glbls, u8, offsetof(CBlkDevGlbls, boot_drv_let)},
      {"first_hd_drv_let", glbls, u8, offsetof(CBlkDevGlbls, first_hd_drv_let)},
      {"first_dvd_drv_let", glbls, u8, offsetof(CBlkDevGlbls, first_dvd_drv_let)},
      {"dvd_boot_is_good", glbls, FindType("Bool", table), offsetof(CBlkDevGlbls, dvd_boot_is_good)},
      {"ins_unit", glbls, FindType("Bool", table), offsetof(CBlkDevGlbls, ins_unit)},
      {"pad", glbls, FindType("Bool", table), offsetof(CBlkDevGlbls, pad), 3},
  };
  for (const auto &member : members) {
    auto *name = StrNew(member.name, heap);
    auto *m = name ? MemberAdd(*member.record, name, *member.type, member.offset)
                   : nullptr;
    if (!m) {
      Free(name);
      return false;
    }
    if (member.count) {
      auto *axis =
          reinterpret_cast<CArrayDim *>(CAlloc(sizeof(CArrayDim), heap));
      if (!axis)
        return false;
      axis->cnt = member.count;
      axis->total_cnt = 1;
      m->dim.next = axis;
      m->dim.total_cnt = member.count;
      m->size = member.count * member.type->size;
    }
  }
  return CmpKernelGlobal(table, "blkdev", glbls, &blkdev) &&
         CmpKernelGlobal(table, "blkdev_text_attr", u8, blkdev_text_attr, 6) &&
         CmpKernelGlobal(table, "drv_text_attr", u8, drv_text_attr, 3);
}

static CHashClass *CmpExceptData(CHashTable *table)
{
  auto *cls = CmpKernelClass(table, "CExcept", sizeof(CExcept));
  if (!cls)
    return nullptr;
  auto *heap = CurrentTask()->data_heap;
  auto *i64 = FindType("I64", table);
  const struct
  {
    const char *name;
    CHashClass *type;
    size_t offset;
  } members[]{
      {"next", cls + 1, offsetof(CExcept, next)},
      {"last", cls + 1, offsetof(CExcept, last)},
      {"hndlr_catch", i64, offsetof(CExcept, hndlr_catch)},
      {"hndlr_untry", i64, offsetof(CExcept, hndlr_untry)},
      {"rsp", i64, offsetof(CExcept, rsp)},
      {"rbp", i64, offsetof(CExcept, rbp)},
      {"rflags", i64, offsetof(CExcept, rflags)},
      {"rsi", i64, offsetof(CExcept, rsi)},
      {"rdi", i64, offsetof(CExcept, rdi)},
      {"r10", i64, offsetof(CExcept, r10)},
      {"r11", i64, offsetof(CExcept, r11)},
      {"r12", i64, offsetof(CExcept, r12)},
      {"r13", i64, offsetof(CExcept, r13)},
      {"r14", i64, offsetof(CExcept, r14)},
      {"r15", i64, offsetof(CExcept, r15)},
  };
  for (const auto &member : members) {
    auto *name = StrNew(member.name, heap);
    if (!name || !MemberAdd(*cls, name, *member.type, member.offset)) {
      Free(name);
      return nullptr;
    }
  }
  return cls;
}

static CHashClass *CmpHeapData(CHashTable *table, CHashClass *task)
{
  auto *cls = CmpKernelClass(table, "CHeapCtrl", sizeof(CHeapCtrl));
  if (!cls)
    return nullptr;
  auto *heap = CurrentTask()->data_heap;
  auto *u32 = FindType("U32", table);
  auto *i64 = FindType("I64", table);
  static_assert(sizeof(CHeapCtrl) == 0x460 &&
                offsetof(CHeapCtrl, hc_signature) == 8 &&
                offsetof(CHeapCtrl, locked_flags) == 0x10 &&
                offsetof(CHeapCtrl, alloced_u8s) == 0x18 &&
                offsetof(CHeapCtrl, used_u8s) == 0x20 &&
                offsetof(CHeapCtrl, mem_task) == 0x28);
  const struct
  {
    const char *name;
    CHashClass *type;
    size_t offset;
  } members[]{
      {"hc_signature", u32, offsetof(CHeapCtrl, hc_signature)},
      {"pad", u32, offsetof(CHeapCtrl, pad)},
      {"locked_flags", i64, offsetof(CHeapCtrl, locked_flags)},
      {"alloced_u8s", i64, offsetof(CHeapCtrl, alloced_u8s)},
      {"used_u8s", i64, offsetof(CHeapCtrl, used_u8s)},
      {"mem_task", task + 1, offsetof(CHeapCtrl, mem_task)},
  };
  for (const auto &member : members) {
    auto *name = StrNew(member.name, heap);
    if (!name || !MemberAdd(*cls, name, *member.type, member.offset)) {
      Free(name);
      return nullptr;
    }
  }
  return cls;
}

static bool CmpTaskData(CHashTable *table, CHashClass *cls)
{
  auto *except = CmpExceptData(table);
  if (!except)
    return false;
  auto *hc = CmpHeapData(table, cls);
  if (!hc)
    return false;
  static_assert(offsetof(CTask, code_heap) == 0x20 &&
                offsetof(CTask, data_heap) == 0x28);
  auto *heap = CurrentTask()->data_heap;
  auto *u32 = FindType("U32", table);
  auto *i64 = FindType("I64", table);
  auto *u8 = FindType("U8", table);
  struct
  {
    const char *name;
    CHashClass *type;
    size_t offset;
    size_t count = 0;
  } members[]{
      {"addr", cls + 1, offsetof(CTask, addr)},
      {"task_signature", u32, offsetof(CTask, task_signature)},
      {"win_inhibit", u32, offsetof(CTask, win_inhibit)},
      {"pix_left", i64, offsetof(CTask, pix_left)},
      {"pix_top", i64, offsetof(CTask, pix_top)},
      {"pix_right", i64, offsetof(CTask, pix_right)},
      {"pix_bottom", i64, offsetof(CTask, pix_bottom)},
      {"pix_width", i64, offsetof(CTask, pix_width)},
      {"pix_height", i64, offsetof(CTask, pix_height)},
      {"rand_seed", i64, offsetof(CTask, rand_seed)},
      {"win_z_num", FindType("U16", table), offsetof(CTask, win_z_num)},
      {"scroll_x", i64, offsetof(CTask, scroll_x)},
      {"scroll_y", i64, offsetof(CTask, scroll_y)},
      {"scroll_z", i64, offsetof(CTask, scroll_z)},
      {"wake_jiffy", i64, offsetof(CTask, wake_jiffy)},
      {"task_flags", u32, offsetof(CTask, task_flags)},
      {"display_flags", u32, offsetof(CTask, display_flags)},
      {"code_heap", hc + 1, offsetof(CTask, code_heap)},
      {"data_heap", hc + 1, offsetof(CTask, data_heap)},
      {"text_attr", u8, offsetof(CTask, text_attr)},
      {"border_attr", u8, offsetof(CTask, border_attr)},
      {"border_src", u8, offsetof(CTask, border_src)},
      {"title_src", u8, offsetof(CTask, title_src)},
      {"task_title", u8, offsetof(CTask, task_title),
       sizeof(CTask::task_title)},
      {"song_task", cls + 1, offsetof(CTask, song_task)},
      {"animate_task", cls + 1, offsetof(CTask, animate_task)},
      {"put_doc", FindType("CDoc", table) + 1, offsetof(CTask, put_doc)},
      {"display_doc", FindType("CDoc", table) + 1,
       offsetof(CTask, display_doc)},
      {"border_doc", FindType("CDoc", table) + 1, offsetof(CTask, border_doc)},
      {"win_left", i64, offsetof(CTask, win_left)},
      {"win_right", i64, offsetof(CTask, win_right)},
      {"win_top", i64, offsetof(CTask, win_top)},
      {"win_bottom", i64, offsetof(CTask, win_bottom)},
      {"win_width", i64, offsetof(CTask, win_width)},
      {"win_height", i64, offsetof(CTask, win_height)},
      {"cur_dir", u8 + 1, offsetof(CTask, cur_dir)},
      {"cur_dv", FindType("CDrv", table) + 1, offsetof(CTask, cur_dv)},
      {"hash_table", FindType("CHashTable", table) + 1, offsetof(CTask, hash_table)},
      {"parent_task", cls + 1, offsetof(CTask, parent_task)},
      {"next_task", cls + 1, offsetof(CTask, next_task)},
      {"last_task", cls + 1, offsetof(CTask, last_task)},
      {"next_child_task", cls + 1, offsetof(CTask, next_child_task)},
      {"last_child_task", cls + 1, offsetof(CTask, last_child_task)},
      {"swap_cnter", i64, offsetof(CTask, swap_cnter)},
      {"next_except", except + 1, offsetof(CTask, next_except)},
      {"last_except", except + 1, offsetof(CTask, last_except)},
      {"except_rbp", i64, offsetof(CTask, except_rbp)},
      {"except_ch", i64, offsetof(CTask, except_ch)},
      {"except_callers", u8 + 1, offsetof(CTask, except_callers),
       sizeof(CTask::except_callers) / sizeof(uint8_t *)},
      {"catch_except", FindType("Bool", table), offsetof(CTask, catch_except)},
      {"task_num", i64, offsetof(CTask, task_num)},
      {"user_data", i64, offsetof(CTask, user_data)},
  };
  for (const auto &member : members) {
    auto *name = StrNew(member.name, heap);
    auto *entry =
        name ? MemberAdd(*cls, name, *member.type, member.offset) : nullptr;
    if (!entry) {
      Free(name);
      return false;
    }
    if (member.count) {
      auto *axis =
          reinterpret_cast<CArrayDim *>(CAlloc(sizeof(CArrayDim), heap));
      if (!axis)
        return false;
      axis->cnt = member.count;
      axis->total_cnt = 1;
      entry->dim.next = axis;
      entry->dim.total_cnt = member.count;
      entry->size = member.count * member.type->size;
    }
  }
  return CmpKernelGlobal(table, "sys_winmgr_task", cls + 1, &sys_winmgr_task) &&
         CmpKernelGlobal(table, "sys_focus_task", cls + 1, &sys_focus_task) &&
         CmpKernelGlobal(table, "sys_task_being_scrn_updated", cls + 1,
                         &sys_task_being_scrn_updated) &&
         CmpJobData(table, cls);
}

static bool CmpKbdData(CHashTable *table)
{
  auto *cls = CmpKernelClass(table, "CKbdStateGlbls", sizeof(CKbdStateGlbls));
  if (!cls)
    return false;
  auto *heap = CurrentTask()->data_heap;
  auto *i64 = FindType("I64", table);
  auto *boolean = FindType("Bool", table);
  const struct {
    const char *name;
    CHashClass *type;
    size_t offset;
  } members[]{
      {"scan_code", i64, offsetof(CKbdStateGlbls, scan_code)},
      {"last_down_scan_code", i64, offsetof(CKbdStateGlbls, last_down_scan_code)},
      {"cnt", i64, offsetof(CKbdStateGlbls, cnt)},
      {"timestamp", i64, offsetof(CKbdStateGlbls, timestamp)},
      {"new_key_timestamp", i64, offsetof(CKbdStateGlbls, new_key_timestamp)},
      {"fifo", FindType("CFifoU8", table) + 1, offsetof(CKbdStateGlbls, fifo)},
      {"fifo2", FindType("CFifoU8", table) + 1, offsetof(CKbdStateGlbls, fifo2)},
      {"scan_code_fifo", FindType("CFifoI64", table) + 1, offsetof(CKbdStateGlbls, scan_code_fifo)},
      {"rst", boolean, offsetof(CKbdStateGlbls, rst)},
      {"irqs_working", boolean, offsetof(CKbdStateGlbls, irqs_working)},
  };
  for (const auto &member : members) {
    auto *name = StrNew(member.name, heap);
    if (!name || !MemberAdd(*cls, name, *member.type, member.offset)) {
      Free(name);
      return false;
    }
  }
  return CmpKernelGlobal(table, "kbd", cls, &kbd);
}

static bool CmpTextData(CHashTable *table)
{
  auto *cls=CmpKernelClass(table,"CTextGlbls",sizeof(CTextGlbls));
  if (!cls)
    return false;
  auto *heap=CurrentTask()->data_heap;
  auto *i64=FindType("I64",table);
  auto *u8=FindType("U8",table);
  auto *u64=FindType("U64",table);
  const struct {
    const char *name;
    CHashClass *type;
    size_t offset;
    size_t count=0;
  } members[]{
    {"raw_col",i64,offsetof(CTextGlbls,raw_col)},
    {"raw_flags",i64,offsetof(CTextGlbls,raw_flags)},
    {"raw_scrn_image",u8+1,offsetof(CTextGlbls,raw_scrn_image)},
    {"rows",i64,offsetof(CTextGlbls,rows)},
    {"cols",i64,offsetof(CTextGlbls,cols)},
    {"font",u64+1,offsetof(CTextGlbls,font)},
    {"aux_font",u64+1,offsetof(CTextGlbls,aux_font)},
    {"vga_alias",u8+1,offsetof(CTextGlbls,vga_alias)},
    {"vga_text_alias",u8+1,offsetof(CTextGlbls,vga_text_alias)},
    {"border_chars",u8,offsetof(CTextGlbls,border_chars),16},
  };
  for (const auto &member:members) {
    auto *name=StrNew(member.name,heap);
    auto *entry=name ? MemberAdd(*cls,name,*member.type,member.offset) : nullptr;
    if (!entry) {
      Free(name);
      return false;
    }
    if (member.count) {
      auto *axis=reinterpret_cast<CArrayDim *>(CAlloc(sizeof(CArrayDim),heap));
      if (!axis)
        return false;
      axis->cnt=member.count;
      axis->total_cnt=1;
      entry->dim.next=axis;
      entry->dim.total_cnt=member.count;
      entry->size=member.count*member.type->size;
    }
  }
  return CmpKernelGlobal(table,"text",cls,&text);
}

static bool CmpMouseData(CHashTable *table)
{
  auto *i_pt = CmpKernelClass(table, "CD3I64", sizeof(CD3I64));
  auto *f_pt = CmpKernelClass(table, "CD3", sizeof(CD3));
  auto *state = CmpKernelClass(table, "CMsStateGlbls", sizeof(CMsStateGlbls));
  auto *hard = CmpKernelClass(table, "CMsHardStateGlbls", sizeof(CMsHardStateGlbls));
  auto *grid = CmpKernelClass(table, "CGridGlbls", sizeof(CGridGlbls));
  if (!i_pt || !f_pt || !state || !hard || !grid)
    return false;
  auto *heap = CurrentTask()->data_heap;
  auto *i64 = FindType("I64", table);
  auto *f64 = FindType("F64", table);
  auto *boolean = FindType("Bool", table);
  const struct {
    const char *name;
    CHashClass *record, *type;
    size_t offset;
  } members[]{
      {"x", i_pt, i64, offsetof(CD3I64, x)},
      {"y", i_pt, i64, offsetof(CD3I64, y)},
      {"z", i_pt, i64, offsetof(CD3I64, z)},
      {"x", f_pt, f64, offsetof(CD3, x)},
      {"y", f_pt, f64, offsetof(CD3, y)},
      {"z", f_pt, f64, offsetof(CD3, z)},
      {"pos", state, i_pt, offsetof(CMsStateGlbls, pos)},
      {"pos_text", state, i_pt, offsetof(CMsStateGlbls, pos_text)},
      {"presnap", state, i_pt, offsetof(CMsStateGlbls, presnap)},
      {"offset", state, i_pt, offsetof(CMsStateGlbls, offset)},
      {"scale", state, f_pt, offsetof(CMsStateGlbls, scale)},
      {"speed", state, f64, offsetof(CMsStateGlbls, speed)},
      {"dbl_time", state, f64, offsetof(CMsStateGlbls, dbl_time)},
      {"left_dbl_time", state, f64, offsetof(CMsStateGlbls, left_dbl_time)},
      {"right_dbl_time", state, f64, offsetof(CMsStateGlbls, right_dbl_time)},
      {"timestamp", state, i64, offsetof(CMsStateGlbls, timestamp)},
      {"lb", state, boolean, offsetof(CMsStateGlbls, lb)},
      {"rb", state, boolean, offsetof(CMsStateGlbls, rb)},
      {"show", state, boolean, offsetof(CMsStateGlbls, show)},
      {"has_wheel", state, boolean, offsetof(CMsStateGlbls, has_wheel)},
      {"left_dbl", state, boolean, offsetof(CMsStateGlbls, left_dbl)},
      {"right_dbl", state, boolean, offsetof(CMsStateGlbls, right_dbl)},
      {"left_down_sent", state, boolean, offsetof(CMsStateGlbls, left_down_sent)},
      {"right_down_sent", state, boolean, offsetof(CMsStateGlbls, right_down_sent)},
      {"pos", hard, i_pt, offsetof(CMsHardStateGlbls, pos)},
      {"prescale", hard, i_pt, offsetof(CMsHardStateGlbls, prescale)},
      {"scale", hard, f_pt, offsetof(CMsHardStateGlbls, scale)},
      {"speed", hard, f64, offsetof(CMsHardStateGlbls, speed)},
      {"timestamp", hard, i64, offsetof(CMsHardStateGlbls, timestamp)},
      {"install_attempts", hard, i64, offsetof(CMsHardStateGlbls, install_attempts)},
      {"pkt_size", hard, i64, offsetof(CMsHardStateGlbls, pkt_size)},
      {"fifo", hard, FindType("CFifoU8", table) + 1, offsetof(CMsHardStateGlbls, fifo)},
      {"fifo2", hard, FindType("CFifoU8", table) + 1, offsetof(CMsHardStateGlbls, fifo2)},
      {"has_wheel", hard, boolean, offsetof(CMsHardStateGlbls, has_wheel)},
      {"has_ext_bttns", hard, boolean, offsetof(CMsHardStateGlbls, has_ext_bttns)},
      {"evt", hard, boolean, offsetof(CMsHardStateGlbls, evt)},
      {"installed", hard, boolean, offsetof(CMsHardStateGlbls, installed)},
      {"install_in_progress", hard, boolean, offsetof(CMsHardStateGlbls, install_in_progress)},
      {"irqs_working", hard, boolean, offsetof(CMsHardStateGlbls, irqs_working)},
      {"snap", grid, boolean, offsetof(CGridGlbls, snap)},
      {"show", grid, boolean, offsetof(CGridGlbls, show)},
      {"coord", grid, boolean, offsetof(CGridGlbls, coord)},
      {"x", grid, f64, offsetof(CGridGlbls, x)},
      {"y", grid, f64, offsetof(CGridGlbls, y)},
      {"z", grid, f64, offsetof(CGridGlbls, z)},
      {"x_offset", grid, f64, offsetof(CGridGlbls, x_offset)},
      {"y_offset", grid, f64, offsetof(CGridGlbls, y_offset)},
      {"z_offset", grid, f64, offsetof(CGridGlbls, z_offset)},
      {"x_speed", grid, f64, offsetof(CGridGlbls, x_speed)},
      {"y_speed", grid, f64, offsetof(CGridGlbls, y_speed)},
      {"z_speed", grid, f64, offsetof(CGridGlbls, z_speed)},
  };
  for (const auto &member : members) {
    auto *name = StrNew(member.name, heap);
    if (!name || !MemberAdd(*member.record, name, *member.type, member.offset)) {
      Free(name);
      return false;
    }
  }
  return CmpKernelGlobal(table, "ms", state, &ms) &&
         CmpKernelGlobal(table, "ms_hard", hard, &ms_hard) &&
         CmpKernelGlobal(table, "ms_grid", grid, &ms_grid);
}

static bool CmpPaletteData(CHashTable *table)
{
  auto *cls = CmpKernelClass(table, "CBGR48", sizeof(CBGR48));
  if (!cls)
    return false;
  cls->fwd_class = FindType("I64i", table);
  auto *heap = CurrentTask()->data_heap;
  auto *u16 = FindType("U16", table);
  constexpr struct
  {
    const char *name;
    size_t offset;
  } members[]{
      {"b", offsetof(CBGR48, b)},
      {"g", offsetof(CBGR48, g)},
      {"r", offsetof(CBGR48, r)},
      {"pad", offsetof(CBGR48, pad)},
  };
  for (const auto &member : members) {
    auto *name = StrNew(member.name, heap);
    if (!name || !MemberAdd(*cls, name, *u16, member.offset)) {
      Free(name);
      return false;
    }
  }
  return CmpKernelGlobal(table, "gr_palette_std", cls, gr_palette_std,
                         COLORS_NUM) &&
         CmpKernelGlobal(table, "gr_palette_gray", cls, gr_palette_gray,
                         COLORS_NUM) &&
         CmpKernelGlobal(table, "gr_rainbow_10", FindType("U8", table),
                         gr_rainbow_10, 10);
}

static bool CmpDCData(CHashTable *table)
{
  auto *color16 = CmpKernelClass(table, "CColorROPU16", sizeof(CColorROPU16));
  auto *color32 = CmpKernelClass(table, "CColorROPU32", sizeof(CColorROPU32));
  auto *p = CmpKernelClass(table, "CD3I32", sizeof(CD3I32));
  auto *sym = CmpKernelClass(table, "CGrSym", sizeof(CGrSym));
  auto *dc = CmpKernelClass(table, "CDC", sizeof(CDC));
  auto *glbl = CmpKernelClass(table, "CGrGlbls", sizeof(CGrGlbls));
  if (!color16 || !color32 || !p || !sym || !dc || !glbl)
    return false;
  color16->fwd_class = FindType("U16i", table);
  color32->fwd_class = FindType("U32i", table);
  auto *heap = CurrentTask()->data_heap;
  auto *u0 = FindType("U0", table);
  auto *u8 = FindType("U8", table);
  auto *u32 = FindType("U32", table);
  auto *i32 = FindType("I32", table);
  auto *u64 = FindType("U64", table);
  auto *i64 = FindType("I64", table);
  auto *boolean = FindType("Bool", table);
  auto *bgr48 = FindType("CBGR48", table);
  auto *task = FindType("CTask", table) + 1;
  const struct
  {
    const char *name;
    CHashClass *record, *type;
    size_t offset;
    size_t count = 0, inner_count = 0;
  } members[]{
      {"color", color16, u8, offsetof(CColorROPU16, color)},
      {"rop", color16, u8, offsetof(CColorROPU16, rop)},
      {"c0", color32, color16, offsetof(CColorROPU32, c0)},
      {"c1", color32, color16, offsetof(CColorROPU32, c1)},
      {"x", p, i32, offsetof(CD3I32, x)},
      {"y", p, i32, offsetof(CD3I32, y)},
      {"z", p, i32, offsetof(CD3I32, z)},
      {"sx", sym, i32, offsetof(CGrSym, sx)},
      {"sy", sym, i32, offsetof(CGrSym, sy)},
      {"sz", sym, i32, offsetof(CGrSym, sz)},
      {"pad", sym, i32, offsetof(CGrSym, pad)},
      {"snx", sym, i64, offsetof(CGrSym, snx)},
      {"sny", sym, i64, offsetof(CGrSym, sny)},
      {"snz", sym, i64, offsetof(CGrSym, snz)},
      {"start", dc, u0, 0},
      {"cdt", dc, u64, offsetof(CDC, cdt)},
      {"x0", dc, i32, offsetof(CDC, x0)},
      {"y0", dc, i32, offsetof(CDC, y0)},
      {"width", dc, i32, offsetof(CDC, width)},
      {"width_internal", dc, i32, offsetof(CDC, width_internal)},
      {"height", dc, i32, offsetof(CDC, height)},
      {"flags", dc, i32, offsetof(CDC, flags)},
      {"end", dc, u0, offsetof(CDC, palette)},
      {"palette", dc, bgr48, offsetof(CDC, palette), COLORS_NUM},
      {"color", dc, color32, offsetof(CDC, color)},
      {"bkcolor", dc, color32, offsetof(CDC, bkcolor)},
      {"color2", dc, color32, offsetof(CDC, color2)},
      {"ls", dc, p, offsetof(CDC, ls)},
      {"dither_probability_u16", dc, u64,
       offsetof(CDC, dither_probability_u16)},
      {"brush", dc, dc + 1, offsetof(CDC, brush)},
      {"r", dc, i64 + 1, offsetof(CDC, r)},
      {"r_norm", dc, i64, offsetof(CDC, r_norm)},
      {"x", dc, i32, offsetof(CDC, x)},
      {"y", dc, i32, offsetof(CDC, y)},
      {"z", dc, i32, offsetof(CDC, z)},
      {"thick", dc, i32, offsetof(CDC, thick)},
      {"sym", dc, sym, offsetof(CDC, sym)},
      {"cur_x", dc, i32, offsetof(CDC, cur_x)},
      {"cur_y", dc, i32, offsetof(CDC, cur_y)},
      {"cur_z", dc, i32, offsetof(CDC, cur_z)},
      {"collision_cnt", dc, i64, offsetof(CDC, collision_cnt)},
      {"nearest_dist", dc, i64, offsetof(CDC, nearest_dist)},
      {"min_x", dc, i64, offsetof(CDC, min_x)},
      {"max_x", dc, i64, offsetof(CDC, max_x)},
      {"min_y", dc, i64, offsetof(CDC, min_y)},
      {"max_y", dc, i64, offsetof(CDC, max_y)},
      {"dc_signature", dc, u32, offsetof(CDC, dc_signature)},
      {"mem_task", dc, task, offsetof(CDC, mem_task)},
      {"win_task", dc, task, offsetof(CDC, win_task)},
      {"alias", dc, dc + 1, offsetof(CDC, alias)},
      {"body", dc, u8 + 1, offsetof(CDC, body)},
      {"depth_buf", dc, i32 + 1, offsetof(CDC, depth_buf)},
      {"db_z", dc, i64, offsetof(CDC, db_z)},
      {"scrn_image", glbl, dc + 1, offsetof(CGrGlbls, scrn_image)},
      {"dc", glbl, dc + 1, offsetof(CGrGlbls, dc)},
      {"dc1", glbl, dc + 1, offsetof(CGrGlbls, dc1)},
      {"dc2", glbl, dc + 1, offsetof(CGrGlbls, dc2)},
      {"dc_cache", glbl, dc + 1, offsetof(CGrGlbls, dc_cache)},
      {"zoomed_dc", glbl, dc + 1, offsetof(CGrGlbls, zoomed_dc)},
      {"text_base", glbl, u32 + 1, offsetof(CGrGlbls, text_base)},
      {"pan_text_x", glbl, i64, offsetof(CGrGlbls, pan_text_x)},
      {"pan_text_y", glbl, i64, offsetof(CGrGlbls, pan_text_y)},
      {"win_z_buf", glbl, FindType("U16",table) + 1, offsetof(CGrGlbls, win_z_buf)},
      {"pen_brushes", glbl, dc + 1, offsetof(CGrGlbls, pen_brushes),
       GR_PEN_BRUSHES_NUM},
      {"collision_pen_brushes", glbl, dc + 1,
       offsetof(CGrGlbls, collision_pen_brushes), GR_PEN_BRUSHES_NUM},
      {"even_pen_brushes", glbl, dc + 1, offsetof(CGrGlbls, even_pen_brushes),
       GR_PEN_BRUSHES_NUM},
      {"odd_pen_brushes", glbl, dc + 1, offsetof(CGrGlbls, odd_pen_brushes),
       GR_PEN_BRUSHES_NUM},
      {"circle_lo", glbl, FindType("I8", table), offsetof(CGrGlbls, circle_lo),
       GR_PEN_BRUSHES_NUM, GR_PEN_BRUSHES_NUM},
      {"circle_hi", glbl, FindType("I8", table), offsetof(CGrGlbls, circle_hi),
       GR_PEN_BRUSHES_NUM, GR_PEN_BRUSHES_NUM},
      {"scrn_zoom", glbl, i64, offsetof(CGrGlbls, scrn_zoom)},
      {"sx", glbl, i64, offsetof(CGrGlbls, sx)},
      {"sy", glbl, i64, offsetof(CGrGlbls, sy)},
      {"continuous_scroll", glbl, boolean,
       offsetof(CGrGlbls, continuous_scroll)},
      {"hide_row", glbl, boolean, offsetof(CGrGlbls, hide_row)},
      {"hide_col", glbl, boolean, offsetof(CGrGlbls, hide_col)},
  };
  for (const auto &member : members) {
    auto *name = StrNew(member.name, heap);
    auto *entry =
        name ? MemberAdd(*member.record, name, *member.type, member.offset)
             : nullptr;
    if (!entry) {
      Free(name);
      return false;
    }
    if (member.count) {
      auto *axis =
          reinterpret_cast<CArrayDim *>(CAlloc(sizeof(CArrayDim), heap));
      if (!axis)
        return false;
      axis->cnt = member.count;
      axis->total_cnt = 1;
      entry->dim.next = axis;
      if (member.inner_count) {
        auto *inner =
            reinterpret_cast<CArrayDim *>(CAlloc(sizeof(CArrayDim), heap));
        if (!inner)
          return false;
        inner->cnt = member.inner_count;
        inner->total_cnt = 1;
        axis->next = inner;
        axis->total_cnt = member.inner_count;
      }
      entry->dim.total_cnt = member.count * axis->total_cnt;
      entry->size = member.type->size * entry->dim.total_cnt;
    }
  }
  constexpr struct
  {
    const char *name;
    size_t offset;
    unsigned argc;
  } callbacks[]{
      {"fp_final_scrn_update", offsetof(CGrGlbls, fp_final_scrn_update), 1},
      {"fp_draw_ms", offsetof(CGrGlbls, fp_draw_ms), 3},
      {"fp_draw_grab_ms", offsetof(CGrGlbls, fp_draw_grab_ms), 4},
  };
  const char *args[]{"dc", "x", "y", "closed"};
  CHashClass *types[]{dc + 1, i64, i64, boolean};
  for (const auto &callback : callbacks)
    if (!CmpKernelCallback(table, *glbl, callback.name, callback.offset, *u0,
                           callback.argc, args, types))
      return false;

  const char *transform_args[]{"dc", "_x", "_y", "_z"};
  CHashClass *transform_types[]{dc + 1, i64 + 1, i64 + 1, i64 + 1};
  if (!CmpKernelCallback(table, *dc, "transform", offsetof(CDC, transform),
                         *u0, 4, transform_args, transform_types))
    return false;

  auto *wm = CmpKernelClass(table, "CWinMgrGlbls", sizeof(CWinMgrGlbls));
  if (!wm)
    return false;
  const struct
  {
    const char *name;
    size_t offset;
    CHashClass *type;
  } options[]{
      {"updates", offsetof(CWinMgrGlbls, updates), i64},
      {"fps", offsetof(CWinMgrGlbls, fps), FindType("F64", table)},
      {"last_refresh_tS", offsetof(CWinMgrGlbls, last_refresh_tS),
       FindType("F64", table)},
      {"show_menu", offsetof(CWinMgrGlbls, show_menu), boolean},
      {"grab_scroll", offsetof(CWinMgrGlbls, grab_scroll), boolean},
      {"grab_scroll_closed", offsetof(CWinMgrGlbls, grab_scroll_closed), boolean},
  };
  for (const auto &member : options) {
    auto *name = StrNew(member.name, heap);
    if (!name || !MemberAdd(*wm, name, *member.type, member.offset)) {
      Free(name);
      return false;
    }
  }
  return CmpKernelGlobal(table, "gr", glbl, &gr) &&
         CmpKernelGlobal(table, "winmgr", wm, &winmgr);
}

static bool CmpScrnCastData(CHashTable *table)
{
  auto *au = CmpKernelClass(table, "CAUData", sizeof(CAUData));
  auto *snd = CmpKernelClass(table, "CSndData", sizeof(CSndData));
  auto *sc = CmpKernelClass(table, "CScrnCastGlbls", sizeof(CScrnCastGlbls));
  if (!au || !snd || !sc)
    return false;
  auto *date = FindType("CDate", table);
  auto *f64 = FindType("F64", table);
  auto *i8 = FindType("I8", table);
  auto *boolean = FindType("Bool", table);
  auto *dc = FindType("CDC", table);
  const struct
  {
    CHashClass *record;
    const char *name;
    CHashClass *type;
    size_t offset;
  } fields[]{
      {au, "cdt", date, offsetof(CAUData, cdt)},
      {au, "ona", i8, offsetof(CAUData, ona)},
      {snd, "next", snd + 1, offsetof(CSndData, next)},
      {snd, "last", snd + 1, offsetof(CSndData, last)},
      {snd, "tS", f64, offsetof(CSndData, tS)},
      {snd, "ona", i8, offsetof(CSndData, ona)},
      {sc, "snd_head", snd, offsetof(CScrnCastGlbls, snd_head)},
      {sc, "t0_now", date, offsetof(CScrnCastGlbls, t0_now)},
      {sc, "t0_tS", f64, offsetof(CScrnCastGlbls, t0_tS)},
      {sc, "print_fmt", FindType("U8", table) + 1,
       offsetof(CScrnCastGlbls, print_fmt)},
      {sc, "dc", dc + 1, offsetof(CScrnCastGlbls, dc)},
      {sc, "dc2_alias", dc + 1, offsetof(CScrnCastGlbls, dc2_alias)},
      {sc, "record", boolean, offsetof(CScrnCastGlbls, record)},
      {sc, "just_audio", boolean, offsetof(CScrnCastGlbls, just_audio)},
      {sc, "ona", i8, offsetof(CScrnCastGlbls, ona)},
  };
  auto *heap = CurrentTask()->data_heap;
  for (const auto &field : fields) {
    auto *name = StrNew(field.name, heap);
    if (!name || !MemberAdd(*field.record, name, *field.type, field.offset)) {
      Free(name);
      return false;
    }
  }
  return CmpKernelGlobal(table, "scrncast", sc, &scrncast);
}

static bool CmpMusicData(CHashTable *table)
{
  auto *cls = CmpKernelClass(table, "CMusicGlbls", sizeof(CMusicGlbls));
  if (!cls)
    return false;
  auto *u8 = FindType("U8", table);
  auto *i64 = FindType("I64", table);
  auto *f64 = FindType("F64", table);
  const struct
  {
    const char *name;
    CHashClass *type;
    size_t offset;
  } fields[]{
      {"cur_song", u8 + 1, offsetof(CMusicGlbls, cur_song)},
      {"cur_song_task", FindType("CTask", table) + 1, offsetof(CMusicGlbls, cur_song_task)},
      {"octave", i64, offsetof(CMusicGlbls, octave)},
      {"note_len", f64, offsetof(CMusicGlbls, note_len)},
      {"note_map", u8, offsetof(CMusicGlbls, note_map)},
      {"mute", FindType("Bool", table), offsetof(CMusicGlbls, mute)},
      {"meter_top", i64, offsetof(CMusicGlbls, meter_top)},
      {"meter_bottom", i64, offsetof(CMusicGlbls, meter_bottom)},
      {"tempo", f64, offsetof(CMusicGlbls, tempo)},
      {"stacatto_factor", f64, offsetof(CMusicGlbls, stacatto_factor)},
      {"play_note_num", i64, offsetof(CMusicGlbls, play_note_num)},
      {"tM_correction", f64, offsetof(CMusicGlbls, tM_correction)},
      {"last_Beat", f64, offsetof(CMusicGlbls, last_Beat)},
      {"last_tM", f64, offsetof(CMusicGlbls, last_tM)},
  };
  auto *heap = CurrentTask()->data_heap;
  for (const auto &field : fields) {
    auto *name = StrNew(field.name, heap);
    auto *m = name ? MemberAdd(*cls, name, *field.type, field.offset) : nullptr;
    if (!m) {
      Free(name);
      return false;
    }
    if (field.offset == offsetof(CMusicGlbls, note_map)) {
      auto *axis = reinterpret_cast<CArrayDim *>(CAlloc(sizeof(CArrayDim), heap));
      if (!axis)
        return false;
      axis->cnt = m->dim.total_cnt = m->size = sizeof(music.note_map);
      axis->total_cnt = 1;
      m->dim.next = axis;
    }
  }
  return CmpKernelGlobal(table, "music", cls, &music);
}

static bool CmpTaskSettingsData(CHashTable *table)
{
  auto *cls = CmpKernelClass(table, "CTaskSettings", sizeof(CTaskSettings));
  if (!cls)
    return false;
  auto *heap = CurrentTask()->data_heap;
  auto *task = FindType("CTask", table);
  auto *dc = FindType("CDC", table);
  auto *u0 = FindType("U0", table);
  auto *i64 = FindType("I64", table);
  auto *u8 = FindType("U8", table);
  auto *boolean = FindType("Bool", table);
  const struct
  {
    const char *name;
    CHashClass *record, *type;
    size_t offset;
    size_t count = 0;
  } members[]{
      {"next_settings", task, cls + 1, offsetof(CTask, next_settings)},
      {"next", cls, cls + 1, offsetof(CTaskSettings, next)},
      {"cur_dir", cls, u8 + 1, offsetof(CTaskSettings, cur_dir)},
      {"left", cls, i64, offsetof(CTaskSettings, left)},
      {"right", cls, i64, offsetof(CTaskSettings, right)},
      {"top", cls, i64, offsetof(CTaskSettings, top)},
      {"bottom", cls, i64, offsetof(CTaskSettings, bottom)},
      {"song_task", cls, task + 1, offsetof(CTaskSettings, song_task)},
      {"animate_task", cls, task + 1, offsetof(CTaskSettings, animate_task)},
      {"scroll_x", cls, i64, offsetof(CTaskSettings, scroll_x)},
      {"scroll_y", cls, i64, offsetof(CTaskSettings, scroll_y)},
      {"scroll_z", cls, i64, offsetof(CTaskSettings, scroll_z)},
      {"palette", cls, FindType("CBGR48", table),
       offsetof(CTaskSettings, palette), COLORS_NUM},
      {"win_inhibit", cls, FindType("U32", table),
       offsetof(CTaskSettings, win_inhibit)},
      {"text_attr", cls, u8, offsetof(CTaskSettings, text_attr)},
      {"title_src", cls, u8, offsetof(CTaskSettings, title_src)},
      {"border_attr", cls, u8, offsetof(CTaskSettings, border_attr)},
      {"border_src", cls, u8, offsetof(CTaskSettings, border_src)},
      {"task_title", cls, u8, offsetof(CTaskSettings, task_title),
       sizeof(CTaskSettings::task_title)},
      {"border", cls, boolean, offsetof(CTaskSettings, border)},
      {"hide_cursor", cls, boolean, offsetof(CTaskSettings, hide_cursor)},
      {"highlight_cursor", cls, boolean,
       offsetof(CTaskSettings, highlight_cursor)},
      {"scroll", cls, boolean, offsetof(CTaskSettings, scroll)},
      {"autocomplete", cls, boolean, offsetof(CTaskSettings, autocomplete)},
      {"pad", cls, boolean, offsetof(CTaskSettings, pad), 3},
  };
  for (const auto &member : members) {
    auto *name = StrNew(member.name, heap);
    auto *entry =
        name ? MemberAdd(*member.record, name, *member.type, member.offset)
             : nullptr;
    if (!entry) {
      Free(name);
      return false;
    }
    if (member.count) {
      auto *axis =
          reinterpret_cast<CArrayDim *>(CAlloc(sizeof(CArrayDim), heap));
      if (!axis)
        return false;
      axis->cnt = member.count;
      axis->total_cnt = 1;
      entry->dim.next = axis;
      entry->dim.total_cnt = member.count;
      entry->size = member.type->size * member.count;
    }
  }
  const char *args[]{"task", "dc"};
  CHashClass *types[]{task + 1, dc + 1};
  return CmpKernelCallback(table, *task, "draw_it", offsetof(CTask, draw_it),
                           *u0, 2, args, types) &&
         CmpKernelCallback(table, *task, "task_end_cb",
                           offsetof(CTask, task_end_cb), *u0, 0, nullptr,
                           nullptr) &&
         CmpKernelCallback(table, *cls, "draw_it",
                           offsetof(CTaskSettings, draw_it), *u0, 2, args,
                           types) &&
         CmpKernelCallback(table, *cls, "task_end_cb",
                           offsetof(CTaskSettings, task_end_cb), *u0, 0,
                           nullptr, nullptr);
}

static bool CmpMenuData(CHashTable *table, CHashClass *task)
{
  auto *entry = CmpKernelClass(table, "CMenuEntry", sizeof(CMenuEntry));
  auto *menu = CmpKernelClass(table, "CMenu", sizeof(CMenu));
  if (!entry || !menu)
    return false;
  auto *i64 = FindType("I64", table);
  auto *u8 = FindType("U8", table);
  auto *boolean = FindType("Bool", table);
  auto *heap = CurrentTask()->data_heap;
  struct
  {
    const char *name;
    CHashClass *cls, *type;
    size_t offset, count = 0;
  } members[]{
      {"next", entry, entry + 1, offsetof(CMenuEntry, next)},
      {"sub", entry, entry + 1, offsetof(CMenuEntry, sub)},
      {"name", entry, u8, offsetof(CMenuEntry, name), sizeof(CMenuEntry::name)},
      {"msg_code", entry, i64, offsetof(CMenuEntry, msg_code)},
      {"arg1", entry, i64, offsetof(CMenuEntry, arg1)},
      {"arg2", entry, i64, offsetof(CMenuEntry, arg2)},
      {"checked", entry, boolean, offsetof(CMenuEntry, checked)},
      {"dir", entry, boolean, offsetof(CMenuEntry, dir)},
      {"pad", entry, boolean, offsetof(CMenuEntry, pad), 6},
      {"next", menu, menu + 1, offsetof(CMenu, next)},
      {"sub", menu, entry + 1, offsetof(CMenu, sub)},
      {"task", menu, task + 1, offsetof(CMenu, task)},
      {"flags", menu, i64, offsetof(CMenu, flags)},
      {"attr", menu, u8, offsetof(CMenu, attr)},
      {"pad", menu, u8, offsetof(CMenu, pad), 7},
      {"cur_menu", task, menu + 1, offsetof(CTask, cur_menu)},
  };
  for (const auto &member : members) {
    auto *name = StrNew(member.name, heap);
    auto *field = name ? MemberAdd(*member.cls, name, *member.type, member.offset)
                       : nullptr;
    if (!field) {
      Free(name);
      return false;
    }
    if (member.count) {
      auto *axis =
          reinterpret_cast<CArrayDim *>(CAlloc(sizeof(CArrayDim), heap));
      if (!axis)
        return false;
      axis->cnt = member.count;
      axis->total_cnt = 1;
      field->dim.next = axis;
      field->dim.total_cnt = member.count;
      field->size = member.type->size * member.count;
    }
  }
  return CmpKernelGlobal(table, "sys_cur_submenu_entry", entry + 1,
                          &sys_cur_submenu_entry);
}

static bool CmpCtrlData(CHashTable *table, CHashClass *task)
{
  auto *ctrl = CmpKernelClass(table, "CCtrl", sizeof(CCtrl));
  auto *scroll = CmpKernelClass(table, "CWinScroll", sizeof(CWinScroll));
  auto *view = CmpKernelClass(table, "CViewAngles", sizeof(CViewAngles));
  if (!ctrl || !scroll || !view)
    return false;
  auto *heap = CurrentTask()->data_heap;
  auto *i64 = FindType("I64", table);
  auto *u32 = FindType("U32", table);
  auto *u8 = FindType("U8", table);
  auto *u0 = FindType("U0", table);
  auto *boolean = FindType("Bool", table);
  auto *dc = FindType("CDC", table);
  auto *f64 = FindType("F64", table);
  auto *color_rop = FindType("CColorROPU32", table);
  struct
  {
    const char *name;
    CHashClass *record, *type;
    size_t offset, count = 0;
  } members[]{
      {"next", ctrl, ctrl + 1, offsetof(CCtrl, next)},
      {"last", ctrl, ctrl + 1, offsetof(CCtrl, last)},
      {"win_task", ctrl, task + 1, offsetof(CCtrl, win_task)},
      {"type", ctrl, i64, offsetof(CCtrl, type)},
      {"flags", ctrl, i64, offsetof(CCtrl, flags)},
      {"left", ctrl, i64, offsetof(CCtrl, left)},
      {"right", ctrl, i64, offsetof(CCtrl, right)},
      {"top", ctrl, i64, offsetof(CCtrl, top)},
      {"bottom", ctrl, i64, offsetof(CCtrl, bottom)},
      {"scrn_left", ctrl, i64, offsetof(CCtrl, scrn_left)},
      {"scrn_right", ctrl, i64, offsetof(CCtrl, scrn_right)},
      {"scrn_top", ctrl, i64, offsetof(CCtrl, scrn_top)},
      {"scrn_bottom", ctrl, i64, offsetof(CCtrl, scrn_bottom)},
      {"state", ctrl, u8 + 1, offsetof(CCtrl, state)},
      {"min", scroll, i64, offsetof(CWinScroll, min)},
      {"pos", scroll, i64, offsetof(CWinScroll, pos)},
      {"max", scroll, i64, offsetof(CWinScroll, max)},
      {"flags", scroll, u32, offsetof(CWinScroll, flags)},
      {"color", scroll, u8, offsetof(CWinScroll, color)},
      {"pad", scroll, u8, offsetof(CWinScroll, pad), 3},
      {"sx", view, i64, offsetof(CViewAngles, sx)},
      {"sy", view, i64, offsetof(CViewAngles, sy)},
      {"sz", view, i64, offsetof(CViewAngles, sz)},
      {"ax", view, f64, offsetof(CViewAngles, ax)},
      {"ay", view, f64, offsetof(CViewAngles, ay)},
      {"az", view, f64, offsetof(CViewAngles, az)},
      {"cx", view, color_rop, offsetof(CViewAngles, cx)},
      {"cy", view, color_rop, offsetof(CViewAngles, cy)},
      {"cz", view, color_rop, offsetof(CViewAngles, cz)},
      {"cbd", view, color_rop, offsetof(CViewAngles, cbd)},
      {"cbg", view, color_rop, offsetof(CViewAngles, cbg)},
      {"cfg", view, color_rop, offsetof(CViewAngles, cfg)},
      {"next_ctrl", task, ctrl + 1, offsetof(CTask, next_ctrl)},
      {"last_ctrl", task, ctrl + 1, offsetof(CTask, last_ctrl)},
      {"horz_scroll", task, scroll, offsetof(CTask, horz_scroll)},
      {"vert_scroll", task, scroll, offsetof(CTask, vert_scroll)},
  };
  for (const auto &member : members) {
    auto *name = StrNew(member.name, heap);
    auto *field = name ? MemberAdd(*member.record, name, *member.type, member.offset)
                       : nullptr;
    if (!field) {
      Free(name);
      return false;
    }
    if (member.count) {
      auto *axis = reinterpret_cast<CArrayDim *>(CAlloc(sizeof(CArrayDim), heap));
      if (!axis)
        return false;
      axis->cnt = member.count;
      axis->total_cnt = 1;
      field->dim.next = axis;
      field->dim.total_cnt = member.count;
      field->size = member.type->size * member.count;
    }
  }
  const char *coords[]{"c", "x", "y", "down"};
  CHashClass *coord_types[]{ctrl + 1, i64, i64, boolean};
  const char *draw_args[]{"dc", "c"};
  CHashClass *draw_types[]{dc + 1, ctrl + 1};
  const char *wheel_args[]{"c", "delta"};
  CHashClass *wheel_types[]{ctrl + 1, i64};
  return CmpKernelCallback(table, *ctrl, "update_derived_vals",
                           offsetof(CCtrl, update_derived_vals), *u0, 1,
                           coords, coord_types) &&
         CmpKernelCallback(table, *ctrl, "draw_it", offsetof(CCtrl, draw_it),
                           *u0, 2, draw_args, draw_types) &&
         CmpKernelCallback(table, *ctrl, "inside_ctrl",
                           offsetof(CCtrl, inside_ctrl), *boolean, 3,
                           coords, coord_types) &&
         CmpKernelCallback(table, *ctrl, "left_click",
                           offsetof(CCtrl, left_click), *u0, 4,
                           coords, coord_types) &&
         CmpKernelCallback(table, *ctrl, "right_click",
                           offsetof(CCtrl, right_click), *u0, 4,
                           coords, coord_types) &&
         CmpKernelCallback(table, *ctrl, "wheel_chg", offsetof(CCtrl, wheel_chg),
                           *u0, 2, wheel_args, wheel_types);
}

static bool CmpClassReportData(CHashTable *table, CHashClass &task)
{
  auto *cpu = CmpKernelClass(table, "CCPU", sizeof(CCPU));
  auto *dying = CmpKernelClass(table, "CTaskDying", sizeof(CTaskDying));
  auto *tss = CmpKernelClass(table, "CTSS", 0);
  if (!cpu || !dying || !tss)
    return false;
  tss->flags |= CF_EXTERN;
  auto *heap = CurrentTask()->data_heap;
  auto *i64 = FindType("I64", table);
  auto *f64 = FindType("F64", table);
  auto *u8 = FindType("U8", table);
  auto *u16 = FindType("U16", table);
  auto *boolean = FindType("Bool", table);
  auto *u0 = FindType("U0", table);
  const struct {
    const char *name;
    CHashClass *cls, *type;
    size_t offset, count = 0;
  } fields[]{
      {"addr", cpu, cpu + 1, offsetof(CCPU, addr)},
      {"num", cpu, i64, offsetof(CCPU, num)},
      {"cpu_flags", cpu, i64, offsetof(CCPU, cpu_flags)},
      {"startup_rip", cpu, i64, offsetof(CCPU, startup_rip)},
      {"idle_pt_hits", cpu, i64, offsetof(CCPU, idle_pt_hits)},
      {"idle_factor", cpu, f64, offsetof(CCPU, idle_factor)},
      {"total_jiffies", cpu, i64, offsetof(CCPU, total_jiffies)},
      {"seth_task", cpu, &task + 1, offsetof(CCPU, seth_task)},
      {"idle_task", cpu, &task + 1, offsetof(CCPU, idle_task)},
      {"tr", cpu, i64, offsetof(CCPU, tr)},
      {"swap_cnter", cpu, i64, offsetof(CCPU, swap_cnter)},
      {"next_dying", cpu, dying + 1, offsetof(CCPU, next_dying)},
      {"last_dying", cpu, dying + 1, offsetof(CCPU, last_dying)},
      {"kill_jiffy", cpu, i64, offsetof(CCPU, kill_jiffy)},
      {"tss", cpu, tss + 1, offsetof(CCPU, tss)},
      {"start_stk", cpu, i64, offsetof(CCPU, start_stk), 16},
      {"next", dying, &task + 1, offsetof(CTaskDying, next)},
      {"last", dying, &task + 1, offsetof(CTaskDying, last)},
      {"wake_jiffy", dying, i64, offsetof(CTaskDying, wake_jiffy)},
      {"gs", &task, cpu + 1, offsetof(CTask, gs)},
      {"next_input_filter_task", &task, &task + 1, offsetof(CTask, next_input_filter_task)},
      {"last_input_filter_task", &task, &task + 1, offsetof(CTask, last_input_filter_task)},
      {"next_sibling_task", &task, &task + 1, offsetof(CTask, next_sibling_task)},
      {"last_sibling_task", &task, &task + 1, offsetof(CTask, last_sibling_task)},
      {"rip", &task, i64, offsetof(CTask, rip)},
      {"rflags", &task, i64, offsetof(CTask, rflags)},
      {"rsp", &task, i64, offsetof(CTask, rsp)},
      {"rsi", &task, i64, offsetof(CTask, rsi)},
      {"rax", &task, i64, offsetof(CTask, rax)},
      {"rcx", &task, i64, offsetof(CTask, rcx)},
      {"rdx", &task, i64, offsetof(CTask, rdx)},
      {"rbx", &task, i64, offsetof(CTask, rbx)},
      {"rbp", &task, i64, offsetof(CTask, rbp)},
      {"rdi", &task, i64, offsetof(CTask, rdi)},
      {"r8", &task, i64, offsetof(CTask, r8)},
      {"r9", &task, i64, offsetof(CTask, r9)},
      {"r10", &task, i64, offsetof(CTask, r10)},
      {"r11", &task, i64, offsetof(CTask, r11)},
      {"r12", &task, i64, offsetof(CTask, r12)},
      {"r13", &task, i64, offsetof(CTask, r13)},
      {"r14", &task, i64, offsetof(CTask, r14)},
      {"r15", &task, i64, offsetof(CTask, r15)},
      {"task_name", &task, u8, offsetof(CTask, task_name), sizeof(CTask::task_name)},
      {"wallpaper_data", &task, u8, offsetof(CTask, wallpaper_data), sizeof(CTask::wallpaper_data)},
      {"pad", &task, u16, offsetof(CTask, pad)},
      {"new_answer", &task, boolean, offsetof(CTask, new_answer)},
      {"answer_type", &task, u8, offsetof(CTask, answer_type)},
      {"answer", &task, i64, offsetof(CTask, answer)},
      {"answer_time", &task, f64, offsetof(CTask, answer_time)},
      {"last_fun", &task, FindType("CHashFun", table) + 1, offsetof(CTask, last_fun)},
      {"last_ode_time", &task, f64, offsetof(CTask, last_ode_time)},
      {"fault_num", &task, i64, offsetof(CTask, fault_num)},
      {"fault_err_code", &task, i64, offsetof(CTask, fault_err_code)},
      {"popup_task", &task, &task + 1, offsetof(CTask, popup_task)},
      {"dbg_task", &task, &task + 1, offsetof(CTask, dbg_task)},
  };
  for (const auto &field : fields) {
    auto *name = StrNew(field.name, heap);
    auto *member = name ? MemberAdd(*field.cls, name, *field.type, field.offset) : nullptr;
    if (!member) {
      Free(name);
      return false;
    }
    if (field.count) {
      auto *dim = reinterpret_cast<CArrayDim *>(CAlloc(sizeof(CArrayDim), heap));
      if (!dim)
        return false;
      dim->cnt = field.count;
      dim->total_cnt = 1;
      member->dim = {dim, 1, int64_t(field.count)};
      member->size = field.type->size * field.count;
    }
  }
  const char *names[]{"task"};
  CHashClass *types[]{&task + 1};
  if (!CmpKernelCallback(table, *cpu, "profiler_timer_irq",
                         offsetof(CCPU, profiler_timer_irq), *u0, 1, names, types))
    return false;
  // Keep these native records in storage order, including fields added by
  // the separate kernel header initializers. Rebuild the lookup tree.
  CHashClass *records[]{&task, cpu};
  for (auto *cls : records) {
    CMemberLst *sorted = nullptr;
    for (auto *m = cls->member_lst_and_root; m;) {
      auto *next = m->next;
      auto **link = &sorted;
      while (*link && (*link)->offset <= m->offset)
        link = &(*link)->next;
      m->next = *link;
      *link = m;
      m = next;
    }
    cls->member_lst_and_root = sorted;
    for (auto *m = sorted; m; m = m->next) {
      m->left = m->right = nullptr;
      if (m != sorted) {
        auto **link = &cls->member_lst_and_root;
        while (*link) {
          auto cmp = StrCmpCpp(reinterpret_cast<const uint8_t *>((*link)->str),
                               reinterpret_cast<const uint8_t *>(m->str));
          link = cmp < 0 ? &(*link)->left : &(*link)->right;
        }
        *link = m;
      }
      cls->last_in_member_lst = m;
    }
  }
  return true;
}

static bool CmpRuntimeHeaders(CHashTable *table)
{
  enum : uint8_t
  {
    U0,
    U64,
    I64,
    I8,
    U8,
    F64,
    Bool,
    U8Ptr,
    TaskPtr,
    JobPtr,
    I64Ptr,
    BGR48,
    BGR48Ptr,
    DCPtr,
    I32Ptr,
    ColorROP16,
    ColorROP,
    D3I32Ptr,
    I16,
    U16,
    I32,
    U32,
    DocPtr,
    DocEntryPtr,
    DocEntryPtrPtr,
    Date,
    DirEntryPtr,
    DirContextPtr,
    U8PtrPtr,
    TaskSettingsPtr,
    D3I64Ptr,
    MenuPtr,
    MenuEntryPtr,
    CtrlPtr,
    FilePtr,
    U32Ptr,
    HashPtr,
    HashTablePtr,
    HashClassPtr,
    MemberPtr,
    MemberMetaPtr,
    DateStructPtr,
    U16Ptr,
    DrivePtr,
    QuePtr,
    CpuPtr,
    DefinePtr,
    BlkDevPtr,
    DrvPtr,
    SndPtr,
    DatePtr,
    FifoU8Ptr,
    FifoI64Ptr,
    BinFilePtr,
    DirEntryPtrPtr,
    PlotFn,
  };
  CHashClass *types[]{
      FindType("U0", table),     FindType("U64", table),
      FindType("I64", table),    FindType("I8", table),
      FindType("U8", table),     FindType("F64", table),    FindType("Bool", table),
      FindType("U8", table) + 1, FindType("CTask", table) + 1,
      FindType("CJob", table) + 1, FindType("I64", table) + 1,
      FindType("CBGR48", table), FindType("CBGR48", table) + 1,
      FindType("CDC", table) + 1, FindType("I32", table) + 1,
      FindType("CColorROPU16", table),
      FindType("CColorROPU32", table), FindType("CD3I32", table) + 1,
      FindType("I16", table), FindType("U16", table),
      FindType("I32", table), FindType("U32", table),
      FindType("CDoc", table) + 1, FindType("CDocEntry", table) + 1,
      FindType("CDocEntry", table) + 2,
      FindType("CDate", table),
      FindType("CDirEntry", table) + 1,
      FindType("CDirContext", table) + 1,
      FindType("U8", table) + 2,
      FindType("CTaskSettings", table) + 1,
      FindType("CD3I64", table) + 1,
      FindType("CMenu", table) + 1,
      FindType("CMenuEntry", table) + 1,
      FindType("CCtrl", table) + 1,
      FindType("CFile", table) + 1,
      FindType("U32", table) + 1,
      FindType("CHash", table) + 1,
      FindType("CHashTable", table) + 1,
      FindType("CHashClass", table) + 1,
      FindType("CMemberLst", table) + 1,
      FindType("CMemberLstMeta", table) + 1,
      FindType("CDateStruct", table) + 1,
      FindType("U16", table) + 1,
      FindType("CDrv", table) + 1,
      FindType("CQue", table) + 1,
      FindType("CCPU", table) + 1,
      FindType("CHashDefineStr", table) + 1,
      FindType("CBlkDev", table) + 1,
      FindType("CDrv", table) + 1,
      FindType("CSndData", table) + 1,
      FindType("CDate", table) + 1,
      FindType("CFifoU8", table) + 1,
      FindType("CFifoI64", table) + 1,
      FindType("CBinFile", table) + 1,
      FindType("CDirEntry", table) + 2,
  };
  auto *task = CurrentTask();
  enum CDefaultKind : uint8_t { Numeric, String, LastClass };
  struct [[gnu::packed]] CHeaderArg
  {
    const char *name = nullptr;
    uint8_t type = 0;
    bool has_default = false;
    CDefaultKind kind = Numeric;
    union [[gnu::packed]] {
      uint64_t dft_val;
      const char *dft_str;
    };

    constexpr CHeaderArg() : dft_val(0) {}
    constexpr CHeaderArg(const char *name, uint8_t type, bool has_default = false,
                          uint64_t value = 0)
        : name(name), type(type), has_default(has_default), dft_val(value) {}
    constexpr CHeaderArg(const char *name, uint8_t type, bool has_default,
                          uint64_t, const char *str)
        : name(name), type(type), has_default(has_default), kind(String),
          dft_str(str) {}
    constexpr CHeaderArg(const char *name, uint8_t type, CDefaultKind kind)
        : name(name), type(type), has_default(true), kind(kind), dft_val(0) {}
  };
  static_assert(sizeof(CHeaderArg) == 19);
  static constexpr struct
  {
    const char *name;
    uint8_t result;
    uint16_t flags;
    // Private compiler metadata stays in the DVD-loaded boot image.
    CHeaderArg args[12];
    const char *import_name = nullptr;
    uint8_t internal_op = 0;
  } headers[]{
      {"Print", U0, FF_DOT_DOT_DOT, {{"fmt", U8Ptr}}},
      {"MStrPrint", U8Ptr, FF_DOT_DOT_DOT, {{"fmt", U8Ptr}}},
      {"StrPrint", U8Ptr, FF_DOT_DOT_DOT, {{"dst", U8Ptr}, {"fmt", U8Ptr}}},
      {"CatPrint", U8Ptr, FF_DOT_DOT_DOT, {{"dst", U8Ptr}, {"fmt", U8Ptr}}},
      {"CAlloc", U8Ptr, FF_RET1, {{"size", I64}, {"mem_task", TaskPtr, true}}},
      {"MAlloc", U8Ptr, FF_RET1, {{"size", I64}, {"mem_task", TaskPtr, true}}, "_MALLOC"},
      {"Now", Date, FF_RET1, {}},
      {"Struct2Date", Date, FF_RET1, {{"_ds", DateStructPtr}}},
      {"Date2Struct", U0, FF_RET1, {{"_ds", DateStructPtr}, {"cdt", Date}}},
      {"YearStartDate", I64, FF_RET1, {{"year", I64}}},
      {"DayOfWeek", I64, FF_RET1, {{"i", I64}}},
      {"FirstDayOfMon", I64, FF_RET1, {{"i", I64}}},
      {"LastDayOfMon", I64, FF_RET1, {{"i", I64}}},
      {"FirstDayOfYear", I64, FF_RET1, {{"i", I64}}},
      {"LastDayOfYear", I64, FF_RET1, {{"i", I64}}},
      {"Cd", Bool, FF_RET1, {{"dirname", U8Ptr, true}, {"make_dirs", Bool, true}}},
      {"IsDir", Bool, FF_RET1, {{"dir_name", U8Ptr}}},
      {"Dir", I64, FF_RET1, {{"files_find_mask", U8Ptr, true, 0, "*"}, {"full", Bool, true}}},
      {"DirCur", U8Ptr, FF_RET1, {{"task", TaskPtr, true}, {"mem_task", TaskPtr, true}}},
      {"DirNameAbs", U8Ptr, FF_RET1, {{"dirname", U8Ptr}}},
      {"FileNameAbs", U8Ptr, FF_RET1, {{"filename", U8Ptr}, {"fuf_flags", I64, true}}},
      {"DirFile", U8Ptr, FF_RET1, {{"dirname", U8Ptr}, {"name", U8Ptr, true}, {"_extension", U8Ptr, true}}},
      {"ExtChg", U8Ptr, FF_RET1, {{"filename", U8Ptr}, {"extension", U8Ptr}}},
      {"ExtDft", U8Ptr, FF_RET1, {{"filename", U8Ptr}, {"extension", U8Ptr}}},
      {"FileExtDot", U8Ptr, FF_RET1, {{"src", U8Ptr}}},
      {"FileExtRem", U8Ptr, FF_RET1, {{"src", U8Ptr}, {"dst", U8Ptr, true}}},
      {"IsDotZ", Bool, FF_RET1, {{"filename", U8Ptr}}},
      {"IsDotC", Bool, FF_RET1, {{"filename", U8Ptr}}},
      {"FileNameChk", Bool, FF_RET1, {{"filename", U8Ptr}}},
      {"CFileNameTo", Bool, FF_RET1, {{"dst", U8Ptr}, {"src", U8Ptr}}},
      {"ToggleZorNotZ", U8Ptr, FF_RET1, {{"name", U8Ptr}}},
      {"FileNameTmpTxt", U8Ptr, FF_RET1, {}},
      {"Name2DirClus", I64, FF_RET1, {{"dv", DrvPtr}, {"dirname", U8Ptr}}},
      {"Name2ParentDirClus", I64, FF_RET1, {{"dv", DrvPtr}, {"dirname", U8Ptr}}},
      {"Cd2DirEntry", DirEntryPtr, FF_RET1, {{"tmpde", DirEntryPtr}, {"abs_name", U8Ptr}}},
      {"PutDirLink", U0, FF_RET1, {{"dirname", U8Ptr}, {"full_name", U8Ptr, true}}},
      {"PutFileLink", U0, FF_RET1, {{"filename", U8Ptr}, {"full_name", U8Ptr, true}, {"line", I64, true}, {"plain_text", Bool, true}}},
      {"FilesFind", DirEntryPtr, FF_RET1, {{"files_find_mask", U8Ptr}, {"fuf_flags", I64, true}}},
      {"FileFind", Bool, FF_RET1,
       {{"filename", U8Ptr}, {"_de", DirEntryPtr, true}, {"fuf_flags", I64, true}}},
      {"Load", U8Ptr, FF_RET1,
       {{"filename", U8Ptr}, {"ld_flags", I64, true},
        {"bfh_addr", BinFilePtr, true, INVALID_PTR}}},
      {"FileRead", U8Ptr, FF_RET1,
       {{"filename", U8Ptr}, {"_size", I64Ptr, true}, {"_attr", I64Ptr, true}}},
      {"FileWrite", I64, FF_RET1,
       {{"filename", U8Ptr}, {"fbuf", U8Ptr}, {"size", I64}, {"cdt", Date, true}, {"attr", I64, true}}},
      {"Let2Drv", DrivePtr, FF_RET1, {{"drv_let", U8, true}, {"except", Bool, true, 1}}},
      {"DrvLock", Bool, FF_RET1, {{"dv", DrvPtr}}},
      {"DrvUnlock", Bool, FF_RET1, {{"dv", DrvPtr}, {"rst", Bool, true}}},
      {"DrvsRelease", U0, FF_RET1, {}},
      {"BlkDevLock", Bool, FF_RET1, {{"bd", BlkDevPtr}}},
      {"BlkDevUnlock", Bool, FF_RET1, {{"bd", BlkDevPtr}, {"rst", Bool, true}}},
      {"BlkDevInit", Bool, FF_RET1, {{"bd", BlkDevPtr}}},
      {"BlkDevsRelease", U0, FF_RET1, {}},
      {"DskCacheInit", U0, FF_RET1, {{"size_in_U8s", I64}}},
      {"DskCacheAdd", U0, FF_RET1, {{"dv", DrvPtr}, {"buf", U8Ptr}, {"blk", I64}, {"cnt", I64}}},
      {"DskCacheInvalidate2", U0, FF_RET1, {{"dv", DrvPtr}}},
      {"RCache", U0, FF_RET1, {{"dv", DrvPtr}, {"_buf", U8PtrPtr}, {"_blk", I64Ptr}, {"_cnt", I64Ptr}}},
      {"DrvMakeFreeSlot", DrvPtr, FF_RET1, {{"drv_let", U8}}},
      {"DrvNextFreeLet", U8, FF_RET1, {{"first_drv_let", U8, true, 'C'}}},
      {"DrvChk", DrvPtr, FF_RET1, {{"dv", DrvPtr}, {"except", Bool, true, 1}}},
      {"DrvFATBlkAlloc", U0, FF_RET1, {{"dv", DrvPtr}}},
      {"DrvEnable", Bool, FF_RET1, {{"drv_let", U8}, {"val", Bool}}},
      {"DrvDel", U0, FF_RET1, {{"dv", DrvPtr}}},
      {"DrvBlkDevDel", U0, FF_RET1, {{"bd", BlkDevPtr}}},
      {"BlkDevDel", U0, FF_RET1, {{"bd", BlkDevPtr}}},
      {"DskCacheInvalidate", U0, FF_RET1, {{"dv", DrvPtr}}},
      {"DskChg", U0, FF_RET1, {{"drv_let", U8, true}}},
      {"DrvFATBlkClean", U0, FF_RET1, {{"dv", DrvPtr}, {"fat_sel", I64, true, 3}}},
      {"DrvFATBlkSet", U0, FF_RET1,
       {{"dv", DrvPtr}, {"c", I64}, {"fat_sel", I64, true, 3}}},
      {"Drv2Let", U8, FF_RET1, {{"dv", DrvPtr, true}}},
      {"Let2Let", U8, FF_RET1, {{"drv_let", U8, true}}},
      {"Let2BlkDevType", I64, FF_RET1, {{"drv_let", U8}}},
      {"DrvIsWritable", BlkDevPtr, FF_RET1, {{"drv_let", U8, true}, {"except", Bool, true}}},
      {"DrvMap", Bool, FF_RET1, {{"drv_let", U8}, {"dv", DrvPtr}}},
      {"Drv", Bool, FF_RET1, {{"drv_let", U8}}},
      {"DrvSerialNum", U8Ptr, FF_RET1, {{"drv_let", U8, true}}},
      {"DrvModelNum", U8Ptr, FF_RET1, {{"drv_let", U8, true}}},
      {"DrvTextAttrGet", U8, FF_RET1, {{"drv_let", U8, true}}},
      {"DrvRep", U0, FF_RET1, {}},
      {"BlkDevChk", BlkDevPtr, FF_RET1, {{"bd", BlkDevPtr}, {"except", Bool, true, 1}}},
      {"Let2BlkDev", BlkDevPtr, FF_RET1, {{"drv_let", U8, true}, {"except", Bool, true, 1}}},
      {"Unmount", U0, FF_RET1, {{"drv_let", U8, true}}},
      {"MountFile", U8, FF_RET1, {{"filename", U8Ptr}}},
      {"BlkRead", Bool, FF_RET1, {{"dv", DrivePtr}, {"buf", U8Ptr}, {"blk", I64}, {"cnt", I64}}},
      {"Fmt", U0, FF_RET1, {{"drv_let", U8}, {"quick", Bool, true, 1},
                     {"confirm", Bool, true, 1}, {"type", I64, true, FSt_FAT32}}},
      {"BlkWrite", Bool, FF_RET1, {{"dv", DrivePtr}, {"buf", U8Ptr}, {"blk", I64}, {"cnt", I64}}},
      {"ClusNumNext", I64, FF_RET1, {{"dv", DrvPtr}, {"c", I64}, {"cnt", I64, true, 1}}},
      {"Clus2Blk", I64, FF_RET1, {{"dv", DrvPtr}, {"c", I64}}},
      {"ClusBlkRead", I64, FF_RET1, {{"dv", DrvPtr}, {"buf", U8Ptr}, {"c", I64}, {"blks", I64}}},
      {"ClusRead", I64, FF_RET1, {{"dv", DrvPtr}, {"buf", U8Ptr}, {"c", I64}, {"cnt", I64}}},
      {"ClusBlkWrite", I64, FF_RET1, {{"dv", DrvPtr}, {"buf", U8Ptr}, {"c", I64}, {"blks", I64}}},
      {"ClusWrite", I64, FF_RET1, {{"dv", DrvPtr}, {"buf", U8Ptr}, {"c", I64}, {"cnt", I64}}},
      {"ClusAlloc", I64, FF_RET1, {{"dv", DrvPtr}, {"c", I64, true}, {"cnt", I64, true, 1}, {"contiguous", Bool, true}}},
      {"FAT32AllocClus", I64, FF_RET1, {{"dv", DrvPtr}, {"c", I64}, {"cnt", I64}}},
      {"FAT32AllocContiguousClus", I64, FF_RET1, {{"dv", DrvPtr}, {"cnt", I64}}},
      {"FAT32FreeClus", U0, FF_RET1, {{"dv", DrvPtr}, {"c", I64}}},
      {"RedSeaAllocClus", I64, FF_RET1, {{"dv", DrvPtr}, {"cnt", I64}}},
      {"RedSeaFreeClus", U0, FF_RET1, {{"dv", DrvPtr}, {"c", I64}, {"cnt", I64}}},
      {"RedSeaFreeFreeLst", U0, FF_RET1, {{"dv", DrvPtr}}},
      {"RedSeaFreeLstBuild", U0, FF_RET1, {{"dv", DrvPtr}}},
      {"RedSeaValidate", Bool, FF_RET1, {{"drv_let", U8}}},
      {"RedSeaFmt", U0, FF_RET1, {{"drv_let", U8}, {"quick", Bool, true, 1}}},
      {"FAT32DirNew", Bool, FF_RET1, {{"dv", DrvPtr}, {"cur_dir", U8Ptr}, {"tmpde", DirEntryPtr}, {"free_old_chain", Bool, true, 1}}},
      {"RedSeaDirNew", Bool, FF_RET1, {{"dv", DrvPtr}, {"cur_dir", U8Ptr}, {"tmpde", DirEntryPtr}, {"free_old_chain", Bool, true, 1}}},
      {"DirNew", Bool, FF_RET1, {{"dv", DrvPtr}, {"cur_dir", U8Ptr}, {"tmpde", DirEntryPtr}, {"free_old_chain", Bool, true, 1}}},
      {"FSize", I64, FF_RET1, {{"f", FilePtr}}},
      {"BlkDevNextFreeSlot", BlkDevPtr, FF_RET1, {{"first_drv_let", U8}, {"type", I64}}},
      {"BlkDevAdd", I64, FF_RET1, {{"bd", BlkDevPtr}, {"prt_num", I64, true, 0x8000000000000000ULL},
                                    {"whole_drv", Bool}, {"make_free", Bool}}},
      {"FOpen", FilePtr, FF_RET1, {{"filename", U8Ptr}, {"flags", U8Ptr}, {"cnt", I64, true}}},
      {"FClose", U0, FF_RET1, {{"f", FilePtr}}},
      {"FBlkRead", Bool, FF_RET1,
       {{"f", FilePtr}, {"buf", U8Ptr}, {"blk", I64, true, FFB_NEXT_BLK}, {"cnt", I64, true, 1}}},
      {"FBlkWrite", Bool, FF_RET1,
       {{"f", FilePtr}, {"buf", U8Ptr}, {"blk", I64, true, FFB_NEXT_BLK}, {"cnt", I64, true, 1}}},
      {"FileAttr", I64, FF_RET1, {{"name", U8Ptr}, {"old_attr", I64, true}}},
      {"CopySingle", Bool, FF_RET1, {{"f1", U8Ptr}, {"f2", U8Ptr}}},
      {"Del", I64, FF_RET1, {{"files_find_mask", U8Ptr}, {"make_mask", Bool, true}, {"del_dir", Bool, true}, {"print_msg", Bool, true, 1}}},
      {"Copy", Bool, FF_RET1, {{"src_files_find_mask", U8Ptr}, {"dst_files_find_mask", U8Ptr, true, 0, "."}}},
      {"Move", Bool, FF_RET1, {{"f1", U8Ptr}, {"f2", U8Ptr}}},
      {"CopyTree", I64, FF_RET1, {{"src_files_find_mask", U8Ptr}, {"dst_files_find_mask", U8Ptr}, {"no_mask", Bool, true, 1}}},
      {"DelTree", I64, FF_RET1, {{"files_find_mask", U8Ptr}, {"fu_flags", U8Ptr, true}}},
      {"DirMk", Bool, FF_RET1, {{"filename", U8Ptr}, {"entry_cnt", I64, true}}},
      {"ScanFlags", U0, FF_RET1, {{"_dst_flags", U8Ptr}, {"lst", U8Ptr}, {"src", U8Ptr}}},
      {"FilesFindMatch", Bool, FF_RET1,
       {{"_test_name", U8Ptr}, {"files_find_mask", U8Ptr}, {"fuf_flags", I64, true}}},
      {"DirEntryDel", U0, FF_RET1, {{"tmpde", DirEntryPtr}}},
      {"DirEntryDel2", U0, FF_RET1, {{"tmpde", DirEntryPtr}}},
      {"DirTreeDel", U0, FF_RET1, {{"tmpde", DirEntryPtr}}},
      {"DirTreeDel2", U0, FF_RET1, {{"tmpde", DirEntryPtr}}},
      {"DirEntryCompareName", I64, FF_RET1, {{"e1", DirEntryPtr}, {"e2", DirEntryPtr}}},
      {"DirEntryCompareClus", I64, FF_RET1, {{"e1", DirEntryPtr}, {"e2", DirEntryPtr}}},
      {"DirFilesSort", U0, FF_RET1, {{"_tmpde", DirEntryPtrPtr}, {"key", I64}}},
      {"DirFilesFlatten", DirEntryPtr, FF_RET1, {{"tmpde", DirEntryPtr}, {"_res", DirEntryPtrPtr}, {"fuf_flags", I64}}},
      {"FilesFind2", DirEntryPtr, FF_RET1, {{"files_find_mask", U8Ptr}, {"fuf_flags", I64}}},
      {"DirContextNew", DirContextPtr, FF_RET1,
       {{"mask", U8Ptr}, {"make_mask", Bool, true}, {"make_dirs", Bool, true}, {"no_mask", Bool, true}}},
      {"DirContextDel", U0, FF_RET1, {{"dirc", DirContextPtr}, {"restore", Bool, true, 1}}},
      {"Str2I64", I64, FF_RET1, {{"st", U8Ptr}, {"radix", I64, true, 10}, {"_end_ptr", U8PtrPtr, true}}},
      {"Str2F64", F64, FF_RET1, {{"src", U8Ptr}, {"_end_ptr", U8PtrPtr, true}}},
      {"Str2Date", Date, FF_RET1, {{"src", U8Ptr}}},
      {"StrScan", U8Ptr, FF_DOT_DOT_DOT, {{"src", U8Ptr}, {"fmt", U8Ptr}}},
      {"SysTry", U0, FF_RET1, {{"catch_start", U8Ptr}, {"untry_start", U8Ptr}}},
      {"SysUntry", U0, FF_RET1, {}},
      {"throw", U0, FF_RET1, {{"ch", I64, true}, {"no_log", Bool, true}}},
      {"Caller", U8Ptr, FF_RET1, {{"num", I64, true, 1}}},
      {"PutExcept", U0, FF_RET1, {{"catch_it", Bool, true, 1}}},
      {"Break", U0, FF_RET1, {}},
      {"BreakLock", Bool, FF_RET1, {{"task", TaskPtr, true}}},
      {"BreakUnlock", Bool, FF_RET1, {{"task", TaskPtr, true}}},
      {"YorN", Bool, FF_RET1, {}},
      {"Free", U0, FF_RET1, {{"addr", U8Ptr}}, "_FREE"},
      {"MSize", I64, FF_RET1, {{"src", U8Ptr}}, "_MSIZE"},
      {"MSize2", I64, FF_RET1, {{"src", U8Ptr}}, "_MSIZE2"},
      {"HashFind", HashPtr, FF_RET1,
       {{"needle_str", U8Ptr}, {"haystack_table", HashTablePtr}, {"mask", I64},
        {"instance", I64, true, 1}}, "_HASH_FIND"},
      {"MemberFind", MemberPtr, FF_RET1,
       {{"needle_str", U8Ptr}, {"haystack_class", HashClassPtr}}},
      {"U", U8Ptr, FF_RET1,
       {{"rip", U8Ptr}, {"cnt", I64, true, 20}, {"seg_size", I64, true, 64}}},
      {"Ui", U0, FF_RET1,
       {{"buf", U8Ptr}, {"_rip", U8PtrPtr}, {"seg_size", I64, true, 64},
        {"_jmp_dst", I64Ptr, true}, {"just_ins", Bool, true}}},
      {"Un", I64, FF_RET1,
       {{"rip", U8Ptr}, {"cnt", I64, true, 0x80}, {"seg_size", I64, true, 64}}},
      {"MemberMetaData", I64, FF_RET1,
       {{"needle_str", U8Ptr}, {"haystack_member_lst", MemberPtr}}},
      {"MemberMetaFind", MemberMetaPtr, FF_RET1,
       {{"needle_str", U8Ptr}, {"haystack_member_lst", MemberPtr}}},
      {"StrNew", U8Ptr, FF_RET1,
       {{"buf", U8Ptr}, {"mem_task", TaskPtr, true}}},
      {"MStrUtil", U8Ptr, FF_RET1,
       {{"src", U8Ptr}, {"flags", I64}, {"indent_scale_factor", F64, true}}},
      {"ScaleIndent", U8Ptr, FF_RET1,
       {{"src", U8Ptr}, {"indent_scale_factor", F64}}},
      {"Spaces2Tabs", I64, FF_RET1, {{"dst", U8Ptr}, {"src", U8Ptr}}},
      {"StrFind", U8Ptr, FF_RET1,
       {{"needle", U8Ptr}, {"haystack_str", U8Ptr}, {"flags", I64, true}}},
      {"StrFirstOcc", U8Ptr, FF_RET1, {{"src", U8Ptr}, {"marker", U8Ptr}}},
      {"StrFirstRem", U8Ptr, FF_RET1,
       {{"src", U8Ptr}, {"marker", U8Ptr}, {"dst", U8Ptr, true}}},
      {"StrICmp", I64, FF_RET1, {{"st1", U8Ptr}, {"st2", U8Ptr}}, "_STRICMP"},
      {"StrIMatch", U8Ptr, FF_RET1,
       {{"needle", U8Ptr}, {"haystack_str", U8Ptr}}, "_STRIMATCH"},
      {"StrLastOcc", U8Ptr, FF_RET1, {{"src", U8Ptr}, {"marker", U8Ptr}}},
      {"StrLastRem", U8Ptr, FF_RET1,
       {{"src", U8Ptr}, {"marker", U8Ptr}, {"dst", U8Ptr, true}}},
      {"StrMatch", U8Ptr, FF_RET1,
       {{"needle", U8Ptr}, {"haystack_str", U8Ptr}}, "_STRMATCH"},
      {"StrNCmp", I64, FF_RET1,
       {{"st1", U8Ptr}, {"st2", U8Ptr}, {"n", I64}}, "_STRNCMP"},
      {"StrNICmp", I64, FF_RET1,
       {{"st1", U8Ptr}, {"st2", U8Ptr}, {"n", I64}}, "_STRNICMP"},
      {"StrOcc", I64, FF_RET1, {{"src", U8Ptr}, {"ch", U8}}},
      {"StrUtil", U8Ptr, FF_RET1, {{"src", U8Ptr}, {"flags", I64}}},
      {"Tabs2Spaces", U8Ptr, FF_RET1, {{"src", U8Ptr}}},
      {"StrCpy", U0, FF_RET1, {{"dst", U8Ptr}, {"src", U8Ptr}}, "_STRCPY"},
      {"StrLen", I64, FF_INTERNAL, {{"st", U8Ptr}}, nullptr, IC_STRLEN},
      {"QueInit", U0, FF_INTERNAL, {{"head", QuePtr}}, nullptr, IC_QUE_INIT},
      {"QueIns", U0, FF_INTERNAL, {{"entry", QuePtr}, {"pred", QuePtr}}, nullptr, IC_QUE_INS},
      {"QueInsRev", U0, FF_INTERNAL, {{"entry", QuePtr}, {"succ", QuePtr}}, nullptr, IC_QUE_INS_REV},
      {"QueRem", U0, FF_INTERNAL, {{"entry", QuePtr}}, nullptr, IC_QUE_REM},
      {"LinkedLstDel", U0, FF_RET1, {{"_lst", U8PtrPtr}}},
      {"LinkedLstCopy", U8Ptr, FF_RET1, {{"_lst", U8PtrPtr}, {"mem_task", TaskPtr, true}}},
      {"LinkedLstCnt", I64, FF_RET1, {{"_lst", U8PtrPtr}}},
      {"LinkedLstSize", I64, FF_RET1, {{"_lst", U8PtrPtr}}},
      {"QueDel", U0, FF_RET1, {{"head", QuePtr}, {"querem", Bool, true}}},
      {"QueCopy", QuePtr, FF_RET1, {{"head", QuePtr}, {"mem_task", TaskPtr, true}}},
      {"QueCnt", I64, FF_RET1, {{"head", QuePtr}}},
      {"QueSize", I64, FF_RET1, {{"head", QuePtr}}},
      {"FifoU8New", FifoU8Ptr, FF_RET1, {{"size", I64}, {"mem_task", TaskPtr, true}}},
      {"FifoU8Del", U0, FF_RET1, {{"f", FifoU8Ptr}}},
      {"FifoU8Ins", Bool, FF_RET1, {{"f", FifoU8Ptr}, {"b", U8}}},
      {"FifoU8Rem", Bool, FF_RET1, {{"f", FifoU8Ptr}, {"_b", U8Ptr}}},
      {"FifoU8Peek", Bool, FF_RET1, {{"f", FifoU8Ptr}, {"_b", U8Ptr}}},
      {"FifoU8Flush", U0, FF_RET1, {{"f", FifoU8Ptr}}},
      {"FifoU8Cnt", I64, FF_RET1, {{"f", FifoU8Ptr}}},
      {"FifoI64New", FifoI64Ptr, FF_RET1, {{"size", I64}, {"mem_task", TaskPtr, true}}},
      {"FifoI64Del", U0, FF_RET1, {{"f", FifoI64Ptr}}},
      {"FifoI64Ins", Bool, FF_RET1, {{"f", FifoI64Ptr}, {"q", I64}}},
      {"FifoI64Rem", Bool, FF_RET1, {{"f", FifoI64Ptr}, {"_q", I64Ptr}}},
      {"FifoI64Peek", Bool, FF_RET1, {{"f", FifoI64Ptr}, {"_q", I64Ptr}}},
      {"FifoI64Flush", U0, FF_RET1, {{"f", FifoI64Ptr}}},
      {"FifoI64Cnt", I64, FF_RET1, {{"f", FifoI64Ptr}}},
      {"StrCmp", I64, FF_RET1, {{"st1", U8Ptr}, {"st2", U8Ptr}}, "_STRCMP"},
      {"MemCmp", I64, FF_RET1, {{"ptr1", U8Ptr}, {"ptr2", U8Ptr}, {"cnt", I64}},
       "_MEMCMP"},
      {"MemCpy", U8Ptr, FF_RET1,
       {{"dst", U8Ptr}, {"src", U8Ptr}, {"cnt", I64}}, "_MEMCPY"},
      {"MemSet", U8Ptr, FF_RET1,
       {{"dst", U8Ptr}, {"val", I64}, {"cnt", I64}}, "_MEMSET"},
      {"MemSetU8", U8Ptr, FF_RET1,
       {{"dst", U8Ptr}, {"val", I64}, {"U8cnt", I64}}, "_MEMSET"},
      {"MemSetU16", U16Ptr, FF_RET1,
       {{"dst", U16Ptr}, {"val", I64}, {"U16cnt", I64}}, "_MEMSET_U16"},
      {"MemSetU32", U32Ptr, FF_RET1,
       {{"dst", U32Ptr}, {"val", I64}, {"U32cnt", I64}}, "_MEMSET_U32"},
      {"MemSetI64", I64Ptr, FF_RET1,
       {{"dst", I64Ptr}, {"val", I64}, {"I64cnt", I64}}, "_MEMSET_I64"},
      {"PutHex", U0, FF_RET1, {{"num", U64}, {"width", I64}}},
      {"PutChars", U0, FF_RET1, {{"ch", U64}}},
      {"PutS", U0, FF_RET1, {{"st", U8Ptr}}},
      {"tS", F64, FF_RET1, {}},
      {"Busy", U0, FF_RET1, {{"microseconds", I64}}},
      {"SleepUntil", U0, FF_RET1, {{"wake_jiffy", I64}}},
      {"Sleep", U0, FF_RET1, {{"mS", I64}}},
      {"Fs", TaskPtr, FF_INTERNAL, {}, nullptr, IC_FS},
      {"Gs", CpuPtr, FF_INTERNAL, {}, nullptr, IC_GS},
      {"ChkPtr", Bool, FF_RET1, {{"ptr", U8Ptr}}},
      {"ClassRep2", Bool, FF_RET1,
       {{"doc", DocPtr}, {"_d", U8Ptr}, {"class_name", U8Ptr, LastClass},
        {"depth", I64}, {"max_depth", I64}, {"dynamic", Bool},
        {"types", I64, true, HTT_CLASS}, {"offset", I64, true}}},
      {"ClassRep", U0, FF_RET1,
       {{"_d", U8Ptr}, {"class_name", U8Ptr, LastClass}, {"max_depth", I64, true, 2},
        {"fun", Bool, true}, {"offset", I64, true}}},
      {"ClassRepD", U0, FF_RET1,
       {{"_d", U8Ptr}, {"class_name", U8Ptr, LastClass}, {"max_depth", I64, true, 2},
        {"fun", Bool, true}, {"offset", I64, true}}},
      {"GetRFlags", I64, FF_INTERNAL, {}, nullptr, IC_GET_RFLAGS},
      {"GetTSC", I64, FF_INTERNAL, {}, nullptr, IC_RDTSC},
      {"Bsf", I64, FF_INTERNAL, {{"bit_field_val", I64}}, nullptr, IC_BSF},
      {"Bsr", I64, FF_INTERNAL, {{"bit_field_val", I64}}, nullptr, IC_BSR},
      {"ATan", F64, FF_INTERNAL, {{"d", F64}}, nullptr, IC_ATAN},
      {"Abs", F64, FF_INTERNAL, {{"d", F64}}, nullptr, IC_ABS},
      {"AbsI64", I64, FF_INTERNAL, {{"i", I64}}, nullptr, IC_ABS_I64},
      {"Cos", F64, FF_INTERNAL, {{"d", F64}}, nullptr, IC_COS},
      {"MaxI64", I64, FF_INTERNAL, {{"n1", I64}, {"n2", I64}}, nullptr, IC_MAX_I64},
      {"MaxU64", U64, FF_INTERNAL, {{"n1", U64}, {"n2", U64}}, nullptr, IC_MAX_U64},
      {"MinI64", I64, FF_INTERNAL, {{"n1", I64}, {"n2", I64}}, nullptr, IC_MIN_I64},
      {"MinU64", U64, FF_INTERNAL, {{"n1", U64}, {"n2", U64}}, nullptr, IC_MIN_U64},
      {"SignI64", I64, FF_INTERNAL, {{"i", I64}}, nullptr, IC_SIGN_I64},
      {"Sin", F64, FF_INTERNAL, {{"d", F64}}, nullptr, IC_SIN},
      {"Sqr", F64, FF_INTERNAL, {{"d", F64}}, nullptr, IC_SQR},
      {"SqrI64", I64, FF_INTERNAL, {{"i", I64}}, nullptr, IC_SQR_I64},
      {"SqrU64", U64, FF_INTERNAL, {{"i", U64}}, nullptr, IC_SQR_U64},
      {"Sqrt", F64, FF_INTERNAL, {{"d", F64}}, nullptr, IC_SQRT},
      {"Tan", F64, FF_INTERNAL, {{"d", F64}}, nullptr, IC_TAN},
      {"ToBool", Bool, FF_INTERNAL, {{"i", I64}}, nullptr, IC_TO_BOOL},
      {"ToI64", I64, FF_INTERNAL, {{"d", F64}}, nullptr, IC_TO_I64},
      {"ToF64", F64, FF_INTERNAL, {{"i", I64}}, nullptr, IC_TO_F64},
      {"ToUpper", I64, FF_INTERNAL, {{"ch", U8}}, nullptr, IC_TOUPPER},
      {"JobDel", U0, FF_RET1, {{"tmpc", JobPtr}}},
      {"TaskRstAwaitingMsg", U0, FF_RET1, {{"task", TaskPtr, true}}},
      {"TaskExe", JobPtr, FF_RET1, {{"srv", TaskPtr}, {"master", TaskPtr}, {"data", U8Ptr}, {"flags", I64}}},
      {"TaskText", JobPtr, FF_RET1, {{"srv", TaskPtr}, {"master", TaskPtr}, {"data", U8Ptr}, {"flags", I64}}},
      {"TaskMsg", JobPtr, FF_RET1, {{"srv", TaskPtr}, {"master", TaskPtr}, {"msg_code", I64}, {"arg1", I64}, {"arg2", I64}, {"flags", I64}}},
      {"JobResScan", Bool, FF_RET1, {{"rqst", JobPtr, true}, {"_res", I64Ptr, true}}},
      {"JobResGet", I64, FF_RET1, {{"rqst", JobPtr, true}}},
      {"TaskWait", U0, FF_RET1, {{"task", TaskPtr, true}, {"cmd_line_pmt", Bool, true}}},
      {"PostMsg", U0, FF_RET1, {{"task", TaskPtr}, {"msg_code", I64}, {"arg1", I64}, {"arg2", I64}, {"flags", I64, true}}},
      {"PostMsgWait", U0, FF_RET1, {{"task", TaskPtr}, {"msg_code", I64}, {"arg1", I64}, {"arg2", I64}, {"flags", I64, true}}},
      {"Msg", U0, FF_RET1, {{"msg_code", I64}, {"arg1", I64}, {"arg2", I64}, {"flags", I64, true}}},
      {"JobsHndlr", I64, FF_RET1, {{"run_flags", I64}, {"task", TaskPtr, true}}},
      {"SrvTaskCont", U0, FF_RET1, {}},
      {"FlushMsgs", I64, FF_RET1, {{"task", TaskPtr, true}}},
      {"Char2ScanCode", I64, FF_RET1, {{"ch", I64}, {"sc_flags", I64, true}}},
      {"ScanCode2Char", U8, FF_RET1, {{"sc", I64}}},
      {"KbdTypeMatic", U0, FF_RET1, {{"delay", U8}}},
      {"GridInit", U0, FF_RET1, {}},
      {"MsSet", U0, FF_RET1,
       {{"x", I64, true, INT64_MAX}, {"y", I64, true, INT64_MAX},
        {"z", I64, true, INT64_MAX}, {"l", I64, true, INT64_MAX},
        {"r", I64, true, INT64_MAX}}},
      {"GrPaletteColorSet", U0, FF_RET1, {{"color_num", I64}, {"bgr48", BGR48}}},
      {"GrPaletteColorGet", BGR48, FF_RET1, {{"color_num", I64}}},
      {"GrPaletteGet", U0, FF_RET1, {{"bgr48", BGR48Ptr}}},
      {"GrPaletteSet", U0, FF_RET1, {{"bgr48", BGR48Ptr}}},
      {"GrScaleZoom", U0, FF_RET1, {{"scale", F64}}},
      {"WinHorz", Bool, FF_RET1, {{"left", I64}, {"right", I64}, {"task", TaskPtr, true}}},
      {"WinVert", Bool, FF_RET1, {{"top", I64}, {"bottom", I64}, {"task", TaskPtr, true}}},
      {"WinRefocus", TaskPtr, FF_RET1, {{"task", TaskPtr, true}}},
      {"WinToTop", I64, FF_RET1, {{"task", TaskPtr, true}, {"update_z_buf", Bool, true, 1}}},
      {"WinFocus", TaskPtr, FF_RET1, {{"task", TaskPtr, true}}},
      {"WinTileHorz", U0, FF_RET1, {}},
      {"WinTileVert", U0, FF_RET1, {}},
      {"WinMax", U0, FF_RET1, {{"task", TaskPtr, true}}},
      {"WinBorder", Bool, FF_RET1, {{"val", Bool, true}, {"task", TaskPtr, true}}},
      {"WinScrollNull", U0, FF_RET1, {{"task", TaskPtr}, {"s", D3I64Ptr}}},
      {"WinScrollRestore", U0, FF_RET1, {{"task", TaskPtr}, {"s", D3I64Ptr}}},
      {"CtrlFindUnique", CtrlPtr, FF_RET1, {{"task", TaskPtr}, {"type", I64}}},
      {"CtrlInside", Bool, FF_RET1, {{"c", CtrlPtr}, {"x", I64}, {"y", I64}}},
      {"WinScrollsInit", U0, FF_RET1, {{"task", TaskPtr}}},
      {"ViewAnglesNew", CtrlPtr, FF_RET1, {{"task", TaskPtr, true}}},
      {"ViewAnglesDel", U0, FF_RET1, {{"task", TaskPtr, true}}},
      {"AutoComplete", Bool, FF_RET1, {{"val", Bool, true}}},
      {"MenuSubEntryFind", MenuEntryPtr, FF_RET1, {{"haystack_first", MenuEntryPtr}, {"needle_entry_name", U8Ptr}}},
      {"MenuEntryFind", MenuEntryPtr, FF_RET1, {{"haystack_menu", MenuPtr}, {"needle_full_name", U8Ptr}}},
      {"MenuNew", MenuPtr, FF_RET1, {{"st", U8Ptr}, {"flags", I64, true}, {"task", TaskPtr, true}}},
      {"MenuFile", MenuPtr, FF_RET1, {{"filename", U8Ptr}, {"flags", I64, true}, {"task", TaskPtr, true}}},
      {"MenuDel", U0, FF_RET1, {{"m", MenuPtr}}},
      {"MenuPush", MenuPtr, FF_RET1, {{"st", U8Ptr}}},
      {"MenuFilePush", MenuPtr, FF_RET1, {{"filename", U8Ptr}}},
      {"MenuPop", U0, FF_RET1, {}},
      {"ScanCode2KeyName", U8Ptr, FF_RET1, {{"sc", I64}}},
      {"SettingsPush", TaskSettingsPtr, FF_RET1,
       {{"task", TaskPtr, true}, {"flags", I64, true}}},
      {"SettingsPop", U0, FF_RET1, {{"task", TaskPtr, true}, {"flags", I64, true}}},
      {"WinZBufUpdate", U0, FF_RET1, {}},
      {"TextChar", Bool, FF_RET1,
       {{"task", TaskPtr}, {"allow_border", Bool, true}, {"x", I64}, {"y", I64}, {"d", I64}},
       "_TEXT_CHAR"},
      {"TextLenStr", Bool, FF_RET1,
       {{"task", TaskPtr}, {"x", I64}, {"y", I64}, {"len", I64}, {"attr", I64}, {"s", U8Ptr}},
       "_TEXT_LEN_STR"},
      {"TextLenAttrStr", Bool, FF_RET1,
       {{"task", TaskPtr}, {"x", I64}, {"y", I64}, {"len", I64}, {"_attr", U32Ptr}},
       "_TEXT_LEN_ATTR_STR"},
      {"TextLenAttr", Bool, FF_RET1,
       {{"task", TaskPtr}, {"x", I64}, {"y", I64}, {"len", I64}, {"attr", I64}},
       "_TEXT_LEN_ATTR"},
      {"TextPrint", U0, FF_DOT_DOT_DOT,
       {{"task", TaskPtr}, {"x", I64}, {"y", I64}, {"attr", I64}, {"fmt", U8Ptr}}},
      {"TextRect", U0, FF_RET1, {{"l", I64}, {"r", I64}, {"t", I64}, {"b", I64}, {"d", I64}}},
      {"TextBorder", U0, FF_RET1,
       {{"task", TaskPtr, true}, {"l", I64}, {"r", I64}, {"t", I64}, {"b", I64},
        {"attr", I64}, {"solid", Bool}}},
      {"DocNew", DocPtr, FF_RET1, {{"filename", U8Ptr, true}, {"task", TaskPtr, true}}},
      {"DocRead", DocPtr, FF_RET1, {{"name", U8Ptr, true}, {"flags", I64, true}}},
      {"DocSave", U8Ptr, FF_RET1, {{"doc", DocPtr}, {"_size", I64Ptr, true}}},
      {"DocType", Bool, FF_RET1,
       {{"doc", DocPtr, true}, {"filename", U8Ptr}, {"trailing_new_lines", I64, true, 1}}},
      {"Type", Bool, FF_RET1, {{"filename", U8Ptr}, {"trailing_new_lines", I64, true, 1}}},
      {"DocPut", DocPtr, FF_RET1, {{"task", TaskPtr, true}}},
      {"DocDisplay", DocPtr, FF_RET1, {{"task", TaskPtr, true}}},
      {"DocBorder", DocPtr, FF_RET1, {{"task", TaskPtr, true}}},
      {"DocDel", U0, FF_RET1, {{"doc", DocPtr}}},
      {"DocRecalc", Bool, FF_RET1, {{"doc", DocPtr}, {"recalc_flags", I64, true}}},
      {"DocMax", I64, FF_RET1, {{"i", I64, true, INT64_MAX}}},
      {"DocDataFmt", U0, FF_RET1,
       {{"doc", DocPtr}, {"doc_e", DocEntryPtr}, {"d", I64, true, UINT64_MAX}}},
      {"DocDataScan", U0, FF_RET1, {{"doc", DocPtr}, {"doc_e", DocEntryPtr}}},
      {"DocForm", Bool, FF_RET1,
       {{"_d", U8Ptr}, {"class_name", U8Ptr, LastClass}, {"dof_flags", I64, true},
        {"header", U8Ptr, true}, {"footer", U8Ptr, true}}},
      {"DocEd", Bool, FF_RET1, {{"doc", DocPtr}, {"dof_flags", I64, true}}},
      {"DocFormFwd", U0, FF_RET1, {{"doc", DocPtr}, {"giveup", Bool, true}}},
      {"DocFormBwd", U0, FF_RET1, {{"doc", DocPtr}, {"giveup", Bool, true}}},
      {"View", Bool, FF_RET1, {}},
      {"HomeSet", U0, FF_RET1, {{"dirname", U8Ptr}}},
      {"StrCpy", U0, FF_RET1, {{"dst", U8Ptr}, {"src", U8Ptr}}, "_STRCPY"},
      {"Define", U8Ptr, FF_RET1, {{"dname", U8Ptr}}},
      {"DefineLoad", DefinePtr, FF_RET1, {{"dname", U8Ptr}, {"st", U8Ptr}}},
      {"DefinePrint", U0, FF_DOT_DOT_DOT, {{"dname", U8Ptr}, {"src", U8Ptr}}},
      {"DefineCnt", I64, FF_RET1, {{"dname", U8Ptr}}},
      {"DefineSub", U8Ptr, FF_RET1, {{"sub", I64}, {"name", U8Ptr}}},
      {"DefineLstLoad", DefinePtr, FF_RET1, {{"dname", U8Ptr}, {"lst", U8Ptr}}},
      {"DefineMatch", I64, FF_RET1,
       {{"needle", U8Ptr}, {"haystack_lst_dname", U8Ptr}, {"flags", I64, true}}},
      {"Color2Str", U8Ptr, FF_RET1, {{"buf", U8Ptr}, {"c", ColorROP}}},
      {"Str2ColorU16", ColorROP16, FF_RET1, {{"st", U8Ptr}}},
      {"Str2ColorU32", ColorROP, FF_RET1, {{"st", U8Ptr}}},
      {"DocPutKey", U0, FF_RET1, {{"doc", DocPtr}, {"ch", I64, true}, {"sc", I64, true}}},
      {"DocTermNew", U0, FF_RET1, {}},
      {"DocGetKey", I64, FF_RET1, {{"_sc", I64Ptr, true}}},
      {"DocScanLine", U8Ptr, FF_RET1, {{"doc", DocPtr}, {"doc_e", DocEntryPtr},
       {"cur_col", I64Ptr, true}, {"_do_e_end", DocEntryPtrPtr, true}}},
      {"GetStr", U8Ptr, FF_RET1,
       {{"msg", U8Ptr, true}, {"dft", U8Ptr, true}, {"flags", I64, true}}},
      {"GetI64", I64, FF_RET1,
       {{"msg", U8Ptr, true}, {"dft", I64, true}, {"lo", I64, true, 0x8000000000000000ULL},
        {"hi", I64, true, 0x7FFFFFFFFFFFFFFFULL}}},
      {"GetF64", F64, FF_RET1,
       {{"msg", U8Ptr, true}, {"dft", F64, true}, {"lo", F64, true, 0xFFEFFFFFFFFFFFFFULL},
        {"hi", F64, true, 0x7FEFFFFFFFFFFFFFULL}}},
      {"GetDate", Date, FF_RET1,
       {{"msg", U8Ptr, true}, {"dft", Date, true, 0x8000000000000000ULL},
        {"lo", Date, true, 0x8000000000000000ULL}, {"hi", Date, true, 0x7FFFFFFFFFFFFFFFULL}}},
      {"GetS", I64, FF_RET1, {{"buf", U8Ptr}, {"size", I64}, {"allow_ext", Bool, true, 1}}},
      {"UserCmdLine", U0, FF_RET1, {{"dummy", I64, true}}},
      {"DocLock", Bool, FF_RET1, {{"doc", DocPtr}}},
      {"DocUnlock", Bool, FF_RET1, {{"doc", DocPtr}}},
      {"DocBottom", U0, FF_RET1, {{"doc", DocPtr, true}}},
      {"DocClear", U0, FF_RET1, {{"doc", DocPtr, true}, {"clear_holds", Bool, true}}},
      {"DocCursor", Bool, FF_RET1, {{"show", Bool, true}, {"doc", DocPtr, true}}},
      {"DocHighlightCursor", Bool, FF_RET1, {{"show", Bool, true}, {"doc", DocPtr, true}}},
      {"DocScroll", Bool, FF_RET1, {{"val", Bool, true}, {"doc", DocPtr, true}}},
      {"DocEntryRun", I64, FF_RET1, {{"doc", DocPtr}, {"doc_e", DocEntryPtr},
                                    {"exited", Bool}, {"_has_action", I64Ptr, true}}},
      {"DocPrint", DocEntryPtr, FF_DOT_DOT_DOT,
       {{"doc", DocPtr, true}, {"fmt", U8Ptr}}},
      {"DrawStdMs", U0, FF_RET1, {{"dc", DCPtr}, {"x", I64}, {"y", I64}}},
      {"PaletteSetStd", U0, FF_RET1, {}},
      {"ClampI64", I64, FF_RET1, {{"i", I64}, {"lo", I64}, {"hi", I64}}, "_CLAMP_I64"},
      {"Clamp", F64, FF_RET1, {{"d", F64}, {"lo", F64}, {"hi", F64}}},
      {"Min", F64, FF_RET1, {{"n1", F64}, {"n2", F64}}},
      {"Max", F64, FF_RET1, {{"n1", F64}, {"n2", F64}}},
      {"Pow10I64", F64, FF_RET1, {{"i", I64}}},
      {"FloorU64", U64, FF_RET1, {{"num", U64}, {"to", U64}}},
      {"CeilU64", U64, FF_RET1, {{"num", U64}, {"to", U64}}},
      {"RoundI64", I64, FF_RET1, {{"num", I64}, {"to", I64}}},
      {"FloorI64", I64, FF_RET1, {{"num", I64}, {"to", I64}}},
      {"CeilI64", I64, FF_RET1, {{"num", I64}, {"to", I64}}},
      {"RandI16", I16, FF_RET1, {}},
      {"RandU16", U16, FF_RET1, {}},
      {"RandI32", I32, FF_RET1, {}},
      {"RandU32", U32, FF_RET1, {}},
      {"RandI64", I64, FF_RET1, {}},
      {"RandU64", U64, FF_RET1, {}},
      {"Rand", F64, FF_RET1, {}},
      {"Seed", I64, FF_RET1, {{"seed", I64, true}, {"task", TaskPtr, true}}},
      {"IsPixCovered0", Bool, FF_RET1, {{"task", TaskPtr}, {"x", I64}, {"y", I64}}},
      {"GrPlot0", Bool, FF_RET1, {{"dc", DCPtr, true}, {"x", I64}, {"y", I64}}},
      {"GrPeek0", I64, FF_RET1, {{"dc", DCPtr, true}, {"x", I64}, {"y", I64}}},
      {"GrBlot", I64, FF_RET1, {{"dc", DCPtr, true}, {"x", I64}, {"y", I64}, {"img", DCPtr}}},
      {"GrRect", I64, FF_RET1, {{"dc", DCPtr, true}, {"x", I64}, {"y", I64}, {"w", I64}, {"h", I64}}},
      {"GrRectB", I64, FF_RET1,
       {{"dc", DCPtr, true}, {"x1", I64}, {"y1", I64}, {"x2", I64},
        {"y2", I64}}},
      {"GrClamp", Bool, FF_RET1,
       {{"dc", DCPtr, true}, {"left", I64Ptr}, {"top", I64Ptr}, {"right", I64Ptr},
        {"bottom", I64Ptr}, {"width", I64, true}, {"height", I64, true}}},
      {"GrPlot", Bool, FF_RET1, {{"dc", DCPtr, true}, {"x", I64}, {"y", I64}}},
      {"GrFillCircle", I64, FF_RET1,
       {{"dc", DCPtr, true}, {"cx", I64}, {"cy", I64}, {"z", I64, true},
        {"diameter", I64}}},
      {"GrPlot3B", Bool, FF_RET1,
       {{"dc", DCPtr, true}, {"x", I64}, {"y", I64}, {"z", I64}}},
      {"GrPlot3", Bool, FF_RET1,
       {{"dc", DCPtr, true}, {"x", I64}, {"y", I64}, {"z", I64}}},
      {"SpriteSize", I64, FF_RET1, {{"elems", U8Ptr}}},
      {"DC2Sprite", U8Ptr, FF_RET1, {{"tmpb", DCPtr}}},
      {"Sprite3", U0, FF_RET1,
       {{"dc", DCPtr, true}, {"x", I64}, {"y", I64}, {"z", I64},
        {"elems", U8Ptr}, {"just_one_elem", Bool, true}}},
      {"Sprite3B", U0, FF_RET1,
       {{"dc", DCPtr, true}, {"x", I64}, {"y", I64}, {"z", I64},
        {"elems", U8Ptr}}},
      {"GrBlot3", I64, FF_RET1,
       {{"dc", DCPtr, true}, {"x1", I64}, {"y1", I64}, {"z1", I64},
        {"img", DCPtr}}},
      {"GrHLine", I64, FF_RET1,
       {{"dc", DCPtr, true}, {"x1", I64}, {"x2", I64}, {"y", I64},
        {"z1", I64, true}, {"z2", I64, true}}},
      {"GrVLine", I64, FF_RET1,
       {{"dc", DCPtr, true}, {"x", I64}, {"y1", I64}, {"y2", I64},
        {"z1", I64, true}, {"z2", I64, true}}},
      {"GrFillTri0", I64, FF_RET1,
       {{"dc", DCPtr, true}, {"p1", D3I32Ptr}, {"p2", D3I32Ptr},
        {"p4", D3I32Ptr}}},
      {"GrFillPoly3", I64, FF_RET1,
       {{"dc", DCPtr, true}, {"n", I64}, {"poly", D3I32Ptr}}},
      {"GrRect3", I64, FF_RET1,
       {{"dc", DCPtr, true}, {"x", I64}, {"y", I64}, {"z", I64}, {"w", I64},
        {"h", I64}}},
      {"GrBorder", U0, FF_RET1,
       {{"dc", DCPtr, true}, {"x1", I64}, {"y1", I64}, {"x2", I64},
        {"y2", I64}, {"step", I64, true, 1}, {"start", I64, true}}},
      {"GrArrow3", Bool, FF_RET1,
       {{"dc", DCPtr, true}, {"x1", I64}, {"y1", I64}, {"z1", I64},
        {"x2", I64}, {"y2", I64}, {"z2", I64}, {"w", F64, true, GR_ARROW_WIDTH},
        {"step", I64, true, 1}, {"start", I64, true}}},
      {"GrPutChar", I64, FF_RET1,
       {{"dc", DCPtr, true}, {"x", I64}, {"y", I64}, {"ch", U8}}},
      {"GrPrint", I64, FF_DOT_DOT_DOT,
       {{"dc", DCPtr, true}, {"x", I64}, {"y", I64}, {"fmt", U8Ptr}}},
      {"PressAKey", I64, FF_RET1, {}},
      {"DCSymmetrySet", Bool, FF_RET1,
       {{"dc", DCPtr, true}, {"x1", I64}, {"y1", I64}, {"x2", I64},
        {"y2", I64}}},
      {"DCSymmetry3Set", Bool, FF_RET1,
       {{"dc", DCPtr, true}, {"x1", I64}, {"y1", I64}, {"z1", I64},
        {"x2", I64}, {"y2", I64}, {"z2", I64}, {"x3", I64}, {"y3", I64},
        {"z3", I64}}},
      {"DCReflect", U0, FF_RET1,
       {{"dc", DCPtr}, {"_x", I64Ptr}, {"_y", I64Ptr}, {"_z", I64Ptr}}},
      {"DocSprite", DocEntryPtr, FF_RET1,
       {{"doc", DocPtr, true}, {"elems", U8Ptr}, {"fmt", U8Ptr, true}}},
      {"Sprite", DocEntryPtr, FF_RET1, {{"elems", U8Ptr}, {"fmt", U8Ptr, true}}},
      {"GrPeek", I64, FF_RET1, {{"dc", DCPtr, true}, {"x", I64}, {"y", I64}}},
      {"GrFloodFill", I64, FF_RET1, {{"dc", DCPtr, true}, {"x", I64}, {"y", I64},
       {"not_color", Bool, true}, {"z", I64, true}, {"db", I32Ptr, true}}},
      {"GrFloodFill3", I64, FF_RET1, {{"dc", DCPtr, true}, {"x", I64}, {"y", I64},
       {"z1", I64}, {"not_color", Bool, true}}},
      {"GrLine", Bool, FF_RET1,
       {{"dc", DCPtr, true}, {"x1", I64}, {"y1", I64}, {"x2", I64},
        {"y2", I64}, {"step", I64, true, 1}, {"start", I64, true}}},
      {"GrLine3", Bool, FF_RET1,
       {{"dc", DCPtr, true}, {"x1", I64}, {"y1", I64}, {"z1", I64},
        {"x2", I64}, {"y2", I64}, {"z2", I64}, {"step", I64, true, 1},
        {"start", I64, true}}},
      {"Circle", Bool, FF_RET1,
       {{"aux_data", U8Ptr}, {"cx", I64}, {"cy", I64}, {"cz", I64},
        {"radius", I64}, {"fp_plot", PlotFn}, {"step", I64, true, 1},
        {"start_radians", F64, true}, {"len_radians", F64, true, 0x401921FB54442D18}}},
      {"Ellipse", Bool, FF_RET1,
       {{"aux_data", U8Ptr}, {"cx", I64}, {"cy", I64}, {"cz", I64},
        {"x_radius", I64}, {"y_radius", I64}, {"fp_plot", PlotFn},
        {"rot_angle", F64, true}, {"step", I64, true, 1}, {"start_radians", F64, true},
        {"len_radians", F64, true, 0x401921FB54442D18}}},
      {"GrCircle", Bool, FF_RET1,
       {{"dc", DCPtr, true}, {"cx", I64}, {"cy", I64}, {"radius", I64},
        {"step", I64, true, 1}, {"start_radians", F64, true},
        {"len_radians", F64, true, 0x401921FB54442D18}}},
      {"GrEllipse", Bool, FF_RET1,
       {{"dc", DCPtr, true}, {"cx", I64}, {"cy", I64}, {"x_radius", I64},
        {"y_radius", I64}, {"rot_angle", F64, true}, {"step", I64, true, 1},
        {"start_radians", F64, true}, {"len_radians", F64, true, 0x401921FB54442D18}}},
      {"GrCircle3", Bool, FF_RET1,
       {{"dc", DCPtr, true}, {"cx", I64}, {"cy", I64}, {"cz", I64}, {"radius", I64},
        {"step", I64, true, 1}, {"start_radians", F64, true},
        {"len_radians", F64, true, 0x401921FB54442D18}}},
      {"GrEllipse3", Bool, FF_RET1,
       {{"dc", DCPtr, true}, {"cx", I64}, {"cy", I64}, {"cz", I64},
        {"x_radius", I64}, {"y_radius", I64}, {"rot_angle", F64, true},
        {"step", I64, true, 1}, {"start_radians", F64, true},
        {"len_radians", F64, true, 0x401921FB54442D18}}},
      {"Arg", F64, FF_RET1, {{"x", F64}, {"y", F64}}},
      {"Line", Bool, FF_RET1,
       {{"aux_data", U8Ptr},
        {"x1", I64},
        {"y1", I64},
        {"z1", I64},
        {"x2", I64},
        {"y2", I64},
        {"z2", I64},
        {"fp_plot", PlotFn},
        {"step", I64, true, 1},
        {"start", I64, true}}},
      {"RegPoly", Bool, FF_RET1,
       {{"aux_data", U8Ptr},
        {"cx", I64},
        {"cy", I64},
        {"cz", I64},
        {"x_radius", I64},
        {"y_radius", I64},
        {"sides", I64},
        {"fp_plot", PlotFn},
        {"rot_angle", F64, true},
        {"step", I64, true, 1},
        {"start_radians", F64, true},
        {"len_radians", F64, true, 0x401921FB54442D18}}},
      {"GrRegPoly", Bool, FF_RET1,
       {{"dc", DCPtr, true},
        {"cx", I64},
        {"cy", I64},
        {"x_radius", I64},
        {"y_radius", I64},
        {"sides", I64},
        {"rot_angle", F64, true},
        {"step", I64, true, 1},
        {"start_radians", F64, true},
        {"len_radians", F64, true, 0x401921FB54442D18}}},
      {"GrRegPoly3", Bool, FF_RET1,
       {{"dc", DCPtr, true},
        {"cx", I64},
        {"cy", I64},
        {"cz", I64},
        {"x_radius", I64},
        {"y_radius", I64},
        {"sides", I64},
        {"rot_angle", F64, true},
        {"step", I64, true, 1},
        {"start_radians", F64, true},
        {"len_radians", F64, true, 0x401921FB54442D18}}},
      {"Bezier2", Bool, FF_RET1,
       {{"aux_data", U8Ptr},
        {"ctrl", D3I32Ptr},
        {"fp_plot", PlotFn},
        {"first", Bool, true, 1}}},
      {"BSpline2", Bool, FF_RET1,
       {{"aux_data", U8Ptr},
        {"ctrl", D3I32Ptr},
        {"cnt", I64},
        {"fp_plot", PlotFn},
        {"closed", Bool, true}}},
      {"Gr2Bezier", Bool, FF_RET1,
       {{"dc", DCPtr, true},
        {"ctrl", D3I32Ptr}}},
      {"Gr2Bezier3", Bool, FF_RET1,
       {{"dc", DCPtr, true},
        {"ctrl", D3I32Ptr}}},
      {"Gr2BSpline", Bool, FF_RET1,
       {{"dc", DCPtr, true},
        {"ctrl", D3I32Ptr},
        {"cnt", I64},
        {"closed", Bool, true}}},
      {"Gr2BSpline3", I64, FF_RET1,
       {{"dc", DCPtr, true},
        {"ctrl", D3I32Ptr},
        {"cnt", I64},
        {"closed", Bool, true}}},
      {"Bezier3", Bool, FF_RET1,
       {{"aux_data", U8Ptr},
        {"ctrl", D3I32Ptr},
        {"fp_plot", PlotFn},
        {"first", Bool, true, 1}}},
      {"BSpline3", Bool, FF_RET1,
       {{"aux_data", U8Ptr},
        {"ctrl", D3I32Ptr},
        {"cnt", I64},
        {"fp_plot", PlotFn},
        {"closed", Bool, true}}},
      {"Gr3Bezier", Bool, FF_RET1,
       {{"dc", DCPtr, true},
        {"ctrl", D3I32Ptr}}},
      {"Gr3Bezier3", Bool, FF_RET1,
       {{"dc", DCPtr, true},
        {"ctrl", D3I32Ptr}}},
      {"Gr3BSpline", Bool, FF_RET1,
       {{"dc", DCPtr, true},
        {"ctrl", D3I32Ptr},
        {"cnt", I64},
        {"closed", Bool, true}}},
      {"Gr3BSpline3", Bool, FF_RET1,
       {{"dc", DCPtr, true},
        {"ctrl", D3I32Ptr},
        {"cnt", I64},
        {"closed", Bool, true}}},
      {"D3I32Dist", F64, FF_RET1,
       {{"p1", D3I32Ptr},
        {"p2", D3I32Ptr}}},
      {"D3I32DistSqr", I64, FF_RET1,
       {{"p1", D3I32Ptr},
        {"p2", D3I32Ptr}}},
      {"D3I32NormSqr", I64, FF_RET1,
       {{"p", D3I32Ptr}}},
      {"DCThickScale", U0, FF_RET1, {{"dc", DCPtr, true}}},
      {"ClipLine", Bool, FF_RET1,
       {{"_x1", I64Ptr}, {"_y1", I64Ptr}, {"_x2", I64Ptr}, {"_y2", I64Ptr},
        {"left", I64}, {"top", I64}, {"right", I64}, {"bottom", I64}}},
      {"Mat4x4MulXYZ", U0, FF_RET1,
       {{"r", I64Ptr}, {"_x", I64Ptr}, {"_y", I64Ptr}, {"_z", I64Ptr}}},
      {"DCTransform", U0, FF_RET1,
       {{"dc", DCPtr}, {"_x", I64Ptr}, {"_y", I64Ptr}, {"_z", I64Ptr}}},
      {"Mat4x4IdentEqu", I64Ptr, FF_RET1, {{"r", I64Ptr}}},
      {"Mat4x4IdentNew", I64Ptr, FF_RET1, {{"mem_task", TaskPtr, true}}},
      {"Mat4x4NormSqr65536", I64, FF_RET1, {{"r", I64Ptr}}},
      {"DCMat4x4Set", U0, FF_RET1, {{"dc", DCPtr, true}, {"r", I64Ptr}}},
      {"DCLighting", U0, FF_RET1,
       {{"dc", DCPtr}, {"p1", D3I32Ptr}, {"p2", D3I32Ptr}, {"p3", D3I32Ptr}, {"color", ColorROP}}},
      {"DCFill", U0, FF_RET1, {{"dc", DCPtr, true}, {"val", ColorROP, true, TRANSPARENT}}},
      {"DCClear", U0, FF_RET1, {{"dc", DCPtr, true}}},
      {"DCRst", U0, FF_RET1, {{"dc", DCPtr}}},
      {"DCExtentsInit", U0, FF_RET1, {{"dc", DCPtr, true}}},
      {"DCAlias", DCPtr, FF_RET1, {{"dc", DCPtr, true}, {"task", TaskPtr, true}}},
      {"DCNew", DCPtr, FF_RET1,
       {{"width", I64}, {"height", I64}, {"task", TaskPtr, true}, {"null_bitmap", Bool, true}}},
      {"DCDel", U0, FF_RET1, {{"dc", DCPtr}}},
      {"DCSize", I64, FF_RET1, {{"dc", DCPtr}}},
      {"DCDepthBufRst", I32Ptr, FF_RET1, {{"dc", DCPtr}}},
      {"DCDepthBufAlloc", I32Ptr, FF_RET1, {{"dc", DCPtr}}},
      {"DCCopy", DCPtr, FF_RET1, {{"dc", DCPtr}, {"task", TaskPtr, true}}},
      {"DCMono", U0, FF_RET1,
       {{"dc", DCPtr}, {"quest", I64, true, TRANSPARENT}, {"true_color", I64, true}, {"false_color", I64, true, COLOR_MONO}}},
      {"DCColorChg", I64, FF_RET1,
       {{"dc", DCPtr}, {"src_color", I64}, {"dst_color", I64, true, TRANSPARENT}}},
      {"DCSave", U8Ptr, FF_RET1,
       {{"dc", DCPtr}, {"_size", I64Ptr, true}, {"dcsf_flags", I64, true, DCSF_COMPRESSED}}},
      {"DCLoad", DCPtr, FF_RET1,
       {{"src", U8Ptr}, {"_size", I64Ptr, true}, {"task", TaskPtr, true}}},
      {"GRWrite", I64, FF_RET1,
       {{"filename", U8Ptr}, {"dc", DCPtr}, {"dcsf_flags", I64, true, DCSF_COMPRESSED}}},
      {"GRRead", DCPtr, FF_RET1, {{"filename", U8Ptr}, {"task", TaskPtr, true}}},
      {"DCExt", DCPtr, FF_RET1,
       {{"dc", DCPtr, true}, {"x1", I64}, {"y1", I64}, {"x2", I64},
        {"y2", I64}, {"task", TaskPtr, true}}},
      {"DCDiff", DCPtr, FF_RET1, {{"base", DCPtr}, {"update", DCPtr}}},
      {"DocGR", U0, FF_RET1, {{"doc", DocPtr, true}, {"filename", U8Ptr}}},
      {"DCScrnCapture", DCPtr, FF_RET1,
       {{"include_zoom", Bool, true, 1}, {"task", TaskPtr, true}}},
      {"GRScrnCaptureRead", Bool, FF_RET1,
       {{"filename", U8Ptr}, {"dc", DCPtr, true}, {"x", I64, true},
        {"y", I64, true}}},
      {"GRScrnCaptureWrite", I64, FF_RET1,
       {{"filename", U8Ptr}, {"include_zoom", Bool, true, 1}}},
      {"D3I32Norm", F64, FF_RET1, {{"p", D3I32Ptr}}},
      {"Mat4x4MulMat4x4Equ", I64Ptr, FF_RET1,
       {{"dst", I64Ptr}, {"m1", I64Ptr}, {"m2", I64Ptr}}},
      {"Mat4x4MulMat4x4New", I64Ptr, FF_RET1,
       {{"m1", I64Ptr}, {"m2", I64Ptr}, {"mem_task", TaskPtr, true}}},
      {"Mat4x4Equ", I64Ptr, FF_RET1, {{"dst", I64Ptr}, {"src", I64Ptr}}},
      {"Mat4x4RotX", I64Ptr, FF_RET1, {{"m", I64Ptr}, {"\xE9", F64}}},
      {"Mat4x4RotY", I64Ptr, FF_RET1, {{"m", I64Ptr}, {"\xE9", F64}}},
      {"Mat4x4RotZ", I64Ptr, FF_RET1, {{"m", I64Ptr}, {"\xE9", F64}}},
      {"Mat4x4Scale", I64Ptr, FF_RET1, {{"m", I64Ptr}, {"s", F64}}},
      {"Mat4x4TranslationEqu", I64Ptr, FF_RET1,
       {{"r", I64Ptr}, {"x", I64}, {"y", I64}, {"z", I64}}},
      {"Mat4x4TranslationAdd", I64Ptr, FF_RET1,
       {{"r", I64Ptr}, {"x", I64}, {"y", I64}, {"z", I64}}},
      {"Refresh", U0, FF_RET1, {{"cnt", I64, true, 1}, {"force", Bool, true}}},
      {"Mat4x4New", I64Ptr, FF_RET1, {{"src", I64Ptr}, {"mem_task", TaskPtr, true}}},
      {"PutKey", U0, FF_RET1, {{"ch", I64, true}, {"sc", I64, true}}},
      {"ScanChar", I64, FF_RET1, {}},
      {"ScanKey", Bool, FF_RET1,
       {{"_ch", I64Ptr, true}, {"_scan_code", I64Ptr, true}, {"echo", Bool, true}}},
      {"GetKey", I64, FF_RET1,
       {{"_scan_code", I64Ptr, true}, {"echo", Bool, true}, {"raw_cursor", Bool, true}}},
      {"GetChar", I64, FF_RET1,
       {{"_scan_code", I64Ptr, true}, {"echo", Bool, true, 1}, {"raw_cursor", Bool, true}}},
      {"ScanMsg", I64, FF_RET1, {{"_arg1", I64Ptr, true}, {"_arg2", I64Ptr, true}, {"mask", I64, true, ~uint64_t{1}}, {"task", TaskPtr, true}}},
      {"GetMsg", I64, FF_RET1, {{"_arg1", I64Ptr, true}, {"_arg2", I64Ptr, true}, {"mask", I64, true, ~uint64_t{1}}, {"task", TaskPtr, true}}},
      {"Yield", U0, FF_RET1, {}, "_YIELD"},
      {"Spawn",
       TaskPtr,
       FF_RET1,
       {{"fp_addr", U8Ptr}, {"data", U8Ptr, true}, {"task_name", U8Ptr, true},
        {"target_cpu", I64, true, UINT64_MAX}, {"parent", TaskPtr, true},
        {"stk_size", I64, true}, {"flags", I64, true, JOBF_ADD_TO_QUE}}},
      {"Exit", U0, FF_RET1, {}},
      {"TaskValidate", Bool, FF_RET1, {{"task", TaskPtr}}},
      {"Suspend", Bool, FF_RET1,
       {{"task", TaskPtr, true}, {"state", Bool, true, 1}}},
      {"IsSuspended", Bool, FF_RET1, {{"task", TaskPtr, true}}},
      {"TaskQueIns", U0, FF_RET1, {{"task", TaskPtr}, {"pred", TaskPtr, true}}},
      {"TaskQueRem", U0, FF_RET1, {{"task", TaskPtr}}},
      {"SingleUser", Bool, FF_RET1, {{"val", Bool}}},
      {"IsSingleUser", Bool, FF_RET1, {}},
      {"Ona2Freq", F64, FF_RET1, {{"ona", I64}}},
      {"Freq2Ona", I8, FF_RET1, {{"freq", F64}}},
      {"Snd", U0, FF_RET1, {{"ona", I8, true}}},
      {"ScrnCast",
       Bool,
       FF_RET1,
       {{"val", Bool, true, 1},
        {"just_audio", Bool, true},
        {"print_fmt", U8Ptr, true, 0, "B:/Tmp/%X.GR"}}},
      {"SndRst", U0, FF_RET1, {}},
      {"SndTaskEndCB", U0, FF_RET1, {}},
      {"tM", F64, FF_RET1, {}},
      {"Beat", F64, FF_RET1, {}},
      {"MusicSetOctave", U8Ptr, FF_RET1, {{"st", U8Ptr}}},
      {"MusicSetMeter", U8Ptr, FF_RET1, {{"st", U8Ptr}}},
      {"MusicSetNoteLen", U8Ptr, FF_RET1, {{"st", U8Ptr}}},
      {"Note2Ona", I8, FF_RET1, {{"note", I64}, {"octave", I64, true, 4}}},
      {"Ona2Note", I8, FF_RET1, {{"ona", I8}}},
      {"Ona2Octave", I8, FF_RET1, {{"ona", I8}}},
      {"Play", U0, FF_RET1, {{"st", U8Ptr}, {"words", U8Ptr, true}}},
      {"MusicSettingsRst", U0, FF_RET1, {}},
      {"SndShift", U0, FF_RET1, {{"head", SndPtr}, {"dt", F64, true}}},
      {"AUWrite", I64, FF_RET1,
       {{"filename", U8Ptr}, {"head", SndPtr}, {"t0_now", DatePtr}, {"t0_tS", F64}}},
      {"Beep", U0, FF_RET1, {{"ona", I8, true, 62}, {"busy", Bool, true}}},
      {"Mute", Bool, FF_RET1, {{"val", Bool}}},
      {"IsMute", Bool, FF_RET1, {}},
      {"Call", I64, FF_RET1, {{"addr", U64}}},
      {"StreamPrint", U0, FF_DOT_DOT_DOT, {{"fmt", U8Ptr}}},
      {"StreamDir", U0, FF_RET1, {}},
      {"StreamExePrint", I64, FF_DOT_DOT_DOT, {{"fmt", U8Ptr}}},
      {"RedSeaISO9660", U0, FF_RET1, {{"iso_filename", U8Ptr}, {"drv_let", U8}}},
      {"RedSeaISO",
       I64,
       FF_RET1,
       {{"iso_filename", U8Ptr, true},
        {"src_dir", U8Ptr},
        {"stage2_filename", U8Ptr, true}}},
      {"Cmp",
       I64,
       FF_RET1,
       {{"filename", U8Ptr},
        {"map_name", U8Ptr, true},
        {"out_name", U8Ptr, true},
        {"mapfile_drv_let", U64, true}}},
      {"ExeFile", I64, FF_RET1, {{"name", U8Ptr}, {"ccf_flags", I64, true}}},
      {"ExePutS",
       I64,
       FF_RET1,
       {{"buf", U8Ptr},
        {"filename", U8Ptr, true},
        {"ccf_flags", I64, true},
        {"htc", U8Ptr, true}}},
  };
  for (const auto &header : headers) {
    auto *symbol = reinterpret_cast<CHashExport *>(
        HashFind(header.name, task->hash_table, HTT_EXPORT_SYS_SYM));
    if (!symbol && !(header.flags & FF_INTERNAL))
      continue;
    auto *fun =
        reinterpret_cast<CHashFun *>(CAlloc(sizeof(CHashFun), task->data_heap));
    if (!fun)
      return false;
    fun->cls.sym.hash.type = HTT_FUN | HTF_PUBLIC;
    fun->cls.sym.hash.str = StrNew(header.name, task->data_heap);
    if (header.import_name)
      fun->cls.sym.import_name = StrNew(header.import_name, task->data_heap);
    if (!fun->cls.sym.hash.str || (header.import_name && !fun->cls.sym.import_name)) {
      HashDel(&fun->cls.sym.hash);
      return false;
    }
    for (const auto &arg : header.args) {
      if (!arg.name)
        break;
      if (arg.type == PlotFn) {
        const char *names[]{"aux", "x", "y", "z"};
        CHashClass *signature[]{types[U8Ptr], types[I64], types[I64], types[I64]};
        if (!CmpKernelCallback(table, fun->cls, arg.name, 16 + 8 * fun->arg_cnt,
                               *types[Bool], 4, names, signature)) {
          HashDel(&fun->cls.sym.hash);
          return false;
        }
        ++fun->arg_cnt;
        continue;
      }
      auto *name = StrNew(arg.name, task->data_heap);
      auto *member = name ? MemberAdd(fun->cls, name, *types[arg.type],
                                      16 + 8 * fun->arg_cnt)
                          : nullptr;
      if (!member) {
        Free(name);
        HashDel(&fun->cls.sym.hash);
        return false;
      }
      if (arg.has_default) {
        member->flags |= MLF_DFT_AVAILABLE;
        if (arg.kind == Numeric)
          member->dft_val = arg.dft_val;
      }
      if (arg.kind == LastClass)
        member->flags |= MLF_LASTCLASS;
      if (arg.kind == String) {
        auto *str = StrNew(arg.dft_str, task->data_heap);
        if (!str) {
          HashDel(&fun->cls.sym.hash);
          return false;
        }
        member->dft_val = reinterpret_cast<uintptr_t>(str);
        member->flags |= MLF_STR_DFT_AVAILABLE;
      }
      ++fun->arg_cnt;
    }
    fun->cls.flags = FF__EXTERN | header.flags;
    fun->return_class = types[header.result];
    fun->exe_addr = reinterpret_cast<uint8_t *>(
        header.flags & FF_INTERNAL ? uintptr_t{header.internal_op}
                                   : symbol->val);
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
  if (!AsmHashLoad())
    return false;
  auto *heap = CurrentTask()->data_heap;
  bool seen[KW_NOARGPOP + 1]{};
  for (int64_t bucket = 0; bucket <= asm_hash->mask; bucket++)
    for (auto *hash = asm_hash->body[bucket]; hash; hash = hash->next) {
      if (hash->type != HTT_KEYWORD)
        continue;
      auto value = reinterpret_cast<const CHashGeneric *>(hash)->user_data0;
      if (value > KW_NOARGPOP || seen[value])
        return false;
      auto *entry =
          reinterpret_cast<CHashGeneric *>(CAlloc(sizeof(CHashGeneric), heap));
      auto *name = entry ? StrNew(hash->str, heap) : nullptr;
      if (!name) {
        Free(entry);
        return false;
      }
      entry->hash.str = name;
      entry->hash.type = HTT_KEYWORD;
      entry->user_data0 = value;
      HashAdd(&entry->hash, table);
      seen[value] = true;
    }
  for (bool defined : seen)
    if (!defined)
      return false;
  for (const auto &type : internal_types_table) {
    auto *entry = PrsClassNew();
    auto *name = entry ? StrNew(reinterpret_cast<const char *>(type.name), heap)
                       : nullptr;
    if (!name) {
      Free(entry);
      return false;
    }
    entry->sym.hash.str = name;
    entry->sym.hash.type = HTT_INTERNAL_TYPE;
    entry->raw_type = type.type;
    entry->flags |= CF_INTERNAL_TYPE;
    entry->size = type.size;
    HashAdd(&entry->sym.hash, table);
  }
  static constexpr struct
  {
    const char *name;
    CRawType type;
  } types[]{{"U16", RT_U16}, {"I16", RT_I16}, {"U32", RT_U32},
            {"I32", RT_I32}, {"U64", RT_U64}, {"I64", RT_I64}};
  for (const auto &type : types) {
    auto *entry = PrsClassNew();
    auto *name = entry ? StrNew(type.name, heap) : nullptr;
    if (!name) {
      Free(entry);
      return false;
    }
    entry->sym.hash.str = name;
    entry->size = TypeSize(type.type);
    HashAdd(&entry->sym.hash, table);
  }
  for (const auto &type : types) {
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
      {"NULL", "0"}, {"TRUE", "1"}, {"FALSE", "0"}, {"ON", "1"}, {"OFF", "0"},
      {"__DIR__", "#exe{StreamDir;}"},
      {"I64_MAX", "0x7FFFFFFFFFFFFFFF"},
      {"I64_MIN", "(-0x8000000000000000)"},
      {"INVALID_PTR", "I64_MAX"},
      {"LDF_NO_ABSS", "1"}, {"LDF_JUST_LOAD", "2"}, {"LDF_SILENT", "4"},
      {"BIN_SIGNATURE_VAL", "'TOSB'"},
      {"F64_MIN", "(0xFFEFFFFFFFFFFFFF(F64))"},
      {"F64_MAX", "(0x7FEFFFFFFFFFFFFF(F64))"},
      {"HTT_CLASS", "0x10"}, {"HTT_INTERNAL_TYPE", "0x20"},
      {"HTT_FUN", "0x40"}, {"MLMF_IS_STR", "1"},
      {"SUF_REM_CTRL_CHARS", "0x001"}, {"SUF_REM_LEADING", "0x002"},
      {"SUF_REM_TRAILING", "0x004"}, {"SUF_REM_SPACES", "0x008"},
      {"SUF_SINGLE_SPACE", "0x010"}, {"SUF_TO_UPPER", "0x020"},
      {"SUF_TO_LOWER", "0x040"}, {"SUF_S2T", "0x080"},
      {"SUF_T2S", "0x100"}, {"SUF_SCALE_INDENT", "0x200"},
      {"SUF_SAFE_DOLLAR", "0x400"}, {"SFF_IGNORE_CASE", "1"},
      {"SFF_WHOLE_LABELS_BEFORE", "2"}, {"SFF_WHOLE_LABELS_AFTER", "4"},
      {"SFG_WHOLE_LABELS", "6"},
      {"pi", "(0x400921FB54442D18(F64))"},
      {"BLACK","0"}, {"BLUE","1"}, {"GREEN","2"}, {"CYAN","3"},
      {"RED","4"}, {"PURPLE","5"}, {"BROWN","6"}, {"LTGRAY","7"},
      {"DKGRAY","8"}, {"LTBLUE","9"}, {"LTGREEN","10"}, {"LTCYAN","11"},
      {"LTRED","12"}, {"LTPURPLE","13"}, {"YELLOW","14"}, {"WHITE","15"},
      {"COLORS_NUM","16"},
      {"TRANSPARENT","0xFF"}, {"COLOR_INVALID","16"}, {"COLOR_MONO","0xFF"},
      {"\xE3", "(0x400921FB54442D18(F64))"},
      {"GR_WIDTH","640"}, {"GR_HEIGHT","480"}, {"GR_SCALE","(1<<32)"},
      {"GR_PEN_BRUSHES_NUM","64"},
      {"SPT_END","0"}, {"SPT_COLOR","1"}, {"SPT_DITHER_COLOR","2"},
      {"SPT_THICK","3"}, {"SPT_PLANAR_SYMMETRY","4"}, {"SPT_TRANSFORM_ON","5"},
      {"SPT_TRANSFORM_OFF","6"}, {"SPT_SHIFT","7"}, {"SPT_PT","8"},
      {"SPT_POLYPT","9"}, {"SPT_LINE","10"}, {"SPT_POLYLINE","11"},
      {"SPT_RECT","12"}, {"SPT_FLOOD_FILL","21"}, {"SPT_FLOOD_FILL_NOT","22"},
      {"SPT_BITMAP","23"}, {"SPT_ARROW","26"},
      {"TEXT_COLS","80"}, {"TEXT_ROWS","60"}, {"TASKf_NONTIMER_RAND","14"},
      {"TASKf_BREAK_LOCKED","10"}, {"TASKf_PENDING_BREAK","11"}, {"TASKf_BREAK_TO_SHIFT_ESC","12"},
      {"ATTRF_BLINK","0x10000000"}, {"ATTRF_INVERT","0x20000000"},
      {"ATTRF_SEL","0x40000000"}, {"ATTRF_UNDERLINE","0x80000000"},
      {"ATTRf_BLINK","28"}, {"ATTRf_INVERT","29"},
      {"ATTRf_SEL","30"}, {"ATTRf_UNDERLINE","31"},
      {"FONT_WIDTH","8"}, {"FONT_HEIGHT","8"},
      {"DCF_COMPRESSED","1"}, {"DCF_PALETTE","2"}, {"DCF_NO_TRANSPARENTS","4"},
      {"DCF_TRANSFORMATION","0x100"}, {"DCF_SYMMETRY","0x200"},
      {"DCF_JUST_MIRROR","0x400"}, {"DCF_LOCATE_NEAREST","0x800"},
      {"DCF_DONT_DRAW","0x1000"}, {"DCF_ALIAS","0x2000"},
      {"DCF_SCRN_BITMAP","0x4000"}, {"DCF_FILL_NOT_COLOR","0x8000"},
      {"DCF_RECORD_EXTENTS","0x10000"}, {"DCF_ON_TOP","0x20000"},
      {"DCSF_COMPRESSED","1"}, {"DCSF_PALETTE_GET","2"},
      {"DCS_SIGNATURE_VAL","0x53437644"},
      {"ROPBF_HALF_RANGE_COLOR","0x10"}, {"ROPBF_TWO_SIDED","0x20"},
      {"ROP_EQU","0"}, {"ROP_XOR","0x100"}, {"ROP_COLLISION","0x200"},
      {"ROP_MONO","0x300"}, {"ROPF_HALF_RANGE_COLOR","0x1000"},
      {"ROPF_TWO_SIDED","0x2000"}, {"ROPF_DITHER","0x40000000"},
      {"ROPF_PROBABILITY_DITHER","0x80000000"},
      {"CH_ESC", "0x1B"},
      {"CH_SHIFT_ESC", "0x1C"},
      {"GSF_SHIFT_ESC_EXIT", "1"},
      {"GSF_WITH_NEW_LINE", "2"},
      {"DOCF_BORDER_DOC", "0x100"},
      {"DOCF_HIDE_CURSOR", "0x800"},
      {"DOCF_NO_SCROLL_BARS", "0x2000"}, {"TSF_SAME_SONG", "1"},
      {"CDATE_FREQ", "49710"},
      {"STR_LEN", "144"}, {"CDIR_FILENAME_LEN", "38"}, {"CDIR_SIZE", "64"},
      {"BLK_SIZE_BITS", "9"}, {"BLK_SIZE", "512"}, {"DVD_BLK_SIZE", "2048"},
      {"BDT_NULL", "0"}, {"BDT_RAM", "1"}, {"BDT_ATA", "2"},
      {"BDT_ISO_FILE_READ", "3"}, {"BDT_ISO_FILE_WRITE", "4"}, {"BDT_ATAPI", "5"},
      {"BDT_TYPES_NUM", "6"}, {"DRVS_NUM", "26"}, {"BLKDEVS_NUM", "26"},
      {"BD_SIGNATURE_VAL", "0x56534442"}, {"DRV_SIGNATURE_VAL", "0x56535644"},
      {"SK_NAME", "0"}, {"SK_CLUS", "1"},
      {"BDF_INITIALIZED", "2"}, {"BDF_REMOVABLE", "1"}, {"BDF_READ_CACHE", "32"},
      {"DVlf_LOCKED", "0"}, {"BDlf_LOCKED", "0"},
      {"DSK_CACHE_HASH_SIZE", "0x2000"}, {"SEMA_DSK_CACHE", "11"},
      {"BDF_INIT_IN_PROGRESS", "128"},
      {"DOF_SIZE_MIN", "1"}, {"DOF_INTERCEPT_TASK_END", "2"},
      {"DOF_DONT_HOME", "4"}, {"DOF_WIN_MAX", "8"}, {"DOF_DONT_TEXT_ATTR", "16"},
      {"DOF_DONT_WINMGR_SYNC", "32"}, {"DOF_DONT_SHOW", "64"},
      {"DOCF_FORM", "0x200"}, {"DOCF_SIZE_MIN", "0x400"},
      {"DOCF_OVERSTRIKE", "0x100000000"},
      {"RS_ATTR_DIR", "0x10"}, {"RS_ATTR_DELETED", "0x100"},
      {"BDF_READ_ONLY", "4"}, {"BDF_READ_ONLY_OVERRIDE", "8"},
      {"BDF_LAST_WAS_WRITE", "16"}, {"RS_ATTR_READ_ONLY", "1"},
      {"FSt_FAT32", "2"}, {"FSt_REDSEA", "1"},
      {"FStf_DISABLE", "15"}, {"FSG_TYPE_MASK", "0x7FFF"},
      {"RS_ATTR_RESIDENT", "0x200"}, {"RS_ATTR_COMPRESSED", "0x400"},
      {"RS_ATTR_CONTIGUOUS", "0x800"},
      {"FFB_NEXT_BLK", "0x7FFFFFFFFFFFFFFF"}, {"FF_WRITE", "1"},
      {"FF_NEW_FILE", "2"}, {"FF_BUF_DIRTY", "4"}, {"FF_NEEDS_WRITE", "8"},
      {"FF_CONTIGUOUS", "16"}, {"FF_USE_OLD_DATETIME", "32"},
      {"FUF_RECURSE", "1"}, {"FUF_SINGLE", "0x200"},
      {"FUF_JUST_DIRS", "0x400"}, {"FUF_JUST_FILES", "0x800"},
      {"FUF_JUST_TXT", "0x1000"}, {"FUF_JUST_DD", "0x2000"},
      {"FUF_JUST_SRC", "0x4000"}, {"FUF_JUST_AOT", "0x8000"},
      {"FUF_JUST_JIT", "0x10000"}, {"FUF_JUST_GR", "0x20000"},
      {"FUF_Z_OR_NOT_Z", "0x40000"}, {"FUF_CLUS_ORDER", "0x80000"},
      {"FUF_SCAN_PARENTS", "0x100000"}, {"FUF_FLATTEN_TREE", "0x200000"},
      {"DOCF_DONT_HIGHLIGHT_CURSOR", "0x1000"},
      {"DOCF_DONT_SHOW", "0x8000"},
      {"DOCF_BWD_MOVEMENT", "0x40000"},
      {"DOCF_DO_FULL_REFRESH", "0x200000"}, {"TASKf_IDLE", "3"},
      {"RECALCt_FIND_CURSOR", "1"},
      {"DOCT_DATA", "30"}, {"DOCT_CHECK_BOX", "31"}, {"DOCT_LST", "32"},
      {"DOCT_TREE", "36"},
      {"DOCEF_TAG", "1"}, {"DOCEF_AUX_STR", "4"}, {"DOCEF_DEFINE", "8"},
      {"DOCEF_CHECKED_COLLAPSED", "(1<<43)"},
      {"DOCEF_REFRESH_DATA", "(1<<45)"}, {"DOCEF_UPDATE_DATA", "(1<<46)"},
      {"DOCEF_DEREF_DATA", "(1<<47)"}, {"DOCEF_HAS_TERMINATOR", "(1<<49)"},
      {"DOCEF_REMALLOC_DATA", "(1<<48)"}, {"DOCEF_LST", "(1<<53)"},
      {"RT_I0", "2"}, {"RT_U0", "3"},
      {"RT_I8", "4"}, {"RT_U8", "5"}, {"RT_I16", "6"}, {"RT_U16", "7"},
      {"RT_I32", "8"}, {"RT_U32", "9"}, {"RT_I64", "10"}, {"RT_PTR", "10"},
      {"RT_U64", "11"}, {"RT_F64", "14"},
      {"RECALCt_TO_SCRN", "2"},
      {"RECALCF_HAS_CURSOR", "0x100"},
      {"DISPLAYf_SHOW", "0"},
      {"DISPLAYf_NO_BORDER", "3"},
      {"WIF_SELF_FOCUS", "0x1"},
      {"WIF_SELF_CTRLS", "0x4"},
      {"WIF_SELF_MS_L", "0x8"},
      {"WIF_SELF_MS_R", "0x20"},
      {"WIF_SELF_MS_WHEEL", "0x80"},
      {"CTRLT_GENERIC", "0"},
      {"CTRLT_WIN_HSCROLL", "1"},
      {"CTRLT_WIN_VSCROLL", "2"},
      {"CTRLT_VIEWING_ANGLES", "3"},
      {"VIEWANGLES_SPACING", "22"}, {"VIEWANGLES_RANGE", "48"},
      {"VIEWANGLES_BORDER", "2"}, {"VIEWANGLES_SNAP", "2"},
      {"CTRLF_SHOW", "1"},
      {"CTRLF_BORDER", "2"},
      {"CTRLF_CAPTURE_LEFT_MS", "4"},
      {"CTRLF_CAPTURE_RIGHT_MS", "8"},
      {"CTRLF_CLICKED", "16"},
      {"WSSf_SET_TO_POS", "0"},
      {"WIF_SELF_BORDER", "0x100"},
      {"WIF_SELF_GRAB_SCROLL", "0x200"},
      {"WIF_SELF_DOC", "0x400"},
      {"WIF_SELF_ODE", "0x800"},
      {"WIF_SELF_KEY_DESC", "0x1000"},
      {"WIF_FOCUS_TASK_MENU", "0x20000"},
      {"WIF_FOCUS_TASK_CTRLS", "0x40000"},
      {"WIF_FOCUS_TASK_MS_L", "0x80000"},
      {"WIF_FOCUS_TASK_MS_L_D", "0x100000"},
      {"WIF_FOCUS_TASK_MS_R", "0x200000"},
      {"WIF_FOCUS_TASK_MS_R_D", "0x400000"},
      {"WIF_FOCUS_TASK_MS_WHEEL", "0x800000"},
      {"WIF_FOCUS_TASK_BORDER", "0x1000000"},
      {"WIF_FOCUS_TASK_GRAB_SCROLL", "0x2000000"},
      {"WIG_DBL_CLICK", "(WIF_FOCUS_TASK_MS_L_D|WIF_FOCUS_TASK_MS_R_D)"},
      {"WIG_TASK_DFT", "(WIF_FOCUS_TASK_MENU|WIG_DBL_CLICK|0xFFFF-WIF_SELF_DOC-WIF_SELF_ODE)"},
      {"WIG_NO_FOCUS_TASK_DFT",
       "(WIG_TASK_DFT-WIF_SELF_BORDER-WIF_SELF_MS_L-WIF_SELF_MS_R-WIG_DBL_CLICK)"},
      {"WIG_USER_TASK_DFT", "WIF_SELF_KEY_DESC"},
      {"WIf_SELF_FOCUS", "0"},
      {"WIf_SELF_CTRLS", "2"},
      {"WIf_SELF_MS_L", "3"},
      {"WIf_SELF_MS_R", "5"},
      {"WIf_SELF_MS_WHEEL", "7"},
      {"WIf_SELF_BORDER", "8"},
      {"WIf_SELF_GRAB_SCROLL", "9"},
      {"WIf_SELF_DOC", "10"},
      {"WIf_SELF_ODE", "11"},
      {"WIf_SELF_KEY_DESC", "12"},
      {"WIf_FOCUS_TASK_MENU", "17"},
      {"WIf_FOCUS_TASK_CTRLS", "18"},
      {"WIf_FOCUS_TASK_MS_L", "19"},
      {"WIf_FOCUS_TASK_MS_L_D", "20"},
      {"WIf_FOCUS_TASK_MS_R", "21"},
      {"WIf_FOCUS_TASK_MS_R_D", "22"},
      {"WIf_FOCUS_TASK_MS_WHEEL", "23"},
      {"WIf_FOCUS_TASK_BORDER", "24"},
      {"WIf_FOCUS_TASK_GRAB_SCROLL", "25"},
      {"SC_F1", "0x3B"},
      {"SC_CURSOR_UP", "0x48"},
      {"SC_CURSOR_LEFT", "0x4B"},
      {"SC_CURSOR_RIGHT", "0x4D"},
      {"SC_CURSOR_DOWN", "0x50"},
      {"SC_INS", "0x52"},
      {"SC_DELETE", "0x53"},
      {"SC_GUI", "0xDB"},
      {"SCF_NO_SHIFT", "(1<<30)"},
      {"SCF_MS_L_DOWN", "(1<<16)"},
      {"SCF_MS_R_DOWN", "(1<<17)"},
      {"SCF_E0_PREFIX", "(1<<7)"},
      {"SCF_KEY_UP", "(1<<8)"},
      {"SCF_SHIFT", "(1<<9)"},
      {"SCF_CTRL", "(1<<10)"},
      {"SCF_ALT", "(1<<11)"},
      {"SCF_CAPS", "(1<<12)"},
      {"SCF_NUM", "(1<<13)"},
      {"SCF_SCROLL", "(1<<14)"},
      {"MSG_NULL", "0"},
      {"MSG_CMD", "1"},
      {"MSG_KEY_DOWN", "2"},
      {"MSG_KEY_UP", "3"},
      {"MSG_MS_MOVE", "4"},
      {"MSG_MS_L_DOWN", "5"},
      {"MSG_MS_L_UP", "6"},
      {"MSG_MS_L_D_DOWN", "7"},
      {"MSG_MS_L_D_UP", "8"},
      {"MSG_MS_R_DOWN", "9"},
      {"MSG_MS_R_UP", "10"},
      {"MSG_MS_R_D_DOWN", "11"},
      {"MSG_MS_R_D_UP", "12"},
      {"JOBf_WAKE_MASTER", "0"},
      {"JOBf_FOCUS_MASTER", "1"},
      {"JOBf_EXIT_ON_COMPLETE", "2"},
      {"JOBf_DONT_FILTER", "3"},
      {"JOBf_HIGHEST_PRIORITY", "4"},
      {"JOBf_DONE", "5"},
      {"JOBf_DISPATCHED", "6"},
      {"JOBf_FREE_ON_COMPLETE", "7"},
      {"JOBf_ADD_TO_QUE", "8"},
      {"JOBT_TEXT_INPUT", "0"},
      {"JOBT_MSG", "1"},
      {"JOBT_EXE_STR", "2"},
      {"JOBT_CALL", "3"},
      {"JOBT_SPAWN_TASK", "4"},
      {"MSG_KEY_DOWN_UP", "-2"},
      {"MSG_MS_L_DOWN_UP", "-5"},
      {"MSG_MS_L_D_DOWN_UP", "-7"},
      {"MSG_MS_R_DOWN_UP", "-9"},
      {"MSG_MS_R_D_DOWN_UP", "-11"},
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
  auto *task = CmpKernelClass(table, "CTask", sizeof(CTask));
  return task && CmpMemberData(table) && CmpRuntimeData(table) &&
         CmpQueData(table) && CmpFifoData(table) && CmpDateData(table) &&
         CmpBinData(table) && CmpDirData(table) &&
         CmpDocData(table, *task) && CmpTaskData(table, task) &&
         CmpKbdData(table) && CmpTextData(table) &&
         CmpMouseData(table) && CmpPaletteData(table) && CmpDCData(table) &&
         CmpScrnCastData(table) && CmpMusicData(table) &&
         CmpTaskSettingsData(table) && CmpMenuData(table, task) &&
         CmpCtrlData(table, task) && CmpClassReportData(table, *task) &&
         CmpRuntimeHeaders(table);
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
