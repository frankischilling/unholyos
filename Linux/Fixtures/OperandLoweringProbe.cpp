#include "../../Compiler/CompilerA.hpp"

#include <initializer_list>
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

void Bytes(CCodeBytes code,std::initializer_list<uint8_t> expected,
           const char *name)
{
  bool okay=code.count==expected.size();
  unsigned i=0;
  for (auto byte:expected)
    okay=okay && code.bytes[i++]==byte;
  Check(okay,name);
}

void AddressBytes()
{
  constexpr auto lea=CMemOp::Lea,load=CMemOp::Load,store=CMemOp::Store;
  Bytes(ICModr1(lea,RT_I64,0,16,0,0),{0x48,0x8D,0x00},"LEA RAX base");
  Bytes(ICModr1(lea,RT_I64,5,16,0,0),{0x48,0x8D,0x45,0},"LEA RBP zero disp");
  Bytes(ICModr1(lea,RT_I64,13,16,0,0),{0x49,0x8D,0x45,0},"LEA R13 zero disp");
  Bytes(ICModr1(lea,RT_I64,4,16,0,0),{0x48,0x8D,0x04,0x24},"LEA RSP SIB");
  Bytes(ICModr1(lea,RT_I64,12,16,0,0),{0x49,0x8D,0x04,0x24},"LEA R12 SIB");
  Bytes(ICModr1(lea,RT_U64,13,12,3,-128),
        {0x4B,0x8D,0x44,0xE5,0x80},"LEA extended base/index scale 8");
  Bytes(ICModr1(lea,RT_I64,0,1,0,0),{0x48,0x8D,0x04,0x08},
        "LEA scale 1");
  Bytes(ICModr1(lea,RT_I64,0,1,1,0),{0x48,0x8D,0x04,0x48},
        "LEA scale 2");
  Bytes(ICModr1(lea,RT_I64,0,1,2,0),{0x48,0x8D,0x04,0x88},
        "LEA scale 4");
  Bytes(ICModr1(lea,RT_I64,0,1,3,0),{0x48,0x8D,0x04,0xC8},
        "LEA scale 8");
  Bytes(ICModr1(lea,RT_I64,3,16,0,INT32_MIN),
        {0x48,0x8D,0x83,0,0,0,0x80},"LEA minimum disp32");
  Bytes(ICModr1(lea,RT_I64,3,16,0,INT32_MAX),
        {0x48,0x8D,0x83,0xFF,0xFF,0xFF,0x7F},"LEA maximum disp32");

  Bytes(ICModr1(load,RT_I8,9,10,1,127),
        {0x4B,0x0F,0xBE,0x44,0x51,0x7F},"MOVSX byte extended SIB");
  Bytes(ICModr1(load,RT_U8,4,16,0,0),
        {0x48,0x0F,0xB6,0x04,0x24},"MOVZX byte RSP");
  Bytes(ICModr1(load,RT_I16,5,16,0,-129),
        {0x48,0x0F,0xBF,0x85,0x7F,0xFF,0xFF,0xFF},"MOVSX word disp32");
  Bytes(ICModr1(load,RT_U16,12,12,2,128),
        {0x4B,0x0F,0xB7,0x84,0xA4,0x80,0,0,0},"MOVZX word R12 index");
  Bytes(ICModr1(load,RT_I32,13,16,0,0),
        {0x49,0x63,0x45,0},"MOVSXD dword R13");
  Bytes(ICModr1(load,RT_U32,13,16,0,0),
        {0x41,0x8B,0x45,0},"MOV zero extends dword R13");
  Bytes(ICModr1(load,RT_U32,0,1,3,0),
        {0x8B,0x04,0xC8},"MOV dword SIB without REX");
  Bytes(ICModr1(load,RT_I64,0,16,0,0),{0x48,0x8B,0},"MOV qword");
  Bytes(ICModr1(load,RT_U64,8,16,0,-1),{0x49,0x8B,0x40,0xFF},
        "MOV unsigned qword extended base");
  Bytes(ICModr1(load,RT_F64,0,16,0,0),{0x48,0x8B,0},"MOV raw F64 bits");

  Bytes(ICModr1(store,RT_I8,0,1,1,0),{0x88,0x04,0x48},"Store byte");
  Bytes(ICModr1(store,RT_U8,12,16,0,0),{0x41,0x88,0x04,0x24},
        "Store byte R12");
  Bytes(ICModr1(store,RT_I16,9,10,2,0x12345678),
        {0x66,0x43,0x89,0x84,0x91,0x78,0x56,0x34,0x12},
        "Store word prefix precedes REX");
  Bytes(ICModr1(store,RT_U16,5,16,0,0),{0x66,0x89,0x45,0},
        "Store word RBP");
  Bytes(ICModr1(store,RT_I32,13,16,0,128),
        {0x41,0x89,0x85,0x80,0,0,0},"Store dword extended disp32");
  Bytes(ICModr1(store,RT_U32,6,16,0,-128),{0x89,0x46,0x80},
        "Store dword negative disp8");
  Bytes(ICModr1(store,RT_I64,4,12,3,0),{0x4A,0x89,0x04,0xE4},
        "Store qword RSP plus R12 index");
  Bytes(ICModr1(store,RT_U64,7,16,0,0),{0x48,0x89,0x07},"Store U64");
  Bytes(ICModr1(store,RT_F64,12,13,3,-4),{0x4B,0x89,0x44,0xEC,0xFC},
        "Store raw F64");

  Check(!ICModr1(lea,RT_I64,16,16,0,0).count,"Reject invalid base");
  Check(!ICModr1(lea,RT_I64,0,17,0,0).count,"Reject invalid index");
  Check(!ICModr1(lea,RT_I64,0,4,0,0).count,"Reject RSP index");
  Check(!ICModr1(lea,RT_I64,0,1,4,0).count,"Reject invalid scale");
  Check(!ICModr1(lea,RT_I64,0,16,1,0).count,"Reject scale without index");
  Check(!ICModr1(lea,RT_I32,0,16,0,0).count,"Reject narrow LEA result");
  Check(!ICModr1(lea,RT_F64,0,16,0,0).count,"Reject float LEA result");
  Check(!ICModr1(load,RT_U0,0,16,0,0).count,"Reject void load");
  Check(!ICModr1(store,RT_AGGREGATE,0,16,0,0).count,"Reject aggregate store");
  Check(!ICModr1(static_cast<CMemOp>(255),RT_I64,0,16,0,0).count,
        "Reject invalid memory operation");
}

void BinaryBytes()
{
  constexpr auto imm=COptRhsKind::Imm32,reg=COptRhsKind::Reg,
                 frame=COptRhsKind::Frame;
  Bytes(ICBinaryRhs(IC_ADD,{imm,127,0}),{0x48,0x83,0xC0,0x7F},"ADD imm8");
  Bytes(ICBinaryRhs(IC_SUB,{imm,-128,0}),{0x48,0x83,0xE8,0x80},"SUB imm8");
  Bytes(ICBinaryRhs(IC_OR,{imm,128,0}),{0x48,0x81,0xC8,0x80,0,0,0},
        "OR imm32");
  Bytes(ICBinaryRhs(IC_AND,{imm,-129,0}),
        {0x48,0x81,0xE0,0x7F,0xFF,0xFF,0xFF},"AND negative imm32");
  Bytes(ICBinaryRhs(IC_XOR,{imm,INT32_MIN,0}),{0x48,0x81,0xF0,0,0,0,0x80},
        "XOR signed imm32 minimum");
  Bytes(ICBinaryRhs(IC_LESS,{imm,INT32_MAX,0}),
        {0x48,0x81,0xF8,0xFF,0xFF,0xFF,0x7F},"CMP signed imm32 maximum");
  Bytes(ICBinaryRhs(IC_MUL,{imm,-8,0}),{0x48,0x6B,0xC0,0xF8},"IMUL imm8");
  Bytes(ICBinaryRhs(IC_MUL,{imm,256,0}),{0x48,0x69,0xC0,0,1,0,0},
        "IMUL imm32");
  Bytes(ICBinaryRhs(IC_ADD,{reg,0,15}),{0x49,0x03,0xC7},"ADD R15");
  Bytes(ICBinaryRhs(IC_SUB,{reg,0,1}),{0x48,0x2B,0xC1},"SUB RCX");
  Bytes(ICBinaryRhs(IC_MUL,{reg,0,12}),{0x49,0x0F,0xAF,0xC4},"IMUL R12");
  Bytes(ICBinaryRhs(IC_EQU_EQU,{reg,0,13}),{0x49,0x3B,0xC5},"CMP R13");
  Bytes(ICBinaryRhs(IC_AND,{reg,0,14}),{0x49,0x23,0xC6},"AND R14");
  Bytes(ICBinaryRhs(IC_OR,{reg,0,14}),{0x49,0x0B,0xC6},"OR R14");
  Bytes(ICBinaryRhs(IC_XOR,{reg,0,14}),{0x49,0x33,0xC6},"XOR R14");
  Bytes(ICBinaryRhs(IC_ADD,{frame,-128,0}),{0x48,0x03,0x45,0x80},
        "ADD frame disp8");
  Bytes(ICBinaryRhs(IC_SUB,{frame,-129,0}),
        {0x48,0x2B,0x85,0x7F,0xFF,0xFF,0xFF},"SUB frame disp32");
  Bytes(ICBinaryRhs(IC_MUL,{frame,128,0}),
        {0x48,0x0F,0xAF,0x85,0x80,0,0,0},"IMUL frame disp32");
  Bytes(ICBinaryRhs(IC_GREATER,{frame,0,0}),{0x48,0x3B,0x45,0},
        "CMP frame zero displacement");
  for (auto op:{IC_EQU_EQU,IC_NOT_EQU,IC_LESS,IC_GREATER_EQU,IC_GREATER,
               IC_LESS_EQU})
    Bytes(ICBinaryRhs(op,{reg,0,12}),{0x49,0x3B,0xC4},
          "All comparison forms emit CMP, not SETcc");
  Check(!ICBinaryRhs(IC_ADD,{reg,0,16}).count,"Reject invalid RHS register");
  Check(!ICBinaryRhs(IC_ADD,{}).count,"Reject generic stack operand");
  Check(!ICBinaryRhs(IC_DIV,{imm,3,0}).count,"Reject division");
  Check(!ICBinaryRhs(IC_SHL,{reg,0,12}).count,"Reject variable shift");
  Check(!ICBinaryRhs(IC_AND_AND,{frame,8,0}).count,"Reject logical operator");
}

struct CProbe
{
  std::vector<CIntermediateCode> code;

  uint32_t Add(CICode op,CRawType type,uint32_t left=NO_IC,
               uint32_t right=NO_IC,uint64_t data=0)
  {
    code.push_back({data,left,right,op,type,false,false,true});
    return code.size()-1;
  }

  uint32_t Int(uint64_t value,CRawType type=RT_I64)
  {
    return Add(IC_IMM_I64,type,NO_IC,NO_IC,value);
  }

  uint32_t Frame(CRawType type,int32_t disp,uint8_t reg=0)
  {
    auto address=Add(IC_RBP,RT_I64,NO_IC,NO_IC,uint64_t(int64_t(disp)));
    auto value=Add(IC_DEREF,type,address);
    code[value].local_reg=reg;
    if (!reg)
      code[value].address={NO_IC,NO_IC,disp,0,true};
    return value;
  }

  void Run()
  {
    OptPass5(code.data(),code.size());
  }

  void Choice(uint32_t i,COptRhsKind kind,int32_t disp,uint8_t reg,
              const char *name)
  {
    const auto &r=code[i].rhs;
    Check(r.kind==kind && r.disp==disp && r.reg==reg,name);
  }
};

void OperandSelection()
{
  constexpr auto imm=COptRhsKind::Imm32,reg=COptRhsKind::Reg,
                 frame=COptRhsKind::Frame,stack=COptRhsKind::Stack;
  CProbe p;
  auto left=p.Add(IC_CALL,RT_I64,NO_IC,0);
  auto small=p.Int(uint64_t(-129)),large=p.Int(0x100000000ULL);
  auto zero_extended=p.Int(0xFFFFFFFFULL,RT_U64);
  auto sign_extended=p.Int(UINT64_MAX,RT_U64);
  auto frame64=p.Frame(RT_I64,-8),frameu=p.Frame(RT_U64,24);
  auto narrow=p.Frame(RT_U16,-10),floating=p.Frame(RT_F64,-24);
  auto allocated=p.Frame(RT_U64,-32,15);
  auto badreg=p.Frame(RT_I64,-40,9);
  auto invalid=p.Frame(RT_I64,-48);
  p.code[invalid].address.valid=false;
  auto computed=p.Frame(RT_I64,-56);
  p.code[computed].address.index=left;
  auto nonframe=p.Frame(RT_I64,-64);
  p.code[nonframe].address.base=left;
  auto wide_offset=p.Frame(RT_I64,-72);
  p.code[wide_offset].address.disp=-80;
  auto converted=p.Add(IC_TO_I64,RT_I64,floating);
  auto call_rhs=p.Add(IC_CALL,RT_I64,NO_IC,0);
  auto add=p.Add(IC_ADD,RT_I64,left,frame64);
  auto sub=p.Add(IC_SUB,RT_I64,left,allocated);
  auto mul=p.Add(IC_MUL,RT_I64,left,small);
  auto cmp=p.Add(IC_GREATER,RT_I64,left,frameu);
  auto bit=p.Add(IC_XOR,RT_U64,left,sign_extended);
  p.Run();
  p.Choice(add,frame,-8,0,"Direct I64 frame RHS");
  p.Choice(sub,reg,0,15,"Promoted register RHS");
  p.Choice(mul,imm,-129,0,"Signed32 immediate RHS");
  p.Choice(cmp,frame,24,0,"Direct U64 argument RHS");
  p.Choice(bit,imm,-1,0,"Unsigned sign-extended immediate bits");
  Check(p.code[add].left==left && p.code[add].right==frame64 &&
        p.code[left].code==IC_CALL && p.code[frame64].code==IC_DEREF,
        "Operand selection does not alter evaluation tree");

  std::vector<uint32_t> rejected;
  for (auto r:{large,zero_extended,narrow,floating,badreg,invalid,computed,
               nonframe,wide_offset,converted,call_rhs})
    rejected.push_back(p.Add(IC_ADD,RT_I64,left,r));
  for (auto op:{IC_DIV,IC_MOD,IC_SHL,IC_SHR,IC_POWER,IC_AND_AND,IC_OR_OR,
               IC_XOR_XOR,IC_ASSIGN,IC_ADD_EQU,IC_MIN_I64})
    rejected.push_back(p.Add(op,RT_I64,left,small));
  rejected.push_back(p.Add(IC_ADD,RT_F64,left,small));
  rejected.push_back(p.Add(IC_ADD,RT_I64,floating,small));
  auto chain=p.Add(IC_LESS,RT_I64,left,small);
  p.code[chain].chain=true;
  auto link=p.Add(IC_LESS,RT_I64,left,small);
  p.code[link].cmp_link=true;
  rejected.push_back(chain);
  rejected.push_back(link);
  rejected.push_back(p.Add(IC_ADD,RT_I64,chain,small));
  rejected.push_back(p.Add(IC_ADD,RT_I64,left,link));
  rejected.push_back(p.Add(IC_ADD,RT_I64,NO_IC,small));
  rejected.push_back(p.Add(IC_ADD,RT_I64,left,NO_IC));
  p.Run();
  for (auto i:rejected)
    p.Choice(i,stack,0,0,"Unsafe or unimplemented operand keeps stack path");

  // Re-running the pass clears stale annotations when the function can
  // observe arithmetic flags or contain unmodeled inline instructions.
  for (auto op:{IC_ASM,IC_GET_RFLAGS}) {
    CProbe q;
    auto a=q.Frame(RT_I64,-8),b=q.Int(5);
    auto node=q.Add(IC_XOR,RT_I64,a,b);
    q.Run();
    q.Choice(node,imm,5,0,"Initial eligible annotation");
    q.Add(op,RT_U0);
    q.Run();
    q.Choice(node,stack,0,0,"Opaque flag observer disables selection");
  }
}

void AddressSelection()
{
  CProbe p;
  auto frame=p.Add(IC_RBP,RT_I64,NO_IC,NO_IC,uint64_t(-160));
  auto index=p.Add(IC_CALL,RT_I64,NO_IC,0);
  auto scaled=p.Add(IC_SHL_CONST,RT_I64,index,p.Int(3),3);
  auto cast=p.Add(IC_HOLYC_TYPECAST,RT_I64,frame);
  auto sum=p.Add(IC_ADD,RT_I64,cast,scaled);
  auto member=p.Add(IC_ADD_CONST,RT_I64,sum,p.Int(4),4);
  auto load=p.Add(IC_DEREF,RT_I32,member);
  auto direct=p.Add(IC_DEREF,RT_U8,frame);
  auto base=p.Add(IC_CALL,RT_I64,NO_IC,0);
  auto dynsum=p.Add(IC_ADD,RT_I64,base,scaled);
  auto negative=p.Add(IC_SUB_CONST,RT_I64,dynsum,p.Int(9),9);
  auto dyn=p.Add(IC_DEREF,RT_U16,negative);
  auto extra=p.Add(IC_ADD,RT_I64,dynsum,index);
  auto nested=p.Add(IC_DEREF,RT_I64,extra);
  auto huge=p.Add(IC_ADD_CONST,RT_I64,frame,p.Int(INT32_MIN),
                  uint64_t(int64_t(INT32_MIN)));
  auto overflow=p.Add(IC_DEREF,RT_I64,huge);
  auto floatbase=p.Add(IC_IMM_F64,RT_F64);
  auto wrong=p.Add(IC_DEREF,RT_I64,floatbase);
  auto promoted=p.Add(IC_DEREF,RT_I64,frame);
  p.code[promoted].local_reg=12;
  auto chain=p.Add(IC_ADD,RT_I64,base,index);
  p.code[chain].cmp_link=true;
  auto linked=p.Add(IC_DEREF,RT_I64,chain);
  OptPass4(p.code.data(),p.code.size());
  auto addr=p.code[load].address;
  Check(addr.valid && addr.base==NO_IC && addr.index==index &&
        addr.shift==3 && addr.disp==-156,"Frame SIB member displacement");
  addr=p.code[direct].address;
  Check(addr.valid && addr.base==NO_IC && addr.index==NO_IC &&
        addr.disp==-160,"Direct frame address");
  addr=p.code[dyn].address;
  Check(addr.valid && addr.base==base && addr.index==index &&
        addr.shift==3 && addr.disp==-9,"Dynamic scaled address with subtraction");
  addr=p.code[nested].address;
  Check(addr.valid && addr.base==dynsum && addr.index==index &&
        !addr.shift,"Second index retains left subtree");
  Check(p.code[base].code==IC_CALL && p.code[index].code==IC_CALL &&
        p.code[member].left==sum,"Address analysis retains evaluation tree");
  for (auto node:{overflow,wrong,promoted,linked})
    Check(!p.code[node].address.valid,"Unsafe address remains unmodified");
  for (auto op:{IC_ASM,IC_GET_RFLAGS}) {
    CProbe q;
    auto a=q.Frame(RT_I64,-8);
    q.Add(op,RT_U0);
    OptPass4(q.code.data(),q.code.size());
    Check(!q.code[a].address.valid,"Flag observer disables address folding");
  }
}
}

int main()
{
  try {
    AddressBytes();
    BinaryBytes();
    OperandSelection();
    AddressSelection();
    std::cout<<"Operand lowering: "<<checks<<" checks passed\n";
  } catch (const std::exception &error) {
    std::cerr<<error.what()<<'\n';
    return 1;
  }
}
