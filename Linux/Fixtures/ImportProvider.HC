extern I64 NativeLinkTwice(I64 value);

public I64 NativeLinkAdd(I64 value)
{
  return NativeLinkTwice(value)+1;
}

I64 NativeLinkTwice(I64 value)
{
  return value*2;
}

public I64 NativeLinkBias=19;
