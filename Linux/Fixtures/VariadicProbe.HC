I64 va_order;

I64 VaMark(I64 value)
{
  va_order=va_order*10+value;
  return value;
}

I64 VaSum(I64 bias=9,...)
{
  I64 i,result=bias+100*argc;
  for (i=0;i<argc;i++)
    result+=argv[i];
  return result;
}

I64 VaOnly(...)
{
  I64 *p=argv;
  if (!argc)
    return 0;
  return 100*argc+*p+p[argc-1];
}

I64 VaRaw(...)
{
  F64 f=argv[0](F64);
  U8 *s=argv[1](U8 *);
  return f*4+s[1];
}

I64 VaMutate(I64 bias,...)
{
  I64 *p=&argv[0];
  *p+=bias;
  argc--;
  return VaSum(argc,*p,argv[1]);
}

I64 VaRecursive(I64 n,...)
{
  if (!n)
    return argc;
  return argv[0]+VaRecursive(n-1,argv[0]+1);
}

I64 VaFixed(I64 a,I64 b)
{
  return a*10+b;
}

U0 VariadicProbe()
{
  I64 i,result;
  "variadic direct %d %d %d %d %d\n",VaSum(),VaSum(4,7,8),
      VaSum(,5),VaOnly(),VaOnly(3,4,5);
  "variadic raw %d mutate %d recurse %d\n",VaRaw(1.75,"AB"),
      VaMutate(3,4,5),VaRecursive(3,2);
  va_order=0;
  result=VaSum(VaMark(1),VaMark(2),VaMark(3));
  "variadic order %d %d\n",result,va_order;
  result=0;
  for (i=0;i<100;i++)
    result+=VaFixed(VaSum(1,2),VaOnly());
  "variadic cleanup %d\n",result;
}

VariadicProbe;
