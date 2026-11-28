#include "../../Compiler/CompilerA.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
unsigned checks;

void Check(bool result,const char *name)
{
  if (!result)
    throw std::runtime_error(name);
  ++checks;
}

struct CProbe
{
  CHashClass i64{},u8{},f64{},ptr{},aggregate{};
  CHashFun fun{};
  CMemberLst members[12]{};
  unsigned used=0;
  std::vector<CIntermediateCode> code;
  COptRegPlan plan;

  CProbe()
  {
    i64.flags=u8.flags=f64.flags=CF_INTERNAL_TYPE;
    i64.raw_type=ptr.raw_type=RT_I64;
    u8.raw_type=RT_U8;
    f64.raw_type=RT_F64;
    i64.size=ptr.size=f64.size=aggregate.size=8;
    u8.size=1;
    ptr.ptr_stars_cnt=1;
    fun.cls.size=-96;
    fun.arg_cnt=2;
  }

  CMemberLst &Member(CHashClass &cls,int64_t offset,int8_t reg=REG_UNDEF)
  {
    auto &m=members[used];
    m.member_class=&cls;
    m.reg=reg;
    m.offset=offset;
    m.size=cls.size;
    m.dim.cnt=m.dim.total_cnt=1;
    if (!used)
      fun.cls.member_lst_and_root=&m;
    else
      members[used-1].next=&m;
    ++used;
    return m;
  }

  uint32_t Add(CICode op,CRawType type,uint32_t left=NO_IC,
               uint32_t right=NO_IC,uint64_t data=0)
  {
    code.push_back({data,left,right,op,type,false,false,true});
    return code.size()-1;
  }

  uint32_t Address(const CMemberLst &m)
  {
    return Add(IC_RBP,RT_I64,NO_IC,NO_IC,m.offset);
  }

  uint32_t Access(const CMemberLst &m,CHashClass *cls=nullptr)
  {
    if (!cls) cls=m.member_class;
    auto a=Address(m);
    auto i=Add(IC_DEREF,ClassRawType(*cls),a);
    code[i].cls=cls;
    return i;
  }

  void Run()
  {
    OptPass3(code.data(),code.size(),fun,plan);
  }

  bool Promoted(const CMemberLst &m) const
  {
    return m.reg>=12 && m.reg<=15 && (fun.used_reg_mask&(1U<<m.reg));
  }
};

void AllocationProbe()
{
  CProbe p;
  auto &arg=p.Member(p.i64,16,REG_ALLOC);
  auto &cold=p.Member(p.i64,24,REG_NONE);
  auto &hot=p.Member(p.i64,-8,REG_ALLOC);
  auto &ptr=p.Member(p.ptr,-16,REG_ALLOC);
  auto &fixed=p.Member(p.i64,-24,15);
  auto &other=p.Member(p.i64,-32);
  auto a=p.Access(arg),h=p.Access(hot),d=p.Access(ptr),f=p.Access(fixed);
  p.Access(cold);
  for (int i=0;i<3;i++) p.Access(hot);
  for (int i=0;i<2;i++) p.Access(other);
  // The call's right field is an arity; the string's fields are sizes.
  p.Add(IC_CALL,RT_I64,NO_IC,p.code[a].left);
  p.Add(IC_STR_CONST,RT_I64,p.code[h].left,p.code[d].left);
  p.Run();
  Check(p.plan.count==4 && p.fun.used_reg_mask==0xF000,"four real registers");
  Check(p.Promoted(arg) && p.Promoted(hot) && arg.reg!=hot.reg,
        "distinct argument and local registers");
  Check(p.Promoted(ptr) && ptr.reg==14,"pointer register preference");
  Check(p.Promoted(fixed) && fixed.reg==15,"explicit register priority");
  Check(cold.reg==REG_NONE && !p.Promoted(other),"noreg and pressure fallback");
  Check(p.code[a].local_reg==arg.reg && p.code[h].local_reg==hot.reg &&
        p.code[d].local_reg==ptr.reg && p.code[f].local_reg==fixed.reg,
        "register operand tags");
}

void AliasProbe()
{
  CProbe p;
  auto &escaped=p.Member(p.i64,-8,REG_ALLOC);
  auto &alias=p.Member(p.i64,-16,REG_ALLOC);
  auto &narrow=p.Member(p.u8,-24,REG_ALLOC);
  auto &fp=p.Member(p.f64,-32,REG_ALLOC);
  auto &object=p.Member(p.aggregate,-40,REG_ALLOC);
  auto &array=p.Member(p.i64,-48,REG_ALLOC);
  CArrayDim dim{};
  array.dim.next=&dim;
  auto &unsupported=p.Member(p.i64,-56,0);
  auto &static_var=p.Member(p.i64,-64,REG_ALLOC);
  static_var.flags|=MLF_STATIC;
  auto e=p.Access(escaped),a=p.Access(alias);
  p.Add(IC_HOLYC_TYPECAST,RT_I64,p.code[e].left);
  p.Access(alias,&p.u8);
  p.Access(narrow);
  p.Access(fp);
  p.Access(object);
  p.Access(array);
  p.Access(unsupported);
  p.Access(static_var);
  p.Run();
  Check(!p.plan.count && !p.fun.used_reg_mask,"unsafe and incompatible slots excluded");
  Check(!p.code[e].local_reg && !p.code[a].local_reg,"escaped and reinterpreted slots stay memory");
  Check(unsupported.reg==REG_NONE,"unsupported named register reports fallback");
}

void FrameExposureProbe()
{
  {
    CProbe p;
    auto &anchor=p.Member(p.i64,-8,REG_NONE);
    auto &target=p.Member(p.i64,-16,REG_ALLOC);
    p.Access(anchor);
    auto t=p.Access(target);
    auto base=p.Add(IC_HOLYC_TYPECAST,RT_I64,p.Address(anchor));
    auto delta=p.Add(IC_IMM_I64,RT_I64,NO_IC,NO_IC,uint64_t(-8));
    auto addr=p.Add(IC_ADD_CONST,RT_I64,base,delta,uint64_t(-8));
    auto slot=p.Add(IC_DEREF,RT_I64,addr);
    p.code[slot].cls=&p.i64;
    auto value=p.Add(IC_IMM_I64,RT_I64,NO_IC,NO_IC,9);
    p.Add(IC_ASSIGN,RT_I64,slot,value);
    p.Access(target);
    p.Run();
    Check(!p.plan.count && !p.fun.used_reg_mask && !p.code[t].local_reg,
          "noreg frame escape can alias a different candidate");
  }
  {
    CProbe p;
    auto &target=p.Member(p.i64,-8,REG_ALLOC);
    auto &narrow=p.Member(p.u8,-9,REG_ALLOC);
    auto &fp=p.Member(p.f64,-24,REG_ALLOC);
    auto t=p.Access(target),n=p.Access(narrow),f=p.Access(fp);
    p.Run();
    Check(p.plan.count==1 && p.Promoted(target) && p.code[t].local_reg,
          "exact narrow and floating frame loads do not disable safe promotion");
    Check(!p.Promoted(narrow) && !p.Promoted(fp) &&
          !p.code[n].local_reg && !p.code[f].local_reg,
          "incompatible types still use memory");
  }
  {
    CProbe p;
    auto &target=p.Member(p.i64,-8,REG_ALLOC);
    auto &other=p.Member(p.i64,-16,REG_NONE);
    auto t=p.Access(target);
    p.Access(other,&p.u8);
    p.Run();
    Check(!p.plan.count && !p.code[t].local_reg,
          "reinterpreted direct access invalidates frame analysis");
  }
  {
    CProbe p;
    auto &target=p.Member(p.i64,-8,REG_ALLOC);
    auto t=p.Access(target);
    auto addr=p.Add(IC_RBP,RT_I64,NO_IC,NO_IC,uint64_t(-32));
    auto load=p.Add(IC_DEREF,RT_I64,addr);
    p.code[load].cls=&p.i64;
    p.Run();
    Check(!p.plan.count && !p.code[t].local_reg,
          "unmodeled frame slot invalidates allocation");
  }
  {
    CProbe p;
    auto &target=p.Member(p.i64,-8,REG_ALLOC);
    auto &other=p.Member(p.i64,-16,REG_NONE);
    auto t=p.Access(target);
    auto zero=p.Add(IC_IMM_I64,RT_I64);
    p.Add(IC_ADD,RT_I64,zero,p.Address(other));
    p.Run();
    Check(!p.plan.count && !p.code[t].local_reg,
          "right operand frame address escape");
  }
  {
    CProbe p;
    auto &target=p.Member(p.i64,-8,REG_ALLOC);
    auto &other=p.Member(p.i64,-16,REG_NONE);
    auto t=p.Access(target);
    auto call=p.Add(IC_CALL_INDIRECT,RT_I64,NO_IC,0);
    p.code[call].call_target=p.Address(other);
    p.Run();
    Check(!p.plan.count && !p.code[t].local_reg,
          "indirect call target frame address escape");
  }
}

void ContextProbe()
{
  CHashFun sys_try{},restore{},ordinary{};
  CHashExport external{};
  sys_try.cls.flags=FF__EXTERN;
  sys_try.cls.sym.hash.str="SysTry";
  restore.cls.flags=CF_EXTERN;
  restore.cls.sym.hash.type=HTT_FUN|HTF_IMPORT;
  restore.cls.sym.hash.str="ResumeTask";
  restore.cls.sym.import_name="_TASK_CONTEXT_RESTORE";
  ordinary.cls.sym.hash.str="SysTry";
  CHashFun alias{};
  alias.cls.flags=FF__EXTERN;
  alias.cls.sym.hash.str="ResumeAlias";
  external.sym.hash.str="_TASK_CONTEXT_RESTORE";
  alias.ext_lst=&external;
  for (auto *callee:{&sys_try,&restore,&alias}) {
    CProbe p;
    auto &target=p.Member(p.i64,-8,REG_ALLOC);
    auto t=p.Access(target);
    p.Add(IC_CALL,RT_I64,NO_IC,0,reinterpret_cast<uintptr_t>(callee));
    p.Run();
    Check(!p.plan.count && !p.code[t].local_reg,
          "known low-level context transfer disables allocation");
  }
  {
    CProbe p;
    auto &target=p.Member(p.i64,-8,REG_ALLOC);
    auto t=p.Access(target);
    p.Add(IC_ADDR,RT_I64,NO_IC,NO_IC,reinterpret_cast<uintptr_t>(&sys_try));
    p.Run();
    Check(!p.plan.count && !p.code[t].local_reg,
          "known context transfer function address disables allocation");
  }
  {
    CProbe p;
    auto &target=p.Member(p.i64,-8,REG_ALLOC);
    auto t=p.Access(target);
    p.Add(IC_CALL,RT_I64,NO_IC,0,reinterpret_cast<uintptr_t>(&ordinary));
    p.Run();
    Check(p.plan.count==1 && p.code[t].local_reg,
          "ordinary local function with same name is not a context import");
  }
}

void ControlProbe()
{
  for (auto op:{IC_ASM,IC_CALL_TRY,IC_CATCH_ENTRY,IC_SUB_CALL,
               IC_SUB_ENTRY,IC_RET,IC_GET_LABEL,IC_GET_RFLAGS}) {
    CProbe p;
    auto &m=p.Member(p.i64,-8,15);
    auto i=p.Access(m);
    p.Add(op,RT_U0);
    p.Run();
    Check(!p.plan.count && m.reg==REG_NONE && !p.code[i].local_reg,
          "opaque or special control-flow fallback");
  }
  CProbe p;
  p.fun.cls.flags=FF_DOT_DOT_DOT;
  auto &m=p.Member(p.i64,16,REG_ALLOC);
  auto i=p.Access(m);
  p.Run();
  Check(!p.plan.count && !p.code[i].local_reg,"variadic fallback");
}
}

int main()
{
  try {
    AllocationProbe();
    AliasProbe();
    FrameExposureProbe();
    ContextProbe();
    ControlProbe();
    std::cout<<"OptPass3: "<<checks<<" checks passed\n";
  } catch (const std::exception &error) {
    std::cerr<<error.what()<<'\n';
    return 1;
  }
}
