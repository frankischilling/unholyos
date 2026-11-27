I64 math_nums[13]={-21,-20,-19,-10,-9,-1,0,1,9,10,11,19,20};
I64 math_steps[8]={1,2,3,7,10,-2,-3,-10};
I64 math_edges[4]={0x8000000000000000,0x8000000000000001,
                   0x7FFFFFFFFFFFFFFE,0x7FFFFFFFFFFFFFFF};
U64 math_ue[6]={0,1,15,0x7FFFFFFFFFFFFFFF,0x8000000000000000,
                0xFFFFFFFFFFFFFFFF};
U64 math_ut[5]={1,3,10,0x8000000000000000,0xFFFFFFFFFFFFFFFF};
U64 math_fv[12]={0xFFF0000000000000,0xC008000000000000,
  0x8000000000000000,0,1,0x8000000000000001,
  0x4002000000000000,0x4008000000000000,0x7FF0000000000000,
  0x7FF8000000001234,0xFFF8000000005678,0x7FF0000000001234};

U0 MathHelpersProbe()
{
  I64 i,j,k,n;
  U64 h=0xCBF29CE484222325,g=0xCBF29CE484222325;
  U64 f=0xCBF29CE484222325;
  F64 *vals=math_fv;
  F64 q,z,p,m;
  I64 *bits;
  F64 (*min_fp)(F64,F64);
  I64 (*ceil_fp)(I64,I64);
  bits=&q; *bits=0x7FF8000000001234;
  bits=&z; *bits=0x8000000000000000;
  bits=&p; *bits=0x7FF0000000000000;
  bits=&m; *bits=0xFFF0000000000000;
  min_fp=&Min;
  ceil_fp=&CeilI64;

  for(i=0;i<13;i++) {
    n=math_nums[i];
    "round %d %d %d %d\n",n,RoundI64(n,10),FloorI64(n,10),CeilI64(n,10);
  }
  for(i=-47;i<=47;i++)
    for(j=0;j<8;j++) {
      h=(h^RoundI64(i,math_steps[j]))*0x100000001B3;
      h=(h^FloorI64(i,math_steps[j]))*0x100000001B3;
      h=(h^CeilI64(i,math_steps[j]))*0x100000001B3;
    }
  for(i=0;i<4;i++)
    for(j=0;j<8;j++) {
      h=(h^RoundI64(math_edges[i],math_steps[j]))*0x100000001B3;
      h=(h^FloorI64(math_edges[i],math_steps[j]))*0x100000001B3;
      h=(h^CeilI64(math_edges[i],math_steps[j]))*0x100000001B3;
    }
  for(i=0;i<6;i++)
    for(j=0;j<5;j++) {
      g=(g^FloorU64(math_ue[i],math_ut[j]))*0x100000001B3;
      g=(g^CeilU64(math_ue[i],math_ut[j]))*0x100000001B3;
    }
  for(i=0;i<12;i++)
    for(j=0;j<12;j++) {
      f=(f^Min(vals[i],vals[j])(I64))*0x100000001B3;
      f=(f^Max(vals[i],vals[j])(I64))*0x100000001B3;
      for(k=0;k<12;k++)
        f=(f^Clamp(vals[i],vals[j],vals[k])(I64))*0x100000001B3;
    }
  "math integer %016X %016X\n",h,g;
  "math floating %016X\n",f;
  "math clamp %016X %016X %016X\n",
    Clamp(-5,-2,3)(I64),Clamp(5,-2,3)(I64),Clamp(1.25,-2,3)(I64);
  "math reversed %016X %016X %016X\n",
    Clamp(5,8,3)(I64),Clamp(10,8,3)(I64),Clamp(-1,8,3)(I64);
  "math zero %016X %016X %016X\n",
    Min(z,0.0)(I64),Max(z,0.0)(I64),Clamp(z,0.0,1.0)(I64);
  "math finite %016X %016X %016X\n",
    Min(-5.5,2.25)(I64),Max(-5.5,2.25)(I64),min_fp(4.0,-3.0)(I64);
  "math nan1 %016X %016X %016X\n",
    Min(q,1.0)(I64),Min(1.0,q)(I64),Max(q,1.0)(I64);
  "math nan2 %016X %016X %016X\n",
    Max(1.0,q)(I64),Clamp(q,-2.0,3.0)(I64),Clamp(1.0,q,3.0)(I64);
  "math infinity %016X %016X %016X\n",
    Min(p,3.0)(I64),Max(m,-3.0)(I64),Clamp(p,-2.0,3.0)(I64);
  "math powers %016X %016X %016X\n",
    Pow10I64(-309)(I64),Pow10I64(2)(I64),Pow10I64(309)(I64);
  "math indirect %d %d\n",ceil_fp(-1,10),ceil_fp(11,10);
  "Math helpers done\n";
}
MathHelpersProbe;
