#include "../KernelB.hpp"

constexpr uint64_t SYS_16MEG_AREA_LIMIT = 0x1000000;

static bool BlkPoolAdd(CBlkPool *bp, uintptr_t base, uint64_t pags)
{
  if (!pags || pags > UINT32_MAX)
    return false;
  CMemLock lock(bp->locked_flags);
  auto *blk = reinterpret_cast<CMemBlk *>(base);
  blk->next = bp->mem_free_lst;
  blk->pags = pags;
  blk->mb_signature = MBS_UNUSED_SIGNATURE_VAL;
  bp->alloced_u8s += pags << MEM_PAG_BITS;
  bp->mem_free_lst = blk;
  return true;
}

static bool BlkPoolInit(CBlkPool *bp, uint64_t pags)
{
  uintptr_t base = reinterpret_cast<uintptr_t>(bp);
  uintptr_t free_base =
      (base + sizeof(CBlkPool) + MEM_PAG_SIZE - 1) & -uintptr_t{MEM_PAG_SIZE};
  uint64_t reserved = (free_base - base) >> MEM_PAG_BITS;
  if (pags <= reserved)
    return false;

  MemSet(bp, 0, sizeof(CBlkPool));
  bp->alloced_u8s = reserved << MEM_PAG_BITS;
  return BlkPoolAdd(bp, free_base, pags - reserved);
}

bool BlkPoolsInit(CTask *task)
{
  uint64_t total = BIOSTotalMem();
  uint64_t code_limit = total;
  if (total > 0x100000000)
    code_limit = 0x80000000;
  else if (total > 0x80000000)
    code_limit = total / 4;

  if (code_limit <= SYS_16MEG_AREA_LIMIT ||
      !BlkPoolAdd(SYS_CODE_BP, SYS_16MEG_AREA_LIMIT,
                  (code_limit - SYS_16MEG_AREA_LIMIT) >> MEM_PAG_BITS))
    return false;
  MEM_HEAP_LIMIT = code_limit - 1;

  if (code_limit < total) {
    for (const auto &range : MEM_E820) {
      if (!range.type)
        break;
      if (range.type != 1)
        continue;

      uint64_t lo = range.base < code_limit ? code_limit : range.base;
      uint64_t hi = range.base + range.len;
      if (lo >= hi)
        continue;

      uint64_t pags = (hi - lo) >> MEM_PAG_BITS;
      if (!SYS_DATA_BP) {
        auto *pool = reinterpret_cast<CBlkPool *>(lo);
        if (!BlkPoolInit(pool, pags))
          return false;
        auto *hc = HeapCtrlInit(task, pool);
        if (!hc)
          return false;
        SYS_DATA_BP = pool;
        task->data_heap = hc;
      } else if (!BlkPoolAdd(SYS_DATA_BP, lo, pags)) {
        return false;
      }
    }
  }
  SYS_RUN_LEVEL |= RLF_FULL_HEAPS;
  return true;
}
