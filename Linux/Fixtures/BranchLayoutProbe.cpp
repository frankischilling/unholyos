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

  uint32_t Add(CICode op,uint64_t data=0,uint32_t right=NO_IC,
               uint32_t end=0)
  {
    code.push_back({data,NO_IC,right,op,RT_U0,false,false,false});
    code.back().branch_end=end;
    return code.size()-1;
  }

  bool Run()
  {
    return OptPass789A(code.data(),code.size());
  }
};

void BoundaryProbe()
{
  constexpr CICode ops[]{IC_JMP,IC_RETURN_VAL,IC_BR_ZERO,IC_BR_NOT_ZERO};
  for (auto op:ops) {
    const uint32_t near_size=
        op==IC_BR_ZERO || op==IC_BR_NOT_ZERO ? 6 : 5;
    const int64_t shrink=near_size-2;
    // Use actual recorded byte offsets, not the indices of the labels.
    // The parser may allocate a forward label before its branch.
    constexpr uint32_t end=512;
    for (int64_t disp:{0,1,126,127,128,129}) {
      CProbe p;
      auto label=p.Add(IC_LABEL,end+disp);
      auto branch=p.Add(op,0,label,end);
      const bool expected=disp<=INT8_MAX;
      Check(p.Run()==expected && p.code[branch].short_branch==expected,
            "forward rel8 boundary");
      Check(!p.Run(),"forward selection is idempotent");
    }
    for (int64_t disp:{-130,-129,-128,-127,-3,-2}) {
      CProbe p;
      // Convert the desired final rel8 distance to the old near layout.
      auto label=p.Add(IC_LABEL,end+disp-shrink);
      auto branch=p.Add(op,0,label,end);
      const bool expected=disp>=INT8_MIN;
      Check(p.Run()==expected && p.code[branch].short_branch==expected,
            "backward rel8 boundary with own shrink");
      Check(!p.Run(),"backward selection is idempotent");
    }
  }
}

void LayoutProbe()
{
  {
    CProbe p;
    auto label=p.Add(IC_LABEL,133);
    auto first=p.Add(IC_JMP,0,label,5);
    auto second=p.Add(IC_JMP,0,label,10);
    Check(p.Run() && !p.code[first].short_branch &&
          p.code[second].short_branch,"first layout selects inner branch");
    // The caller's next complete layout accounts for the second shrink.
    p.code[label].data-=3;
    p.code[second].branch_end-=3;
    Check(p.Run() && p.code[first].short_branch &&
          p.code[second].short_branch,"second layout enables outer branch");
    p.code[label].data-=3;
    p.code[first].branch_end-=3;
    p.code[second].branch_end-=3;
    Check(!p.Run(),"fixed point retains both selected branches");
  }
  {
    CProbe p;
    auto label=p.Add(IC_LABEL,0);
    auto branch=p.Add(IC_BR_ZERO,0,label,132);
    Check(p.Run() && p.code[branch].short_branch,
          "offset zero is a valid backward label");
  }
  {
    CProbe p;
    auto label=p.Add(IC_LABEL,INT32_MAX);
    auto branch=p.Add(IC_RETURN_VAL,0,label,INT32_MAX-127);
    Check(p.Run() && p.code[branch].short_branch,
          "large valid offsets use signed displacement arithmetic");
  }
}

void InvalidProbe()
{
  {
    CProbe p;
    auto label=p.Add(IC_LABEL,0);
    auto absent=p.Add(IC_JMP,0,label,0);
    auto tiny=p.Add(IC_BR_ZERO,0,label,5);
    auto bad=p.Add(IC_JMP,0,NO_IC,5);
    auto self=p.Add(IC_RETURN_VAL,0,p.code.size(),5);
    auto huge=p.Add(IC_JMP,0,label,UINT32_MAX);
    Check(!p.Run() && !p.code[absent].short_branch &&
          !p.code[tiny].short_branch && !p.code[bad].short_branch &&
          !p.code[self].short_branch && !p.code[huge].short_branch,
          "missing or malformed recorded branch offsets and targets");
  }
  {
    CProbe p;
    auto invalid=p.Add(IC_IMM_I64,10);
    auto branch=p.Add(IC_JMP,0,invalid,10);
    auto unknown=p.Add(IC_LABEL,UINT64_MAX);
    auto unresolved=p.Add(IC_BR_ZERO,0,unknown,10);
    auto mid=p.Add(IC_LABEL,8);
    auto overlap=p.Add(IC_BR_NOT_ZERO,0,mid,10);
    Check(!p.Run() && !p.code[branch].short_branch &&
          !p.code[unresolved].short_branch && !p.code[overlap].short_branch,
          "nonlabel, unresolved label, and interior target rejected");
  }
  {
    CProbe p;
    auto label=p.Add(IC_LABEL,128);
    for (auto op:{IC_SUB_CALL,IC_CALL,IC_CALL_INDIRECT,IC_SWITCH,
                 IC_NOBOUND_SWITCH,IC_RET,IC_END_EXP})
      p.Add(op,0,label,127);
    Check(!p.Run(),"unsupported instructions never shorten");
    for (const auto &node:p.code)
      Check(!node.short_branch,"unsupported flag unchanged");
  }
}

void BarrierProbe()
{
  for (auto op:{IC_ASM,IC_ASM_ADDR,IC_GET_LABEL}) {
    CProbe p;
    auto label=p.Add(IC_LABEL,100);
    auto branch=p.Add(IC_JMP,0,label,5);
    p.Add(op);
    Check(!p.Run() && !p.code[branch].short_branch,
          "opaque layout disables relaxation for the entire block");
  }
}
} // namespace

int main()
{
  try {
    BoundaryProbe();
    LayoutProbe();
    InvalidProbe();
    BarrierProbe();
    std::cout<<"Branch layout: "<<checks<<" checks passed\n";
  } catch (const std::exception &e) {
    std::cerr<<"Branch layout: "<<e.what()<<'\n';
    return 1;
  }
  return 0;
}
