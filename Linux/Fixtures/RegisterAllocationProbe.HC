// Register hints and their fallback behavior in PrsVar.HC and OptPass3.HC.
I64 RegisterProbeLeaf(I64 reg value)
{
  I64 reg R15 v=value*3;
  return v+1;
}

I64 RegisterProbeMixed(I64 reg a,I64 noreg b,reg I64 c=7)
{
  I64 reg x=a+b,noreg y=c,reg z=2;
  return RegisterProbeLeaf(x)+y*z;
}

I64 RegisterProbeNested(I64 value)
{
  I64 reg R15 keep=value+5;
  I64 reg a=RegisterProbeLeaf(value),b=RegisterProbeLeaf(keep);
  return keep+a+b;
}

U0 RegisterProbeAdd(I64 *ptr,I64 n)
{
  *ptr+=n;
}

I64 RegisterProbeAddress(I64 value)
{
  I64 reg local=value+1;
  I64 reg *ptr=&local;
  RegisterProbeAdd(ptr,9);
  return local+*ptr;
}

I64 RegisterProbeParamAddress(I64 reg value)
{
  RegisterProbeAdd(&value,5);
  return value;
}

I64 RegisterProbePressure(I64 seed)
{
  I64 reg a=seed,reg b=seed+1,reg c=seed+2,reg d=seed+3,
      reg e=seed+4,reg f=seed+5,reg g=seed+6,reg h=seed+7,
      reg i=seed+8,reg j=seed+9;
  return a+b+c+d+e+f+g+h+i+j+RegisterProbeLeaf(seed);
}

I64 RegisterProbeTry(I64 seed)
{
  I64 reg value=seed;
  try {
    value+=RegisterProbeLeaf(seed);
  } catch {
    value=-1;
    Fs->catch_except=TRUE;
  }
  return value;
}

I64 RegisterProbeStatic(I64 seed)
{
  static I64 value=3;
  I64 noreg (*fn)(I64 n);
  fn=&RegisterProbeLeaf;
  value+=fn(seed);
  return value;
}

U0 RegisterAllocationProbe()
{
  I64 res=0;
  if (RegisterProbeMixed(4,6)==45 &&
      RegisterProbeMixed(4,6,3)==37) res|=1;
  if (RegisterProbeNested(4)==50) res|=2;
  if (RegisterProbeAddress(6)==32) res|=4;
  if (RegisterProbeParamAddress(17)==22) res|=8;
  if (RegisterProbePressure(4)==98) res|=16;
  if (RegisterProbeTry(5)==21) res|=32;
  if (RegisterProbeStatic(2)==10 &&
      RegisterProbeStatic(3)==20) res|=64;
  "Register allocation %X\n",res;
}

RegisterAllocationProbe;
