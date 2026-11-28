#include "CompilerB.hpp"

uint8_t CmpCondition(CICode op, bool uns)
{
  switch (op) {
  case IC_EQU_EQU:
    return 4; // E
  case IC_NOT_EQU:
    return 5; // NE
  case IC_LESS:
    return uns ? 2 : 12;
  case IC_GREATER_EQU:
    return uns ? 3 : 13;
  case IC_GREATER:
    return uns ? 7 : 15;
  default:
    return uns ? 6 : 14; // BE or LE
  }
}

void IntBinary(CCodeOut &out, CICode op, bool uns)
{
  switch (op) {
  case IC_ADD:
    out.Bytes(0x48, 0x01, 0xC8);
    break; // ADD RAX,RCX
  case IC_SUB:
    out.Bytes(0x48, 0x29, 0xC8);
    break;
  case IC_MUL:
    out.Bytes(0x48, 0x0F, 0xAF, 0xC1);
    break;
  case IC_AND:
  case IC_AND_AND:
    out.Bytes(0x48, 0x21, 0xC8);
    break;
  case IC_OR:
  case IC_OR_OR:
    out.Bytes(0x48, 0x09, 0xC8);
    break;
  case IC_XOR:
  case IC_XOR_XOR:
    out.Bytes(0x48, 0x31, 0xC8);
    break;
  case IC_SHL:
    out.Bytes(0x48, 0xD3, 0xE0);
    break;
  case IC_SHR:
    out.Bytes(0x48, 0xD3, uns ? 0xE8 : 0xF8);
    break;
  case IC_DIV:
  case IC_MOD:
    if (uns)
      out.Bytes(0x31, 0xD2, 0x48, 0xF7, 0xF1); // XOR EDX,EDX / DIV RCX
    else
      out.Bytes(0x48, 0x99, 0x48, 0xF7, 0xF9); // CQO / IDIV RCX
    if (op == IC_MOD)
      out.Bytes(0x48, 0x89, 0xD0);
    break;
  default:
    out.Bytes(0x48, 0x39, 0xC8); // CMP RAX,RCX
    out.SetBool(CmpCondition(op, uns));
    break;
  }
}

void ICCompoundAssign(CCodeOut &out, CIntermediateCode node,
                             const CIntermediateCode &rhs, CRawType left,
                             bool by_val, unsigned stack_slots,
                             uint8_t local_reg,
                             const COptAddress *frame)
{
  constexpr CICode binary_ops[]{IC_SHL, IC_SHR, IC_MUL, IC_DIV, IC_MOD,
                                IC_AND, IC_OR,  IC_XOR, IC_ADD, IC_SUB};
  auto op = binary_ops[node.code - IC_SHL_EQU];
  auto right = rhs.type;
  bool bit_op = node.code <= IC_SHR_EQU ||
                (IC_AND_EQU <= node.code && node.code <= IC_XOR_EQU);
  if (bit_op) {
    out.Convert(right, RT_I64);
  } else if (IsInt(left) && IsInt(right) && rhs.code == IC_IMM_I64 &&
             rhs.data && !(rhs.data & (rhs.data - 1))) {
    // OptPass012 replaces powers of two in *=, /= and %=.
    switch (op) {
    case IC_MUL:
    case IC_DIV:
      out.MoveRAX(__builtin_ctzll(rhs.data));
      op = op == IC_MUL ? IC_SHL : IC_SHR;
      break;
    case IC_MOD:
      out.MoveRAX(rhs.data - 1);
      op = IC_AND;
      break;
    default:
      break;
    }
  }
  out.Bytes(0x48, 0x89, 0xC1);       // MOV RCX,RAX
  if (local_reg)
    out.LocalLoad(local_reg);
  else if (frame)
    out.Memory(CMemOp::Load,left,5,16,0,frame->disp);
  else {
    out.Bytes(0x48, 0x8B, 0x04, 0x24); // MOV RAX,[RSP]
    out.Load(left);
  }
  if (!bit_op && (left == RT_F64 || right == RT_F64)) {
    node.code = op;
    FltBinary(out, node, left, right, stack_slots);
    out.Convert(RT_F64, left);
    out.Extend(left, 0xC0);
  } else {
    IntBinary(out, op, IsUnsigned(left));
    if (by_val && rhs.code == IC_IMM_I64 && (op == IC_ADD || op == IC_SUB))
      out.Extend(left, 0xC0);
  }
}
