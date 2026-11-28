I64 optimizer_trace;

I64 OptimizerMark(I64 n)
{
  optimizer_trace=optimizer_trace*10+n;
  return n;
}

U0 OptimizerProbe()
{
  I64 res=0,n=-17,value,values[3];
  U64 u=0x8000000000000000;
  I8 narrow=127;
  I64 *p=values;
  if (1+2*3==7 && -17/4==-4 && -17%4==-1 &&
      0xFFFFFFFFFFFFFFFF/2==0x7FFFFFFFFFFFFFFF) res|=1;
  if ((1<<70)==64 && (-16>>2)==-4 &&
      (0x8000000000000000>>2)==0x2000000000000000 && ~5==-6) res|=2;
  if (n*8==-136 && 8*n==-136 && n+7==-10 && n-3==-20 &&
      (n>>2)==-5 && u/8==0x1000000000000000) res|=4;
  if (2.5+3*2.0==8.5 && 2.5%1.5==1.0 && 3.0/2.0==1.5 &&
      2.5<3.0 && 2.5>2.0) res|=8;
  if (1<2<3 && !(3<2<4) && (10<20)==1) res|=16;
  optimizer_trace=0;
  if ((0 && OptimizerMark(1)) || ((2+3==5) && OptimizerMark(2))) {
    if (OptimizerMark(3) || OptimizerMark(4)) {
      if (optimizer_trace==23) res|=32;
    }
  }
  optimizer_trace=0;
  value=OptimizerMark(5)*0;
  if (!value && optimizer_trace==5) res|=64;
  optimizer_trace=0;
  if (1<OptimizerMark(2)<4 && optimizer_trace==2) res|=128;
  values[0]=4;
  values[1]=5;
  values[2]=6;
  if (*(p+(1+1))==6 && p[2*0]==4 && narrow+2==129) res|=256;
  if (0 && (1/0)) res=0;
  if (1 || (1/0)) res|=512;
  "Optimizer probe %03X\n",res;
}

OptimizerProbe;
