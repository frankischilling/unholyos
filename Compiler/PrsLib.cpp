#include "../Kernel/KernelB.hpp"
#include "CompilerA.hpp"

CMemberLst *MemberAdd(CHashClass &owner, const char *name, CHashClass &cls,
                      int64_t offset)
{
  auto **link = &owner.member_lst_and_root;
  while (*link) {
    auto cmp = StrCmpCpp(reinterpret_cast<const uint8_t *>((*link)->str),
                         reinterpret_cast<const uint8_t *>(name));
    if (!cmp)
      return nullptr;
    link = cmp < 0 ? &(*link)->left : &(*link)->right;
  }
  auto *member = reinterpret_cast<CMemberLst *>(
      CAlloc(sizeof(CMemberLst), CurrentTask()->data_heap));
  if (!member)
    return nullptr;
  member->str = name;
  member->member_class = &cls;
  member->member_class_base = &cls - cls.ptr_stars_cnt;
  member->size = cls.size;
  member->dim.cnt = member->dim.total_cnt = 1;
  member->offset = offset;
  ++owner.member_cnt;
  *link = member;
  // The empty tail's next link is the root slot assigned above.
  if (owner.last_in_member_lst &&
      owner.last_in_member_lst !=
          reinterpret_cast<CMemberLst *>(&owner.member_lst_and_root))
    owner.last_in_member_lst->next = member;
  owner.last_in_member_lst = member;
  return member;
}

CHashClass *PrsClass(const CCmpCtrl &cc)
{
  if (cc.token != TK_IDENT || !cc.hash_entry ||
      !(cc.hash_entry->type & (HTT_CLASS | HTT_INTERNAL_TYPE)))
    return nullptr;
  auto *cls = reinterpret_cast<CHashClass *>(cc.hash_entry);
  auto type = static_cast<CRawType>(cls->raw_type);
  return TypeSize(type) || IsVoid(type) ? cls : nullptr;
}

CHashClass *PrsStars(CCmpCtrl &cc, CHashClass *cls)
{
  for (; cc.token == '*'; Lex(cc)) {
    if (cls->ptr_stars_cnt == PTR_STARS_NUM)
      return nullptr;
    ++cls;
  }
  return cc.token == TK_ERROR ? nullptr : cls;
}

CHashClass *PrsClassNew()
{
  auto *cls = reinterpret_cast<CHashClass *>(CAlloc(
      sizeof(CHashClass) * (PTR_STARS_NUM + 1), CurrentTask()->code_heap));
  if (!cls)
    return nullptr;
  for (unsigned stars = 0; stars <= PTR_STARS_NUM; ++stars) {
    auto &type = cls[stars];
    type.sym.hash.type = HTT_CLASS;
    type.raw_type = RT_I64;
    type.size = sizeof(uint8_t *);
    type.ptr_stars_cnt = stars;
  }
  cls->last_in_member_lst =
      reinterpret_cast<CMemberLst *>(&cls->member_lst_and_root);
  cls->size = 0;
  return cls;
}

CHashFun *PrsFunNew()
{
  auto *fun = reinterpret_cast<CHashFun *>(
      CAlloc(sizeof(CHashFun) * (PTR_STARS_NUM + 1), CurrentTask()->code_heap));
  if (!fun)
    return nullptr;
  for (unsigned stars = 0; stars <= PTR_STARS_NUM; ++stars) {
    auto &type = fun[stars].cls;
    type.sym.hash.type = HTT_FUN;
    type.raw_type = RT_I64;
    type.size = sizeof(uint8_t *);
    type.ptr_stars_cnt = stars;
  }
  fun->cls.last_in_member_lst =
      reinterpret_cast<CMemberLst *>(&fun->cls.member_lst_and_root);
  fun->cls.size = 0;
  return fun;
}

// The statement stream links only its own nodes. Operand trees share the same
// storage and use their indices even when the array grows.
CCodeCtrl::~CCodeCtrl()
{
  if (has_misc_data || asm_first)
    for (uint32_t i = 0; i < used; ++i) {
      if (code[i].code == IC_STR_CONST)
        Free(reinterpret_cast<void *>(code[i].data));
      else if (code[i].code == IC_ASM)
        AsmDel(reinterpret_cast<CAsm *>(code[i].data));
      else if (code[i].code == IC_SWITCH || code[i].code == IC_NOBOUND_SWITCH)
        Free(reinterpret_cast<void *>(code[i].data));
    }
  while (labels) {
    auto *label = labels;
    labels = label->next;
    Free(label->name);
    Free(label);
  }
  Free(code);
}

uint32_t CCodeCtrl::Add(const CIntermediateCode &entry)
{
  if (failed)
    return NO_IC;
  if (used == capacity) {
    uint32_t count = capacity ? capacity * 2 : 32;
    auto *next = count > capacity ? MAlloc(size_t{count} * sizeof(entry),
                                           CurrentTask()->data_heap)
                                  : nullptr;
    if (!next) {
      failed = true;
      return NO_IC;
    }
    if (used)
      MemCpy(next, code, used * sizeof(entry));
    Free(code);
    code = reinterpret_cast<CIntermediateCode *>(next);
    capacity = count;
  }
  code[used] = entry;
  return used++;
}

void CCodeCtrl::Append(uint32_t index)
{
  if (index == NO_IC)
    return;
  if (last == NO_IC)
    first = index;
  else
    code[last].next = index;
  last = index;
}

void CCodeCtrl::Append(CICode op, uint32_t left, uint32_t right)
{
  Append(Add({0, left, right, op, RT_U0, false, false, false}));
}

uint32_t CCodeCtrl::LabelNew()
{
  return Add({0, NO_IC, NO_IC, IC_LABEL, RT_U0, false, false, false});
}

CCodeLabel *CCodeCtrl::GoToLabelFind(const char *name, bool create)
{
  for (auto *label = labels; label; label = label->next)
    if (!StrCmpCpp(reinterpret_cast<const uint8_t *>(label->name),
                   reinterpret_cast<const uint8_t *>(name)))
      return label;
  if (!create)
    return nullptr;

  auto *heap = CurrentTask()->data_heap;
  auto *label =
      reinterpret_cast<CCodeLabel *>(MAlloc(sizeof(CCodeLabel), heap));
  auto *str = label ? StrNew(name, heap) : nullptr;
  auto index = str ? LabelNew() : NO_IC;
  if (index == NO_IC) {
    Free(str);
    Free(label);
    return nullptr;
  }
  *label = {labels, str, index, false};
  labels = label;
  return label;
}

bool CCodeCtrl::GoToLabelsDefined() const
{
  for (auto *label = labels; label; label = label->next)
    if (!label->defined)
      return false;
  return true;
}

uint32_t CCodeCtrl::LValue(uint32_t address, CHashClass &cls,
                           const CArrayDim *dim, const CHashFun *fun_ptr)
{
  if (address == NO_IC || (dim && cls.ptr_stars_cnt == PTR_STARS_NUM))
    return NO_IC;
  if (dim)
    return Add({0, address, NO_IC, IC_HOLYC_TYPECAST, RT_I64, false, false,
                true, NO_IC, &cls + 1, dim, fun_ptr});
  return Add({0, address, NO_IC, IC_DEREF, ClassRawType(cls), false, false,
              true, NO_IC, &cls, nullptr, fun_ptr});
}
