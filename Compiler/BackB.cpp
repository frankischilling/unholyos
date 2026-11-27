#include "CompilerB.hpp"

// Scalar unary instructions and the integer intrinsic family.
bool ICUnary(CCodeOut &out,CICode op,CRawType type)
{
  switch (op) {
    case IC_TOUPPER:
      out.Convert(type,RT_I64);
      out.Bytes(0x48,0x83,0xF8,'a',0x7C,10);
      out.Bytes(0x48,0x83,0xF8,'z',0x7F,4);
      out.Bytes(0x48,0x83,0xC0,0xE0);
      break;
    case IC_TO_BOOL:
      out.Convert(type,RT_I64);
      out.Test();
      out.SetBool(5);
      break;
    case IC_BSF: case IC_BSR:
      out.Convert(type,RT_I64);
      out.Bytes(0x48,0x0F,op==IC_BSF?0xBC:0xBD,0xC0);
      out.Bytes(0x75,3,0x48,0xF7,0xD0); // JNZ / NOT RAX
      break;
    case IC_ABS_I64:
      out.Convert(type,RT_I64);
      out.Test();
      out.Bytes(0x79,3,0x48,0xF7,0xD8); // JNS / NEG RAX
      break;
    case IC_SIGN_I64:
      out.Convert(type,RT_I64);
      ICCopyTemplate(out,CMP_TEMPLATE_SIGN_I64);
      break;
    case IC_SQR_I64: case IC_SQR_U64:
      out.Convert(type,RT_I64);
      if (op==IC_SQR_I64)
        out.Bytes(0x48,0x0F,0xAF,0xC0); // IMUL RAX,RAX
      else
        out.Bytes(0x48,0xF7,0xE0); // MUL RAX
      break;
    case IC_STRLEN:
      ICCopyTemplate(out,CMP_TEMPLATE_STRLEN);
      break;
    default:
      return false;
  }
  return true;
}

void ICMinMax(CCodeOut &out,CICode op)
{
  uint8_t condition=op==IC_MIN_I64?0x4F:op==IC_MAX_I64?0x4C:
                    op==IC_MIN_U64?0x47:0x42;
  out.Bytes(0x59,0x48,0x39,0xC8); // POP RCX / CMP RAX,RCX
  out.Bytes(0x48,0x0F,condition,0xC1);
}

// RAX holds the bit-field address; RCX holds a variable bit index.
// The reference uses a 32-bit memory operand for immediate indices below 32.
void ICBitOps(CCodeOut &out,CICode op,bool immediate,int64_t bit)
{
  bool locked=op>=IC_LBTS && op<=IC_LBTC;
  unsigned kind=locked?op-IC_LBTS+1:op-IC_BT;
  out.Bytes(0x48,0x89,0xC2,0x31,0xC0); // MOV RDX,RAX / XOR EAX,EAX
  if (locked) out.Bytes(0xF0);
  if (!immediate || bit>=32) out.Bytes(0x48);
  if (immediate)
    out.Bytes(0x0F,0xBA,0x22+(kind<<3),bit); // BT/BTS/BTR/BTC [RDX],imm8
  else
    out.Bytes(0x0F,0xA3+(kind<<3),0x0A); // BT/BTS/BTR/BTC [RDX],RCX
  out.Bytes(0x0F,0x92,0xC0); // SETC AL (return the previous bit)
}

// The pointer is evaluated after the divisor. A failed DIV must not store
// a quotient; the original unsigned dividend remains in memory on a fault.
void ICModU64(CCodeOut &out)
{
  out.Bytes(0x49,0x89,0xC0); // MOV R8,RAX
  out.Bytes(0x49,0x8B,0x00); // MOV RAX,[R8]
  out.Bytes(0x31,0xD2,0x48,0xF7,0xF1); // XOR EDX,EDX / DIV RCX
  out.Bytes(0x49,0x89,0x00,0x48,0x89,0xD0); // MOV [R8],RAX / MOV RAX,RDX
}

// RAX and RCX are the two addresses. Load both values before either store,
// including for overlapping fields. These operations are not atomic.
void ICSwap(CCodeOut &out,CICode op)
{
  switch (op) {
    case IC_SWAP_U8:
      out.Bytes(0x0F,0xB6,0x10,0x44,0x0F,0xB6,0x01);
      out.Bytes(0x88,0x11,0x44,0x88,0x00);
      break;
    case IC_SWAP_U16:
      out.Bytes(0x0F,0xB7,0x10,0x44,0x0F,0xB7,0x01);
      out.Bytes(0x66,0x89,0x11,0x66,0x44,0x89,0x00);
      break;
    case IC_SWAP_U32:
      out.Bytes(0x8B,0x10,0x44,0x8B,0x01,0x89,0x11,0x44,0x89,0x00);
      break;
    case IC_SWAP_I64:
      out.Bytes(0x48,0x8B,0x10,0x4C,0x8B,0x01,0x48,0x89,0x11,0x4C,0x89,0x00);
      break;
    default:
      out.failed=true;
  }
}

void ICPort(CCodeOut &out,CICode op,bool immediate,uint8_t port)
{
  bool input=op<=IC_IN_U32;
  unsigned width=input?op-IC_IN_U8:op-IC_OUT_U8;
  if (input) {
    if (!immediate) out.Bytes(0x48,0x89,0xC2); // MOV RDX,RAX
    out.Bytes(0x31,0xC0); // Zero-extend every input width.
  }
  if (width==1) out.Bytes(0x66);
  uint8_t opcode=(input?0xE4:0xE6)+(width!=0)+(immediate?0:8);
  out.Bytes(opcode);
  if (immediate) out.Bytes(port);
}
