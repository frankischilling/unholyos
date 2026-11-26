I64 FpPair(I64 x,I64 y)
{
  return x*10+y;
}

I64 FpZero()
{
  return 9;
}

I64 FpOther(I64 x,I64 y)
{
  return x*100+y;
}

noargpop I64 FpCallerPop(I64 x)
{
  return x+2;
}

F64 FpMixed(F64 x,I64 y)
{
  return x+y;
}

class CFpProbe
{
  I64 tag;
  I64 (*run)(I64 x,I64 y=5);
  F64 (*mixed)(F64 x,I64 y);
};

I64 (*fp_global)(I64,I64);
noargpop I64 (*fp_caller_pop)(I64);

I64 FpArgument(I64 (*cb)(I64,I64),I64 x,I64 y)
{
  return cb(x,y);
}

I64 FpSwap()
{
  fp_global=&FpOther;
  return 7;
}

U0 FunctionPtrProbe()
{
  CFpProbe box;
  I64 (*fn)(I64,I64);
  I64 (**slot)(I64,I64);
  I64 (*zero)();
  I64 (*table)(I64,I64)[2];
  fn=&FpPair;
  slot=&fn;
  zero=&FpZero;
  table[0]=fn;
  table[1]=&FpOther;
  box.run=fn;
  box.mixed=&FpMixed;
  fp_caller_pop=&FpCallerPop;
  fp_global=fn;
  "function pointers %d %d %d %d %d %d %d\n",
      fn(4,2),(*slot)(1,3),box.run(3),FpArgument(fn,2,9),
      table[1](2,1)+table[0](1,2),fp_caller_pop(5),zero();
  I64 converted=box.mixed(1.75,6);
  "function pointer float %d %d\n",converted,sizeof(CFpProbe);
  "function pointer target %d\n",fp_global(2,FpSwap());
  "function pointer rebound %d\n",fp_global(2,7);
}

FunctionPtrProbe;
