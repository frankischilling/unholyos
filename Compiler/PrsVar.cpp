#include "../Kernel/KernelB.hpp"
#include "CompilerA.hpp"

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
