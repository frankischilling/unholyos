#include "../KernelB.hpp"

CHeapCtrl *HeapCtrlInit(CTask *task, CBlkPool *bp)
{
  auto *hc =
      reinterpret_cast<CHeapCtrl *>(MAlloc(sizeof(CHeapCtrl), task->data_heap));
  if (!hc)
    return nullptr;

  MemSet(hc, 0, sizeof(CHeapCtrl));
  hc->hc_signature = HEAP_CTRL_SIGNATURE_VAL;
  hc->mem_task = task;
  hc->bp = bp;
  QueInit(hc->next_mem_blk, hc->last_mem_blk);
  hc->next_um = hc->last_um = reinterpret_cast<CMemUsed *>(
      reinterpret_cast<uintptr_t>(&hc->next_um) - sizeof(void *));
  return hc;
}
