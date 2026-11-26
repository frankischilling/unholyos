U0 BreakProbe()
{
  I64 res=0,i,ch,sc;
  CExcept *first=Fs->next_except,*last=Fs->last_except;
  try {
    Break;
  } catch {
    if (Fs->except_ch=='Break')
      res|=1;
    Fs->catch_except=TRUE;
  }
  if (BreakLock && !BreakLock && BreakUnlock && !BreakUnlock)
    res|=2;
  try {
    BreakLock;
    "Break probe locked\n";
    for (i=0;i<500 && !(Fs->task_flags & 1<<TASKf_PENDING_BREAK);i++)
      Sleep(10);
    if (Fs->task_flags & 1<<TASKf_PENDING_BREAK &&
        !(Fs->task_flags & 1<<TASKf_BREAK_LOCKED))
      res|=4;
    BreakUnlock;
  } catch {
    if (Fs->except_ch=='Break')
      res|=8;
    Fs->catch_except=TRUE;
  }
  FlushMsgs;
  Fs->task_flags|=1<<TASKf_BREAK_TO_SHIFT_ESC;
  Break;
  if (ScanMsg(&ch,&sc,1<<MSG_KEY_DOWN)==MSG_KEY_DOWN &&
      ch==CH_SHIFT_ESC && sc==0x20100000201)
    res|=16;
  FlushMsgs;
  "Break probe shift\n";
  for (i=0;i<500;i++) {
    if (ScanMsg(&ch,&sc,1<<MSG_KEY_DOWN)==MSG_KEY_DOWN &&
        ch==CH_SHIFT_ESC && sc==0x20100000201) {
      res|=32;
      break;
    }
    Sleep(10);
  }
  Fs->task_flags&=~(1<<TASKf_BREAK_TO_SHIFT_ESC);
  if (!(Fs->task_flags & (1<<TASKf_BREAK_LOCKED|1<<TASKf_PENDING_BREAK)) &&
      Fs->next_except==first && Fs->last_except==last)
    res|=64;
  "Break probe %X\n",res;
}

BreakProbe;
