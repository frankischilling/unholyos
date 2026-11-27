#include "CompilerB.hpp"

extern "C" const uint32_t CMP_TEMPLATES[];

bool ICCopyTemplate(CCodeOut &out,CmpTemplate id)
{
  if (id>=CMP_TEMPLATE_COUNT) {
    out.failed=true;
    return false;
  }
  uint32_t start=CMP_TEMPLATES[id],end=CMP_TEMPLATES[id+1];
  if (start<sizeof(uint32_t)*(CMP_TEMPLATE_COUNT+1) ||
      end<=start || end>CMP_TEMPLATES[CMP_TEMPLATE_COUNT] ||
      end-start>128) {
    out.failed=true;
    return false;
  }
  auto *bytes=reinterpret_cast<const uint8_t *>(CMP_TEMPLATES);
  for (auto i=start;i<end;i++)
    out.Bytes(bytes[i]);
  return true;
}

bool ICFTemplateFun(CCodeOut &out,CICode op,CRawType type)
{
  CmpTemplate id;
  switch (op) {
    case IC_SQR:  id=CMP_TEMPLATE_SQR; break;
    case IC_ABS:  id=CMP_TEMPLATE_ABS; break;
    case IC_SQRT: id=CMP_TEMPLATE_SQRT; break;
    case IC_SIN:  id=CMP_TEMPLATE_SIN; break;
    case IC_COS:  id=CMP_TEMPLATE_COS; break;
    case IC_TAN:  id=CMP_TEMPLATE_TAN; break;
    case IC_ATAN: id=CMP_TEMPLATE_ATAN; break;
    default: return false;
  }
  // Keep the earlier direct FILD path for existing integer trig operands.
  // New intrinsic operands enter as binary64 values.
  if (type!=RT_F64 && (op==IC_SQRT || op==IC_SIN || op==IC_COS)) {
    out.Bytes(0x50);
    out.FloatMem(0xDF,5,0);
    out.Bytes(0xD9,op==IC_SQRT?0xFA:op==IC_SIN?0xFE:0xFF);
    out.FloatMem(0xDD,3,0);
    out.Bytes(0x58);
  } else {
    out.Convert(type,RT_F64);
    ICCopyTemplate(out,id);
  }
  return true;
}
