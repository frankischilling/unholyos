#include "CompilerA.hpp"

static bool OptStackBinary(CICode op)
{
  switch (op) {
    case IC_ADD: case IC_SUB: case IC_MUL: case IC_DIV: case IC_MOD:
    case IC_AND: case IC_OR: case IC_XOR: case IC_SHL: case IC_SHR:
      return true;
    default:
      return ICIsComparison(op);
  }
}

static bool OptStackInt(const CIntermediateCode &node)
{
  return IsInt(node.type) && !node.chain && !node.cmp_link;
}

// Only inspect operands the emitter can actually evaluate. In particular,
// the right field of a constant operation is not a second expression.
// Bound work even for an unusually deep or heavily shared operand DAG.
static bool OptR9Free(const CIntermediateCode *code,uint32_t i,
                      unsigned depth,unsigned &budget)
{
  if (depth>=512 || !budget)
    return false;
  --budget;
  const auto &node=code[i];
  if (!OptStackInt(node) || node.stack_reg)
    return false;
  auto child=[&](uint32_t index) {
    return index<i && OptR9Free(code,index,depth+1,budget);
  };
  switch (node.code) {
    case IC_IMM_I64:
    case IC_RBP:
    case IC_DATA_ADDR:
    case IC_GLBL_ADDR:
      return true;
    case IC_DEREF: {
      if (node.local_reg)
        return node.local_reg>=12 && node.local_reg<=15;
      const auto &addr=node.address;
      if (!addr.valid)
        return child(node.left);
      return addr.shift<=3 && (addr.index!=NO_IC || !addr.shift) &&
             (addr.base==NO_IC || child(addr.base)) &&
             (addr.index==NO_IC || child(addr.index));
    }
    case IC_SHL_CONST: case IC_SHR_CONST:
      // The emitter still reads the original right operand's type.
      return node.right<i && child(node.left);
    case IC_ADD_CONST: case IC_SUB_CONST:
    case IC_COM: case IC_NOT: case IC_UNARY_MINUS:
    case IC_HOLYC_TYPECAST: case IC_TO_I64: case IC_SQR_I64:
      return child(node.left);
    // Do not keep R9 live through a potentially trapping divide.
    case IC_DIV: case IC_MOD:
      return false;
    default:
      if (!OptStackBinary(node.code))
        return false;
      // Pass5 can emit a folded RHS directly. Conservatively validate the
      // original operand too, so the generic fallback is equally safe.
      switch (node.rhs.kind) {
        case COptRhsKind::Stack:
        case COptRhsKind::Imm32:
        case COptRhsKind::Frame:
          break;
        case COptRhsKind::Reg:
          if (node.rhs.reg<12 || node.rhs.reg>15)
            return false;
          break;
        default:
          return false;
      }
      return child(node.left) && child(node.right);
  }
}

void OptPass6(CIntermediateCode *code,uint32_t count,CHashFun *fun)
{
  constexpr uint32_t r9_mask=1U<<9; // REGG_STK_TMP in KernelA.HH.
  bool safe=!fun || !(fun->cls.flags&FF_DOT_DOT_DOT);
  for (uint32_t i=0;i<count;i++) {
    code[i].stack_reg=false;
    switch (code[i].code) {
      case IC_ASM: case IC_ASM_ADDR: case IC_GET_LABEL:
      case IC_GET_RFLAGS: case IC_CALL_TRY: case IC_CATCH_ENTRY:
      case IC_SUB_CALL: case IC_SUB_ENTRY: case IC_RET:
        safe=false;
        break;
      default:
        break;
    }
  }
  if (!safe)
    return;

  // Like OptPass6Lag's single lowest stack slot, the left result lives
  // in R9 only while its RHS executes. Earlier nodes are already scored;
  // a selected node in that RHS would overwrite the live temporary.
  bool used=false;
  for (uint32_t i=0;i<count;i++) {
    auto &node=code[i];
    if (!OptStackBinary(node.code) || !OptStackInt(node) ||
        node.rhs.kind!=COptRhsKind::Stack || node.left>=i || node.right>=i ||
        !OptStackInt(code[node.left]) || !OptStackInt(code[node.right]))
      continue;
    unsigned budget=512;
    if (OptR9Free(code,node.right,0,budget)) {
      node.stack_reg=true;
      used=true;
    }
  }
  if (fun && used) {
    // Other compiler passes may already account for R9. Only add bits.
    fun->used_reg_mask|=r9_mask;
    fun->clobbered_reg_mask|=r9_mask;
  }
}
