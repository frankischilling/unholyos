I64 call_log = 0, call_result;

I64 CallStamp(I64 n)
{
  return call_log = call_log * 10 + n;
}

public I64 CallPack(I64 a, I64 b=7, I64 c=9)
{
  return 100 * a + 10 * b + c;
}

noargpop I64 CallDifference(I64 a, I64 b)
{
  return a - b;
}

I64 CallSum(I64 a, I64 b)
{
  return a + b;
}

F64 CallScale(F64 a, F64 b=3.25)
{
  return a * b;
}

I64 CallInt(I64 a)
{
  return a + 1;
}

F64 CallFloat(I64 a)
{
  return a + 2;
}

I64 CallDefault(I64 a=7)
{
  return a;
}

U64 CallShift(U64 a)
{
  return a >> 3;
}

I64 CallZero()
{
  return 10;
}

I64 CallSaved()
{
  return CallZero;
}

I64 CallZero()
{
  return 20;
}

I64 CallAssign(I64 a)
{
  return a = a + 12;
}

CallPack(2);
CallPack(3,,4);
CallPack(CallStamp(1),CallStamp(2),CallStamp(3));
call_log;
call_result = CallDifference(CallPack(4,5,6),CallPack(1,2,3));
call_result;
CallSum(CallPack(1),CallPack(2));
CallScale(4);
CallScale(2,5);
CallInt(-7.9);
CallFloat(3);
CallDefault + 3;
CallShift(0x8000000000000000);
CallSaved() + CallZero;
CallAssign(5);

I64 CallSeries(I64 n)
{
  I64 i, j, sum=0;
  for (i=0,j=9; i<n; i++,j--) {
    if (i==3)
      break;
    sum=sum+i*j;
  }
  return sum;
}

I64 CallWhile(I64 n)
{
  I64 m=0;
  do {
    m++;
  } while (m<n);
  while (m<5) {
    if (++m==4)
      break;
  }
  return m;
}

I64 CallMark(I64 a)
{
  call_log=call_log*10+a;
  return a!=2;
}

I64 CallBranch(I64 a)
{
  call_log=0;
  if ((a && CallMark(1)) || (!a && !CallMark(2) && CallMark(3))) {
    if (CallMark(4) || CallMark(5))
      return call_log;
  } else
    return 0;
  return -1;
}

// Recurse2 and Recurse3 are from Terry A. Davis's Demo/StkGrow.HC.
I64 Recurse2(I64 n)
{
  if (n)
    return 1+Recurse2(n-1);
  else
    return 0;
}

I64 Recurse3(I64 n,I64 lo,I64 hi)
{
  if (lo<=n<hi)
    return 1+Recurse3(n-1,lo,hi);
  else if (n)
    return Recurse3(n-1,lo,hi);
  else
    return 0;
}

I64 CallFactorial(I64 n)
{
  if (n<2)
    return 1;
  return n*CallFactorial(n-1);
}

F64 CallFloatLocal(F64 a)
{
  F64 b=a+1, c=b++;
  return c*10+(--b);
}

U0 CallVoid(I64 n)
{
  if (n)
    return;
  call_log=42;
}

I64 CallCheckVoid(I64 n)
{
  call_log=0;
  CallVoid(n);
  return call_log;
}

CallSeries(0);
CallSeries(6);
CallWhile(0);
CallWhile(5);
CallBranch(0);
CallBranch(1);
Recurse2(18);
Recurse3(23,10,20);
CallFactorial(7);
CallFloatLocal(2.5);
CallCheckVoid(0);
CallCheckVoid(1);
