#include "../../Compiler/CompilerA.hpp"

#include <bit>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
unsigned checks;

void Check(bool result,const char *name)
{
  if (!result)
    throw std::runtime_error(name);
  checks++;
}

struct CProbe
{
  std::vector<CIntermediateCode> code;

  uint32_t Add(CICode op,CRawType type,uint32_t left=NO_IC,
               uint32_t right=NO_IC,uint64_t bits=0,bool runtime=false,
               bool chain=false)
  {
    code.push_back({bits,left,right,op,type,chain,false,runtime});
    return code.size()-1;
  }

  uint32_t Int(uint64_t bits,CRawType type=RT_I64)
  {
    return Add(IC_IMM_I64,type,NO_IC,NO_IC,bits);
  }

  uint32_t Float(double value)
  {
    return Add(IC_IMM_F64,RT_F64,NO_IC,NO_IC,std::bit_cast<uint64_t>(value));
  }

  void Run()
  {
    OptPass012(code.data(),code.size());
  }

  void Value(uint32_t i,uint64_t value,const char *name)
  {
    Check(code[i].data==value && code[i].left==NO_IC && code[i].right==NO_IC &&
          code[i].code==(code[i].type==RT_F64?IC_IMM_F64:IC_IMM_I64),name);
  }

  void FloatValue(uint32_t i,double value,const char *name)
  {
    Value(i,std::bit_cast<uint64_t>(value),name);
  }
};

void IntegerProbe()
{
  CProbe p;
  auto one=p.Int(1),two=p.Int(2),three=p.Int(3),four=p.Int(4),
       neg=p.Int(-17),wide=p.Int(UINT64_MAX,RT_U64),zero=p.Int(0),
       min=p.Int(INT64_MIN),minus_one=p.Int(-1),
       narrow=p.Int(127,RT_I8),byte=p.Int(2,RT_U8);
  auto mul=p.Add(IC_MUL,RT_I64,two,three);
  auto sum=p.Add(IC_ADD,RT_I64,one,mul);
  auto div=p.Add(IC_DIV,RT_I64,neg,four);
  auto rem=p.Add(IC_MOD,RT_I64,neg,four);
  auto udiv=p.Add(IC_DIV,RT_U64,wide,two);
  auto shift=p.Add(IC_SHL,RT_I64,one,p.Int(70));
  auto sright=p.Add(IC_SHR,RT_I64,neg,two);
  auto uright=p.Add(IC_SHR,RT_U64,wide,two);
  auto add_wrap=p.Add(IC_ADD,RT_U64,wide,one);
  auto neg_wrap=p.Add(IC_UNARY_MINUS,RT_I64,min);
  auto narrow_add=p.Add(IC_ADD,RT_U8,narrow,byte);
  auto mixed_cmp=p.Add(IC_LESS,RT_I64,neg,byte);
  auto scmp=p.Add(IC_LESS,RT_I64,neg,two);
  auto bnot=p.Add(IC_NOT,RT_I64,zero);
  auto bxor=p.Add(IC_XOR_XOR,RT_I64,one,three);
  auto inv=p.Add(IC_COM,RT_I64,four);
  auto bits=p.Add(IC_OR,RT_I64,two,four);
  auto cast=p.Add(IC_HOLYC_TYPECAST,RT_U8,p.Int(0x1234));
  CHashClass cls{};
  p.code[cast].cls=&cls;
  p.code[cast].grouped=true;
  auto minv=p.Add(IC_MIN_I64,RT_I64,two,neg);
  auto maxv=p.Add(IC_MAX_I64,RT_I64,two,neg);
  auto sqr=p.Add(IC_SQR_I64,RT_I64,neg);
  auto usqr=p.Add(IC_SQR_U64,RT_U64,wide);
  auto umin=p.Add(IC_MIN_U64,RT_U64,two,wide);
  auto umax=p.Add(IC_MAX_U64,RT_U64,two,wide);
  auto abs=p.Add(IC_ABS_I64,RT_I64,neg);
  auto absmin=p.Add(IC_ABS_I64,RT_I64,min);
  auto sign=p.Add(IC_SIGN_I64,RT_I64,neg);
  auto signzero=p.Add(IC_SIGN_I64,RT_I64,zero);
  auto signpos=p.Add(IC_SIGN_I64,RT_I64,three);
  auto boolean=p.Add(IC_TO_BOOL,RT_I8,wide);
  auto upper=p.Add(IC_TOUPPER,RT_I64,p.Int('z'));
  auto unchanged=p.Add(IC_TOUPPER,RT_I64,p.Int(0x10061));
  auto lowbit=p.Add(IC_BSF,RT_I64,p.Int(0x8000000000000010ULL));
  auto highbit=p.Add(IC_BSR,RT_I64,p.Int(0x8000000000000010ULL));
  auto scan_zero=p.Add(IC_BSF,RT_I64,zero);
  auto fault=p.Add(IC_DIV,RT_I64,one,zero);
  auto overflow=p.Add(IC_DIV,RT_I64,min,minus_one);
  auto rem_fault=p.Add(IC_MOD,RT_I64,min,minus_one);
  p.Run();
  p.Value(sum,7,"nested arithmetic");
  p.Value(div,-4,"signed division");
  p.Value(rem,-1,"signed remainder");
  p.Value(udiv,UINT64_MAX/2,"unsigned division");
  p.Value(shift,64,"masked shift count");
  p.Value(sright,-5,"arithmetic shift");
  p.Value(uright,UINT64_MAX/4,"logical shift");
  p.Value(add_wrap,0,"integer wrap");
  p.Value(neg_wrap,INT64_MIN,"signed negation wrap");
  p.Value(narrow_add,129,"narrow arithmetic result is not a load");
  p.Value(mixed_cmp,0,"mixed comparison uses unsigned interpretation");
  p.Value(scmp,1,"signed comparison");
  p.Value(bnot,1,"logical not");
  p.Value(bxor,0,"logical xor");
  p.Value(inv,~uint64_t{4},"complement");
  p.Value(bits,6,"bitwise operation");
  p.Value(cast,0x1234,"literal bit cast");
  Check(p.code[cast].type==RT_U8 && p.code[cast].cls==&cls &&
        p.code[cast].grouped,"cast metadata");
  p.Value(minv,-17,"minimum intrinsic");
  p.Value(maxv,2,"maximum intrinsic");
  p.Value(sqr,289,"square intrinsic");
  p.Value(usqr,1,"unsigned square wraps");
  p.Value(umin,2,"unsigned minimum");
  p.Value(umax,UINT64_MAX,"unsigned maximum");
  p.Value(abs,17,"integer absolute");
  p.Value(absmin,INT64_MIN,"integer absolute wraps");
  p.Value(sign,-1,"negative sign");
  p.Value(signzero,0,"zero sign");
  p.Value(signpos,1,"positive sign");
  p.Value(boolean,1,"integer Boolean conversion");
  p.Value(upper,'Z',"ASCII uppercase");
  p.Value(unchanged,0x10061,"uppercase uses full character value");
  p.Value(lowbit,4,"low bit scan");
  p.Value(highbit,63,"high bit scan");
  p.Value(scan_zero,UINT64_MAX,"zero bit scan");
  Check(p.code[fault].code==IC_DIV && p.code[overflow].code==IC_DIV &&
        p.code[rem_fault].code==IC_MOD,"division faults stay executable");
}

void FloatProbe()
{
  CProbe p;
  auto one=p.Float(1.0),three=p.Float(3.0),two=p.Float(2.0),
       x=p.Float(2.5),y=p.Float(1.5),zero=p.Float(0.0),negzero=p.Float(-0.0);
  auto add=p.Add(IC_ADD,RT_F64,x,p.Int(6));
  auto sub=p.Add(IC_SUB,RT_F64,x,y);
  auto mul=p.Add(IC_MUL,RT_F64,x,two);
  auto div=p.Add(IC_DIV,RT_F64,three,two);
  auto mod=p.Add(IC_MOD,RT_F64,x,y);
  auto eq=p.Add(IC_EQU_EQU,RT_I64,zero,negzero);
  auto req=p.Add(IC_EQU_EQU,RT_I64,zero,negzero,0,true);
  auto cmp=p.Add(IC_LESS,RT_I64,x,three);
  auto neg=p.Add(IC_UNARY_MINUS,RT_F64,zero);
  auto not_zero=p.Add(IC_NOT,RT_F64,negzero);
  auto to_int=p.Add(IC_TO_I64,RT_I64,three);
  auto to_float=p.Add(IC_TO_F64,RT_F64,p.Int(-17));
  auto inexact=p.Add(IC_DIV,RT_F64,one,three);
  auto divzero=p.Add(IC_DIV,RT_F64,one,zero);
  auto rounded=p.Add(IC_ADD,RT_F64,one,p.Float(0x1p-25));
  auto fractional=p.Add(IC_TO_I64,RT_I64,y);
  auto nan=p.Add(IC_LESS,RT_I64,p.Float(std::bit_cast<double>(0x7FF8000000000000ULL)),one);
  auto sqr=p.Add(IC_SQR,RT_F64,p.Float(-2.5));
  auto abs=p.Add(IC_ABS,RT_F64,p.Float(-2.5));
  auto abszero=p.Add(IC_ABS,RT_F64,negzero);
  auto rounded_sqr=p.Add(IC_SQR,RT_F64,p.Float(1.0000000000000002));
  auto absnan=p.Add(IC_ABS,RT_F64,p.Float(std::bit_cast<double>(0xFFF8000000000000ULL)));
  p.Run();
  p.FloatValue(add,8.5,"mixed exact floating add");
  p.FloatValue(sub,1.0,"exact floating subtract");
  p.FloatValue(mul,5.0,"exact floating multiply");
  p.FloatValue(div,1.5,"exact floating divide");
  p.FloatValue(mod,1.0,"exact floating remainder");
  p.Value(eq,0,"constant floating equality is bitwise");
  p.Value(req,1,"runtime floating equality uses numeric order");
  p.Value(cmp,1,"floating comparison");
  p.FloatValue(neg,-0.0,"floating negation sign");
  p.Value(not_zero,0,"raw floating truth");
  p.Value(to_int,3,"exact float to integer");
  p.FloatValue(to_float,-17.0,"exact integer to float");
  p.FloatValue(sqr,6.25,"exact floating square");
  p.FloatValue(abs,2.5,"floating absolute");
  p.FloatValue(abszero,0.0,"floating absolute clears sign of zero");
  Check(p.code[rounded_sqr].code==IC_SQR && p.code[absnan].code==IC_ABS,
        "floating intrinsics retain rounding and exceptional cases");
  Check(p.code[inexact].code==IC_DIV && p.code[divzero].code==IC_DIV &&
        p.code[rounded].code==IC_ADD && p.code[fractional].code==IC_TO_I64 &&
        p.code[nan].code==IC_LESS,"rounding and exceptional cases stay executable");
}

void ControlProbe()
{
  CProbe p;
  auto one=p.Int(1),two=p.Int(2),zero=p.Int(0),eight=p.Int(8),
       u=p.Int(8,RT_U64);
  auto dyn=p.Add(IC_CALL,RT_I64,NO_IC,0,0,true);
  auto sum=p.Add(IC_ADD,RT_I64,dyn,two,0,true);
  auto product=p.Add(IC_MUL,RT_I64,eight,dyn,0,true);
  auto shift=p.Add(IC_SHR,RT_I64,dyn,two,0,true);
  auto divide=p.Add(IC_DIV,RT_U64,dyn,u,0,true);
  auto signed_divide=p.Add(IC_DIV,RT_I64,dyn,eight,0,true);
  auto effect=p.Add(IC_MUL,RT_I64,dyn,zero,0,true);
  auto eager=p.Add(IC_AND_AND,RT_I64,zero,dyn,0,true);
  auto c1=p.Add(IC_LESS,RT_I64,one,two);
  auto c2=p.Add(IC_LESS,RT_I64,c1,dyn,0,true,true);
  auto jump=p.Add(IC_BR_ZERO,RT_U0,zero,99);
  auto never=p.Add(IC_BR_ZERO,RT_U0,one,99);
  auto store=p.Add(IC_ASSIGN,RT_I64,dyn,sum,0,true);
  p.Run();
  Check(p.code[sum].code==IC_ADD_CONST && p.code[sum].data==2 &&
        p.code[sum].left==dyn,"add immediate");
  Check(p.code[product].code==IC_SHL_CONST && p.code[product].data==3 &&
        p.code[product].left==dyn,"power of two multiply");
  Check(p.code[shift].code==IC_SHR_CONST && p.code[shift].data==2,
        "constant shift");
  Check(p.code[divide].code==IC_SHR_CONST && p.code[divide].data==3 &&
        p.code[signed_divide].code==IC_DIV,"only unsigned division strength reduction");
  Check(p.code[effect].code==IC_MUL && p.code[eager].code==IC_AND_AND &&
        p.code[dyn].code==IC_CALL && p.code[store].code==IC_ASSIGN,
        "effectful operands and stores retained");
  Check(p.code[c1].code==IC_LESS && p.code[c1].right==two &&
        p.code[c2].chain,"chained comparison retains middle operand");
  Check(p.code[jump].code==IC_JMP && p.code[jump].right==99 &&
        p.code[jump].left==zero,"always taken branch");
  Check(p.code[never].code==IC_END_EXP && p.code[never].right==NO_IC &&
        p.code[never].left==one,"never taken branch");
}
}

int main()
{
  try {
    IntegerProbe();
    FloatProbe();
    ControlProbe();
    std::cout<<"OptPass012: "<<checks<<" checks passed\n";
  } catch (const std::exception &error) {
    std::cerr<<error.what()<<'\n';
    return 1;
  }
}
