#include "../Kernel/KernelB.hpp"
#include "../Adam/Gr/GrGlbls.hpp"
#include "CompilerA.hpp"

extern "C" const char CompilerOpCodes[];
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
  auto *heap = adam_task->data_heap;
  auto *table = HashTableNew(1024, heap);
  if (!table)
    return false;
  CCmpCtrl cc;
  CLexFile file;
  LexSourceInit(cc, file, CompilerOpCodes, 0, "::/Compiler/OpCodes.DD");
  bool okay = true;
  while (LexRaw(cc) > TK_EOF)
    if (!AsmPrsHashEntry(cc, table, heap)) {
      okay = false;
      break;
    }
  okay = okay && cc.token == TK_EOF;
  LexSourceDel(cc);
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

static bool CmpDocData(CHashTable *table)
{
  auto *doc = CmpKernelClass(table, "CDoc", sizeof(CDoc));
  auto *entry = CmpKernelClass(table, "CDocEntry", sizeof(CDocEntry));
  if (!doc || !entry)
    return false;
  auto *i32 = FindType("I32", table);
  auto *u64 = FindType("U64", table);
  auto *heap = CurrentTask()->data_heap;
  const struct
  {
    const char *name;
    CHashClass *type;
    size_t offset;
  } members[]{
      {"flags", u64, offsetof(CDoc, flags)},
      {"top_line_num", i32, offsetof(CDoc, top_line_num)},
      {"line_start_col", i32, offsetof(CDoc, line_start_col)},
  };
  for (const auto &member : members) {
    auto *name = StrNew(member.name, heap);
    if (!name || !MemberAdd(*doc, name, *member.type, member.offset)) {
      Free(name);
      return false;
    }
  }
  return true;
}

static bool CmpTaskData(CHashTable *table)
{
  auto *heap = CurrentTask()->data_heap;
  auto *cls = PrsClassNew();
  if (!cls)
    return false;
  cls->sym.hash.type |= HTF_PUBLIC;
  cls->sym.hash.str = StrNew("CTask", heap);
  cls->size = sizeof(CTask);
  if (!cls->sym.hash.str) {
    HashDel(&cls->sym.hash);
    return false;
  }
  HashAdd(&cls->sym.hash, table);
  auto *u32 = FindType("U32", table);
  auto *i64 = FindType("I64", table);
  auto *u8 = FindType("U8", table);
  struct {
    const char *name;
    CHashClass *type;
    size_t offset;
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
      {"win_z_num", FindType("U16",table), offsetof(CTask, win_z_num)},
      {"scroll_x", i64, offsetof(CTask, scroll_x)},
      {"scroll_y", i64, offsetof(CTask, scroll_y)},
      {"wake_jiffy", i64, offsetof(CTask, wake_jiffy)},
      {"task_flags", u32, offsetof(CTask, task_flags)},
      {"display_flags", u32, offsetof(CTask, display_flags)},
      {"text_attr", u8, offsetof(CTask, text_attr)},
      {"border_attr", u8, offsetof(CTask, border_attr)},
      {"put_doc", FindType("CDoc", table) + 1, offsetof(CTask, put_doc)},
      {"display_doc", FindType("CDoc", table) + 1, offsetof(CTask, display_doc)},
      {"border_doc", FindType("CDoc", table) + 1, offsetof(CTask, border_doc)},
      {"win_left", i64, offsetof(CTask, win_left)},
      {"win_right", i64, offsetof(CTask, win_right)},
      {"win_top", i64, offsetof(CTask, win_top)},
      {"win_bottom", i64, offsetof(CTask, win_bottom)},
      {"cur_dir", u8 + 1, offsetof(CTask, cur_dir)},
      {"parent_task", cls + 1, offsetof(CTask, parent_task)},
      {"next_task", cls + 1, offsetof(CTask, next_task)},
      {"last_task", cls + 1, offsetof(CTask, last_task)},
      {"next_child_task", cls + 1, offsetof(CTask, next_child_task)},
      {"last_child_task", cls + 1, offsetof(CTask, last_child_task)},
      {"swap_cnter", i64, offsetof(CTask, swap_cnter)},
      {"task_num", i64, offsetof(CTask, task_num)},
      {"user_data", i64, offsetof(CTask, user_data)},
  };
  for (const auto &member : members) {
    auto *name = StrNew(member.name, heap);
    if (!name || !MemberAdd(*cls, name, *member.type, member.offset)) {
      Free(name);
      return false;
    }
  }
  return CmpKernelGlobal(table, "sys_winmgr_task", cls + 1, &sys_winmgr_task) &&
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
    size_t count = 0;
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
      entry->dim.total_cnt = member.count;
      entry->size = member.type->size * member.count;
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
  for (const auto &callback : callbacks) {
    CPrsDeclarator decl;
    decl.name = StrNew(callback.name, heap);
    decl.fun_ptr = PrsFunPtrNew(*u0, 1, FF_RET1);
    if (!decl.name || !decl.fun_ptr)
      return false;
    auto *entry = MemberAdd(*glbl, decl.name, u0[1], callback.offset);
    if (!entry)
      return false;
    auto &signature = *FunPtrBase(decl.fun_ptr);
    decl.Bind(*entry);
    for (unsigned i = 0; i < callback.argc; i++) {
      auto *name = StrNew(args[i], heap);
      auto *arg = name ? MemberAdd(signature.cls, name, *types[i], 16 + 8 * i)
                       : nullptr;
      if (!arg) {
        Free(name);
        return false;
      }
      ++signature.arg_cnt;
    }
  }

  auto *wm = CmpKernelClass(table, "CWinMgrGlbls", sizeof(CWinMgrGlbls));
  if (!wm)
    return false;
  constexpr struct
  {
    const char *name;
    size_t offset;
  } options[]{
      {"grab_scroll", offsetof(CWinMgrGlbls, grab_scroll)},
      {"grab_scroll_closed", offsetof(CWinMgrGlbls, grab_scroll_closed)},
  };
  for (const auto &member : options) {
    auto *name = StrNew(member.name, heap);
    if (!name || !MemberAdd(*wm, name, *boolean, member.offset)) {
      Free(name);
      return false;
    }
  }
  return CmpKernelGlobal(table, "gr", glbl, &gr) &&
         CmpKernelGlobal(table, "winmgr", wm, &winmgr);
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
    ColorROP,
    D3I32Ptr,
    I16,
    U16,
    I32,
    U32,
    DocPtr,
    DocEntryPtr,
  };
  CHashClass *types[]{
      FindType("U0", table),     FindType("U64", table),
      FindType("I64", table),    FindType("I8", table),
      FindType("U8", table),     FindType("F64", table),    FindType("Bool", table),
      FindType("U8", table) + 1, FindType("CTask", table) + 1,
      FindType("CJob", table) + 1, FindType("I64", table) + 1,
      FindType("CBGR48", table), FindType("CBGR48", table) + 1,
      FindType("CDC", table) + 1, FindType("I32", table) + 1,
      FindType("CColorROPU32", table), FindType("CD3I32", table) + 1,
      FindType("I16", table), FindType("U16", table),
      FindType("I32", table), FindType("U32", table),
      FindType("CDoc", table) + 1, FindType("CDocEntry", table) + 1,
  };
  auto *task = CurrentTask();
  static constexpr struct
  {
    const char *name;
    uint8_t result;
    uint16_t flags;
    struct
    {
      const char *name;
      uint8_t type;
      bool has_default = false;
      uint64_t dft_val = 0;
      const char *dft_str = nullptr;
    } args[8];
    const char *import_name = nullptr;
    uint8_t internal_op = 0;
  } headers[]{
      {"Print", U0, FF_DOT_DOT_DOT, {{"fmt", U8Ptr}}},
      {"MStrPrint", U8Ptr, FF_DOT_DOT_DOT, {{"fmt", U8Ptr}}},
      {"Free", U0, FF_RET1, {{"addr", U8Ptr}}, "_FREE"},
      {"MemCpy", U8Ptr, FF_RET1,
       {{"dst", U8Ptr}, {"src", U8Ptr}, {"cnt", I64}}, "_MEMCPY"},
      {"PutHex", U0, FF_RET1, {{"num", U64}, {"width", I64}}},
      {"PutChars", U0, FF_RET1, {{"ch", U64}}},
      {"PutS", U0, FF_RET1, {{"st", U8Ptr}}},
      {"tS", F64, FF_RET1, {}},
      {"Busy", U0, FF_RET1, {{"microseconds", I64}}},
      {"SleepUntil", U0, FF_RET1, {{"wake_jiffy", I64}}},
      {"Sleep", U0, FF_RET1, {{"mS", I64}}},
      {"Fs", TaskPtr, FF_INTERNAL, {}, nullptr, IC_FS},
      {"GetRFlags", I64, FF_INTERNAL, {}, nullptr, IC_GET_RFLAGS},
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
      {"WinZBufUpdate", U0, FF_RET1, {}},
      {"TextChar", Bool, FF_RET1,
       {{"task", TaskPtr}, {"allow_border", Bool, true}, {"x", I64}, {"y", I64}, {"d", I64}},
       "_TEXT_CHAR"},
      {"TextLenStr", Bool, FF_RET1,
       {{"task", TaskPtr}, {"x", I64}, {"y", I64}, {"len", I64}, {"attr", I64}, {"s", U8Ptr}},
       "_TEXT_LEN_STR"},
      {"TextLenAttr", Bool, FF_RET1,
       {{"task", TaskPtr}, {"x", I64}, {"y", I64}, {"len", I64}, {"attr", I64}},
       "_TEXT_LEN_ATTR"},
      {"TextRect", U0, FF_RET1, {{"l", I64}, {"r", I64}, {"t", I64}, {"b", I64}, {"d", I64}}},
      {"TextBorder", U0, FF_RET1,
       {{"task", TaskPtr, true}, {"l", I64}, {"r", I64}, {"t", I64}, {"b", I64},
        {"attr", I64}, {"solid", Bool}}},
      {"DocNew", DocPtr, FF_RET1, {{"filename", U8Ptr, true}, {"task", TaskPtr, true}}},
      {"DocPut", DocPtr, FF_RET1, {{"task", TaskPtr, true}}},
      {"DocDisplay", DocPtr, FF_RET1, {{"task", TaskPtr, true}}},
      {"DocBorder", DocPtr, FF_RET1, {{"task", TaskPtr, true}}},
      {"DocDel", U0, FF_RET1, {{"doc", DocPtr}}},
      {"DocRecalc", Bool, FF_RET1, {{"doc", DocPtr}, {"recalc_flags", I64, true}}},
      {"DocPutKey", U0, FF_RET1, {{"doc", DocPtr}, {"ch", I64, true}, {"sc", I64, true}}},
      {"DocPrint", DocEntryPtr, FF_DOT_DOT_DOT,
       {{"doc", DocPtr, true}, {"fmt", U8Ptr}}},
      {"DrawStdMs", U0, FF_RET1, {{"dc", DCPtr}, {"x", I64}, {"y", I64}}},
      {"PaletteSetStd", U0, FF_RET1, {}},
      {"ClampI64", I64, FF_RET1, {{"i", I64}, {"lo", I64}, {"hi", I64}}, "_CLAMP_I64"},
      {"SignI64", I64, FF_RET1, {{"i", I64}}},
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
      {"GrPeek", I64, FF_RET1, {{"dc", DCPtr, true}, {"x", I64}, {"y", I64}}},
      {"GrLine", Bool, FF_RET1,
       {{"dc", DCPtr, true}, {"x1", I64}, {"y1", I64}, {"x2", I64},
        {"y2", I64}, {"step", I64, true, 1}, {"start", I64, true}}},
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
      {"Mat4x4Equ", I64Ptr, FF_RET1, {{"dst", I64Ptr}, {"src", I64Ptr}}},
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
      {"Beep", U0, FF_RET1, {{"ona", I8, true, 62}, {"busy", Bool, true}}},
      {"Mute", Bool, FF_RET1, {{"val", Bool}}},
      {"IsMute", Bool, FF_RET1, {}},
      {"Call", I64, FF_RET1, {{"addr", U64}}},
      {"StreamPrint", U0, FF_DOT_DOT_DOT, {{"fmt", U8Ptr}}},
      {"StreamExePrint", I64, FF_DOT_DOT_DOT, {{"fmt", U8Ptr}}},
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
        member->dft_val = arg.dft_val;
      }
      if (arg.dft_str) {
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
    fun->exe_addr = reinterpret_cast<uint8_t *>(symbol ? symbol->val : uintptr_t{header.internal_op});
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
      {"NULL", "0"}, {"TRUE", "1"}, {"FALSE", "0"}, {"ON", "1"}, {"OFF", "0"},
      {"BLACK","0"}, {"BLUE","1"}, {"GREEN","2"}, {"CYAN","3"},
      {"RED","4"}, {"PURPLE","5"}, {"BROWN","6"}, {"LTGRAY","7"},
      {"DKGRAY","8"}, {"LTBLUE","9"}, {"LTGREEN","10"}, {"LTCYAN","11"},
      {"LTRED","12"}, {"LTPURPLE","13"}, {"YELLOW","14"}, {"WHITE","15"},
      {"COLORS_NUM","16"},
      {"TRANSPARENT","0xFF"}, {"COLOR_MONO","0xFF"},
      {"GR_WIDTH","640"}, {"GR_HEIGHT","480"}, {"GR_SCALE","(1<<32)"},
      {"TEXT_COLS","80"}, {"TEXT_ROWS","60"}, {"TASKf_NONTIMER_RAND","14"},
      {"DCF_COMPRESSED","1"}, {"DCF_PALETTE","2"}, {"DCF_NO_TRANSPARENTS","4"},
      {"DCF_TRANSFORMATION","0x100"}, {"DCF_SYMMETRY","0x200"},
      {"DCF_JUST_MIRROR","0x400"}, {"DCF_LOCATE_NEAREST","0x800"},
      {"DCF_DONT_DRAW","0x1000"}, {"DCF_ALIAS","0x2000"},
      {"DCF_SCRN_BITMAP","0x4000"}, {"DCF_FILL_NOT_COLOR","0x8000"},
      {"DCF_RECORD_EXTENTS","0x10000"}, {"DCF_ON_TOP","0x20000"},
      {"DCSF_COMPRESSED","1"}, {"DCSF_PALETTE_GET","2"},
      {"DCS_SIGNATURE_VAL","0x53437644"},
      {"ROP_EQU","0"}, {"ROP_XOR","0x100"}, {"ROP_COLLISION","0x200"},
      {"ROP_MONO","0x300"}, {"ROPF_HALF_RANGE_COLOR","0x1000"},
      {"ROPF_TWO_SIDED","0x2000"}, {"ROPF_DITHER","0x40000000"},
      {"ROPF_PROBABILITY_DITHER","0x80000000"},
      {"CH_ESC", "0x1B"},
      {"CH_SHIFT_ESC", "0x1C"},
      {"DOCF_BORDER_DOC", "0x100"},
      {"DOCF_HIDE_CURSOR", "0x800"},
      {"DOCF_DONT_HIGHLIGHT_CURSOR", "0x1000"},
      {"DOCF_DONT_SHOW", "0x8000"},
      {"RECALCt_FIND_CURSOR", "1"},
      {"RECALCt_TO_SCRN", "2"},
      {"RECALCF_HAS_CURSOR", "0x100"},
      {"DISPLAYf_SHOW", "0"},
      {"DISPLAYf_NO_BORDER", "3"},
      {"WIF_SELF_DOC", "0x400"},
      {"WIF_SELF_FOCUS", "1"},
      {"WIF_FOCUS_TASK_MS_L", "0x80000"},
      {"WIF_SELF_MS_L", "0x0008"},
      {"WIF_SELF_MS_R", "0x0020"},
      {"WIF_FOCUS_TASK_MS_L_D", "0x100000"},
      {"WIF_FOCUS_TASK_MS_R_D", "0x400000"},
      {"SCF_MS_L_DOWN", "(1<<16)"},
      {"SCF_MS_R_DOWN", "(1<<17)"},
      {"WIG_USER_TASK_DFT", "0x1000"},
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
  return CmpRuntimeData(table) && CmpDocData(table) && CmpTaskData(table) &&
         CmpKbdData(table) &&
         CmpMouseData(table) && CmpPaletteData(table) && CmpDCData(table) &&
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
