#pragma once

#include "../Kernel/KernelB.hpp"
#include "CompilerA.hpp"

enum CmpTemplate : uint8_t
{
  CMP_TEMPLATE_INC,CMP_TEMPLATE_DEC,CMP_TEMPLATE_MOD,
  CMP_TEMPLATE_LESS,CMP_TEMPLATE_GREATER,CMP_TEMPLATE_LESS_EQU,
  CMP_TEMPLATE_GREATER_EQU,CMP_TEMPLATE_STRLEN,CMP_TEMPLATE_RDTSC,
  CMP_TEMPLATE_SIGN_I64,CMP_TEMPLATE_FSTP,CMP_TEMPLATE_FLD,
  CMP_TEMPLATE_SQR,CMP_TEMPLATE_ABS,CMP_TEMPLATE_SQRT,CMP_TEMPLATE_SIN,
  CMP_TEMPLATE_COS,CMP_TEMPLATE_TAN,CMP_TEMPLATE_ATAN,
  CMP_TEMPLATE_COUNT
};

class CCodeOut
{
public:
  explicit CCodeOut(uint8_t *buf, CAOT *aot, const COptRegPlan &locals)
      : buf(buf), aot(aot), locals(locals)
  {
  }

  template <class... T> void Bytes(T... bytes)
  {
    (Byte(static_cast<uint8_t>(bytes)), ...);
  }

  void MoveRAX(uint64_t bits)
  {
    Bytes(0x48, 0xB8);
    Immediate(bits, 8);
  }

  void Instruction(const CCodeBytes &ins)
  {
    if (!ins.count)
      failed=true;
    for (unsigned i=0;i<ins.count;i++) Byte(ins.bytes[i]);
  }

  void Memory(CMemOp op,CRawType type,uint8_t base,
              uint8_t index=16,uint8_t shift=0,int32_t disp=0)
  {
    Instruction(ICModr1(op,type,base,index,shift,disp));
  }

  void AddSub(bool inc,uint64_t amount)
  {
    if (amount==uint64_t(int64_t(int8_t(amount))))
      Bytes(0x48,0x83,inc?0xC0:0xE8,amount);
    else if (amount==uint64_t(int64_t(int32_t(amount)))) {
      Bytes(0x48,inc?0x05:0x2D);
      Immediate(amount,4);
    } else {
      Bytes(0x48,0xBA); // MOV RDX,imm64
      Immediate(amount,8);
      Bytes(0x48,inc?0x01:0x29,0xD0);
    }
  }

  void LocalLoad(uint8_t reg)
  {
    Bytes(0x4C,0x89,0xC0+((reg&7)<<3)); // MOV RAX,R12..R15
  }

  void LocalStore(uint8_t reg)
  {
    Bytes(0x49,0x89,0xC0+(reg&7)); // MOV R12..R15,RAX
  }

  void LocalFrame(bool load,const COptReg &reg)
  {
    bool short_disp=reg.offset>=-128 && reg.offset<=127;
    Bytes(0x4C,load?0x8B:0x89,(short_disp?0x45:0x85)+((reg.reg&7)<<3));
    Immediate(reg.offset,short_disp?1:4);
  }

  void LocalsToFrame()
  {
    for (unsigned i=0;i<locals.count;i++) LocalFrame(false,locals.regs[i]);
  }

  void LocalsFromFrame()
  {
    for (unsigned i=0;i<locals.count;i++) LocalFrame(true,locals.regs[i]);
  }

  unsigned LocalsEnter()
  {
    for (unsigned i=0;i<locals.count;i++)
      Bytes(0x41,0x50+(locals.regs[i].reg&7));
    // Preserve the initial contents of frame slots, including arguments.
    LocalsFromFrame();
    return locals.count;
  }

  void LocalsLeave()
  {
    LocalsToFrame();
    for (unsigned i=locals.count;i;i--)
      Bytes(0x41,0x58+(locals.regs[i-1].reg&7));
  }

  void StringAddress(size_t offset)
  {
    if (aot) {
      Bytes(0x48, 0x8D, 0x05); // LEA RAX,[RIP+disp32]
      Immediate(offset - (size + 4), 4);
    } else
      MoveRAX(buf ? reinterpret_cast<uintptr_t>(buf + offset) : 0);
  }

  void RelAddress(const void *symbol, const char *name = nullptr)
  {
    if (name)
      Bytes(0x48, 0xB8); // MOV RAX,imm64
    else
      Bytes(0x48, 0x8D, 0x05); // LEA RAX,[RIP+disp32]
    if (buf && !AOTRelAdd(*aot, aot->size + size, symbol, name))
      failed = true;
    Immediate(0, name ? 8 : 4);
  }

  void HeapGlblAddress(CAOTHeapGlbl &glbl)
  {
    Bytes(0x48, 0xB8); // MOV RAX,imm64
    if (buf && !AOTHeapGlblRefAdd(glbl, aot->size + size))
      failed = true;
    Immediate(0, 8);
  }

  bool Asm(CAsm &as)
  {
    if (buf && !AsmEmit(as, buf, size))
      return false;
    size += AsmLayout(as, size);
    return true;
  }

  bool AOT() const { return aot != nullptr; }
  bool failed = false;
  bool r9_live=false;

  // A call is an opaque R9 clobber. Spill before evaluating its target
  // or arguments, so those expressions may also use the temporary.
  bool SpillR9()
  {
    bool saved=r9_live;
    if (saved) {
      Bytes(0x41,0x51); // PUSH R9
      r9_live=false;
    }
    return saved;
  }

  void RestoreR9(bool saved)
  {
    if (saved) Bytes(0x41,0x59); // POP R9
    r9_live=saved;
  }

  void Immediate(uint64_t bits, unsigned bytes)
  {
    for (unsigned i = 0; i < bytes; ++i)
      Byte(bits >> (8 * i));
  }

  void StackAdjust(int32_t bytes)
  {
    if (bytes >= -128 && bytes <= 127)
      Bytes(0x48, 0x83, 0xC4, bytes);
    else {
      Bytes(0x48, 0x81, 0xC4);
      Immediate(static_cast<uint32_t>(bytes), 4);
    }
  }

  size_t Branch(CICode op,bool short_branch=false)
  {
    if (short_branch) {
      Bytes(op==IC_JMP?0xEB:op==IC_BR_ZERO?0x74:0x75,0);
      return size;
    }
    if (op == IC_JMP || op == IC_SUB_CALL)
      Bytes(op == IC_JMP ? 0xE9 : 0xE8);
    else
      Bytes(0x0F, op == IC_BR_ZERO ? 0x84 : 0x85);
    Immediate(0, 4);
    return size;
  }

  void PatchBranch(size_t end, size_t target,bool short_branch=false)
  {
    if (!buf)
      return;
    int64_t disp=static_cast<int64_t>(target)-static_cast<int64_t>(end);
    if (short_branch && (disp<INT8_MIN || disp>INT8_MAX)) {
      failed=true;
      return;
    }
    if (disp<INT32_MIN || disp>INT32_MAX) {
      failed=true;
      return;
    }
    unsigned width=short_branch?1:4;
    for (unsigned i=0;i<width;i++)
      buf[end-width+i]=static_cast<uint32_t>(disp)>>(8*i);
  }

  void LabelBranch(CIntermediateCode &node,CICode op,size_t target)
  {
    auto end=Branch(op,node.short_branch);
    if (end>INT32_MAX) failed=true;
    node.branch_end=static_cast<uint32_t>(end);
    PatchBranch(end,target,node.short_branch);
  }

  void Switch(const CSwitchTable &sw, const CIntermediateCode *code,
              bool nobound)
  {
    if (sw.lo) {
      Bytes(0x48, 0xBA); // MOV RDX,imm64
      Immediate(sw.lo, 8);
      Bytes(0x48, 0x29, 0xD0); // SUB RAX,RDX
    }
    if (!nobound) {
      Bytes(0x48, 0x3D); // CMP RAX,imm32
      Immediate(sw.range, 4);
      Bytes(0x0F, 0x83); // JAE default
      Immediate(0, 4);
      PatchBranch(size, code[sw.dft].data);
    }
    Bytes(0x48, 0x8D, 0x15); // LEA RDX,[RIP+table]
    Immediate(9, 4);
    Bytes(0x48, 0x63, 0x04, 0x82);       // MOVSXD RAX,DWORD [RDX+RAX*4]
    Bytes(0x48, 0x01, 0xD0, 0xFF, 0xE0); // ADD RAX,RDX / JMP RAX
    auto table = size;
    const auto *targets = reinterpret_cast<const uint32_t *>(&sw + 1);
    for (uint32_t i = 0; i < sw.range; ++i)
      Immediate(code[targets[i]].data - table, 4);
  }

  void Extend(CRawType type, uint8_t modrm)
  {
    switch (TypeSize(type)) {
    case 1:
      Bytes(0x48, 0x0F, IsUnsigned(type) ? 0xB6 : 0xBE, modrm);
      break;
    case 2:
      Bytes(0x48, 0x0F, IsUnsigned(type) ? 0xB7 : 0xBF, modrm);
      break;
    case 4:
      if (IsUnsigned(type))
        Bytes(0x8B, modrm);
      else
        Bytes(0x48, 0x63, modrm);
      break;
    }
  }

  void Load(CRawType type)
  {
    if (TypeSize(type) == 8)
      Bytes(0x48, 0x8B, 0x00); // MOV RAX,[RAX]
    else
      Extend(type, 0x00);
  }

  void Store(CRawType type)
  {
    size_t width = TypeSize(type);
    if (width == 8)
      Byte(0x48);
    else if (width == 2)
      Byte(0x66);
    Bytes(width == 1 ? 0x88 : 0x89, 0x02); // MOV [RDX],AL/AX/EAX/RAX
  }

  void SetBool(uint8_t condition)
  {
    Bytes(0x0F, 0x90 + condition, 0xC0); // SETcc AL
    Bytes(0x0F, 0xB6, 0xC0);             // MOVZX EAX,AL
  }

  void Test()
  {
    Bytes(0x48, 0x85, 0xC0); // TEST RAX,RAX
  }

  void FloatMem(uint8_t opcode, uint8_t op, unsigned slot)
  {
    Byte(opcode);
    Bytes((slot ? 0x44 : 0x04) + (op << 3), 0x24);
    if (slot)
      Byte(slot * 8);
  }

  void ToFloat(CRawType type, unsigned slot)
  {
    if (type != RT_F64) {
      FloatMem(0xDF, 5, slot); // FILD
      FloatMem(0xDD, 3, slot); // FSTP
    }
  }

  void Convert(CRawType from, CRawType to)
  {
    if (!IsVoid(from) && !IsVoid(to) && (from == RT_F64) != (to == RT_F64)) {
      Bytes(0x50); // PUSH RAX
      if (to == RT_F64)
        ToFloat(from, 0);
      else {
        FloatMem(0xDD, 0, 0); // FLD
        FloatMem(0xDD, 1, 0); // FISTTP
      }
      Bytes(0x58); // POP RAX
    }
  }

  void DropPair()
  {
    Bytes(0x48, 0x8D, 0x64, 0x24, 16); // LEA RSP,[RSP+16]
  }

  size_t size = 0;

private:
  void Byte(uint8_t byte)
  {
    if (buf)
      buf[size] = byte;
    ++size;
  }

  uint8_t *buf;
  CAOT *aot;
  const COptRegPlan &locals;
};


uint8_t CmpCondition(CICode op,bool uns);
void IntBinary(CCodeOut &out,CICode op,bool uns);
void FltBinary(CCodeOut &out,const CIntermediateCode &node,
               CRawType left,CRawType right,unsigned stack_slots);
void ICCompoundAssign(CCodeOut &out,CIntermediateCode node,
                      const CIntermediateCode &rhs,CRawType left,
                      bool by_val,unsigned stack_slots,
                      uint8_t local_reg=0,const COptAddress *frame=nullptr);
bool ICUnary(CCodeOut &out,CICode op,CRawType type);
void ICMinMax(CCodeOut &out,CICode op);
void ICBitOps(CCodeOut &out,CICode op,bool immediate,int64_t bit);
void ICModU64(CCodeOut &out);
void ICSwap(CCodeOut &out,CICode op);
void ICPort(CCodeOut &out,CICode op,bool immediate,uint8_t port);
bool ICCopyTemplate(CCodeOut &out,CmpTemplate id);
bool ICFTemplateFun(CCodeOut &out,CICode op,CRawType type);
uintptr_t CmpSysExport(const char *name);
