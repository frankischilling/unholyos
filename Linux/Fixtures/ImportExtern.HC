extern I64 NativeLinkAdd(I64 value);
extern I64 NativeLinkResult;
_extern SYS_COMPILE_TIME I64 NativeJitDate;

extern I64 NativeJitFn(I64 n);
extern I64 NativeJitValue;
I64 NativeJitUse(I64 n)
{
  return NativeJitFn(n)+NativeJitValue;
}

I64 NativeJitFn(I64 n)
{
  return n+3;
}

I64 NativeJitValue=44;
"native extern %d %d %d\n",NativeLinkAdd(7),NativeLinkResult,NativeJitUse(2);
extern I64 NativeLinkDateResult;
"native extern date %d\n",NativeLinkDateResult==NativeJitDate;

I64 NativeLateFn(I64 value)
{
  return value*3;
}
I64 NativeLateData=22;
