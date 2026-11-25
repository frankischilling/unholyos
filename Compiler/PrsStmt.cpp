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
    if (depth == 127 || cc.token <= TK_EOF || coc.failed)
      return false;
    if (cc.token == ';')
      return Consume(';');
    if (cc.token == '{')
      return Block(depth, brk) && Consume('}');
    if (auto *cls = PrsClass(cc))
      return cc.htc.fun && PrsLocal(*cc.htc.fun, *cls);
    auto keyword = Keyword(cc);
    switch (keyword) {
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
    case KW_BREAK:
      if (brk == NO_IC)
        return false;
      Lex(cc);
      coc.Append(IC_JMP, NO_IC, brk);
      return Consume(';');
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

  bool PrsLocal(CHashFun &fun, CHashClass &base, bool is_static = false)
  {
    do {
      Lex(cc);
      auto *cls = PrsStars(cc, &base);
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

static bool PrsFun(CCmpCtrl &cc, CHashClass &type, char *name,
                   uint32_t visibility, uint16_t flags)
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
  if (!PrsFunArgs(cc, *fun) || Lex(cc) != '{') {
    HashDel(&symbol.hash);
    return false;
  }
  if (fun->arg_cnt && 8 * fun->arg_cnt <= INT16_MAX)
    fun->cls.flags |= FF_RET1;
  auto *table = cc.htc.glbl_hash_table;
  HashAdd(&symbol.hash, table);
  auto *old_local = cc.htc.local_var_lst, *old_fun = cc.htc.fun;
  cc.htc.local_var_lst = cc.htc.fun = fun;
  CCodeCtrl coc;
  CPrsStmt ps(cc, coc);
  uint8_t *machine = nullptr;
  if (ps.Body())
    machine = COCCompile(coc, NO_IC, ClassRawType(type), fun);
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
  CurrentTask()->last_fun = fun;
  Lex(cc);
  return true;
}

static bool PrsGlobal(CCmpCtrl &cc, CHashClass &type, char *name,
                      uint32_t visibility)
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
      !PrsVarInit(cc, type, var->dim, var->data_addr, heap) ||
      (cc.token != ',' && cc.token != ';')) {
    HashDel(&var->sym.hash);
    return false;
  }
  if (var->dim.next)
    var->flags |= GVF_ARRAY;
  var->size = type.size * var->dim.total_cnt;
  HashSrcFileSetCpp(&cc, &var->sym, 0);
  HashAdd(&var->sym.hash, cc.htc.glbl_hash_table);
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
  auto *base = PrsClass(cc);
  if (base)
    Lex(cc);
  bool forward = !base && Keyword(cc) == KW_EXTERN;
  if (forward)
    Lex(cc);
  auto keyword = Keyword(cc);
  if (keyword == KW_CLASS || keyword == KW_UNION) {
    base = PrsClassDef(cc, keyword == KW_UNION, forward, visibility, base);
    if (!base)
      return false;
    if (cc.token == ';') {
      Lex(cc);
      return true;
    }
  } else if (forward)
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
        return PrsFun(cc, *cls, name, visibility, flags);
      if (flags || cc.token == '(') {
        Free(name);
        return false;
      }
      if (!PrsGlobal(cc, *cls, name, visibility))
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
    machine = COCCompile(coc, NO_IC, type);
    if (machine && coc.has_misc_data)
      cc.flags |= CCF_HAS_MISC_DATA;
    return machine != nullptr;
  }
  machine = LexStatementExpression2Bin(cc, type);
  if (!machine || cc.token != ';') {
    Free(machine);
    machine = nullptr;
    return false;
  }
  Lex(cc);
  return true;
}
