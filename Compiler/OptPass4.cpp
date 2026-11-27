#include "CompilerA.hpp"

// Classify an indexed-IR address without removing its operand nodes. The
// emitter must still evaluate the base before the index, exactly once each.
static bool OptAddrType(const CIntermediateCode &node)
{
  return IsInt(node.type) && TypeSize(node.type)==8 &&
         !node.chain && !node.cmp_link;
}

static bool OptAddrDisp(int64_t &disp,int64_t delta)
{
  if (delta<INT32_MIN || delta>INT32_MAX)
    return false;
  int64_t sum=disp+delta;
  if (sum<INT32_MIN || sum>INT32_MAX)
    return false;
  disp=sum;
  return true;
}

static bool OptAddress(const CIntermediateCode *code,uint32_t root,
                       COptAddress &addr)
{
  uint32_t base=root,index=NO_IC;
  uint8_t shift=0;
  int64_t disp=0;
  bool changed=false;
  unsigned depth=0;

  while (++depth<=512) {
    const auto &node=code[base];
    if (!OptAddrType(node))
      return false;
    if (node.code==IC_RBP) {
      if (!OptAddrDisp(disp,static_cast<int64_t>(node.data)))
        return false;
      addr={NO_IC,index,static_cast<int32_t>(disp),shift,true};
      return true;
    }
    if (node.left>=base)
      break;
    if (node.code==IC_HOLYC_TYPECAST &&
        OptAddrType(code[node.left])) {
      // A 64-bit HolyC cast changes the class, not the address bits.
      base=node.left;
      continue;
    }
    if (node.code==IC_ADD_CONST || node.code==IC_SUB_CONST) {
      int64_t delta=static_cast<int64_t>(node.data);
      if (delta<INT32_MIN || delta>INT32_MAX ||
          !OptAddrDisp(disp,node.code==IC_ADD_CONST?delta:-delta))
        return false;
      base=node.left;
      changed=true;
      continue;
    }
    if (node.code==IC_ADD && index==NO_IC && node.right<base &&
        IsInt(code[node.right].type) && !code[node.right].chain &&
        !code[node.right].cmp_link) {
      index=node.right;
      const auto &rhs=code[index];
      if (rhs.code==IC_SHL_CONST && rhs.data<=3 && rhs.left<index &&
          IsInt(code[rhs.left].type) && !code[rhs.left].chain &&
          !code[rhs.left].cmp_link) {
        shift=rhs.data;
        index=rhs.left;
      }
      // Do not extract a second index from the left subtree; that would
      // change operand order or require an additional register.
      base=node.left;
      changed=true;
      continue;
    }
    break;
  }
  if (depth>512 || !changed)
    return false;
  addr={base,index,static_cast<int32_t>(disp),shift,true};
  return true;
}

void OptPass4(CIntermediateCode *code,uint32_t count)
{
  bool safe=true;
  for (uint32_t i=0;i<count;i++) {
    code[i].address={};
    // Address folding can replace ADD/SHL with LEA or a memory operand.
    // Retain the existing flag-producing sequence around explicit flag
    // reads and inline assembly.
    if (code[i].code==IC_ASM || code[i].code==IC_GET_RFLAGS)
      safe=false;
  }
  if (!safe)
    return;
  for (uint32_t i=0;i<count;i++) {
    auto &node=code[i];
    if (node.code==IC_DEREF && !node.local_reg && node.left<i &&
        TypeSize(node.type))
      OptAddress(code,node.left,node.address);
  }
}
