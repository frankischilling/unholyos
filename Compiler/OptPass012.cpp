#include "CompilerA.hpp"

// The parser already establishes the typed expression links (pass zero in
// the reference). Passes one and two simplify the indexed operand trees.
static bool OptImmediate(const CIntermediateCode &node)
{
  return node.code==IC_IMM_I64 || node.code==IC_IMM_F64;
}

static void OptSetImmediate(CIntermediateCode &node,uint64_t bits)
{
  // Keep the parser's type, class, grouping and runtime provenance. In
  // particular a narrow arithmetic result is not a narrow memory load.
  node.code=node.type==RT_F64?IC_IMM_F64:IC_IMM_I64;
  node.data=bits;
  node.left=node.right=NO_IC;
}

static uint64_t OptShiftRight(uint64_t value,uint64_t count,bool uns)
{
  count&=63;
  if (!count || uns || !(value>>63))
    return value>>count;
  return (value>>count) | (~uint64_t{0}<<(64-count));
}

template <class T> static bool OptCompare(CICode op,T a,T b)
{
  switch (op) {
    case IC_EQU_EQU: return a==b;
    case IC_NOT_EQU: return a!=b;
    case IC_LESS: return a<b;
    case IC_GREATER_EQU: return a>=b;
    case IC_GREATER: return a>b;
    case IC_LESS_EQU: return a<=b;
    default: return false;
  }
}

static bool OptIntBinary(CICode op,uint64_t a,uint64_t b,bool uns,
                         uint64_t &res)
{
  switch (op) {
    case IC_ADD: res=a+b; break;
    case IC_SUB: res=a-b; break;
    case IC_MUL: res=a*b; break;
    case IC_AND: res=a&b; break;
    case IC_OR: res=a|b; break;
    case IC_XOR: res=a^b; break;
    case IC_SHL: res=a<<(b&63); break;
    case IC_SHR: res=OptShiftRight(a,b,uns); break;
    case IC_DIV:
    case IC_MOD:
      // IDIV faults must still occur when this expression is executed.
      if (!b || (!uns && a==(uint64_t{1}<<63) && b==UINT64_MAX))
        return false;
      if (uns)
        res=op==IC_DIV?a/b:a%b;
      else
        res=op==IC_DIV?int64_t(a)/int64_t(b):int64_t(a)%int64_t(b);
      break;
    default:
      if (!ICIsComparison(op))
        return false;
      res=uns?OptCompare(op,a,b):OptCompare(op,int64_t(a),int64_t(b));
  }
  return true;
}

// Exact binary64 operations only. Leaving rounded, exceptional and denormal
// operations to the back end preserves the guest x87 environment, including
// exceptions, control word and the precision of intermediate computations.
struct COptF64
{
  uint64_t sig;
  int exp;
  bool neg;
};

static constexpr uint64_t F64_SIGN=uint64_t{1}<<63,
                          F64_FRAC=(uint64_t{1}<<52)-1;

static bool OptF64Decode(uint64_t bits,COptF64 &n)
{
  unsigned exp=(bits>>52)&0x7FF;
  if (exp==0x7FF || (!exp && (bits&F64_FRAC)))
    return false;
  n={exp?((uint64_t{1}<<52)|(bits&F64_FRAC)):0,
     exp?int(exp)-1023-52:0,bool(bits&F64_SIGN)};
  return true;
}

static bool OptF64Pack(unsigned __int128 sig,int exp,bool neg,uint64_t &bits,
                       unsigned precision=53)
{
  if (!sig) {
    bits=neg?F64_SIGN:0;
    return true;
  }
  // Arithmetic is folded only if the result is exact even with the x87
  // precision control set to single precision. FILD/FSTP conversion uses
  // the full binary64 destination precision regardless of that control.
  while (sig>(uint64_t{1}<<precision)-1) {
    if (sig&1)
      return false;
    sig>>=1;
    exp++;
  }
  auto n=uint64_t(sig);
  while (n<(uint64_t{1}<<52)) {
    n<<=1;
    exp--;
  }
  exp+=52;
  if (exp< -1022 || exp>1023)
    return false;
  bits=(neg?F64_SIGN:0) | (uint64_t(exp+1023)<<52) | (n&F64_FRAC);
  return true;
}

static bool OptToF64(const CIntermediateCode &node,uint64_t &bits)
{
  if (node.type==RT_F64) {
    bits=node.data;
    return true;
  }
  // The current back end uses FILD of a signed 64-bit stack slot.
  bool neg=node.data>>63;
  return OptF64Pack(neg?uint64_t{0}-node.data:node.data,0,neg,bits);
}

static bool OptToI64(const CIntermediateCode &node,uint64_t &bits)
{
  bits=node.data;
  if (node.type!=RT_F64)
    return IsInt(node.type);
  COptF64 n;
  if (!OptF64Decode(bits,n))
    return false;
  if (!n.sig) {
    bits=0;
    return true;
  }
  if (n.exp>=0) {
    if (n.exp>11 || n.sig>((n.neg?F64_SIGN:F64_SIGN-1)>>n.exp))
      return false;
    bits=n.sig<<n.exp;
  } else {
    if (n.exp<=-64 || (n.sig&((uint64_t{1}<<-n.exp)-1)))
      return false;
    bits=n.sig>>-n.exp;
  }
  if (n.neg)
    bits=uint64_t{0}-bits;
  return true;
}

static bool OptF64Binary(const CIntermediateCode &node,uint64_t a,uint64_t b,
                         uint64_t &res)
{
  auto op=node.code;
  // For constant equality the reference compares the encoded values.
  if ((!node.runtime && (op==IC_EQU_EQU || op==IC_NOT_EQU)) ||
      op==IC_AND || op==IC_OR || op==IC_XOR || op==IC_SHL || op==IC_SHR)
    return OptIntBinary(op,a,b,false,res);
  COptF64 x,y;
  if (!OptF64Decode(a,x) || !OptF64Decode(b,y))
    return false;
  if (ICIsComparison(op)) {
    // Binary64 order agrees with the magnitude field, reversed for negatives.
    auto key=[](uint64_t bits) {
      if (!(bits&~F64_SIGN)) bits=0;
      return bits&F64_SIGN?~bits:bits|F64_SIGN;
    };
    res=OptCompare(op,key(a),key(b));
    return true;
  }
  if (op==IC_MUL)
    return OptF64Pack(static_cast<unsigned __int128>(x.sig)*y.sig,
                      x.exp+y.exp,x.neg!=y.neg,res,24);
  if (op==IC_DIV) {
    if (!y.sig) return false;
    while (!(y.sig&1)) {
      y.sig>>=1;
      y.exp++;
    }
    if (x.sig%y.sig) return false;
    return OptF64Pack(x.sig/y.sig,x.exp-y.exp,x.neg!=y.neg,res,24);
  }
  if (op==IC_ADD || op==IC_SUB) {
    if (op==IC_SUB) y.neg=!y.neg;
    if (!x.sig || !y.sig) {
      if (!x.sig && !y.sig && x.neg!=y.neg)
        return false; // The zero sign depends on the rounding mode.
      return OptF64Pack(x.sig?x.sig:y.sig,x.sig?x.exp:y.exp,
                        x.sig?x.neg:y.neg,res,24);
    }
    int exp=x.exp<y.exp?x.exp:y.exp;
    if (x.exp-exp>73 || y.exp-exp>73)
      return false;
    auto p=static_cast<unsigned __int128>(x.sig)<<(x.exp-exp);
    auto q=static_cast<unsigned __int128>(y.sig)<<(y.exp-exp);
    if (x.neg==y.neg)
      return OptF64Pack(p+q,exp,x.neg,res,24);
    if (p==q) return false;
    return OptF64Pack(p>q?p-q:q-p,exp,p>q?x.neg:y.neg,res,24);
  }
  if (op==IC_MOD && y.sig) {
    if (!x.sig) { res=a; return true; }
    int exp=x.exp<y.exp?x.exp:y.exp;
    if (x.exp-exp>10 || y.exp-exp>10)
      return false;
    auto p=x.sig<<(x.exp-exp),q=y.sig<<(y.exp-exp);
    return OptF64Pack(p%q,exp,x.neg,res,24);
  }
  return false;
}

static bool OptFold(CIntermediateCode &node,const CIntermediateCode &left,
                    const CIntermediateCode *right,uint64_t &res)
{
  auto op=node.code;
  auto a=left.data;
  if (!OptImmediate(left) || !TypeSize(node.type))
    return false;
  switch (op) {
    case IC_HOLYC_TYPECAST: res=a; return true;
    case IC_TO_I64: return OptToI64(left,res);
    case IC_TO_F64: return OptToF64(left,res);
    case IC_NOT: res=!a; return true;
    case IC_COM: res=~a; return true;
    case IC_UNARY_MINUS:
      res=left.type==RT_F64?a^F64_SIGN:uint64_t{0}-a;
      return true;
    case IC_SQR_I64:
      if (!OptToI64(left,a)) return false;
      res=a*a;
      return true;
    default: break;
  }
  if (!right || !OptImmediate(*right))
    return false;
  auto b=right->data;
  if (op==IC_AND_AND || op==IC_OR_OR || op==IC_XOR_XOR) {
    res=op==IC_AND_AND?(a && b):op==IC_OR_OR?(a || b):bool(a)!=bool(b);
    return true;
  }
  if (op==IC_MIN_I64 || op==IC_MAX_I64) {
    if (!OptToI64(left,a) || !OptToI64(*right,b)) return false;
    res=(op==IC_MIN_I64?int64_t(a)<int64_t(b):int64_t(a)>int64_t(b))?a:b;
    return true;
  }
  if (left.type==RT_F64 || right->type==RT_F64)
    return OptToF64(left,a) && OptToF64(*right,b) && OptF64Binary(node,a,b,res);
  return OptIntBinary(op,a,b,IsUnsigned(left.type)||IsUnsigned(right->type),res);
}

static void OptStrength(CIntermediateCode &node,const CIntermediateCode *code)
{
  const auto &left=code[node.left],&right=code[node.right];
  if (!IsInt(left.type) || !IsInt(right.type))
    return;
  bool lconst=left.code==IC_IMM_I64,rconst=right.code==IC_IMM_I64;
  if (lconst && !rconst && (node.code==IC_ADD || node.code==IC_MUL)) {
    auto tmp=node.left;
    node.left=node.right;
    node.right=tmp;
    rconst=true;
  }
  if (!rconst)
    return;
  uint64_t n=code[node.right].data;
  switch (node.code) {
    case IC_ADD:
    case IC_SUB:
      if (n==uint64_t(int64_t(int32_t(n)))) {
        node.code=node.code==IC_ADD?IC_ADD_CONST:IC_SUB_CONST;
        node.data=n;
      }
      break;
    case IC_SHL:
    case IC_SHR:
      node.code=node.code==IC_SHL?IC_SHL_CONST:IC_SHR_CONST;
      node.data=n&63;
      break;
    case IC_MUL:
    case IC_DIV:
      if (n && !(n&(n-1)) && (node.code==IC_MUL ||
          IsUnsigned(left.type) || IsUnsigned(right.type))) {
        node.code=node.code==IC_MUL?IC_SHL_CONST:IC_SHR_CONST;
        node.data=__builtin_ctzll(n);
      }
      break;
    default: break;
  }
}

void OptPass012(CIntermediateCode *code,uint32_t count)
{
  // A chained comparison needs the preceding comparison's right operand
  // still in RCX. Keep those comparison nodes intact.
  for (uint32_t i=0;i<count;i++)
    if (code[i].chain && code[i].left<i)
      code[code[i].left].cmp_link=true;

  // Expression children precede their parents. Do not walk the other fields
  // as links: e.g. IC_CALL.right is an argument count, IC_STR_CONST.left is
  // a byte count, and statement .right fields can be forward labels.
  for (uint32_t i=0;i<count;i++) {
    auto &node=code[i];
    if (node.left>=i || node.chain || node.cmp_link)
      continue;
    const auto &left=code[node.left];
    if (node.code==IC_BR_ZERO || node.code==IC_BR_NOT_ZERO) {
      if (OptImmediate(left)) {
        bool taken=(node.code==IC_BR_ZERO)==!left.data;
        node.code=taken?IC_JMP:IC_END_EXP;
        if (!taken) node.right=NO_IC;
      }
      continue;
    }
    switch (node.code) {
      case IC_HOLYC_TYPECAST: case IC_TO_I64: case IC_TO_F64:
      case IC_NOT: case IC_COM: case IC_UNARY_MINUS: case IC_SQR_I64:
      case IC_MIN_I64: case IC_MAX_I64:
      case IC_ADD: case IC_SUB: case IC_MUL: case IC_DIV: case IC_MOD:
      case IC_AND: case IC_OR: case IC_XOR: case IC_SHL: case IC_SHR:
      case IC_AND_AND: case IC_OR_OR: case IC_XOR_XOR:
      case IC_EQU_EQU: case IC_NOT_EQU: case IC_LESS: case IC_GREATER_EQU:
      case IC_GREATER: case IC_LESS_EQU: {
        uint64_t bits;
        auto *right=node.right<i?&code[node.right]:nullptr;
        if (OptFold(node,left,right,bits))
          OptSetImmediate(node,bits);
        else if (right)
          OptStrength(node,code);
        break;
      }
      default: break;
    }
  }
}
