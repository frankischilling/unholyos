// Compile the original Adam source, including its original quirks.
#include "/Adam/AMath.HC"

Bool AdamMathNear(F64 a,F64 b)
{
  return Abs(a-b)<0.000000001;
}

U0 AdamMathProbe()
{
  I64 res=0,i;
  F64 mag,arg,x,y;
  Complex a,b,c,zeros[2];
  U16 cw=Fstcw;
  I64 xaddr=&a.x,yaddr=&a.y;
  if (sizeof(Complex)==16 && yaddr-xaddr==8) res|=1;
  R2P(&mag,&arg,3,4);
  P2R(&x,&y,mag,arg);
  if (AdamMathNear(mag,5) && AdamMathNear(x,3) && AdamMathNear(y,4)) res|=2;
  R2P(&mag,,5,12);
  P2R(,&y,2,0);
  if (AdamMathNear(mag,13) && y==0) res|=4;
  if (AdamMathNear(Wrap(4.0),4.0-6.283185307179586) &&
      AdamMathNear(Wrap(-1,0),5.283185307179586) &&
      DistSqrI64(2,5,5,9)==25) res|=8;
  // The original inverse-trig endpoints intentionally retain their behavior.
  if (AdamMathNear(ASin(-1),1.5707963267948966) && ACos(-1)==0 &&
      AdamMathNear(ASin(0.5),0.5235987755982989) &&
      AdamMathNear(ACos(0),1.5707963267948966)) res|=16;
  if (AdamMathNear(Exp(0),1) && AdamMathNear(Exp(Ln(2)),2) &&
      AdamMathNear(Pow10(2),100) && AdamMathNear(Log10(100),2) &&
      AdamMathNear(Log2(16),4) && AdamMathNear(Pow(-2,3),-8)) res|=32;
  if (AdamMathNear(Sinh(0),0) && AdamMathNear(Cosh(0),1) &&
      AdamMathNear(Sqr(Cosh(1))-Sqr(Sinh(1)),1)) res|=64;
  if (Round(2.5)==2 && Round(3.5)==4 && Round(-2.5)==-2 &&
      Floor(-1.25)==-2 && Ceil(-1.25)==-1 && Trunc(-1.25)==-1 &&
      Sign(-3)==-1 && Sign(0)==0 && Sign(3)==1 && Fstcw==cw) res|=128;
  CEqu(&a,3,4); CEqu(&b,1,2);
  if (CAdd(&c,&a,&b)==&c && c.x==4 && c.y==6 &&
      CSub(&c,&a,&b)==&c && c.x==2 && c.y==2) res|=256;
  if (CMul(&c,&a,&b)==&c && c.x==-5 && c.y==10 &&
      CDiv(&c,&a,&b)==&c && AdamMathNear(c.x,2.2) &&
      AdamMathNear(c.y,-0.4)) res|=512;
  if (CCopy(&c,&a)==&c && CScale(&c,2)==&c && c.x==6 && c.y==8)
    res|=1024;
  CEqu(&zeros[0],1,0); CEqu(&zeros[1],-1,0); CEqu(&a,2,1);
  if (CPoly(&c,2,zeros,&a)==&c && c.x==2 && c.y==4 &&
      CPoly(&c,0,zeros,&a)==&c && c.x==1 && !c.y) res|=2048;
  Fldcw(cw|0xC00);
  Bool okay=Fstcw==(cw|0xC00);
  for (i=0;i<32;i++) {
    if (!AdamMathNear(Exp(Ln(2)),2) || Round(3.5)!=4 ||
        Fstcw!=(cw|0xC00)) okay=FALSE;
  }
  Fclex;
  Fldcw(cw);
  if (okay && Fstcw==cw && !(Fstsw&0x3F)) res|=4096;
  "Adam math %X\n",res;
}

AdamMathProbe;
