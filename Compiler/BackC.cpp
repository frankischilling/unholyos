#include "../Kernel/KernelB.hpp"
#include "CompilerA.hpp"

extern "C" uint64_t F64PowerBits(uint64_t base, uint64_t exponent);

class CCodeOut
{
public:
  explicit CCodeOut(uint8_t *buf, CAOT *aot) : buf(buf), aot(aot)
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
    if (op == IC_JMP)
      Bytes(0xE9);
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
    if (from != RT_U0 && to != RT_U0 && (from == RT_F64) != (to == RT_F64)) {
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
                             bool by_val, unsigned stack_slots)
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
  out.Bytes(0x48, 0x8B, 0x04, 0x24); // MOV RAX,[RSP]
  out.Load(left);
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

static bool Emit(CCodeOut &out, const CIntermediateCode *code, uint32_t index,
                 unsigned depth = 0, unsigned stack_slots = 0)
{
  // The parser has a fixed nesting limit. A flat expression can still have
  // a deeper tree on the left than it did on the operator stack.
  if (depth == 512)
    return false;
  const auto &node = code[index];
  auto op = node.code;
  if (op == IC_CALL) {
    auto *fun = reinterpret_cast<const CHashFun *>(node.data);
    uint32_t slots = node.right, arg = node.left;
    bool pad = !((stack_slots + slots) & 1);
    if (pad)
      out.StackAdjust(-8);
    // Arguments are evaluated and pushed from right to left.
    for (uint32_t pushed = 0; pushed < slots; ++pushed) {
      const auto &push = code[arg];
      if (!Emit(out, code, push.left, depth + 1, stack_slots + pad + pushed))
        return false;
      out.Convert(code[push.left].type, push.type);
      out.Bytes(0x50); // PUSH RAX
      arg = push.right;
    }
    bool indirect = !out.AOT() && (fun->cls.flags & CF_EXTERN);
    if (out.AOT()) {
      const auto &sym = fun->cls.sym;
      const char *name = sym.import_name;
      if (fun->ext_lst)
        out.RelAddress(fun->ext_lst);
      else {
        if (!name && (fun->cls.flags & FF__EXTERN))
          name = sym.hash.str;
        out.RelAddress(fun, name);
      }
    } else
      out.MoveRAX(indirect ? reinterpret_cast<uintptr_t>(&fun->exe_addr)
                           : reinterpret_cast<uintptr_t>(fun->exe_addr));
    out.Bytes(0xFF, indirect ? 0x10 : 0xD0); // CALL [RAX] or CALL RAX
    uint32_t drop = pad + (FunPopsArgs(*fun) ? 0 : slots);
    if (drop)
      out.StackAdjust(8 * drop);
    return true;
  }
  if (op == IC_RBP) {
    out.Bytes(0x48, 0x8D, 0x85); // LEA RAX,[RBP+disp32]
    out.Immediate(node.data, 4);
    return true;
  }
  if (op == IC_ASSIGN || ICIsCompoundAssign(op)) {
    const auto &target = code[node.left];
    if (!Emit(out, code, target.left, depth + 1, stack_slots))
      return false;
    out.Bytes(
        0x50); // Save the address through evaluation of the right operand.
    if (!Emit(out, code, node.right, depth + 1, stack_slots + 1))
      return false;
    if (op == IC_ASSIGN) {
      out.Convert(code[node.right].type, target.type);
      out.Extend(target.type, 0xC0);
    } else {
      auto address = code[target.left].code;
      ICCompoundAssign(out, node, code[node.right], target.type,
                       address == IC_IMM_I64 || address == IC_DATA_ADDR ||
                           address == IC_GLBL_ADDR || address == IC_RBP,
                       stack_slots + 1);
    }
    out.Bytes(0x5A); // POP RDX
    out.Store(target.type);
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
    if (!Emit(out, code, target.left, depth + 1, stack_slots))
      return false;
    out.Bytes(0x50); // Save the address for the store.
    out.Load(target.type);
    const bool postfix = op == IC_PP_ || op == IC_MM_;
    const bool inc = op == IC__PP || op == IC_PP_;
    if (postfix)
      out.Bytes(0x50);
    if (target.type == RT_F64) {
      out.Bytes(0xB9, 1, 0, 0, 0); // MOV ECX,1
      CIntermediateCode math{0,      NO_IC, NO_IC, inc ? IC_ADD : IC_SUB,
                             RT_F64, false, false, true};
      FltBinary(out, math, target.type, RT_I64, stack_slots + 1 + postfix);
    } else {
      out.Bytes(0x48, 0x83, inc ? 0xC0 : 0xE8, node.data); // ADD/SUB RAX,imm8
      out.Extend(target.type, 0xC0);
    }
    if (postfix)
      out.Bytes(0x59); // POP RCX
    out.Bytes(0x5A);   // POP RDX
    out.Store(target.type);
    if (postfix)
      out.Bytes(0x48, 0x89, 0xC8); // MOV RAX,RCX
    return true;
  }
  if (!Emit(out, code, node.left, depth + 1, stack_slots))
    return false;
  auto left = code[node.left].type;
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
  if (op == IC_TO_I64) {
    out.Convert(left, RT_I64);
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
    case IC_LABEL:
      node.data = out.size;
      break;
    case IC_JMP:
      out.PatchBranch(out.Branch(IC_JMP), code[node.right].data);
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
    stack_slots = 1;
  }
  if (root == NO_IC) {
    if (!EmitBody(out, coc, type, stack_slots))
      return false;
  } else {
    if (!Emit(out, coc.code, root, 0, stack_slots))
      return false;
    out.Convert(coc.code[root].type, type);
  }
  if (fun)
    out.Bytes(0xC9); // LEAVE
  if (fun && fun->arg_cnt && FunPopsArgs(*fun)) {
    out.Bytes(0xC2); // RET imm16
    out.Immediate(8 * fun->arg_cnt, 2);
  } else
    out.Bytes(0xC3); // RET
  return true;
}

uint8_t *COCCompile(CCodeCtrl &coc, uint32_t root, CRawType type,
                    const CHashFun *fun, CAOT *aot, size_t *bytes)
{
  CCodeOut size(nullptr, aot);
  if (coc.failed || !EmitFunction(size, coc, root, type, fun) ||
      size.failed || size.size > INT32_MAX)
    return nullptr;
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
  CCodeOut out(result, aot);
  if (!EmitFunction(out, coc, root, type, fun) || out.failed) {
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
