I64 GotoProbe(I64 n)
{
  I64 noreg result=n;
  goto check;
  result=1000;
again:
  result+=2;
check:
  if (result<5)
    goto again;
  goto from_asm;
  result=1000;
  asm {
from_asm:
    ADD U64 &result[RBP],37
    JMP finished
  }
  result=1000;
finished:
  return result;
}

I64 NativeGoto(I64 n)
{
again:
  if (n<5) {
    n++;
    goto again;
  }
  return n;
}

"goto %d %d\n",GotoProbe(3),NativeGoto(1);
