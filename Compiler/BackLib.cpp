#include "CompilerA.hpp"

// The reference splits ModR/M selection from the SIB/displacement tail.
// Here the indexed-IR back end emits fixed-size instruction byte records.
struct CModr
{
  uint8_t rex,modr,sib,disp_size;
  bool has_sib,valid;
};

static CModr ICModr1(uint8_t base,uint8_t index,uint8_t shift,int32_t disp,
                    uint8_t reg,bool wide)
{
  constexpr uint8_t no_index=16;
  if (base>=16 || reg>=16 || index>no_index || index==4 || shift>3 ||
      (index==no_index && shift))
    return {};

  const bool has_index=index!=no_index;
  // RSP and R12 require SIB even without an index. RBP and R13 require a
  // displacement even at offset zero (mod=00 would encode RIP-relative).
  const bool sib=has_index || (base&7)==4;
  uint8_t mode=!disp && (base&7)!=5?0:disp>=INT8_MIN && disp<=INT8_MAX?1:2;
  uint8_t rex=0x40 | (wide?8:0) | (reg>=8?4:0) |
              (has_index && index>=8?2:0) | (base>=8?1:0);
  if (rex==0x40)
    rex=0; // Only AL/AX/EAX/RAX are used by the public encoders.
  return {rex,uint8_t((mode<<6)|((reg&7)<<3)|(sib?4:base&7)),
          uint8_t((shift<<6)|((has_index?index&7:4)<<3)|(base&7)),
          uint8_t(mode==1?1:mode==2?4:0),sib,true};
}

static void ICU8(CCodeBytes &code,uint8_t byte)
{
  code.bytes[code.count++]=byte;
}

static void ICImm(CCodeBytes &code,uint32_t bits,unsigned size)
{
  for (unsigned i=0;i<size;i++)
    ICU8(code,bits>>(8*i));
}

static void ICModr2(CCodeBytes &code,const CModr &modr,int32_t disp)
{
  ICU8(code,modr.modr);
  if (modr.has_sib)
    ICU8(code,modr.sib);
  ICImm(code,static_cast<uint32_t>(disp),modr.disp_size);
}

CCodeBytes ICModr1(CMemOp op,CRawType type,uint8_t base,uint8_t index,
                    uint8_t shift,int32_t disp)
{
  size_t width=TypeSize(type);
  bool wide=width==8;
  switch (op) {
    case CMemOp::Lea:
      if (!wide || !IsInt(type))
        return {};
      break;
    case CMemOp::Load:
      if (!width)
        return {};
      // MOVZX/MOVSX to RAX, or MOVSXD for signed 32-bit memory.
      wide=type!=RT_U32;
      break;
    case CMemOp::Store:
      if (!width)
        return {};
      break;
    default:
      return {};
  }
  auto modr=ICModr1(base,index,shift,disp,0,wide);
  if (!modr.valid)
    return {};

  CCodeBytes code{};
  if (op==CMemOp::Store && width==2)
    ICU8(code,0x66);
  if (modr.rex)
    ICU8(code,modr.rex);
  switch (op) {
    case CMemOp::Lea:
      ICU8(code,0x8D);
      break;
    case CMemOp::Load:
      switch (type) {
        case RT_I8: case RT_U8:
        case RT_I16: case RT_U16:
          ICU8(code,0x0F);
          ICU8(code,width==1?(IsUnsigned(type)?0xB6:0xBE):
                             (IsUnsigned(type)?0xB7:0xBF));
          break;
        case RT_I32:
          ICU8(code,0x63);
          break;
        default:
          ICU8(code,0x8B);
          break;
      }
      break;
    case CMemOp::Store:
      ICU8(code,width==1?0x88:0x89);
      break;
  }
  ICModr2(code,modr,disp);
  return code;
}

// One integer operand already resides in RAX. The second is immediate,
// register or RBP-relative memory. Comparison emits CMP; the back end
// remains responsible for selecting the signed/unsigned SETcc.
CCodeBytes ICBinaryRhs(CICode op,const COptRhs &rhs)
{
  uint8_t opcode,slash;
  switch (op) {
    case IC_ADD: opcode=0x03; slash=0; break;
    case IC_OR:  opcode=0x0B; slash=1; break;
    case IC_AND: opcode=0x23; slash=4; break;
    case IC_SUB: opcode=0x2B; slash=5; break;
    case IC_XOR: opcode=0x33; slash=6; break;
    case IC_MUL: opcode=0xAF; slash=0; break;
    default:
      if (!ICIsComparison(op))
        return {};
      opcode=0x3B;
      slash=7;
      break;
  }

  CCodeBytes code{};
  switch (rhs.kind) {
    case COptRhsKind::Imm32: {
      bool short_imm=rhs.disp>=INT8_MIN && rhs.disp<=INT8_MAX;
      ICU8(code,0x48);
      if (op==IC_MUL) {
        ICU8(code,short_imm?0x6B:0x69);
        ICU8(code,0xC0); // IMUL RAX,RAX,imm
      } else {
        ICU8(code,short_imm?0x83:0x81);
        ICU8(code,0xC0|(slash<<3));
      }
      ICImm(code,static_cast<uint32_t>(rhs.disp),short_imm?1:4);
      break;
    }
    case COptRhsKind::Reg:
      if (rhs.reg>=16)
        return {};
      ICU8(code,0x48|(rhs.reg>=8?1:0));
      if (op==IC_MUL)
        ICU8(code,0x0F);
      ICU8(code,opcode);
      ICU8(code,0xC0|(rhs.reg&7));
      break;
    case COptRhsKind::Frame: {
      auto modr=ICModr1(5,16,0,rhs.disp,0,true);
      if (!modr.valid)
        return {};
      ICU8(code,modr.rex);
      if (op==IC_MUL)
        ICU8(code,0x0F);
      ICU8(code,opcode);
      ICModr2(code,modr,rhs.disp);
      break;
    }
    default:
      return {};
  }
  return code;
}
