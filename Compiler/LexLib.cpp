#include "../Kernel/KernelB.hpp"
#include "CompilerA.hpp"

extern "C" uint64_t MemberMetaDataCpp(const char *name, CMemberLst *member)
{
  auto *meta = MemberMetaFindCpp(name, member);
  return meta ? meta->user_data : 0;
}

extern "C" CMemberLstMeta *MemberMetaFindCpp(const char *name, CMemberLst *member)
{
  if (member && name)
    for (auto *meta = member->meta; meta; meta = meta->next)
      if (!StrCmpCpp(reinterpret_cast<const uint8_t *>(meta->str),
                     reinterpret_cast<const uint8_t *>(name)))
        return meta;
  return nullptr;
}

CMemberLst *MemberFind(const char *name, CHashClass *cls)
{
  for (; cls; cls = cls->base_class)
    for (auto *member = cls->member_lst_and_root; member;) {
      auto cmp = StrCmpCpp(reinterpret_cast<const uint8_t *>(member->str),
                           reinterpret_cast<const uint8_t *>(name));
      if (!cmp) {
        ++member->use_cnt;
        return member;
      }
      member = cmp < 0 ? member->left : member->right;
    }
  return nullptr;
}

extern "C" CMemberLst *MemberFindCpp(const char *name, CHashClass *cls)
{
  return name ? MemberFind(name, cls) : nullptr;
}

bool MemberLstCmp(const CMemberLst *left, const CMemberLst *right, int64_t count)
{
  constexpr uint16_t defaults = MLF_DFT_AVAILABLE | MLF_STR_DFT_AVAILABLE;
  while (left && right && count--) {
    if (StrCmpCpp(reinterpret_cast<const uint8_t *>(left->str),
                 reinterpret_cast<const uint8_t *>(right->str)) ||
        left->member_class != right->member_class ||
        left->member_class_base != right->member_class_base)
      return false;
    if ((left->flags & defaults) != (right->flags & defaults))
      return false;
    if (left->flags & MLF_STR_DFT_AVAILABLE) {
      if (StrCmpCpp(reinterpret_cast<const uint8_t *>(left->dft_val),
                   reinterpret_cast<const uint8_t *>(right->dft_val)))
        return false;
    } else if ((left->flags & MLF_DFT_AVAILABLE) && left->dft_val != right->dft_val) {
      return false;
    }
    left = left->next;
    right = right->next;
  }
  return count < 0 || (!left && !right);
}

static void MemberLstDel(CMemberLst *member)
{
  while (member) {
    auto *next = member->next;
    Free(const_cast<char *>(member->str));
    LinkedLstDelCpp(member->dim.next);
    if (member->flags & MLF_STR_DFT_AVAILABLE)
      Free(reinterpret_cast<void *>(member->dft_val));
    if (member->flags & MLF_FUN) {
      auto *fun = member->fun_ptr - member->fun_ptr->cls.ptr_stars_cnt;
      HashDel(&fun->cls.sym.hash);
    }
    for (auto *meta = member->meta; meta;) {
      auto *following = meta->next;
      Free(const_cast<char *>(meta->str));
      if (meta->flags & MLMF_IS_STR)
        Free(reinterpret_cast<void *>(meta->user_data));
      Free(meta);
      meta = following;
    }
    Free(member);
    member = next;
  }
}

void ClassMemberLstDel(CHashClass *cls)
{
  MemberLstDel(cls->member_lst_and_root);
  cls->size = 0;
  cls->last_in_member_lst =
      reinterpret_cast<CMemberLst *>(&cls->member_lst_and_root);
  cls->member_lst_and_root = nullptr;
  cls->member_class_base_root = nullptr;
  cls->member_cnt = 0;
  if (cls->sym.hash.type & HTT_FUN)
    reinterpret_cast<CHashFun *>(cls)->arg_cnt = 0;
}
