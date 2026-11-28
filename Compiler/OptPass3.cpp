#include "CompilerA.hpp"

// OptPass012 has already established the operand tree. Score each frame
// member by direct dereferences, rejecting addresses that escape the lvalue.
static bool OptFrameMember(const CHashFun &fun,const CMemberLst &m)
{
  if (!m.member_class || m.size!=8 || m.dim.next ||
      m.dim.total_cnt!=1 || (m.flags&(MLF_STATIC|MLF_DOT_DOT_DOT)) ||
      m.reg==REG_NONE || m.offset%8)
    return false;
  auto type=ClassRawType(*m.member_class);
  if (type!=RT_I64 && type!=RT_U64)
    return false;
  if (m.offset<0)
    return m.offset>=fun.cls.size && m.offset<=-8;
  return m.offset>=16 && uint64_t(m.offset-16)/8<fun.arg_cnt;
}

static bool OptFrameRef(const CIntermediateCode *code,uint32_t count,
                        uint32_t index,int64_t &offset)
{
  if (index>=count || code[index].code!=IC_RBP)
    return false;
  offset=static_cast<int64_t>(code[index].data);
  return true;
}

static bool OptFrameAccess(const CIntermediateCode &node,const CMemberLst &m)
{
  return node.code==IC_DEREF && m.member_class &&
         node.cls==m.member_class && !node.dim &&
         node.type==ClassRawType(*m.member_class);
}

static bool OptDirectFrameAccess(const CHashFun &fun,
                                 const CIntermediateCode &node,int64_t offset)
{
  auto width=TypeSize(node.type);
  if (!width)
    return false;
  for (auto *m=fun.cls.member_lst_and_root;m;m=m->next)
    if (offset==m->offset && m->size>=int64_t(width) &&
        !(m->flags&(MLF_STATIC|MLF_DOT_DOT_DOT)) && !m->dim.next &&
        m->dim.total_cnt==1 && OptFrameAccess(node,*m))
      return offset<0?offset>=fun.cls.size && offset<=-int64_t(width):
          offset>=16 && !(offset%8) && uint64_t(offset-16)/8<fun.arg_cnt;
  return false;
}

static bool OptFrameExposed(const CIntermediateCode *code,uint32_t count,
                            const CHashFun &fun)
{
  // HolyC can form a pointer to one frame member and use it to access an
  // adjacent slot. An escape of ANY frame address therefore invalidates
  // the register allocation, even when that member is declared noreg.
  // Externally fabricated frame addresses (e.g. from arbitrary assembly)
  // are not tracked; arbitrary frame introspection is not guaranteed to
  // observe register variables, in the reference either.
  for (uint32_t i=0;i<count;i++) {
    const auto &node=code[i];
    // A string node's left/right fields are byte count and data offset,
    // and calls use right for argument count, not an operand index.
    if (node.code==IC_STR_CONST)
      continue;
    int64_t offset;
    if (OptFrameRef(code,count,node.left,offset) &&
        !OptDirectFrameAccess(fun,node,offset))
      return true;
    if (node.code!=IC_CALL && node.code!=IC_CALL_INDIRECT &&
        node.code!=IC_CALL_TRY && node.code!=IC_GET_LABEL &&
        node.code!=IC_JMP && node.code!=IC_SUB_CALL &&
        node.code!=IC_BR_ZERO && node.code!=IC_BR_NOT_ZERO &&
        node.code!=IC_RETURN_VAL && node.code!=IC_RET &&
        OptFrameRef(code,count,node.right,offset))
      return true;
    if (node.code==IC_CALL_INDIRECT &&
        OptFrameRef(code,count,node.call_target,offset))
      return true;
  }
  return false;
}

static bool OptNameEqual(const char *a,const char *b)
{
  if (!a) return false;
  while (*a && *a==*b) {
    ++a;
    ++b;
  }
  return *a==*b;
}

static bool OptContextName(const char *name)
{
  // SysTry saves a register snapshot; TaskContextRestore transfers to a
  // saved task rather than returning through the ordinary call epilogue.
  return OptNameEqual(name,"SysTry") ||
         OptNameEqual(name,"TaskContextRestore") ||
         OptNameEqual(name,"_TASK_CONTEXT_RESTORE");
}

bool OptContextTransfer(const CIntermediateCode &node)
{
  if ((node.code!=IC_CALL && node.code!=IC_ADDR) || !node.data)
    return false;
  const auto &fun=*reinterpret_cast<const CHashFun *>(node.data);
  const auto &sym=fun.cls.sym;
  if (!(fun.cls.flags&(CF_EXTERN|FF__EXTERN)) &&
      !(sym.hash.type&HTF_IMPORT))
    return false;
  if (OptContextName(sym.hash.str) || OptContextName(sym.import_name))
    return true;
  if ((fun.cls.flags&FF__EXTERN) && fun.ext_lst)
    return OptContextName(
        static_cast<const CHashExport *>(fun.ext_lst)->sym.hash.str);
  return false;
}

static uint32_t OptMemberScore(const CIntermediateCode *code,uint32_t count,
                               const CMemberLst &m)
{
  uint32_t score=0;
  for (uint32_t i=0;i<count;i++) {
    int64_t offset;
    if (OptFrameAccess(code[i],m) &&
        OptFrameRef(code,count,code[i].left,offset) && offset==m.offset &&
        score<UINT32_MAX)
      ++score;
  }
  return score;
}

static bool OptRegUsed(const COptRegPlan &plan,const CMemberLst *m,uint8_t reg)
{
  for (unsigned i=0;i<plan.count;i++)
    if (plan.regs[i].m==m || plan.regs[i].reg==reg)
      return true;
  return false;
}

static void OptRegAdd(COptRegPlan &plan,CMemberLst &m,uint8_t reg)
{
  plan.regs[plan.count++]={&m,m.offset,reg};
}

void OptPass3(CIntermediateCode *code,uint32_t count,CHashFun &fun,
              COptRegPlan &plan,bool no_reg_var)
{
  plan={};
  constexpr uint32_t reg_mask=0xF000; // R12..R15, preserved by SysV.
  fun.used_reg_mask&=~reg_mask;
  bool safe=!(fun.cls.flags&FF_DOT_DOT_DOT) && !no_reg_var;
  for (uint32_t i=0;i<count;i++) {
    code[i].local_reg=0;
    switch (code[i].code) {
      case IC_ASM: case IC_CALL_TRY: case IC_CATCH_ENTRY:
      case IC_SUB_CALL: case IC_SUB_ENTRY: case IC_RET:
      case IC_GET_LABEL: case IC_GET_RFLAGS:
        safe=false;
        break;
      default:
        if (OptContextTransfer(code[i]))
          safe=false;
        break;
    }
  }

  if (safe && !OptFrameExposed(code,count,fun)) {
    // Honor supported explicitly named registers before automatic choices.
    for (auto *m=fun.cls.member_lst_and_root;m;m=m->next)
      if (m->reg>=12 && m->reg<=15 && !OptRegUsed(plan,m,m->reg) &&
          OptFrameMember(fun,*m) && OptMemberScore(code,count,*m))
        OptRegAdd(plan,*m,m->reg);

    // As in the reference, prefer R12/R13 for non-pointers, leaving the
    // simpler-addressing R14/R15 for either kind of local. The other
    // reference registers are not in the native SysV nonvolatile set.
    for (uint8_t reg=12;reg<=15;reg++) {
      if (OptRegUsed(plan,nullptr,reg))
        continue;
      CMemberLst *best=nullptr;
      uint32_t best_score=0;
      for (auto *m=fun.cls.member_lst_and_root;m;m=m->next) {
        if ((m->reg!=REG_UNDEF && m->reg!=REG_ALLOC) ||
            OptRegUsed(plan,m,reg) || !OptFrameMember(fun,*m) ||
            (reg<14 && m->member_class->ptr_stars_cnt))
          continue;
        uint32_t score=OptMemberScore(code,count,*m);
        if (!score || (score<2 && m->reg!=REG_ALLOC))
          continue;
        if (!best || (m->reg==REG_ALLOC && best->reg!=REG_ALLOC) ||
            (m->reg==best->reg && score>best_score)) {
          best=m;
          best_score=score;
        }
      }
      if (best)
        OptRegAdd(plan,*best,reg);
    }
  }

  // An explicit request that could not be honored must not be reported
  // as an allocated register in the function's member metadata.
  for (auto *m=fun.cls.member_lst_and_root;m;m=m->next)
    if (m->reg>=0 && m->reg<16 && !OptRegUsed(plan,m,0))
      m->reg=REG_NONE;

  for (unsigned r=0;r<plan.count;r++) {
    const auto &reg=plan.regs[r];
    reg.m->reg=reg.reg;
    fun.used_reg_mask|=uint32_t{1}<<reg.reg;
    for (uint32_t i=0;i<count;i++) {
      int64_t offset;
      if (OptFrameAccess(code[i],*reg.m) &&
          OptFrameRef(code,count,code[i].left,offset) && offset==reg.offset)
        code[i].local_reg=reg.reg;
    }
  }
}
