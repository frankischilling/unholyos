// Exercise the original task and heap inspection implementation.
#include "/Adam/AMem.HC"

U0 AdamMemProbe()
{
  I64 res=0,before,alloced,flags;
  CHeapCtrl *hc=Fs->data_heap;
  U8 *small,*large;
  if (sizeof(CMemUnused)==16 && sizeof(CMemUsed)==16 &&
      sizeof(CMemBlk)==24 && sizeof(CHeapCtrl)==0x460 &&
      offset(CHeapCtrl.heap_hash)==0x60 &&
      offset(CMemBlk.pags)==20 && offset(CMemUsed.start)==16) res|=1;
  before=TaskMemUsed;
  alloced=TaskMemAlloced;
  if (before>0 && alloced>=before &&
      TaskMemUsed(Fs,TRUE)==before && TaskMemAlloced(Fs,TRUE)==alloced &&
      !TaskMemUsed(INVALID_PTR) && !TaskMemAlloced(INVALID_PTR)) res|=2;
  small=MAlloc(40);
  large=MAlloc(4096);
  if (small && large) {
    if (TaskMemUsed==before+MSize2(small)+MSize2(large) &&
        TaskMemAlloced>=alloced) res|=4;
    if (IsInHeap(small) && IsInHeap(large,Fs,FALSE) &&
        IsInHeapCtrl(small,hc) && IsInHeapCtrl(large,hc,FALSE) &&
        !IsInHeapCtrl(NULL,hc) && !IsInHeap(INVALID_PTR) &&
        !IsInHeap(small,INVALID_PTR)) res|=8;
    if (HeapWalk && HeapCtrlWalk(hc)) res|=16;
    flags=GetRFlags;
    if (IsInHeapCtrl(small,hc) && HeapCtrlWalk(hc) &&
        !Bt(&hc->locked_flags,HClf_LOCKED) &&
        !((GetRFlags^flags)&0x200)) res|=32;
    Free(small); small=NULL;
    Free(large); large=NULL;
    if (TaskMemUsed==before && HeapWalk) res|=64;
  }
  Free(small); Free(large);
  if (!HeapWalk(INVALID_PTR) && !HeapRep(Fs) && !HeapRep(NULL)) res|=128;
  // The unlocked traversal must not release a lock held by its caller.
  flags=GetRFlags;
  PUSHFD
  CLI
  if (!LBts(&hc->locked_flags,HClf_LOCKED)) {
    if (IsInHeapCtrl(hc->next_mem_blk,hc,FALSE) &&
        Bt(&hc->locked_flags,HClf_LOCKED)) res|=256;
    LBtr(&hc->locked_flags,HClf_LOCKED);
  }
  POPFD
  if (!((GetRFlags^flags)&0x200) && HeapWalk) res|=512;
  before=TaskMemUsed;
  small=MAlloc(48,Fs->code_heap);
  large=MAlloc(4096,Fs->data_heap);
  if (small && large &&
      TaskMemUsed==before+MSize2(small)+MSize2(large) &&
      IsInHeapCtrl(small,Fs->code_heap) && IsInHeapCtrl(large,Fs->data_heap) &&
      IsInHeap(small) && IsInHeap(large) && HeapWalk &&
      (Fs->code_heap==Fs->data_heap ||
       !IsInHeapCtrl(small,Fs->data_heap) && !IsInHeapCtrl(large,Fs->code_heap)))
    res|=1024;
  Free(small); Free(large);
  if (TaskMemUsed==before && HeapWalk) res|=2048;
  "Adam memory %X\n",res;
}

AdamMemProbe;
