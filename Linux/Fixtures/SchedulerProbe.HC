I64 sched_trace,sched_local,sched_span,sched_suspended,sched_exit;
CTask *sched_parent;

U0 SchedChild(U8 *opaque)
{
  I64 *trace=opaque;
  I64 start=cnts.jiffies;
  F64 noreg fp=0.0;
  sched_local=Fs!=sched_parent && Fs->parent_task==sched_parent;
  *trace=*trace*10+1;
  asm { FLD1 }
  Sleep(20);
  asm { FSTP U64 &fp[RBP] }
  sched_span=cnts.jiffies-start>=20 && fp==1.0;
  *trace=*trace*10+3;
}

U0 SchedExit(U8 *opaque)
{
  if (opaque)
    sched_trace=0;
  sched_trace=sched_trace*10+4;
  Exit;
  sched_trace=0;
}

U0 SchedulerProbe()
{
  CTask *child;
  F64 noreg fp=1.0;
  sched_trace=0;
  sched_parent=Fs;
  child=Spawn(&SchedChild,&sched_trace,,,sched_parent);
  if (TaskValidate(child)) {
    sched_suspended=!Suspend(child) && IsSuspended(child);
    Yield;
    sched_suspended=sched_suspended && !sched_trace && Suspend(child,FALSE);
    SingleUser(TRUE);
    Sleep(0);
    sched_suspended=sched_suspended && !sched_trace && IsSingleUser;
    SingleUser(FALSE);
    asm { FLDZ }
    Yield;
    asm { FSTP U64 &fp[RBP] }
    sched_suspended=sched_suspended && fp==0.0;
    sched_trace=sched_trace*10+2;
    Sleep(40);
    while (TaskValidate(child))
      Yield;
    child=Spawn(&SchedExit);
    Sleep(0);
    sched_exit=!TaskValidate(child);
  }
  "sched %d %d %d %d %d\n",sched_trace,sched_local,sched_span,sched_suspended,sched_exit;
}

SchedulerProbe;
