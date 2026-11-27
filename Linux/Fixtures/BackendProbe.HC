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
  "Backend probe %X\n",res;
}

BackendProbe;
