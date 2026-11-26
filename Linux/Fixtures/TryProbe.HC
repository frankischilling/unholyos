I64 try_top;

I64 TryReturnQueue(CExcept *frame)
{
  if (frame==Fs->last_except)
    return 41;
  return -1;
}

I64 TryNormal(I64 early)
{
  CExcept *frame=Fs->last_except;
  try {
    try {
      if (early)
        return TryReturnQueue(frame);
    } catch {
      Fs->catch_except=TRUE;
    }
  } catch {
    Fs->catch_except=TRUE;
  }
  if (frame==Fs->last_except)
    return 42;
  return -2;
}

U0 TryLeaf()
{
  CLI
  throw('Leaf',TRUE);
}

I64 TryAccept()
{
  I64 value=1;
  try {
    TryLeaf;
    value=-1;
  } catch {
    if (Fs->except_ch=='Leaf' && Fs->except_rbp &&
        Fs->except_callers[0] && Fs->except_callers[1] &&
        !Fs->catch_except && (GetRFlags & 0x200))
      value=73;
    Fs->catch_except=TRUE;
  }
  return value;
}

I64 TryReject()
{
  I64 value=1;
  try {
    try {
      throw('Outer',TRUE);
    } catch {
      if (Fs->except_ch=='Outer')
        value=value*10+2;
    }
    value=-1;
  } catch {
    if (Fs->except_ch=='Outer')
      value=value*10+3;
    Fs->catch_except=TRUE;
  }
  return value;
}

I64 TryNested()
{
  I64 value=1;
  try {
    throw('First',TRUE);
  } catch {
    if (Fs->except_ch=='First') {
      try {
        throw('Second',TRUE);
      } catch {
        if (Fs->except_ch=='Second')
          value=value*10+2;
        Fs->catch_except=TRUE;
      }
      value=value*10+3;
    }
    Fs->catch_except=TRUE;
  }
  return value;
}

U0 TryProbe()
{
  CExcept *first=Fs->next_except,*last=Fs->last_except;
  I64 normal=TryNormal(0),early=TryNormal(1);
  I64 accepted=TryAccept,rejected=TryReject,nested=TryNested;
  "try %d %d %d %d %d %d %d\n",normal,early,accepted,rejected,nested,try_top,
    Fs->next_except==first && Fs->last_except==last;
}

try {
  throw('Top',TRUE);
  try_top=-1;
} catch {
  if (Fs->except_ch=='Top')
    try_top=5;
  Fs->catch_except=TRUE;
}

TryProbe;
