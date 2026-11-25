#include "../Kernel/KernelB.hpp"
#include "CompilerA.hpp"

static int64_t Keyword(const CCmpCtrl &cc)
{
  if (cc.token != TK_IDENT || !cc.hash_entry ||
      !(cc.hash_entry->type & HTT_KEYWORD))
    return -1;
  return reinterpret_cast<const CHashGeneric *>(cc.hash_entry)->user_data0;
}

static bool PrsClassBody(CCmpCtrl &cc, CHashClass &owner, bool overlay,
                         unsigned depth)
{
  if (depth == 127 || cc.token != '{')
    return false;
  auto origin = owner.size;
  for (Lex(cc); cc.token != '}';) {
    if (cc.token == ';') {
      Lex(cc);
      continue;
    }
    if (Keyword(cc) == KW_UNION) {
      Lex(cc);
      if (!PrsClassBody(cc, owner, true, depth + 1))
        return false;
      Lex(cc);
      continue;
    }
    auto *base = PrsClass(cc);
    if (!base)
      return false;
    do {
      Lex(cc);
      auto *cls = PrsStars(cc, base);
      if (!cls || !ClassComplete(*cls) || cc.token != TK_IDENT ||
          MemberFind(cc.cur_str, &owner))
        return false;
      auto offset = overlay ? origin : owner.size;
      auto *member = MemberAdd(owner, cc.cur_str, *cls, offset);
      if (!member)
        return false;
      cc.cur_str = nullptr;
      Lex(cc);
      if (!PrsArrayDims(cc, member->dim, cls->size))
        return false;
      member->size = cls->size * member->dim.total_cnt;
      if (offset > INT32_MAX - member->size)
        return false;
      auto end = offset + member->size;
      if (end > owner.size)
        owner.size = end;
    } while (cc.token == ',');
    if (cc.token != ';')
      return false;
    Lex(cc);
  }
  return true;
}

static CHashClass *PrsClassDef(CCmpCtrl &cc, bool overlay, bool forward,
                               uint32_t visibility, CHashClass *scalar)
{
  auto *table = cc.htc.glbl_hash_table;
  if (!table || Lex(cc) != TK_IDENT ||
      (scalar && (scalar->ptr_stars_cnt || !TypeSize(ClassRawType(*scalar)))))
    return nullptr;
  auto *cls = reinterpret_cast<CHashClass *>(
      HashSingleTableFind(cc.cur_str, table, HTT_CLASS));
  bool reuse = cls && (cls->flags & CF_EXTERN);
  if (!reuse) {
    cls = PrsClassNew();
    if (!cls)
      return nullptr;
    cls->sym.hash.str = cc.cur_str;
    cc.cur_str = nullptr;
    cls->flags = CF_EXTERN;
    HashAdd(&cls->sym.hash, table);
  }
  Lex(cc);
  if (forward && cc.token == ';')
    return cls;
  bool okay = !forward;
  if (okay && cc.token == ':') {
    Lex(cc);
    auto *base = PrsClass(cc);
    okay = base && !scalar && ClassComplete(*base) &&
           (base->sym.hash.type & HTT_CLASS);
    if (okay) {
      cls->base_class = base;
      cls->size = base->size;
      Lex(cc);
    }
  }
  okay = okay && PrsClassBody(cc, *cls, overlay, 0) && cls->size &&
         (!scalar || cls->size == scalar->size);
  if (!okay) {
    if (reuse) {
      ClassMemberLstDel(cls);
      cls->last_in_member_lst = nullptr;
      cls->base_class = nullptr;
    } else {
      HashRemove(&cls->sym.hash, table);
      HashDel(&cls->sym.hash);
    }
    return nullptr;
  }
  if (scalar) {
    cls->raw_type = ClassRawType(*scalar);
    cls->fwd_class = scalar;
  }
  cls->flags &= ~CF_EXTERN;
  cls->sym.hash.type |= visibility;
  HashSrcFileSetCpp(&cc, &cls->sym, 0);
  Lex(cc);
  return cls;
}

static bool PrsFunArgs(CCmpCtrl &cc, CHashFun &fun)
{
  Lex(cc); // Skip '('.
  if (cc.token == ')')
    return true;
  do {
    auto *cls = PrsClass(cc);
    if (!cls)
      return false;
    Lex(cc);
    cls = PrsStars(cc, cls);
    if (!cls || !TypeSize(ClassRawType(*cls)) || cc.token != TK_IDENT ||
        fun.arg_cnt >= UINT16_MAX / 8)
      return false;
    auto *member = MemberAdd(fun.cls, cc.cur_str, *cls, 16 + 8 * fun.arg_cnt);
    if (!member)
      return false;
    cc.cur_str = nullptr;
    member->size = 8;
    ++fun.arg_cnt;
    if (Lex(cc) == '=') {
      Lex(cc);
      auto *machine = LexTypedExpression2Bin(cc, ClassRawType(*cls));
      if (!machine || (cc.token != ')' && cc.token != ',')) {
        Free(machine);
        return false;
      }
      auto okay = CmpCall(cc, machine, member->dft_val);
      Free(machine);
      if (!okay)
        return false;
      member->flags |= MLF_DFT_AVAILABLE;
    }
    if (cc.token == ')')
      return true;
    if (cc.token != ',')
      return false;
    Lex(cc);
  } while (true);
}

struct CSwitchCase
{
  CSwitchCase *next;
  int64_t lo, hi;
  uint32_t label;
};

struct CSwitchParse
{
  ~CSwitchParse()
  {
    while (cases) {
      auto *c = cases;
      cases = c->next;
      Free(c);
    }
  }

  CSwitchCase *cases = nullptr;
  int64_t lo = INT64_MAX, hi = INT64_MIN, previous = INT64_MIN;
  uint32_t dft, brk;
  bool has_default = false;
};

struct CSubSwitch
{
  const CSubSwitch *parent;
  uint32_t start, brk;
};

class CPrsStmt
{
public:
  CPrsStmt(CCmpCtrl &cc, CCodeCtrl &coc) : cc(cc), coc(coc)
  {
    if (cc.htc.fun)
      leave = coc.LabelNew();
  }

  bool Body()
  {
    if (!Block(0, NO_IC))
      return false;
    coc.Append(leave);
    return !coc.failed;
  }

  bool Statement(unsigned depth, uint32_t brk)
  {
    if (depth >= 127 || cc.token <= TK_EOF || coc.failed)
      return false;
    if (cc.token == ';')
      return Consume(';');
    if (cc.token == '{')
      return Block(depth, brk) && Consume('}');
    if (auto *cls = PrsClass(cc))
      return cc.htc.fun && PrsLocal(*cc.htc.fun, *cls);
    auto keyword = Keyword(cc);
    switch (keyword) {
    case KW_ASM:
      return cc.htc.fun && PrsAsmBlk(cc, cc.aot, &coc);
    case KW_STATIC:
      Lex(cc);
      if (auto *cls = PrsClass(cc))
        return cc.htc.fun && PrsLocal(*cc.htc.fun, *cls, true);
      return false;
    case KW_IF:
      Lex(cc);
      return PrsIf(depth, brk);
    case KW_FOR:
    case KW_WHILE:
    case KW_DO:
      Lex(cc);
      return PrsLoop(keyword, depth);
    case KW_SWITCH:
      return PrsSwitch(depth);
    case KW_BREAK:
      if (brk == NO_IC)
        return false;
      Lex(cc);
      coc.Append(IC_JMP, NO_IC, brk);
      return Consume(';');
    case KW_GOTO:
      return cc.htc.fun && PrsGoTo();
    case KW_RETURN:
      if (leave == NO_IC)
        return false;
      if (Lex(cc) == ';')
        coc.Append(IC_RETURN_VAL, NO_IC, leave);
      else {
        auto value = PrsExpression(cc, coc);
        if (value == NO_IC || coc.code[value].type == RT_AGGREGATE)
          return false;
        coc.Append(IC_RETURN_VAL, value, leave);
      }
      return Consume(';');
    default:
      if (cc.htc.fun && cc.token == TK_IDENT && !cc.local_var_entry) {
        if (IsAsmStatement(cc))
          return PrsAsmBlk(cc, cc.aot, &coc, true);
        if (!cc.hash_entry)
          return PrsGoToLabel();
      }
      return ExpressionStatement(';');
    }
  }

private:
  bool Consume(int64_t token)
  {
    if (cc.token != token)
      return false;
    return Lex(cc) != TK_ERROR;
  }

  bool Block(unsigned depth, uint32_t brk)
  {
    if (!Consume('{'))
      return false;
    while (cc.token != '}')
      if (!Statement(depth + 1, brk))
        return false;
    return true;
  }

  bool PrsGoTo()
  {
    if (Lex(cc) != TK_IDENT)
      return false;
    auto *label = coc.GoToLabelFind(cc.cur_str, true);
    if (!label || Lex(cc) != ';')
      return false;
    coc.Append(IC_JMP, NO_IC, label->ic);
    return Consume(';');
  }

  bool PrsGoToLabel()
  {
    auto *name = cc.cur_str;
    cc.cur_str = nullptr;
    auto *label = Lex(cc) == ':' ? coc.GoToLabelFind(name, true) : nullptr;
    Free(const_cast<char *>(name));
    if (!label || label->defined)
      return false;
    label->defined = true;
    coc.Append(label->ic);
    return Consume(':');
  }

  bool PrsLocal(CHashFun &fun, CHashClass &base, bool is_static = false)
  {
    do {
      Lex(cc);
      auto *cls = PrsStars(cc, &base);
      if (Keyword(cc) == KW_NOREG)
        Lex(cc);
      if (!cls || !ClassComplete(*cls) || cc.token != TK_IDENT)
        return false;
      auto *member = MemberAdd(fun.cls, cc.cur_str, *cls, 0);
      if (!member)
        return false;
      cc.cur_str = nullptr;
      Lex(cc);
      if (!PrsArrayDims(cc, member->dim, cls->size))
        return false;
      if (is_static) {
        member->flags |= MLF_STATIC;
        if (!PrsVarInit(cc, *cls, member->dim, member->static_data,
                        CurrentTask()->code_heap))
          return false;
        member->size = cls->size * member->dim.total_cnt;
        continue;
      }
      member->size = cls->size * member->dim.total_cnt;
      int64_t align = member->size >= 8   ? 8
                      : member->size >= 4 ? 4
                      : member->size >= 2 ? 2
                                          : 1;
      auto frame = (-fun.cls.size + member->size + align - 1) & -align;
      if (frame > INT32_MAX - 15)
        return false;
      member->offset = fun.cls.size = -frame;
      if (cc.token == '=') {
        if (member->dim.next || !TypeSize(ClassRawType(*cls)))
          return false;
        Lex(cc);
        auto value = PrsExpression(cc, coc);
        if (value == NO_IC || !TypeSize(coc.code[value].type))
          return false;
        auto type = ClassRawType(*cls);
        auto address = coc.Add({static_cast<uint64_t>(member->offset), NO_IC,
                                NO_IC, IC_RBP, RT_I64, false, false, true});
        auto slot = coc.LValue(address, *cls);
        auto store =
            coc.Add({0, slot, value, IC_ASSIGN, type, false, false, true});
        coc.Append(IC_END_EXP, store);
      }
    } while (cc.token == ',');
    return Consume(';');
  }

  bool Condition(int64_t end, CICode branch, uint32_t target)
  {
    auto value = PrsExpression(cc, coc);
    if (value == NO_IC || !TypeSize(coc.code[value].type) || !Consume(end))
      return false;
    return OptBranch(coc, value, branch, target);
  }

  bool PrsIf(unsigned depth, uint32_t brk)
  {
    auto otherwise = coc.LabelNew();
    if (!Consume('(') || !Condition(')', IC_BR_ZERO, otherwise) ||
        !Statement(depth + 1, brk))
      return false;
    if (Keyword(cc) != KW_ELSE) {
      coc.Append(otherwise);
      return true;
    }
    auto done = coc.LabelNew();
    coc.Append(IC_JMP, NO_IC, done);
    coc.Append(otherwise);
    Lex(cc);
    if (!Statement(depth + 1, brk))
      return false;
    coc.Append(done);
    return true;
  }

  bool PrsLoop(int64_t keyword, unsigned depth)
  {
    auto start = coc.LabelNew(), done = coc.LabelNew();
    if (keyword == KW_DO) {
      coc.Append(start);
      if (!Statement(depth + 1, done) || Keyword(cc) != KW_WHILE)
        return false;
      Lex(cc);
      if (!Consume('(') || !Condition(')', IC_BR_NOT_ZERO, start) ||
          !Consume(';'))
        return false;
    } else {
      if (!Consume('('))
        return false;
      if (keyword == KW_FOR && !Statement(depth + 1, NO_IC))
        return false;
      coc.Append(start);
      if (!Condition(keyword == KW_FOR ? ';' : ')', IC_BR_ZERO, done))
        return false;
      uint32_t step_first = NO_IC, step_last = NO_IC;
      if (keyword == KW_FOR) {
        if (cc.token == ')') {
          if (!Consume(')'))
            return false;
        } else {
          // Parse the increment before the body, then link it after the body.
          auto before = coc.last;
          if (!ExpressionStatement(')') || coc.failed)
            return false;
          step_first = coc.code[before].next;
          step_last = coc.last;
          coc.code[before].next = NO_IC;
          coc.last = before;
        }
      }
      if (!Statement(depth + 1, done))
        return false;
      if (step_first != NO_IC) {
        coc.Append(step_first);
        coc.last = step_last;
      }
      coc.Append(IC_JMP, NO_IC, start);
    }
    coc.Append(done);
    return true;
  }

  bool SwitchValue(int64_t &value)
  {
    auto *machine = LexTypedExpression2Bin(cc, RT_I64);
    if (!machine)
      return false;
    uint64_t bits;
    bool okay = CmpCall(cc, machine, bits);
    Free(machine);
    if (okay)
      value = static_cast<int64_t>(bits);
    return okay;
  }

  void SubSwitchCalls(const CSubSwitch *sub)
  {
    if (!sub)
      return;
    SubSwitchCalls(sub->parent);
    if (sub->start != NO_IC)
      coc.Append(IC_SUB_CALL, NO_IC, sub->start);
  }

  bool PrsSwitchCase(CSwitchParse &sw, const CSubSwitch *sub, bool dft)
  {
    auto label = dft ? sw.dft : coc.LabelNew();
    auto fallthrough = sub ? coc.LabelNew() : NO_IC;
    if (sub)
      coc.Append(IC_JMP, NO_IC, fallthrough);
    coc.Append(label);
    SubSwitchCalls(sub);
    coc.Append(fallthrough);
    Lex(cc);

    if (dft) {
      if (sw.has_default)
        return false;
      sw.has_default = true;
      return Consume(':');
    }

    int64_t lo, hi;
    if (cc.token == ':') {
      if (sw.previous == INT64_MAX)
        return false;
      lo = sw.previous == INT64_MIN ? 0 : sw.previous + 1;
    } else if (!SwitchValue(lo))
      return false;
    hi = lo;
    if (cc.token == TK_ELLIPSIS) {
      Lex(cc);
      if (!SwitchValue(hi))
        return false;
    }
    if (!Consume(':'))
      return false;
    if (lo > hi) {
      auto t = lo;
      lo = hi;
      hi = t;
    }
    auto *entry = reinterpret_cast<CSwitchCase *>(
        MAlloc(sizeof(CSwitchCase), CurrentTask()->data_heap));
    if (!entry)
      return false;
    *entry = {sw.cases, lo, hi, label};
    sw.cases = entry;
    if (lo < sw.lo)
      sw.lo = lo;
    if (hi > sw.hi)
      sw.hi = hi;
    sw.previous = hi;
    return !coc.failed;
  }

  bool PrsSubSwitch(CSwitchParse &sw, unsigned depth, const CSubSwitch *parent)
  {
    if (depth >= 127 || Lex(cc) != ':' || !Consume(':'))
      return false;
    CSubSwitch sub{parent, NO_IC, coc.LabelNew()};
    auto keyword = Keyword(cc);
    if (keyword != KW_CASE && keyword != KW_DFT && keyword != KW_START &&
        keyword != KW_END) {
      auto fallthrough = coc.LabelNew();
      auto jump =
          coc.Add({0, NO_IC, fallthrough, IC_JMP, RT_U0, false, false, false});
      sub.start = coc.LabelNew();
      auto entry =
          coc.Add({0, NO_IC, NO_IC, IC_SUB_ENTRY, RT_U0, false, false, false});
      coc.Append(jump);
      coc.Append(sub.start);
      coc.Append(entry);
      do {
        if (!Statement(depth + 1, NO_IC))
          return false;
        keyword = Keyword(cc);
      } while (keyword != KW_CASE && keyword != KW_DFT && keyword != KW_START &&
               keyword != KW_END);
      if (coc.failed)
        return false;
      if (keyword == KW_END) {
        coc.code[jump].code = IC_END_EXP;
        coc.code[entry].code = IC_END_EXP;
      } else {
        coc.Append(IC_RET);
        coc.Append(fallthrough);
        coc.Append(IC_SUB_CALL, NO_IC, sub.start);
      }
    }
    return PrsSwitchBody(sw, depth + 1, &sub);
  }

  bool PrsSwitchBody(CSwitchParse &sw, unsigned depth, const CSubSwitch *sub)
  {
    auto brk = sub ? sub->brk : sw.brk;
    while (!coc.failed && cc.token > TK_EOF) {
      auto keyword = Keyword(cc);
      if (keyword == KW_END || cc.token == '}') {
        if (keyword == KW_END) {
          if (!sub || Lex(cc) != ':' || !Consume(':'))
            return false;
        } else if (sub || !Consume('}'))
          return false;
        coc.Append(brk);
        return !coc.failed;
      }
      if (keyword == KW_START) {
        if (!PrsSubSwitch(sw, depth, sub))
          return false;
      } else if (keyword == KW_CASE || keyword == KW_DFT) {
        if (!PrsSwitchCase(sw, sub, keyword == KW_DFT))
          return false;
      } else if (!Statement(depth + 1, brk))
        return false;
    }
    return false;
  }

  bool PrsSwitch(unsigned depth)
  {
    auto open = Lex(cc);
    if (open != '(' && open != '[')
      return false;
    Lex(cc);
    auto value = PrsExpression(cc, coc);
    if (value == NO_IC || !TypeSize(coc.code[value].type) ||
        !Consume(open == '(' ? ')' : ']') || !Consume('{'))
      return false;
    CSwitchParse sw;
    sw.dft = coc.LabelNew();
    sw.brk = coc.LabelNew();
    auto op = open == '(' ? IC_SWITCH : IC_NOBOUND_SWITCH;
    auto dispatch = coc.Add({0, value, NO_IC, op, RT_U0, false, false, false});
    coc.Append(dispatch);
    if (!PrsSwitchBody(sw, depth, nullptr) || !sw.cases)
      return false;
    if (!sw.has_default)
      coc.Append(sw.dft);
    if (0 < sw.lo && sw.lo <= 16)
      sw.lo = 0;
    auto delta = uint64_t(sw.hi) - uint64_t(sw.lo);
    if (delta >= 0xFFFF)
      return false;
    auto range = static_cast<uint32_t>(delta + 1);
    auto *table = reinterpret_cast<CSwitchTable *>(
        MAlloc(sizeof(CSwitchTable) + range * sizeof(uint32_t),
               CurrentTask()->data_heap));
    if (!table)
      return false;
    *table = {sw.lo, range, sw.dft};
    coc.code[dispatch].data = reinterpret_cast<uintptr_t>(table);
    coc.has_misc_data = true;
    auto *targets = reinterpret_cast<uint32_t *>(table + 1);
    for (uint32_t i = 0; i < range; ++i)
      targets[i] = sw.dft;
    for (auto *c = sw.cases; c; c = c->next) {
      auto first = uint64_t(c->lo) - uint64_t(sw.lo);
      auto last = uint64_t(c->hi) - uint64_t(sw.lo);
      for (auto i = first; i <= last; ++i) {
        if (targets[i] != sw.dft)
          return false;
        targets[i] = c->label;
      }
    }
    return !coc.failed;
  }

  bool ExpressionStatement(int64_t end)
  {
    do {
      auto value = cc.token == TK_STR || cc.token == TK_CHAR_CONST
                       ? PrsPrint(cc, coc)
                       : PrsExpression(cc, coc);
      if (value == NO_IC)
        return false;
      coc.Append(IC_END_EXP, value);
    } while (cc.token == ',' && Consume(','));
    return Consume(end);
  }

  CCmpCtrl &cc;
  CCodeCtrl &coc;
  uint32_t leave = NO_IC;
};

extern "C" void UndefinedExternHC();
extern "C" void SysSymImportsResolveCpp(const char *name, int64_t flags);

struct CPrsDecl
{
  ~CPrsDecl() { Free(alias); }

  bool Import() const { return mode == KW_IMPORT || mode == KW__IMPORT; }

  int64_t mode = -1;
  char *alias = nullptr;
};

static const CHashExport *PrsExternSym(const char *name, CHashTable *table)
{
  auto *entry = HashFind(name, table, HTT_EXPORT_SYS_SYM);
  return entry && !(entry->type & (HTF_UNRESOLVED | HTF_IMPORT))
             ? reinterpret_cast<const CHashExport *>(entry)
             : nullptr;
}

static bool PrsFunDecl(CCmpCtrl &cc, CHashFun &fun, const CPrsDecl &decl,
                       CAOT *aot)
{
  auto &sym = fun.cls.sym;
  auto *table = cc.htc.glbl_hash_table;
  if (cc.token != ';')
    return false;
  if (decl.Import()) {
    if (!aot || (decl.alias && *decl.alias != '_'))
      return false;
    sym.import_name = StrNew(decl.alias ? decl.alias : sym.hash.str,
                             CurrentTask()->data_heap);
    if (!sym.import_name)
      return false;
    sym.hash.type |= HTF_IMPORT;
  } else {
    auto *prior = HashSingleTableFind(sym.hash.str, table, HTT_FUN);
    if (!decl.alias && prior && !(prior->type & (HTF_IMPORT | HTF_UNRESOLVED))) {
      HashDel(&sym.hash);
      Lex(cc);
      return true;
    }
    const char *name = decl.alias ? decl.alias : sym.hash.str;
    auto *external = aot && !decl.alias ? nullptr : PrsExternSym(name, table);
    if (decl.alias && (!external || *name != '_'))
      return false;
    if (external) {
      if (aot) {
        if (external->sym.hash.type & HTF_IMM)
          return false;
        if ((sym.hash.type & HTF_PUBLIC) &&
            !AOTLabel(*aot, nullptr, sym.hash.str, external->val))
          return false;
        fun.ext_lst = const_cast<CHashExport *>(external);
        fun.cls.flags |= FF__EXTERN;
      } else {
        fun.exe_addr = reinterpret_cast<uint8_t *>(external->val);
        fun.cls.flags = (fun.cls.flags & ~CF_EXTERN) | FF__EXTERN;
      }
    } else {
      sym.hash.type |= HTF_UNRESOLVED;
      if (!aot)
        fun.exe_addr = reinterpret_cast<uint8_t *>(UndefinedExternHC);
    }
  }
  HashAdd(&sym.hash, table);
  Lex(cc);
  return true;
}

static void PrsFunResolve(CHashFun &fun, CHashTable *table, CAOT *aot)
{
  const char *name = fun.cls.sym.hash.str;
  for (uint64_t i = 2;; ++i) {
    auto *entry = HashSingleTableFind(name, table, HTT_FUN, i);
    if (!entry || !(entry->type & HTF_UNRESOLVED))
      break;
    if (aot)
      AOTRelResolve(*aot, entry, &fun);
    else
      reinterpret_cast<CHashFun *>(entry)->exe_addr = fun.exe_addr;
    entry->type &= ~HTF_UNRESOLVED;
  }
}

static bool PrsFun(CCmpCtrl &cc, CHashClass &type, char *name,
                   uint32_t visibility, uint16_t flags, CAOT *aot,
                   const CPrsDecl &decl)
{
  auto *heap = CurrentTask()->data_heap;
  auto *fun = reinterpret_cast<CHashFun *>(CAlloc(sizeof(CHashFun), heap));
  if (!fun || ClassRawType(type) == RT_AGGREGATE) {
    Free(fun);
    Free(name);
    return false;
  }
  auto &symbol = fun->cls.sym;
  symbol.hash.str = name;
  symbol.hash.type = HTT_FUN | visibility;
  fun->cls.flags = flags | CF_EXTERN;
  fun->return_class = &type;
  HashSrcFileSetCpp(&cc, &symbol, 0);
  if (!PrsFunArgs(cc, *fun)) {
    HashDel(&symbol.hash);
    return false;
  }
  Lex(cc);
  if (fun->arg_cnt && 8 * fun->arg_cnt <= INT16_MAX)
    fun->cls.flags |= FF_RET1;
  if (decl.mode >= 0) {
    if (PrsFunDecl(cc, *fun, decl, aot))
      return true;
    HashDel(&symbol.hash);
    return false;
  }
  if (cc.token != '{') {
    HashDel(&symbol.hash);
    return false;
  }
  auto *table = cc.htc.glbl_hash_table;
  HashAdd(&symbol.hash, table);
  auto *old_local = cc.htc.local_var_lst, *old_fun = cc.htc.fun;
  cc.htc.local_var_lst = cc.htc.fun = fun;
  CCodeCtrl coc;
  CPrsStmt ps(cc, coc);
  uint8_t *machine = nullptr;
  size_t size;
  if (ps.Body())
    machine = COCCompile(coc, NO_IC, ClassRawType(type), fun, aot, &size);
  cc.htc.local_var_lst = old_local;
  cc.htc.fun = old_fun;
  if (!machine) {
    for (auto *member = fun->cls.member_lst_and_root; member;
         member = member->next)
      if (member->flags & MLF_STATIC)
        Free(member->static_data);
    HashRemove(&symbol.hash, table);
    HashDel(&symbol.hash);
    return false;
  }
  fun->exe_addr = machine;
  fun->cls.flags &= ~CF_EXTERN;
  if (aot) {
    if (!AOTStore(*aot, fun, visibility & HTF_PUBLIC ? name : nullptr,
                  machine, size))
      return false;
    for (auto *member = fun->cls.member_lst_and_root; member;
         member = member->next)
      if ((member->flags & MLF_STATIC) &&
          !AOTStore(*aot, member->static_data, nullptr, member->static_data,
                    member->size))
        return false;
  }
  PrsFunResolve(*fun, table, aot);
  if (!aot) {
    SysSymImportsResolveCpp(name, 0);
    CurrentTask()->last_fun = fun;
  }
  Lex(cc);
  return true;
}

static bool PrsGlobalDecl(CCmpCtrl &cc, CHashGlblVar &var, const CPrsDecl &decl,
                          CAOT *aot)
{
  auto &sym = var.sym;
  if (decl.Import()) {
    if (!aot)
      return false;
    sym.import_name = StrNew(decl.alias ? decl.alias : sym.hash.str,
                             CurrentTask()->data_heap);
    if (!sym.import_name)
      return false;
    sym.hash.type |= HTF_IMPORT;
    var.flags |= GVF_IMPORT;
  } else {
    auto *table = cc.htc.glbl_hash_table;
    auto *external = aot && !decl.alias ? nullptr : PrsExternSym(
        decl.alias ? decl.alias : sym.hash.str, table);
    if (decl.alias && !external)
      return false;
    auto *prior = aot ? nullptr :
        HashSingleTableFind(sym.hash.str, table, HTT_GLBL_VAR);
    if (external) {
      if (aot) {
        if (external->sym.hash.type & HTF_IMM)
          return false;
        var.data_addr = reinterpret_cast<uint8_t *>(
            const_cast<CHashExport *>(external));
      } else
        var.data_addr = reinterpret_cast<uint8_t *>(external->val);
      var.flags |= GVF_ALIAS;
    } else if (prior && !(prior->type & (HTF_IMPORT | HTF_UNRESOLVED))) {
      auto *old = reinterpret_cast<const CHashGlblVar *>(prior);
      var.data_addr = old->data_addr;
      var.flags |= GVF_ALIAS;
    } else {
      sym.hash.type |= HTF_UNRESOLVED;
      var.flags |= GVF_EXTERN;
    }
  }
  return true;
}

static bool PrsGlobal(CCmpCtrl &cc, CHashClass &type, char *name,
                      uint32_t visibility, CAOT *aot, const CPrsDecl &decl)
{
  auto *heap = CurrentTask()->data_heap;
  auto *var =
      reinterpret_cast<CHashGlblVar *>(CAlloc(sizeof(CHashGlblVar), heap));
  if (!var || !ClassComplete(type)) {
    Free(var);
    Free(name);
    return false;
  }
  var->sym.hash.str = name;
  var->sym.hash.type = HTT_GLBL_VAR | visibility;
  var->var_class = &type;
  if (!PrsArrayDims(cc, var->dim, type.size) ||
      !(decl.mode < 0 ? PrsVarInit(cc, type, var->dim, var->data_addr, heap)
                      : PrsGlobalDecl(cc, *var, decl, aot)) ||
      (cc.token != ',' && cc.token != ';')) {
    HashDel(&var->sym.hash);
    return false;
  }
  if (var->dim.next)
    var->flags |= GVF_ARRAY;
  var->size = type.size * var->dim.total_cnt;
  HashSrcFileSetCpp(&cc, &var->sym, 0);
  auto *table = cc.htc.glbl_hash_table;
  if (aot && (var->flags & GVF_ALIAS) && (visibility & HTF_PUBLIC)) {
    auto *external = reinterpret_cast<const CHashExport *>(var->data_addr);
    if (!AOTLabel(*aot, nullptr, name, external->val)) {
      HashDel(&var->sym.hash);
      return false;
    }
  }
  HashAdd(&var->sym.hash, table);
  if (decl.mode >= 0)
    return true;
  if (aot)
    return AOTStore(*aot, var, visibility & HTF_PUBLIC ? name : nullptr,
                    var->data_addr, var->size);
  for (uint64_t i = 2;; ++i) {
    auto *entry = HashSingleTableFind(name, table, HTT_GLBL_VAR, i);
    if (!entry || !(entry->type & HTF_UNRESOLVED))
      break;
    auto *prior = reinterpret_cast<CHashGlblVar *>(entry);
    prior->data_addr = var->data_addr;
    prior->flags |= GVF_ALIAS;
    entry->type &= ~HTF_UNRESOLVED;
  }
  SysSymImportsResolveCpp(name, 0);
  return true;
}

bool PrsStreamBlk(CCmpCtrl &cc)
{
  auto *task = CurrentTask();
  auto *table = task->hash_table;
  if (!table)
    return false;
  auto saved = cc.htc;
  auto old_flags = saved.old_flags;
  saved.old_flags = cc.flags;
  CStreamBlk stream{};
  QueIns(stream, *cc.last_stream_blk);
  cc.htc.next = &saved;
  cc.htc.local_var_lst = cc.htc.fun = nullptr;
  cc.htc.hash_table_lst = cc.htc.define_hash_table = cc.htc.local_hash_table =
      cc.htc.glbl_hash_table = table;
  constexpr auto mode =
      CCF_ASM_EXPRESSIONS | CCF_AOT_COMPILE | CCF_EXE_BLK | CCF_HAS_MISC_DATA;
  cc.flags = (cc.flags & ~mode) | CCF_EXE_BLK;
  Lex(cc); // Skip '{'.
  uint64_t ignored;
  bool okay = ExeCmdLine(cc, ignored) && cc.token == '}' && !stream.error;
  cc.flags = (cc.flags & ~mode) | (saved.old_flags & mode);
  cc.htc = saved;
  cc.htc.old_flags = old_flags;
  QueRem(stream);
  if (okay && stream.body && *stream.body)
    return LexIncludeStr(cc, StrNew("StreamBlk", task->data_heap), stream.body,
                         0);
  Free(stream.body);
  return okay;
}

bool LexStmt2Bin(CCmpCtrl &cc, uint8_t *&machine, CRawType &type)
{
  auto *aot = cc.flags & CCF_AOT_COMPILE ? cc.aot : nullptr;
  size_t size;
  cc.flags &= ~CCF_HAS_MISC_DATA;
  machine = nullptr;
  if (cc.token == ';') {
    Lex(cc);
    return true;
  }
  uint32_t visibility = 0;
  uint16_t flags = 0;
  while (true) {
    auto keyword = Keyword(cc);
    if (keyword == KW_PUBLIC)
      visibility = HTF_PUBLIC;
    else if (keyword == KW_ARGPOP || keyword == KW_NOARGPOP)
      flags = keyword == KW_ARGPOP ? FF_ARGPOP : FF_NOARGPOP;
    else
      break;
    Lex(cc);
  }
  CPrsDecl decl;
  auto keyword = Keyword(cc);
  if (keyword == KW_ASM)
    return !visibility && !flags && PrsAsmBlk(cc, aot);
  if (keyword == KW_EXTERN || keyword == KW_IMPORT || keyword == KW__IMPORT ||
      keyword == KW__EXTERN) {
    decl.mode = keyword;
    Lex(cc);
    if (keyword == KW__IMPORT || keyword == KW__EXTERN) {
      if (cc.token != TK_IDENT)
        return false;
      decl.alias = const_cast<char *>(cc.cur_str);
      cc.cur_str = nullptr;
      Lex(cc);
    }
  }
  auto *base = PrsClass(cc);
  if (base)
    Lex(cc);
  bool forward = decl.mode == KW_EXTERN && !base;
  keyword = Keyword(cc);
  if ((decl.mode < 0 || forward) && (keyword == KW_CLASS || keyword == KW_UNION)) {
    base = PrsClassDef(cc, keyword == KW_UNION, forward, visibility, base);
    if (!base)
      return false;
    if (cc.token == ';') {
      Lex(cc);
      return true;
    }
  } else if (!base && decl.mode >= 0)
    return false;
  if (base) {
    if (!cc.htc.glbl_hash_table)
      return false;
    for (bool first = true;; first = false) {
      auto *cls = PrsStars(cc, base);
      if (!cls || cc.token != TK_IDENT)
        return false;
      auto *name = const_cast<char *>(cc.cur_str);
      cc.cur_str = nullptr;
      Lex(cc);
      if (first && cc.token == '(')
        return PrsFun(cc, *cls, name, visibility, flags, aot, decl);
      if (flags || cc.token == '(') {
        Free(name);
        return false;
      }
      if (!PrsGlobal(cc, *cls, name, visibility, aot, decl))
        return false;
      if (cc.token == ';') {
        Lex(cc);
        return true;
      }
      Lex(cc);
    }
  }
  if (visibility || flags)
    return false;
  if (cc.token == '{' || cc.token == TK_STR || cc.token == TK_CHAR_CONST ||
      keyword == KW_IF || keyword == KW_FOR || keyword == KW_WHILE ||
      keyword == KW_DO) {
    CCodeCtrl coc;
    CPrsStmt ps(cc, coc);
    if (!ps.Statement(0, NO_IC))
      return false;
    type = RT_I64;
    machine = COCCompile(coc, NO_IC, type, nullptr, aot, &size);
    if (machine && coc.has_misc_data)
      cc.flags |= CCF_HAS_MISC_DATA;
    if (!machine)
      return false;
  } else {
    machine = LexStatementExpression2Bin(cc, type, aot, &size);
    if (!machine || cc.token != ';') {
      Free(machine);
      machine = nullptr;
      return false;
    }
    Lex(cc);
  }
  if (!aot)
    return true;
  bool okay = AOTStore(*aot, nullptr, nullptr, machine, size, true);
  Free(machine);
  machine = nullptr;
  return okay;
}
