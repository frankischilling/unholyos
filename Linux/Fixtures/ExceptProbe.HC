#include "T:/Kernel/KernelA.HH"
#include "T:/Compiler/CompilerA.HH"

_intern IC_FS CTask *ProbeExceptTask();
_intern IC_GET_RFLAGS I64 ProbeExceptFlags();
import U0 SysTry(U8 *catch_start,U8 *untry_start);
import U0 SysUntry();
import U0 throw(I64 ch=0,Bool no_log=FALSE);
import U8 *Caller(I64 depth=1);
import U0 PutExcept(Bool catch_it=TRUE);

I64 ProbeNormal(I64 early)
{
  try {
    if (early)
      return 41;
  } catch {
    ProbeExceptTask()->catch_except=TRUE;
    return -1;
  }
  return 42;
}

I64 ProbeThrowLeaf()
{
  CLI
  throw('Leaf',TRUE);
  return -1;
}

I64 ProbeAccept()
{
  I64 result=1;
  try {
    result=ProbeThrowLeaf();
    result=2;
  } catch {
    CTask *task=ProbeExceptTask();
    if (task->except_ch=='Leaf' && task->except_rbp &&
        task->except_callers[0] && task->except_callers[1] &&
        !task->catch_except && (ProbeExceptFlags & 0x200))
      result=73;
    task->catch_except=TRUE;
  }
  return result;
}

I64 ProbeReject()
{
  I64 result=1;
  try {
    try {
      throw('Outer',TRUE);
      result=-1;
    } catch {
      if (ProbeExceptTask()->except_ch=='Outer')
        result=result*10+2;
    }
    result=-2;
  } catch {
    if (ProbeExceptTask()->except_ch=='Outer')
      result=result*10+3;
    ProbeExceptTask()->catch_except=TRUE;
  }
  return result;
}

I64 ProbeNested()
{
  I64 result=1;
  try {
    throw('First',TRUE);
  } catch {
    if (ProbeExceptTask()->except_ch=='First') {
      try {
        throw('Second',TRUE);
      } catch {
        if (ProbeExceptTask()->except_ch=='Second')
          result=result*10+2;
        ProbeExceptTask()->catch_except=TRUE;
      }
      result=result*10+3;
    }
    ProbeExceptTask()->catch_except=TRUE;
  }
  return result;
}

Bool ProbeCallerInner(U8 *inner,U8 *outer)
{
  U8 *a=Caller(0),*b=Caller(1);
  return inner<a<inner+256 && outer<b<outer+256 && !Caller(1024);
}

Bool ProbeCallerOuter(U8 *outer)
{
  return ProbeCallerInner(&ProbeCallerInner,outer);
}

Bool ProbeCallers()
{
  return ProbeCallerOuter(&ProbeCallerOuter);
}

I64 ProbePutExcept()
{
  I64 result=1;
  try {
    throw('Print',TRUE);
    result=-1;
  } catch {
    if (ProbeExceptTask()->except_ch=='Print')
      result=2;
    PutExcept(TRUE);
  }
  return result;
}

I64 ProbeExceptAll()
{
  CTask *task=ProbeExceptTask();
  CExcept *first=task->next_except,*last=task->last_except;
  I64 before=task->data_heap->used_u8s;
  I64 result=(ProbeNormal(0)==42 && ProbeNormal(1)==41)
    +2*(ProbeAccept()==73)+4*(ProbeReject()==123)+8*(ProbeNested()==123)
    +16*ProbeCallers();
  if (task->next_except==first && task->last_except==last &&
      task->data_heap->used_u8s==before)
    result+=64;
  if (ProbePutExcept()==2 && task->next_except==first && task->last_except==last)
    result+=32;
  return result;
}

public I64 ExceptProbeResult;
ExceptProbeResult=ProbeExceptAll();
