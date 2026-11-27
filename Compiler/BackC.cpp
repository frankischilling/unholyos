#include "CompilerB.hpp"

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
  if (op==IC_RDTSC)
    return ICCopyTemplate(out,CMP_TEMPLATE_RDTSC);
  if (op>=IC_IN_U8 && op<=IC_IN_U32) {
    const auto &port=code[node.left];
    bool immediate=port.code==IC_IMM_I64 && int64_t(port.data)<=255;
    if (!immediate && !Emit(out,code,node.left,depth+1,stack_slots))
      return false;
    ICPort(out,op,immediate,port.data);
    return true;
  }
  if (op>=IC_OUT_U8 && op<=IC_OUT_U32) {
    const auto &port=code[node.left];
    bool immediate=port.code==IC_IMM_I64 && int64_t(port.data)<=255;
    if (!Emit(out,code,node.right,depth+1,stack_slots)) return false;
    if (!immediate) {
      out.Bytes(0x50); // Preserve value across port evaluation.
      if (!Emit(out,code,node.left,depth+1,stack_slots+1)) return false;
      out.Bytes(0x48,0x89,0xC2,0x58); // MOV RDX,RAX / POP RAX
    }
    ICPort(out,op,immediate,port.data);
    return true;
  }
  if (op>=IC_BT && op<=IC_LBTC) {
    const auto &bit=code[node.right];
    bool immediate=bit.code==IC_IMM_I64 && int64_t(bit.data)<64;
    if (!immediate) {
      if (!Emit(out,code,node.right,depth+1,stack_slots)) return false;
      out.Bytes(0x50);
    }
    if (!Emit(out,code,node.left,depth+1,stack_slots+!immediate)) return false;
    if (!immediate) out.Bytes(0x59); // POP RCX
    ICBitOps(out,op,immediate,bit.data);
    return true;
  }
  if ((op>=IC_SWAP_U8 && op<=IC_SWAP_I64) || op==IC_MOD_U64) {
    if (!Emit(out,code,node.right,depth+1,stack_slots)) return false;
    out.Bytes(0x50);
    if (!Emit(out,code,node.left,depth+1,stack_slots+1)) return false;
    out.Bytes(0x59); // POP RCX
    if (op==IC_MOD_U64) ICModU64(out);
    else ICSwap(out,op);
    return !out.failed;
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
  if (op>=IC_MIN_I64 && op<=IC_MAX_U64) {
    if (!Emit(out, code, node.right, depth + 1, stack_slots))
      return false;
    out.Convert(code[node.right].type, RT_I64);
    out.Bytes(0x50); // PUSH RAX
    if (!Emit(out, code, node.left, depth + 1, stack_slots + 1))
      return false;
    out.Convert(code[node.left].type, RT_I64);
    ICMinMax(out,op);
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
    bool spilled=out.SpillR9();
    stack_slots+=spilled;
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
    out.RestoreR9(spilled);
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
      ICCopyTemplate(out,inc?CMP_TEMPLATE_INC:CMP_TEMPLATE_DEC);
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
  if (ICUnary(out,op,left) || ICFTemplateFun(out,op,left))
    return true;
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
  bool use_r9=node.stack_reg && !out.r9_live;
  if (node.chain) {
    out.Bytes(0x50); // Keep the preceding comparison's result.
    out.Bytes(0x51); // Reuse the preceding comparison's right operand.
    left = code[code[node.left].right].type;
  } else if (use_r9) {
    out.Bytes(0x49,0x89,0xC1); // MOV R9,RAX
    out.r9_live=true;
  } else
    out.Bytes(0x50);
  if (!Emit(out, code, node.right, depth + 1,
            stack_slots + (node.chain ? 2 : use_r9 ? 0 : 1)))
    return false;
  if (logical) {
    out.Test();
    out.SetBool(5);
  }
  out.Bytes(0x48,0x89,0xC1); // MOV RCX,RAX
  if (use_r9) {
    out.Bytes(0x4C,0x89,0xC8); // MOV RAX,R9
    out.r9_live=false;
  } else
    out.Bytes(0x58); // POP RAX
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
      out.LabelBranch(node,node.code,code[node.right].data);
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
      out.LabelBranch(node,node.code,code[node.right].data);
      break;
    default:
      if (node.left != NO_IC && !Emit(out, code, node.left, 0, stack_slots))
        return false;
      if (node.code == IC_RETURN_VAL) {
        if (node.left != NO_IC)
          out.Convert(code[node.left].type, type);
        out.LabelBranch(node,IC_JMP,code[node.right].data);
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
  OptPass6(coc.code,coc.Count(),fun);
  for (uint32_t i=0;i<coc.Count();i++) {
    coc.code[i].branch_end=0;
    coc.code[i].short_branch=false;
    if (coc.code[i].code==IC_LABEL)
      coc.code[i].data=UINT64_MAX;
  }
  CCodeOut size(nullptr, aot, locals);
  if (!EmitFunction(size, coc, root, type, fun) || size.failed ||
      size.size > INT32_MAX)
    return nullptr;
  // Shortening an instruction cannot increase a branch distance when
  // layout contains no alignment-sensitive assembly. Iterate with a
  // bounded number of rounds; every round preserves earlier selections.
  for (unsigned pass=0;pass<32 && OptPass789A(coc.code,coc.Count());pass++) {
    size.size=0;
    if (!EmitFunction(size,coc,root,type,fun) || size.failed ||
        size.size>INT32_MAX)
      return nullptr;
  }
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
      out.size!=size.size ||
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
