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
