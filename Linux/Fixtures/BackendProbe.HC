// Runtime intrinsic and power semantics from the pinned compiler.
I64 backend_order;

I64 BackendMark(I64 n)
{
  backend_order=backend_order*10+n;
  return n;
}

I64 BackendSign(I64 n)
{
  return SignI64(n);
}

I64 BackendAbsI64(I64 n)
{
  return AbsI64(n);
}

U64 BackendSquareU64(U64 n)
{
  return SqrU64(n);
}

Bool BackendUnsigned(U64 hi,U64 lo)
{
  return MinU64(hi,lo)==lo && MaxU64(lo,hi)==hi;
}

Bool BackendScan(I64 n,I64 first,I64 last)
{
  return Bsf(n)==first && Bsr(n)==last;
}

Bool BackendBool(I64 n)
{
  return ToBool(n);
}

I64 BackendUpper(U8 n)
{
  return ToUpper(n);
}

Bool BackendFloat(F64 n,F64 sq,F64 ab)
{
  return Sqr(n)==sq && Abs(n)==ab;
}

Bool BackendNear(F64 x,F64 y)
{
  F64 d=x-y;
  return -0.00000001<d && d<0.00000001;
}

F64 BackendRoundTrip(F64 x)
{
  return Tan(ATan(x));
}

Bool BackendConvert(I64 i,F64 f)
{
  return Sqr(i)==9.0 && Abs(i)==3.0 && SignI64(f)==-1 &&
      AbsI64(f)==3 && SqrI64(f)==9;
}

F64 BackendPower(F64 base,F64 exponent)
{
  return base`exponent;
}

I64 BackendSpillLeaf(I64 noreg a,I64 noreg b,I64 noreg c)
{
  return a+b*c;
}

I64 BackendSpillNested(I64 noreg a,I64 noreg b)
{
  I64 noreg (*fn)(I64 a,I64 b,I64 c);
  fn=&BackendSpillLeaf;
  return a+(fn(b,2,3)*BackendSpillLeaf(4,5,6));
}

Bool BackendBits(U8 *p,I64 bit)
{
  if (Bt(p,bit) || Bts(p,bit)!=0 || !Bt(p,bit)) return FALSE;
  if (!Bts(p,bit) || !Btc(p,bit) || Bt(p,bit)) return FALSE;
  if (Btr(p,bit) || LBts(p,bit) || !Bt(p,bit)) return FALSE;
  if (!LBtc(p,bit) || Bt(p,bit) || LBtr(p,bit)) return FALSE;
  if (LBts(p,bit) || !LBtr(p,bit) || Bt(p,bit)) return FALSE;
  return TRUE;
}

#define BACKEND_SET_BIT 0x78
public _intern BACKEND_SET_BIT Bool BackendDeclaredSet(U8 *p,I64 bit);
_intern (0xAE+1) U64 BackendDeclaredMod(U64 *q,U64 d);
_intern 0x76 I64 BackendDeclaredTSC();

Bool BackendDeclarations()
{
  U64 word=0,q=29;
  I64 t=BackendDeclaredTSC;
  if (BackendDeclaredSet(&word,63) || word!=0x8000000000000000 ||
      !BackendDeclaredSet(&word,63)) return FALSE;
  return BackendDeclaredMod(&q,6)==5 && q==4 && BackendDeclaredTSC>=t;
}

Bool BackendBitMemory()
{
  I64 words[4];
  U8 *p=words;
  I64 i;
  for (i=0;i<4;i++) words[i]=0;
  if (!BackendBits(p,0) || !BackendBits(p,31) || !BackendBits(p,63) ||
      !BackendBits(p,64) || !BackendBits(p,127) ||
      !BackendBits(p+8,-1)) return FALSE;
  if (Bts(p,31) || Bts(p,32) || Bts(p,63) || Bts(p,64)) return FALSE;
  if (words[0]!=0x8000000180000000 || words[1]!=1) return FALSE;
  if (!LBtr(p,31) || !LBtc(p,32) || !Btr(p,63) || !Btc(p,64))
    return FALSE;
  return !words[0] && !words[1] && !words[2] && !words[3];
}

Bool BackendSwaps()
{
  U8 a[3];
  U16 b[3];
  U32 c[3];
  I64 d[3];
  a[0]=0x12; a[1]=0xFE; a[2]=0xAB;
  b[0]=0xABCD; b[1]=0xFEDC; b[2]=0x5678;
  c[0]=0x12345678; c[1]=0xFEDCBA98; c[2]=0x76543210;
  d[0]=0x123456789ABCDEF0; d[1]=0xFEDCBA9876543210; d[2]=77;
  SwapU8(a,a+1); SwapU16(b,b+1); SwapU32(c,c+1); SwapI64(d,d+1);
  SwapU8(a,a); SwapU16(b,b); SwapU32(c,c); SwapI64(d,d);
  return a[0]==0xFE && a[1]==0x12 && a[2]==0xAB &&
      b[0]==0xFEDC && b[1]==0xABCD && b[2]==0x5678 &&
      c[0]==0xFEDCBA98 && c[1]==0x12345678 && c[2]==0x76543210 &&
      d[0]==0xFEDCBA9876543210 && d[1]==0x123456789ABCDEF0 && d[2]==77;
}

U64 backend_dividend;

U64 *BackendDividend(I64 marker)
{
  BackendMark(marker);
  return &backend_dividend;
}

Bool BackendModulo()
{
  U64 rem;
  backend_dividend=0xFEDCBA9876543210;
  rem=ModU64(&backend_dividend,0x100000000);
  if (rem!=0x76543210 || backend_dividend!=0xFEDCBA98) return FALSE;
  backend_order=0;
  backend_dividend=29;
  rem=ModU64(BackendDividend(1),BackendMark(6));
  if (rem!=5 || backend_dividend!=4 || backend_order!=61) return FALSE;
  backend_dividend=3;
  rem=ModU64(&backend_dividend,7);
  return rem==3 && !backend_dividend;
}

Bool BackendPorts(I64 port)
{
  U8 saved=InU8(port),v;
  OutU8(port,0xA5);
  v=InU8(port);
  OutU8(port,saved);
  return v==0xA5;
}

Bool BackendPciPorts()
{
  U32 old=InU32(0xCF8),id;
  U16 vendor;
  U8 low;
  OutU32(0xCF8,0x80000000);
  id=InU32(0xCFC);
  vendor=InU16(0xCFC);
  low=InU8(0xCFC);
  OutU32(0xCF8,old);
  return id==0x12378086 && vendor==0x8086 && low==0x86;
}

U0 BackendProbe()
{
  I64 res=0,i,t0,t1;
  Bool okay=TRUE;
  F64 x;
  if (BackendSign(-123)==-1 && BackendSign(0)==0 &&
      BackendSign(123)==1) res|=1;
  if (BackendAbsI64(-12345)==12345 && !BackendAbsI64(0) &&
      BackendAbsI64(12345)==12345) res|=2;
  if (BackendSquareU64(7)==49 &&
      BackendSquareU64(0x100000001)==0x200000001) res|=4;
  if (BackendUnsigned(0xFEDCBA9876543210,0x123456789) &&
      BackendUnsigned(0x8000000000000000,0x7FFFFFFFFFFFFFFF))
    res|=8;
  if (BackendScan(0x8000000000000010,4,63) &&
      BackendScan(0x100000000,32,32)) res|=16;
  if (BackendBool(-123) && !BackendBool(0) && BackendBool(256))
    res|=32;
  if (BackendUpper('a')=='A' && BackendUpper('z')=='Z' &&
      BackendUpper('!')=='!' && BackendUpper('Q')=='Q') res|=64;
  // The pinned lexer rounds the decimal 6.25 one binary64 ULP upward.
  // Check both that behavior and the exact x87 square result.
  if ((6.25)(I64)==0x4019000000000001 &&
      BackendFloat(-2.5,(0x4019000000000000)(F64),2.5) &&
      BackendFloat(3.0,9.0,3.0)) res|=128;
  if (BackendNear(BackendRoundTrip(0.5),0.5) &&
      BackendNear(BackendRoundTrip(-0.75),-0.75) &&
      BackendNear(ATan(1.0),0.7853981633974483)) res|=256;
  if (BackendConvert(-3,-3.75)) res|=512;

  backend_order=0;
  if (MinU64(BackendMark(1),BackendMark(2))==1 &&
      backend_order==21) res|=1024;
  // Repeated nested x87 operations must leave the floating stack balanced.
  for (i=1;i<=32;i++) {
    x=ToF64(i)/64.0;
    if (!BackendNear(BackendRoundTrip(x),x) ||
        !BackendNear(Abs(Sqr(-x)),x*x)) okay=FALSE;
  }
  if (okay) res|=2048;

  if (BackendNear(BackendPower(2.0,10.0),1024.0) &&
      BackendNear(BackendPower(9.0,0.5),3.0)) res|=4096;
  if (BackendNear(BackendPower(-2.0,3.0),-8.0) &&
      BackendNear(BackendPower(-2.0,4.0),16.0)) res|=8192;
  if (BackendPower(0.0,0.0)==1.0 &&
      BackendPower(-7.0,0.0)==1.0 &&
      BackendPower(0.0,3.0)==0.0 &&
      BackendPower(0.0,-3.0)==0.0) res|=16384;
  t0=GetTSC;
  t1=GetTSC();
  if (t0>0 && t1>=t0 &&
      BackendNear(BackendPower(2.0,-3.0),0.125) &&
      BackendNear(Sqr(BackendPower(2.0,3.0)),64.0)) res|=32768;
  if (BackendSpillNested(13,7)==455 && BackendSpillNested(-1,-4)==67)
    res|=0x10000;
  if (BackendBitMemory()) res|=0x20000;
  if (BackendSwaps()) res|=0x40000;
  if (BackendModulo()) res|=0x80000;
  if (BackendPorts(0x3FF)) res|=0x100000;
  if (BackendPciPorts()) res|=0x200000;
  if (BackendDeclarations()) res|=0x400000;
  "Backend probe %X\n",res;
}

BackendProbe;
