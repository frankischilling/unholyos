#include "../Kernel/KernelB.hpp"
#include "CompilerA.hpp"

CPrsDeclarator::~CPrsDeclarator()
{
  Free(const_cast<char *>(name));
  if (fun_ptr)
    HashDel(&FunPtrBase(fun_ptr)->cls.sym.hash);
}

char *CPrsDeclarator::TakeName()
{
  auto *result = const_cast<char *>(name);
  name = nullptr;
  return result;
}

CHashFun *CPrsDeclarator::TakeFun()
{
  auto *result = fun_ptr;
  fun_ptr = nullptr;
  return result;
}

void CPrsDeclarator::Bind(CMemberLst &member)
{
  name = nullptr;
  member.reg = reg;
  if (fun_ptr) {
    member.flags |= MLF_FUN;
    member.fun_ptr = TakeFun();
  }
}

bool PrsReg(CCmpCtrl &cc, int8_t &reg)
{
  while (cc.token == TK_IDENT && cc.hash_entry &&
         (cc.hash_entry->type & HTT_KEYWORD)) {
    auto keyword =
        reinterpret_cast<const CHashGeneric *>(cc.hash_entry)->user_data0;
    if (keyword == KW_NOREG) {
      reg = REG_NONE;
      Lex(cc);
    } else if (keyword == KW_REG) {
      reg = REG_ALLOC;
      if (Lex(cc) == TK_IDENT) {
        auto *names = reinterpret_cast<const CHashDefineStr *>(
            HashFind("ST_U64_REGS", CurrentTask()->hash_table, HTT_DEFINE_STR));
        auto index = names ? LstMatchCpp(
            reinterpret_cast<const uint8_t *>(cc.cur_str),
            reinterpret_cast<const uint8_t *>(names->data), 0) : -1;
        if (index >= 0 && index < 16) {
          reg = index;
          Lex(cc);
        }
      }
    } else
      break;
  }
  return cc.token != TK_ERROR;
}

CHashFun *PrsFunPtrNew(CHashClass &result, unsigned stars, uint16_t flags)
{
  auto *fun = reinterpret_cast<CHashFun *>(
      CAlloc(sizeof(CHashFun) * (stars + 1), CurrentTask()->data_heap));
  if (!fun)
    return nullptr;
  fun->cls.sym.hash.type = HTT_FUN;
  fun->cls.flags = flags;
  fun->return_class = &result;
  for (unsigned i = 1; i <= stars; ++i)
    fun[i].cls.ptr_stars_cnt = i;
  return fun + stars;
}

bool PrsType(CCmpCtrl &cc, CHashClass &base, CPrsDeclarator &decl,
             uint16_t flags, bool local, unsigned depth)
{
  if (local && !PrsReg(cc, decl.reg))
    return false;
  auto *cls = PrsStars(cc, &base);
  if (!cls || depth == 127)
    return false;
  if (local && !PrsReg(cc, decl.reg))
    return false;

  unsigned stars = 0;
  if (cc.token == '(') {
    while (Lex(cc) == '*')
      if (++stars > PTR_STARS_NUM)
        return false;
    if (!stars)
      return false;
  }
  if (cc.token == TK_IDENT) {
    decl.name = cc.cur_str;
    cc.cur_str = nullptr;
    Lex(cc);
  }
  decl.cls = cls;
  if (!stars)
    return true;
  if (cc.token != ')' || Lex(cc) != '(' || ClassRawType(*cls) == RT_AGGREGATE)
    return false;

  decl.fun_ptr = PrsFunPtrNew(*cls, stars, flags);
  auto *raw = reinterpret_cast<CHashClass *>(
      HashFind("U0", cc.htc.hash_table_lst, HTT_INTERNAL_TYPE));
  if (!decl.fun_ptr || !raw)
    return false;
  decl.cls = raw + stars;
  auto &fun = *FunPtrBase(decl.fun_ptr);
  if (!PrsFunArgs(cc, fun, depth + 1))
    return false;
  if (fun.arg_cnt && 8 * fun.arg_cnt <= INT16_MAX &&
      !(fun.cls.flags & FF_DOT_DOT_DOT))
    fun.cls.flags |= FF_RET1;
  Lex(cc);
  return true;
}

static bool PrsDotDotDot(CCmpCtrl &cc, CHashFun &fun, int8_t reg)
{
  if (Lex(cc) != ')')
    return false;
  auto *i64 = reinterpret_cast<CHashClass *>(
      HashFind("I64i", cc.htc.hash_table_lst, HTT_INTERNAL_TYPE));
  if (!i64)
    return false;
  fun.cls.flags |= FF_DOT_DOT_DOT;

  auto *heap = CurrentTask()->data_heap;
  auto offset = 16 + 8 * fun.arg_cnt;
  auto *name = StrNew("argc", heap);
  auto *member = name ? MemberAdd(fun.cls, name, *i64, offset) : nullptr;
  if (!member) {
    Free(name);
    return false;
  }
  member->flags = MLF_DOT_DOT_DOT;
  member->reg = reg;
  --fun.cls.member_cnt; // Only explicitly declared arguments are counted.

  name = StrNew("argv", heap);
  member = name ? MemberAdd(fun.cls, name, *i64, offset + 8) : nullptr;
  if (!member) {
    Free(name);
    return false;
  }
  member->flags = MLF_DOT_DOT_DOT;
  member->reg = reg;
  --fun.cls.member_cnt;
  auto *dim = reinterpret_cast<CArrayDim *>(CAlloc(sizeof(CArrayDim), heap));
  if (!dim)
    return false;
  // The reference uses an arbitrary array bound; actual slots follow argc.
  member->dim.total_cnt = dim->cnt = 127;
  dim->total_cnt = 1;
  member->dim.next = dim;
  return true;
}

bool PrsFunArgs(CCmpCtrl &cc, CHashFun &fun, unsigned depth)
{
  Lex(cc); // Skip '('.
  if (cc.token == ')')
    return true;
  do {
    CPrsDeclarator decl;
    if (!PrsReg(cc, decl.reg))
      return false;
    if (cc.token == TK_ELLIPSIS)
      return PrsDotDotDot(cc, fun, decl.reg);
    auto *base = PrsClass(cc);
    if (!base)
      return false;
    Lex(cc);
    if (!PrsType(cc, *base, decl, 0, true, depth) ||
        !TypeSize(ClassRawType(*decl.cls)) || fun.arg_cnt >= UINT16_MAX / 8)
      return false;
    if (!decl.name && (cc.token == ',' || cc.token == ')')) {
      auto *name = StrNew("_anon_0000", CurrentTask()->data_heap);
      if (!name)
        return false;
      for (unsigned i = 0; i < 4; ++i)
        name[9 - i] = "0123456789abcdef"[(fun.arg_cnt >> (4 * i)) & 15];
      decl.name = name;
    }
    if (!decl.name)
      return false;
    auto *member =
        MemberAdd(fun.cls, decl.name, *decl.cls, 16 + 8 * fun.arg_cnt);
    if (!member)
      return false;
    decl.Bind(*member);
    member->size = 8;
    ++fun.arg_cnt;
    if (cc.token == '=') {
      Lex(cc);
      if (cc.token == TK_IDENT && cc.hash_entry &&
          (cc.hash_entry->type & HTT_KEYWORD) &&
          reinterpret_cast<CHashGeneric *>(cc.hash_entry)->user_data0 == KW_LASTCLASS) {
        member->flags |= MLF_LASTCLASS;
        Lex(cc);
      } else if (cc.token == TK_STR && decl.cls->ptr_stars_cnt) {
        size_t size;
        auto *str = LexExtStr(cc, size);
        if (!str)
          return false;
        member->dft_val = reinterpret_cast<uintptr_t>(str);
        member->flags |= MLF_STR_DFT_AVAILABLE;
      } else {
        auto *machine = LexTypedExpression2Bin(cc, ClassRawType(*decl.cls));
        if (!machine || (cc.token != ')' && cc.token != ',')) {
          Free(machine);
          return false;
        }
        auto okay = CmpCall(cc, machine, member->dft_val);
        Free(machine);
        if (!okay)
          return false;
      }
      if (cc.token != ')' && cc.token != ',')
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

bool PrsMemberMeta(CCmpCtrl &cc, CMemberLst &member)
{
  while (cc.token == TK_IDENT) {
    auto *meta = reinterpret_cast<CMemberLstMeta *>(
        CAlloc(sizeof(CMemberLstMeta), CurrentTask()->data_heap));
    if (!meta)
      return false;
    meta->next = member.meta;
    member.meta = meta;
    meta->str = cc.cur_str;
    cc.cur_str = nullptr;
    if (Lex(cc) == TK_STR) {
      size_t size;
      auto *str = LexExtStr(cc, size);
      if (!str)
        return false;
      meta->flags = MLMF_IS_STR;
      meta->user_data = reinterpret_cast<uintptr_t>(str);
    } else if (!LexExpression(cc, meta->user_data))
      return false;
  }
  return cc.token != TK_ERROR;
}

bool PrsArrayDims(CCmpCtrl &cc, CArrayDim &dim, size_t item_size)
{
  dim.total_cnt = 1;
  auto **tail = &dim.next;
  int64_t nonzero_product = 1;
  unsigned rank = 0;
  while (cc.token == '[') {
    if (++rank > 127)
      return false;
    Lex(cc);
    int64_t count = 0;
    // In the pinned compiler an omitted first dimension has zero elements.
    if (cc.token != ']' || rank != 1) {
      auto *machine = LexTypedExpression2Bin(cc, RT_I64);
      uint64_t value;
      bool okay = machine && cc.token == ']' && CmpCall(cc, machine, value);
      Free(machine);
      if (!okay || value > INT32_MAX / item_size / nonzero_product)
        return false;
      count = value;
    }
    if (count)
      nonzero_product *= count;
    auto *axis = reinterpret_cast<CArrayDim *>(
        CAlloc(sizeof(CArrayDim), CurrentTask()->data_heap));
    if (!axis)
      return false;
    axis->cnt = count;
    axis->total_cnt = 1;
    for (auto *prev = &dim; prev; prev = prev->next)
      prev->total_cnt *= count;
    *tail = axis;
    tail = &axis->next;
    Lex(cc);
  }
  return cc.token != TK_ERROR;
}

static bool PrsScalarInit(CCmpCtrl &cc, CHashClass &cls, uint8_t *dst)
{
  auto flags = cc.flags;
  cc.flags &= ~CCF_HAS_MISC_DATA;
  auto *machine =
      LexTypedExpression2Bin(cc, ClassRawType(cls), cls.ptr_stars_cnt);
  bool keep = cc.flags & CCF_HAS_MISC_DATA;
  cc.flags = (cc.flags & ~CCF_HAS_MISC_DATA) | (flags & CCF_HAS_MISC_DATA);
  if (!machine || (cc.token != ',' && cc.token != '}' && cc.token != ';')) {
    Free(machine);
    return false;
  }
  uint64_t value;
  auto okay = CmpCall(cc, machine, value);
  if (!keep)
    Free(machine);
  if (okay)
    MemCpy(dst, &value, cls.size);
  return okay;
}

static bool PrsVarInit2(CCmpCtrl &cc, CHashClass &cls, const CArrayDim *axis,
                        uint8_t *dst, unsigned depth)
{
  if (depth == 127)
    return false;
  if (axis) {
    auto type = ClassRawType(cls);
    if (cc.token == TK_STR && !axis->next && (type == RT_U8 || type == RT_I8)) {
      size_t size;
      auto *text = LexExtStr(cc, size);
      if (!text)
        return false;
      auto width = static_cast<size_t>(axis->cnt);
      MemCpy(dst, text, size < width ? size : width);
      Free(text);
      return true;
    }
    bool braces = cc.token == '{';
    if (braces)
      Lex(cc);
    auto stride = axis->total_cnt * cls.size;
    for (int64_t i = 0; i < axis->cnt; ++i) {
      if (i && cc.token == ',')
        Lex(cc);
      if (!PrsVarInit2(cc, cls, axis->next, dst + i * stride, depth + 1))
        return false;
    }
    if (braces) {
      if (cc.token == ',')
        Lex(cc);
      if (cc.token != '}')
        return false;
      Lex(cc);
    }
    return true;
  }
  if (ClassRawType(cls) != RT_AGGREGATE)
    return PrsScalarInit(cc, cls, dst);
  if (cc.token != '{')
    return false;
  Lex(cc);
  // Initializers walk the class's own member list from the start of the object.
  int64_t offset = 0;
  for (auto *member = cls.member_lst_and_root; member; member = member->next) {
    if (member->size > cls.size - offset ||
        !PrsVarInit2(cc, *member->member_class, member->dim.next, dst + offset,
                     depth + 1))
      return false;
    offset += member->size;
    if (cc.token == ',')
      Lex(cc);
  }
  if (cc.token != '}')
    return false;
  Lex(cc);
  return true;
}

bool PrsVarInit(CCmpCtrl &cc, CHashClass &cls, CArrayDim &dim, uint8_t *&data,
                CHeapCtrl *heap)
{
  auto size = cls.size * dim.total_cnt;
  data = MAlloc(size, heap);
  if (!data)
    return false;
  if (sys_var_init_flag)
    MemSet(data, sys_var_init_val, size);
  if (cc.token != '=')
    return true;
  Lex(cc);
  return PrsVarInit2(cc, cls, dim.next, data, 0);
}
