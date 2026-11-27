#include "../Kernel/KernelB.hpp"
#include "CompilerA.hpp"

extern "C" uint64_t F64PowerBits(uint64_t base, uint64_t exponent);

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

  bool Asm(CAsm &as)
  {
    if (buf && !AsmEmit(as, buf, size))
      return false;
    size += AsmLayout(as, size);
    return true;
  }

  bool AOT() const { return aot != nullptr; }
  bool failed = false;

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

  size_t Branch(CICode op)
  {
    if (op == IC_JMP || op == IC_SUB_CALL)
      Bytes(op == IC_JMP ? 0xE9 : 0xE8);
    else
      Bytes(0x0F, op == IC_BR_ZERO ? 0x84 : 0x85);
    Immediate(0, 4);
    return size;
  }

  void PatchBranch(size_t end, size_t target)
  {
    if (!buf)
      return;
    auto disp = static_cast<uint32_t>(target - end);
    for (unsigned i = 0; i < 4; ++i)
      buf[end - 4 + i] = disp >> (8 * i);
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

static uint8_t CmpCondition(CICode op, bool uns)
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

static void IntBinary(CCodeOut &out, CICode op, bool uns)
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

static void FltBinary(CCodeOut &out, const CIntermediateCode &node,
                      CRawType left, CRawType right, unsigned stack_slots)
{
  auto op = node.code;
  out.Bytes(0x50, 0x51); // PUSH RAX / PUSH RCX
  out.ToFloat(left, 1);
  out.ToFloat(right, 0);
  if (op == IC_POWER) {
    out.Bytes(0x48, 0x8B, 0x7C, 0x24, 8, 0x48, 0x8B, 0x34, 0x24);
    out.DropPair();
    bool pad = !(stack_slots & 1);
    if (pad)
      out.Bytes(0x48, 0x83, 0xEC, 8); // SUB RSP,8
    if (out.AOT())
      out.failed = true;
    out.MoveRAX(reinterpret_cast<uintptr_t>(F64PowerBits));
    out.Bytes(0xFF, 0xD0); // CALL RAX
    if (pad)
      out.Bytes(0x48, 0x83, 0xC4, 8); // ADD RSP,8
    return;
  }
  if (op == IC_AND || op == IC_OR || op == IC_XOR || op == IC_SHL ||
      op == IC_SHR ||
      (!node.runtime && (op == IC_EQU_EQU || op == IC_NOT_EQU))) {
    out.Bytes(0x48, 0x8B, 0x44, 0x24, 8); // MOV RAX,[RSP+8]
    switch (op) {
    case IC_AND:
      out.Bytes(0x48, 0x23, 0x04, 0x24);
      break;
    case IC_OR:
      out.Bytes(0x48, 0x0B, 0x04, 0x24);
      break;
    case IC_XOR:
      out.Bytes(0x48, 0x33, 0x04, 0x24);
      break;
    case IC_SHL:
    case IC_SHR:
      out.Bytes(0x48, 0x8B, 0x0C, 0x24); // MOV RCX,[RSP]
      IntBinary(out, op, false);
      break;
    default:
      out.Bytes(0x48, 0x3B, 0x04, 0x24); // CMP RAX,[RSP]
      out.SetBool(CmpCondition(op, true));
      break;
    }
    out.DropPair();
    return;
  }
  if (ICIsComparison(op)) {
    out.FloatMem(0xDD, 0, 0);
    out.FloatMem(0xDD, 0, 1);
    out.Bytes(0xDF, 0xE9, 0xDD, 0xD8); // FUCOMIP ST0,ST1 / FSTP ST0
    out.DropPair();
    out.SetBool(CmpCondition(op, true));
    if (op == IC_LESS || op == IC_LESS_EQU || op == IC_EQU_EQU)
      out.Bytes(0x0F, 0x9B, 0xC2, 0x20, 0xD0); // SETNP DL / AND AL,DL
    else if (op == IC_NOT_EQU)
      out.Bytes(0x0F, 0x9A, 0xC2, 0x08, 0xD0); // SETP DL / OR AL,DL
    return;
  }
  if (op == IC_MOD) {
    out.FloatMem(0xDD, 0, 0);
    out.FloatMem(0xDD, 0, 1);
    // FPREM may need more than one reduction. C2 marks a partial result.
    out.Bytes(0xD9, 0xF8, 0xDF, 0xE0, 0xF6, 0xC4, 4, 0x75, 0xF7);
    out.Bytes(0xDD, 0xD9); // FSTP ST1
  } else {
    out.FloatMem(0xDD, 0, 1);
    uint8_t math = op == IC_ADD ? 0 : op == IC_MUL ? 1 : op == IC_SUB ? 4 : 6;
    out.FloatMem(0xDC, math, 0);
  }
  out.FloatMem(0xDD, 3, 1);
  out.Bytes(0x59, 0x58); // POP RCX / POP RAX
}

static void ICCompoundAssign(CCodeOut &out, CIntermediateCode node,
                             const CIntermediateCode &rhs, CRawType left,
                             bool by_val, unsigned stack_slots,
                             uint8_t local_reg = 0,
                             const COptAddress *frame = nullptr)
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

static void FunAddress(CCodeOut &out, const CHashFun &fun, bool indirect)
{
  if (out.AOT()) {
    if (fun.ext_lst)
      out.RelAddress(fun.ext_lst);
    else {
      const auto &sym = fun.cls.sym;
      const char *name = sym.import_name;
      if (!name && (fun.cls.flags & FF__EXTERN))
        name = sym.hash.str;
      out.RelAddress(&fun, name);
    }
  } else
    out.MoveRAX(indirect ? reinterpret_cast<uintptr_t>(&fun.exe_addr)
                         : reinterpret_cast<uintptr_t>(fun.exe_addr));
}

static bool Emit(CCodeOut &out,const CIntermediateCode *code,uint32_t index,
                 unsigned depth=0,unsigned stack_slots=0);

static bool FrameAddress(const COptAddress &addr)
{
  return addr.valid && addr.base==NO_IC && addr.index==NO_IC;
}

static bool EmitAddress(CCodeOut &out,const CIntermediateCode *code,
                        const CIntermediateCode &target,CMemOp op,
                        unsigned depth,unsigned stack_slots)
{
  const auto &addr=target.address;
  if (!addr.valid) {
    if (!Emit(out,code,target.left,depth,stack_slots)) return false;
    if (op==CMemOp::Load) out.Load(target.type);
    return true;
  }
  uint8_t base=5,index=16;
  if (addr.base!=NO_IC) {
    if (!Emit(out,code,addr.base,depth,stack_slots)) return false;
    base=0;
  }
  if (addr.index!=NO_IC) {
    if (base==0) out.Bytes(0x50); // Preserve base across index evaluation.
    if (!Emit(out,code,addr.index,depth,stack_slots+(base==0))) return false;
    if (base==0) {
      out.Bytes(0x5A); // POP RDX
      base=2;
    }
    index=0;
  }
  out.Memory(op,op==CMemOp::Lea?RT_I64:target.type,base,index,
             addr.shift,addr.disp);
  return !out.failed;
}

static bool Emit(CCodeOut &out, const CIntermediateCode *code, uint32_t index,
                 unsigned depth, unsigned stack_slots)
{
  // The parser has a fixed nesting limit. A flat expression can still have
  // a deeper tree on the left than it did on the operator stack.
  if (depth >= 512)
    return false;
  const auto &node = code[index];
  auto op = node.code;
  if (op == IC_GET_RFLAGS) {
    out.Bytes(0x9C, 0x58); // PUSHFQ; POP RAX
    return true;
  }
  if (op == IC_FS || op == IC_GS) {
    out.Bytes(op == IC_FS ? 0x64 : 0x65, 0x48, 0x8B, 0x04, 0x25);
    out.Immediate(0, 4);
    return true;
  }
  if (op == IC_QUE_INS || op == IC_QUE_INS_REV) {
    if (!Emit(out, code, node.right, depth + 1, stack_slots))
      return false;
    out.Bytes(0x50); // PUSH predecessor or successor
    if (!Emit(out, code, node.left, depth + 1, stack_slots + 1))
      return false;
    out.Bytes(0x59); // POP RCX
    if (op == IC_QUE_INS) {
      out.Bytes(0x48,0x8B,0x11);      // MOV RDX,[RCX]
      out.Bytes(0x48,0x89,0x01);      // MOV [RCX],RAX
      out.Bytes(0x48,0x89,0x10);      // MOV [RAX],RDX
      out.Bytes(0x48,0x89,0x48,0x08); // MOV [RAX+8],RCX
      out.Bytes(0x48,0x89,0x42,0x08); // MOV [RDX+8],RAX
    } else {
      out.Bytes(0x48,0x8B,0x51,0x08); // MOV RDX,[RCX+8]
      out.Bytes(0x48,0x89,0x02);      // MOV [RDX],RAX
      out.Bytes(0x48,0x89,0x08);      // MOV [RAX],RCX
      out.Bytes(0x48,0x89,0x50,0x08); // MOV [RAX+8],RDX
      out.Bytes(0x48,0x89,0x41,0x08); // MOV [RCX+8],RAX
    }
    return true;
  }
  if (op == IC_MIN_I64 || op == IC_MAX_I64) {
    if (!Emit(out, code, node.right, depth + 1, stack_slots))
      return false;
    out.Convert(code[node.right].type, RT_I64);
    out.Bytes(0x50); // PUSH RAX
    if (!Emit(out, code, node.left, depth + 1, stack_slots + 1))
      return false;
    out.Convert(code[node.left].type, RT_I64);
    out.Bytes(0x59, 0x48, 0x39, 0xC8); // POP RCX / CMP RAX,RCX
    out.Bytes(0x48, 0x0F, op == IC_MIN_I64 ? 0x4F : 0x4C, 0xC1);
    return true;
  }
  if (op == IC_ADDR) {
    const auto &fun = *reinterpret_cast<const CHashFun *>(node.data);
    bool indirect = !out.AOT() && (fun.cls.flags & CF_EXTERN);
    FunAddress(out, fun, indirect);
    if (indirect)
      out.Load(RT_I64);
    return true;
  }
  if (op == IC_GET_LABEL) {
    out.Bytes(0x48, 0x8D, 0x05); // LEA RAX,[RIP+label]
    out.Immediate(0, 4);
    out.PatchBranch(out.size, code[node.right].data);
    return true;
  }
  if (op == IC_CALL || op == IC_CALL_INDIRECT || op == IC_CALL_TRY) {
    auto *fun = reinterpret_cast<const CHashFun *>(node.data);
    uint32_t slots = node.right, arg = node.left;
    bool indirect_call = op == IC_CALL_INDIRECT;
    // SysTryHC aligns its own C++ call and saves the unpadded guest stack.
    bool pad =
        op != IC_CALL_TRY && !((stack_slots + slots + indirect_call) & 1);
    if (pad)
      out.StackAdjust(-8);
    if (indirect_call) {
      if (!Emit(out, code, node.call_target, depth + 1, stack_slots + pad))
        return false;
      out.Bytes(0x50); // Keep the target below the arguments.
    }
    // Arguments are evaluated and pushed from right to left.
    for (uint32_t pushed = 0; pushed < slots; ++pushed) {
      const auto &push = code[arg];
      if (!Emit(out, code, push.left, depth + 1,
                stack_slots + pad + indirect_call + pushed))
        return false;
      out.Convert(code[push.left].type, push.type);
      out.Bytes(0x50); // PUSH RAX
      arg = push.right;
    }
    // Opaque HolyC imports and handwritten assembly need not follow the
    // native register convention. Synchronize homes after argument/target
    // evaluation so mutations in those expressions are not lost.
    out.LocalsToFrame();
    if (indirect_call) {
      out.Bytes(0xFF, 0x94, 0x24); // CALL [RSP+disp32]
      out.Immediate(8 * slots, 4);
    } else {
      bool indirect = !out.AOT() && (fun->cls.flags & CF_EXTERN);
      FunAddress(out, *fun, indirect);
      out.Bytes(0xFF, indirect ? 0x10 : 0xD0); // CALL [RAX] or CALL RAX
    }
    uint32_t drop = pad + indirect_call + (FunPopsArgs(*fun) ? 0 : slots);
    if (drop)
      out.StackAdjust(8 * drop);
    out.LocalsFromFrame();
    return true;
  }
  if (op == IC_RBP) {
    out.Bytes(0x48, 0x8D, 0x85); // LEA RAX,[RBP+disp32]
    out.Immediate(node.data, 4);
    return true;
  }
  if (op == IC_ASSIGN || ICIsCompoundAssign(op)) {
    const auto &target = code[node.left];
    if (target.local_reg) {
      if (!Emit(out,code,node.right,depth+1,stack_slots))
        return false;
      if (op==IC_ASSIGN)
        out.Convert(code[node.right].type,target.type);
      else
        ICCompoundAssign(out,node,code[node.right],target.type,true,
                         stack_slots,target.local_reg);
      out.LocalStore(target.local_reg);
      return true;
    }
    bool frame=FrameAddress(target.address);
    if (!frame) {
      if (!EmitAddress(out,code,target,CMemOp::Lea,depth+1,stack_slots))
        return false;
      out.Bytes(0x50); // Save address across RHS evaluation.
    }
    if (!Emit(out, code, node.right, depth + 1, stack_slots + !frame))
      return false;
    if (op == IC_ASSIGN) {
      out.Convert(code[node.right].type, target.type);
      out.Extend(target.type, 0xC0);
    } else {
      auto address = code[target.left].code;
      ICCompoundAssign(out, node, code[node.right], target.type,
                       address == IC_IMM_I64 || address == IC_DATA_ADDR ||
                           address == IC_GLBL_ADDR || address == IC_RBP,
                       stack_slots + !frame,0,frame?&target.address:nullptr);
    }
    if (frame)
      out.Memory(CMemOp::Store,target.type,5,16,0,target.address.disp);
    else {
      out.Bytes(0x5A); // POP RDX
      out.Store(target.type);
    }
    return true;
  }
  if (op == IC_STR_CONST) {
    out.StringAddress(node.right);
    return true;
  }
  if (op == IC_ASM_ADDR) {
    if (!out.AOT())
      return false;
    out.RelAddress(reinterpret_cast<const void *>(node.data));
    return true;
  }
  if (op == IC_GLBL_ADDR) {
    auto *var = reinterpret_cast<const CHashGlblVar *>(node.data);
    if (out.AOT())
      out.RelAddress(var->flags & GVF_ALIAS
                         ? static_cast<const void *>(var->data_addr)
                         : static_cast<const void *>(var),
                     var->sym.import_name);
    else if (var->flags & GVF_EXTERN) {
      out.MoveRAX(reinterpret_cast<uintptr_t>(&var->data_addr));
      out.Load(RT_I64);
    } else
      out.MoveRAX(reinterpret_cast<uintptr_t>(var->data_addr));
    return true;
  }
  if (op == IC_DATA_ADDR) {
    if (out.AOT())
      out.RelAddress(reinterpret_cast<const void *>(node.data));
    else
      out.MoveRAX(node.data);
    return true;
  }
  if (op == IC_IMM_I64 || op == IC_IMM_F64) {
    out.MoveRAX(node.data);
    return true;
  }
  if (IC__PP <= op && op <= IC_MM_) {
    const auto &target = code[node.left];
    if (target.local_reg) {
      bool postfix=op==IC_PP_ || op==IC_MM_;
      bool inc=op==IC__PP || op==IC_PP_;
      out.LocalLoad(target.local_reg);
      if (postfix)
        out.Bytes(0x48,0x89,0xC1); // MOV RCX,RAX
      out.AddSub(inc,node.data);
      out.LocalStore(target.local_reg);
      if (postfix)
        out.Bytes(0x48,0x89,0xC8); // MOV RAX,RCX
      return true;
    }
    bool frame=FrameAddress(target.address);
    if (frame)
      out.Memory(CMemOp::Load,target.type,5,16,0,target.address.disp);
    else {
      if (!EmitAddress(out,code,target,CMemOp::Lea,depth+1,stack_slots))
        return false;
      out.Bytes(0x50); // Save the address for the store.
      out.Load(target.type);
    }
    const bool postfix = op == IC_PP_ || op == IC_MM_;
    const bool inc = op == IC__PP || op == IC_PP_;
    if (postfix)
      out.Bytes(0x50);
    if (target.type == RT_F64) {
      out.Bytes(0xB9, 1, 0, 0, 0); // MOV ECX,1
      CIntermediateCode math{0,      NO_IC, NO_IC, inc ? IC_ADD : IC_SUB,
                             RT_F64, false, false, true};
      FltBinary(out, math, target.type, RT_I64, stack_slots + !frame + postfix);
    } else {
      out.AddSub(inc,node.data);
      out.Extend(target.type, 0xC0);
    }
    if (postfix)
      out.Bytes(0x59); // POP RCX
    if (frame)
      out.Memory(CMemOp::Store,target.type,5,16,0,target.address.disp);
    else {
      out.Bytes(0x5A); // POP RDX
      out.Store(target.type);
    }
    if (postfix)
      out.Bytes(0x48, 0x89, 0xC8); // MOV RAX,RCX
    return true;
  }
  if (op==IC_DEREF && node.local_reg) {
    out.LocalLoad(node.local_reg);
    return true;
  }
  if (op==IC_DEREF && node.address.valid)
    return EmitAddress(out,code,node,CMemOp::Load,depth+1,stack_slots);
  if (!Emit(out, code, node.left, depth + 1, stack_slots))
    return false;
  auto left = code[node.left].type;
  if (op == IC_QUE_INIT) {
    out.Bytes(0x48,0x89,0x00);      // MOV [RAX],RAX
    out.Bytes(0x48,0x89,0x40,0x08); // MOV [RAX+8],RAX
    return true;
  }
  if (op == IC_QUE_REM) {
    out.Bytes(0x48,0x8B,0x08);      // MOV RCX,[RAX]
    out.Bytes(0x48,0x8B,0x50,0x08); // MOV RDX,[RAX+8]
    out.Bytes(0x48,0x89,0x0A);      // MOV [RDX],RCX
    out.Bytes(0x48,0x89,0x51,0x08); // MOV [RCX+8],RDX
    return true;
  }
  if (op == IC_STRLEN) {
    out.Bytes(0x31,0xC9);           // XOR ECX,ECX
    out.Bytes(0x80,0x3C,0x08,0);    // CMP BYTE [RAX+RCX],0
    out.Bytes(0x74,5);              // JE done
    out.Bytes(0x48,0xFF,0xC1);      // INC RCX
    out.Bytes(0xEB,0xF5);           // JMP CMP
    out.Bytes(0x48,0x89,0xC8);      // MOV RAX,RCX
    return true;
  }
  if (op == IC_SQR_I64) {
    out.Convert(left, RT_I64);
    out.Bytes(0x48, 0x0F, 0xAF, 0xC0); // IMUL RAX,RAX
    return true;
  }
  if (op == IC_SQRT || op == IC_SIN || op == IC_COS) {
    out.Bytes(0x50); // PUSH RAX
    if (left == RT_F64)
      out.FloatMem(0xDD, 0, 0); // FLD
    else
      out.FloatMem(0xDF, 5, 0); // FILD
    out.Bytes(0xD9, op == IC_SQRT ? 0xFA : op == IC_SIN ? 0xFE : 0xFF);
    out.FloatMem(0xDD, 3, 0); // FSTP
    out.Bytes(0x58); // POP RAX
    return true;
  }
  if (op == IC_DEREF) {
    if (!TypeSize(node.type))
      return false;
    out.Load(node.type);
    return true;
  }
  if (op == IC_HOLYC_TYPECAST) {
    if (code[node.left].code == IC_DEREF)
      out.Extend(node.type, 0xC0);
    return true;
  }
  if (op == IC_TO_I64 || op == IC_TO_F64) {
    out.Convert(left, op == IC_TO_I64 ? RT_I64 : RT_F64);
    return true;
  }
  if (op == IC_NOT) {
    out.Test();
    out.SetBool(4);
    return true;
  }
  if (op == IC_COM || op == IC_UNARY_MINUS) {
    if (op == IC_UNARY_MINUS && left == RT_F64)
      out.Bytes(0x48, 0x0F, 0xBA, 0xF8, 63); // BTC RAX,63
    else
      out.Bytes(0x48, 0xF7, op == IC_COM ? 0xD0 : 0xD8);
    return true;
  }
  if (op == IC_ADD_CONST || op == IC_SUB_CONST) {
    out.AddSub(op==IC_ADD_CONST,node.data);
    return true;
  }
  if (op == IC_SHL_CONST || op == IC_SHR_CONST) {
    bool uns=IsUnsigned(left) || IsUnsigned(code[node.right].type);
    out.Bytes(0x48,0xC1,op==IC_SHL_CONST?0xE0:uns?0xE8:0xF8,node.data);
    return true;
  }
  if (node.rhs.kind!=COptRhsKind::Stack) {
    auto ins=ICBinaryRhs(op,node.rhs);
    if (ins.count) {
      out.Instruction(ins);
      if (ICIsComparison(op))
        out.SetBool(CmpCondition(op,IsUnsigned(left) ||
                                  IsUnsigned(code[node.right].type)));
      return true;
    }
  }
  // A value expression evaluates both operands. Branch lowering belongs to
  // the statement compiler.
  bool logical = op == IC_AND_AND || op == IC_OR_OR || op == IC_XOR_XOR;
  if (logical) {
    out.Test();
    out.SetBool(5);
  }
  if (node.chain) {
    out.Bytes(0x50); // Keep the preceding comparison's result.
    out.Bytes(0x51); // Reuse the preceding comparison's right operand.
    left = code[code[node.left].right].type;
  } else
    out.Bytes(0x50);
  if (!Emit(out, code, node.right, depth + 1,
            stack_slots + (node.chain ? 2 : 1)))
    return false;
  if (logical) {
    out.Test();
    out.SetBool(5);
  }
  out.Bytes(0x48, 0x89, 0xC1, 0x58); // MOV RCX,RAX / POP RAX
  auto right = code[node.right].type;
  if (!logical && (left == RT_F64 || right == RT_F64 || op == IC_POWER))
    FltBinary(out, node, left, right, stack_slots);
  else
    IntBinary(out, op, IsUnsigned(left) || IsUnsigned(right));
  if (node.chain)
    out.Bytes(0x5A, 0x48, 0x21, 0xD0); // POP RDX / AND RAX,RDX
  return true;
}

static bool EmitBody(CCodeOut &out, CCodeCtrl &coc, CRawType type,
                     unsigned stack_slots)
{
  auto *code = coc.code;
  for (auto i = coc.first; i != NO_IC; i = code[i].next) {
    auto &node = code[i];
    switch (node.code) {
    case IC_ASM:
      if (!out.Asm(*reinterpret_cast<CAsm *>(node.data)))
        return false;
      break;
    case IC_LABEL:
      node.data = out.size;
      break;
    case IC_JMP:
      // A folded condition still leaves its value in RAX, as the original
      // conditional branch did.
      if (node.left!=NO_IC && !Emit(out,code,node.left,0,stack_slots))
        return false;
      [[fallthrough]];
    case IC_SUB_CALL:
      out.PatchBranch(out.Branch(node.code), code[node.right].data);
      break;
    case IC_SUB_ENTRY:
      ++stack_slots;
      break;
    case IC_CATCH_ENTRY:
      node.data = stack_slots;
      stack_slots = 1; // TestExcept enters the handler on its aligned stack.
      break;
    case IC_RET:
      if (node.right == NO_IC)
        --stack_slots;
      else
        stack_slots = code[node.right].data;
      out.Bytes(0xC3);
      break;
    case IC_SWITCH:
    case IC_NOBOUND_SWITCH:
      if (!Emit(out, code, node.left, 0, stack_slots))
        return false;
      out.Convert(code[node.left].type, RT_I64);
      out.Switch(*reinterpret_cast<const CSwitchTable *>(node.data), code,
                 node.code == IC_NOBOUND_SWITCH);
      break;
    case IC_BR_ZERO:
    case IC_BR_NOT_ZERO:
      if (!Emit(out, code, node.left, 0, stack_slots))
        return false;
      out.Test();
      out.PatchBranch(out.Branch(node.code), code[node.right].data);
      break;
    default:
      if (node.left != NO_IC && !Emit(out, code, node.left, 0, stack_slots))
        return false;
      if (node.code == IC_RETURN_VAL) {
        if (node.left != NO_IC)
          out.Convert(code[node.left].type, type);
        out.PatchBranch(out.Branch(IC_JMP), code[node.right].data);
      }
      break;
    }
  }
  return true;
}

static bool EmitFunction(CCodeOut &out, CCodeCtrl &coc, uint32_t root,
                         CRawType type, const CHashFun *fun)
{
  unsigned stack_slots = 0;
  if (fun) {
    out.Bytes(0x55, 0x48, 0x89, 0xE5); // PUSH RBP / MOV RBP,RSP
    auto frame_size = static_cast<int32_t>((-fun->cls.size + 15) & ~15);
    if (frame_size)
      out.StackAdjust(-frame_size);
    stack_slots = 1+out.LocalsEnter();
  }
  if (root == NO_IC) {
    if (!EmitBody(out, coc, type, stack_slots))
      return false;
  } else {
    if (!Emit(out, coc.code, root, 0, stack_slots))
      return false;
    out.Convert(coc.code[root].type, type);
  }
  if (fun) {
    out.LocalsLeave();
    out.Bytes(0xC9); // LEAVE
  }
  if (fun && fun->arg_cnt && FunPopsArgs(*fun)) {
    out.Bytes(0xC2); // RET imm16
    out.Immediate(8 * fun->arg_cnt, 2);
  } else
    out.Bytes(0xC3); // RET
  return true;
}

uint8_t *COCCompile(CCodeCtrl &coc, uint32_t root, CRawType type,
                    CHashFun *fun, CAOT *aot, size_t *bytes)
{
  if (coc.failed || !coc.GoToLabelsDefined())
    return nullptr;
  OptPass012(coc.code,coc.Count());
  COptRegPlan locals;
  if (fun)
    OptPass3(coc.code,coc.Count(),*fun,locals);
  OptPass4(coc.code,coc.Count());
  OptPass5(coc.code,coc.Count());
  CCodeOut size(nullptr, aot, locals);
  if (!EmitFunction(size, coc, root, type, fun) || size.failed ||
      size.size > INT32_MAX)
    return nullptr;
  if (coc.asm_first)
    AsmLabelsLayout(*coc.asm_first);
  auto total_size = size.size;
  if (coc.has_misc_data)
    for (uint32_t i = 0; i < coc.Count(); ++i) {
      auto &node = coc.code[i];
      if (node.code != IC_STR_CONST)
        continue;
      if (node.left > INT32_MAX - total_size)
        return nullptr;
      node.right = total_size;
      total_size += node.left;
    }
  auto *result = MAlloc(total_size, CurrentTask()->code_heap);
  if (!result)
    return nullptr;
  CCodeOut out(result, aot, locals);
  if (!EmitFunction(out, coc, root, type, fun) || out.failed ||
      (coc.asm_first && !AsmPublish(*coc.asm_first, result))) {
    Free(result);
    return nullptr;
  }
  if (coc.has_misc_data)
    for (uint32_t i = 0; i < coc.Count(); ++i) {
      const auto &node = coc.code[i];
      if (node.code == IC_STR_CONST)
        MemCpy(result + node.right, reinterpret_cast<const void *>(node.data),
               node.left);
    }
  if (bytes)
    *bytes = total_size;
  return result;
}
