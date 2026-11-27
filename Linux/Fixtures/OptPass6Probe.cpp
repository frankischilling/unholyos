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
  std::vector<CIntermediateCode> code;
  CHashFun fun{};

  uint32_t Add(CICode op,CRawType type=RT_I64,uint32_t left=NO_IC,
               uint32_t right=NO_IC,uint64_t data=0)
  {
    code.push_back({data,left,right,op,type,false,false,true});
    return code.size()-1;
  }

  uint32_t Int(uint64_t data=1,CRawType type=RT_I64)
  {
    return Add(IC_IMM_I64,type,NO_IC,NO_IC,data);
  }

  uint32_t Frame(int32_t disp,CRawType type=RT_I64)
  {
    return Add(IC_DEREF,type,Add(IC_RBP,RT_I64,NO_IC,NO_IC,disp));
  }

  uint32_t Binary(CICode op=IC_ADD,CRawType type=RT_I64)
  {
    auto a=Frame(-8,type),b=Frame(-16,type);
    return Add(op,type,a,b);
  }

  void Run(bool with_fun=true)
  {
    OptPass6(code.data(),code.size(),with_fun?&fun:nullptr);
  }

  bool Selected(uint32_t index) const
  {
    return code[index].stack_reg;
  }

  bool Mask() const
  {
    constexpr uint32_t r9=1U<<9;
    return (fun.used_reg_mask&r9) && (fun.clobbered_reg_mask&r9);
  }
};

void SelectionProbe()
{
  constexpr CICode binary[]{
      IC_ADD,IC_SUB,IC_MUL,IC_DIV,IC_MOD,IC_AND,IC_OR,IC_XOR,IC_SHL,IC_SHR,
      IC_EQU_EQU,IC_NOT_EQU,IC_LESS,IC_GREATER_EQU,IC_GREATER,IC_LESS_EQU};
  for (auto op:binary) {
    CProbe p;
    auto node=p.Binary(op);
    p.fun.used_reg_mask=1U<<14;
    p.fun.clobbered_reg_mask=1U<<2;
    p.Run();
    Check(p.Selected(node),"integer binary stack temporary");
    Check(p.Mask() && p.fun.used_reg_mask==((1U<<14)|(1U<<9)) &&
          p.fun.clobbered_reg_mask==((1U<<2)|(1U<<9)),
          "R9 metadata retains other register bits");
  }
  CProbe p;
  auto node=p.Binary(IC_XOR,RT_U8);
  p.Run(false);
  Check(p.Selected(node) && !p.Mask(),"standalone narrow integer expression");
}

void LifetimeProbe()
{
  {
    CProbe p;
    auto inner=p.Binary(),rhs=p.Frame(-24);
    auto outer=p.Add(IC_SUB,RT_I64,inner,rhs);
    p.Run();
    Check(p.Selected(inner) && p.Selected(outer),
          "left subtree completes before outer save");
  }
  {
    CProbe p;
    auto lhs=p.Frame(-24),inner=p.Binary();
    auto outer=p.Add(IC_SUB,RT_I64,lhs,inner);
    p.Run();
    Check(p.Selected(inner) && !p.Selected(outer),
          "nested RHS temporary cannot overlap R9 lifetime");
  }
  {
    CProbe p;
    auto lhs=p.Frame(-24),inner=p.Binary();
    auto unary=p.Add(IC_COM,RT_I64,inner);
    auto outer=p.Add(IC_XOR,RT_I64,lhs,unary);
    p.Run();
    Check(p.Selected(inner) && !p.Selected(outer),
          "R9 use through unary descendant");
  }
  {
    CProbe p;
    auto lhs=p.Frame(-8),right=p.Frame(-16),imm=p.Int(7);
    auto inner=p.Add(IC_XOR,RT_I64,right,imm);
    p.code[inner].rhs={COptRhsKind::Imm32,7,0};
    auto outer=p.Add(IC_ADD,RT_I64,lhs,inner);
    p.Run();
    Check(!p.Selected(inner) && p.Selected(outer),
          "Pass5-folded RHS needs no temporary");
  }
  {
    CProbe p;
    auto lhs=p.Frame(-8),rhs=p.Binary();
    auto fold=p.Add(IC_ADD_CONST,RT_I64,lhs,rhs,17);
    auto outer=p.Add(IC_XOR,RT_I64,lhs,fold);
    p.Run();
    Check(p.Selected(rhs) && p.Selected(outer),
          "constant operation does not evaluate its retained right index");
  }
}

void AddressProbe()
{
  {
    CProbe p;
    auto lhs=p.Frame(-8),base=p.Frame(16),index=p.Frame(24);
    auto raw=p.Add(IC_ADD,RT_I64,base,index);
    auto load=p.Add(IC_DEREF,RT_I16,raw);
    p.code[load].address={base,index,4,1,true};
    auto outer=p.Add(IC_SUB,RT_I64,lhs,load);
    p.Run();
    Check(p.Selected(raw) && p.Selected(outer),
          "folded address skips raw ADD and executes safe base/index");
  }
  {
    CProbe p;
    auto lhs=p.Frame(-8),base=p.Frame(16),index=p.Binary();
    auto raw=p.Add(IC_ADD,RT_I64,base,index);
    auto load=p.Add(IC_DEREF,RT_I64,raw);
    p.code[load].address={base,index,4,3,true};
    auto outer=p.Add(IC_ADD,RT_I64,lhs,load);
    p.Run();
    Check(p.Selected(index) && !p.Selected(outer),
          "folded address index can clobber R9");
  }
  {
    CProbe p;
    auto lhs=p.Frame(-8),base=p.Binary(),index=p.Frame(24);
    auto raw=p.Add(IC_ADD,RT_I64,base,index);
    auto load=p.Add(IC_DEREF,RT_I64,raw);
    p.code[load].address={base,index,0,0,true};
    auto outer=p.Add(IC_ADD,RT_I64,lhs,load);
    p.Run();
    Check(p.Selected(base) && !p.Selected(outer),
          "folded address base can clobber R9");
  }
  {
    CProbe p;
    auto lhs=p.Frame(-8),rhs=p.Frame(-16);
    p.code[rhs].local_reg=14;
    auto outer=p.Add(IC_ADD,RT_I64,lhs,rhs);
    p.Run();
    Check(p.Selected(outer),"promoted nonvolatile register load");
    p.code[rhs].local_reg=9;
    p.Run();
    Check(!p.Selected(outer) && p.Mask(),
          "R9 operand rejected on rerun without clearing metadata");
  }
  for (auto kind:{COptRhsKind::Imm32,COptRhsKind::Reg,COptRhsKind::Frame}) {
    CProbe p;
    auto node=p.Binary();
    p.code[node].rhs={kind,7,14};
    p.Run();
    Check(!p.Selected(node),"already lowered binary stays direct");
  }
}

void BarrierProbe()
{
  constexpr CICode barriers[]{
      IC_ASM,IC_ASM_ADDR,IC_GET_LABEL,IC_GET_RFLAGS,IC_CALL_TRY,
      IC_CATCH_ENTRY,IC_SUB_CALL,IC_SUB_ENTRY,IC_RET};
  for (auto op:barriers) {
    CProbe p;
    auto node=p.Binary();
    p.Run();
    Check(p.Selected(node) && p.Mask(),"initial temporary selected");
    p.Add(op);
    p.Run();
    Check(!p.Selected(node) && p.Mask(),
          "opaque function resets selection but preserves R9 metadata");
  }
  constexpr CICode rhs_barriers[]{
      IC_CALL,IC_CALL_INDIRECT,IC_ASSIGN,IC_ADD_EQU,IC__PP,IC_PP_,
      IC_STRLEN,IC_QUE_REM,IC_FS,IC_GS,IC_ADDR,IC_POWER,IC_DIV,IC_MOD};
  for (auto op:rhs_barriers) {
    CProbe p;
    auto lhs=p.Frame(-8),a=p.Frame(-16),b=p.Frame(-24);
    auto rhs=p.Add(op,RT_I64,a,b);
    auto outer=p.Add(IC_ADD,RT_I64,lhs,rhs);
    p.Run();
    Check(!p.Selected(outer),"unsafe RHS excluded");
  }
  {
    CProbe p;
    auto lhs=p.Add(IC_CALL),rhs=p.Frame(-8);
    auto outer=p.Add(IC_SUB,RT_I64,lhs,rhs);
    p.Run();
    Check(p.Selected(outer),"left call returns before temporary becomes live");
  }
  {
    CProbe p;
    auto node=p.Binary();
    p.fun.cls.flags|=FF_DOT_DOT_DOT;
    p.Run();
    Check(!p.Selected(node) && !p.Mask(),"variadic function fallback");
  }
  {
    CProbe p;
    auto node=p.Binary();
    p.fun.cls.flags|=FF_DOT_DOT_DOT;
    p.fun.used_reg_mask=(1U<<9)|(1U<<14);
    p.fun.clobbered_reg_mask=(1U<<9)|(1U<<2);
    p.Run();
    Check(!p.Selected(node) &&
          p.fun.used_reg_mask==((1U<<9)|(1U<<14)) &&
          p.fun.clobbered_reg_mask==((1U<<9)|(1U<<2)),
          "fallback preserves preexisting R9 and other register metadata");
  }
  {
    CProbe p;
    auto f=p.Add(IC_IMM_F64,RT_F64),a=p.Frame(-8);
    auto cast=p.Add(IC_TO_I64,RT_I64,f);
    auto node=p.Add(IC_ADD,RT_I64,a,cast);
    auto mixed=p.Add(IC_ADD,RT_F64,a,f);
    p.Run();
    Check(!p.Selected(node) && !p.Selected(mixed),"floating RHS and result excluded");
  }
  {
    CProbe p;
    auto cmp=p.Binary(IC_LESS);
    p.code[cmp].cmp_link=true;
    auto chain=p.Add(IC_LESS,RT_I64,cmp,p.Frame(-24));
    p.code[chain].chain=true;
    auto logical=p.Binary(IC_AND_AND);
    p.Run();
    Check(!p.Selected(cmp) && !p.Selected(chain) && !p.Selected(logical),
          "comparison chains and logical operations excluded");
  }
}

void BoundsProbe()
{
  {
    CProbe p;
    auto a=p.Int(),bad=p.Add(IC_COM,RT_I64,NO_IC);
    auto node=p.Add(IC_ADD,RT_I64,a,bad);
    auto forward=p.Add(IC_ADD,RT_I64,a,NO_IC);
    p.Run();
    Check(!p.Selected(node) && !p.Selected(forward),"invalid operand indices");
  }
  {
    CProbe p;
    auto a=p.Int(),value=a;
    for (unsigned i=0;i<520;i++)
      value=p.Add(IC_COM,RT_I64,value);
    auto node=p.Add(IC_ADD,RT_I64,a,value);
    p.Run();
    Check(!p.Selected(node),"bounded deep operand traversal");
  }
  {
    CProbe p;
    auto a=p.Int(),value=a;
    for (unsigned i=0;i<15;i++) {
      value=p.Add(IC_ADD,RT_I64,value,value);
      p.code[value].rhs={COptRhsKind::Imm32,1,0};
    }
    auto node=p.Add(IC_ADD,RT_I64,a,value);
    p.Run();
    Check(!p.Selected(node),"bounded shared operand DAG traversal");
  }
}
} // namespace

int main()
{
  try {
    SelectionProbe();
    LifetimeProbe();
    AddressProbe();
    BarrierProbe();
    BoundsProbe();
    std::cout<<"OptPass6: "<<checks<<" checks passed\n";
  } catch (const std::exception &e) {
    std::cerr<<"OptPass6: "<<e.what()<<'\n';
    return 1;
  }
  return 0;
}
