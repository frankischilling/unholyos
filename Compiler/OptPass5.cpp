#include "CompilerA.hpp"

static bool OptRhsOpcode(CICode op)
{
  switch (op) {
    case IC_ADD: case IC_SUB: case IC_MUL:
    case IC_AND: case IC_OR: case IC_XOR:
      return true;
    default:
      return ICIsComparison(op);
  }
}

static bool OptRhsInt(const CIntermediateCode &node)
{
  return IsInt(node.type) && !node.chain && !node.cmp_link;
}

void OptPass5(CIntermediateCode *code,uint32_t count)
{
  bool safe=true;
  for (uint32_t i=0;i<count;i++) {
    code[i].rhs={};
    if (code[i].code==IC_ASM || code[i].code==IC_GET_RFLAGS)
      safe=false;
  }
  if (!safe)
    return;

  // The reference substitutes stack arguments with MOV/REG operands.
  // Keep the indexed tree intact, and select only RHS operands whose
  // evaluation can occur directly in the final arithmetic instruction.
  for (uint32_t i=0;i<count;i++) {
    auto &node=code[i];
    if (!OptRhsOpcode(node.code) || !OptRhsInt(node) ||
        node.left>=i || node.right>=i)
      continue;
    const auto &left=code[node.left],&right=code[node.right];
    if (!OptRhsInt(left) || !OptRhsInt(right))
      continue;
    if (right.code==IC_IMM_I64 &&
        right.data==uint64_t(int64_t(int32_t(right.data)))) {
      node.rhs={COptRhsKind::Imm32,static_cast<int32_t>(right.data),0};
      continue;
    }
    if (right.code!=IC_DEREF || TypeSize(right.type)!=8)
      continue;
    if (right.local_reg>=12 && right.local_reg<=15) {
      node.rhs={COptRhsKind::Reg,0,right.local_reg};
      continue;
    }
    // A direct frame slot needs neither an address computation nor a
    // conversion. Read it after the left operand, including any mutations
    // performed by the left subtree. Do not fold other memory expressions.
    const auto &addr=right.address;
    if (!right.local_reg && addr.valid && addr.base==NO_IC &&
        addr.index==NO_IC && !addr.shift && right.left<node.right &&
        code[right.left].code==IC_RBP &&
        code[right.left].data==uint64_t(int64_t(addr.disp)))
      node.rhs={COptRhsKind::Frame,addr.disp,0};
  }
}
