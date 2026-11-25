#include "../KernelB.hpp"

static CMemBlk *MemPagPop(CMemBlk *&head)
{
  CMemBlk *blk = head;
  if (blk)
    head = blk->next;
  return blk;
}

static CMemBlk *MemPagTake(CBlkPool *bp, size_t pags)
{
  unsigned i = 9;
  if (pags < MEM_FREE_PAG_HASH_SIZE) {
    if (auto *blk = MemPagPop(bp->free_pag_hash[pags]))
      return blk;
  } else {
    i = 64 - __builtin_clzll(pags - MEM_EXTRA_HASH2_PAGS);
    pags = (uint64_t{1} << i) + MEM_EXTRA_HASH2_PAGS;
    if (auto *blk = MemPagPop(bp->free_pag_hash2[i]))
      return blk;
  }

  auto **link = &bp->mem_free_lst;
  while (auto *blk = *link) {
    if (blk->pags == pags)
      return MemPagPop(*link);
    if (blk->pags > pags) {
      blk->pags -= pags;
      auto *res =
          reinterpret_cast<CMemBlk *>(reinterpret_cast<uintptr_t>(blk) +
                                      (uint64_t{blk->pags} << MEM_PAG_BITS));
      res->pags = pags;
      return res;
    }
    link = &blk->next;
  }

  // Larger cached blocks are returned whole when the main list has no fit.
  while (++i < 64 - MEM_PAG_BITS)
    if (auto *blk = MemPagPop(bp->free_pag_hash2[i]))
      return blk;
  return nullptr;
}

static CMemBlk *MemPagAlloc(size_t pags, CBlkPool *bp)
{
  if (!pags || pags > UINT32_MAX - MEM_EXTRA_HASH2_PAGS)
    return nullptr;
  CMemLock lock(bp->locked_flags);
  auto *blk = MemPagTake(bp, pags);
  if (blk)
    bp->used_u8s += uint64_t{blk->pags} << MEM_PAG_BITS;
  return blk;
}

CMemBlk *MemPagTaskAlloc(size_t pags, CHeapCtrl *hc)
{
  auto *blk = MemPagAlloc(pags, hc->bp);
  if (!blk)
    return nullptr;

  auto *head = reinterpret_cast<CMemBlk *>(&hc->next_mem_blk);
  blk->last = hc->last_mem_blk;
  blk->next = head;
  if (blk->last == head)
    hc->next_mem_blk = blk;
  else
    blk->last->next = blk;
  hc->last_mem_blk = blk;
  blk->mb_signature = MBS_USED_SIGNATURE_VAL;
  hc->alloced_u8s += uint64_t{blk->pags} << MEM_PAG_BITS;

  size_t count, threshold = MEM_HEAP_HASH_SIZE >> 4;
  do {
    count = 0;
    auto **link = &hc->malloc_free_lst;
    while (auto *unused = *link) {
      if (static_cast<size_t>(unused->size) < threshold) {
        *link = unused->next;
        auto &slot = hc->heap_hash[unused->size / sizeof(void *)];
        unused->next = slot;
        slot = unused;
      } else {
        count++;
        link = &unused->next;
      }
    }
    threshold <<= 1;
  } while (count > 8 && threshold <= MEM_HEAP_HASH_SIZE);
  return blk;
}

void MemPagTaskUnlink(CMemBlk *blk, CHeapCtrl *hc)
{
  auto *head = reinterpret_cast<CMemBlk *>(&hc->next_mem_blk);
  if (blk->last == head)
    hc->next_mem_blk = blk->next;
  else
    blk->last->next = blk->next;
  if (blk->next == head)
    hc->last_mem_blk = blk->last;
  else
    blk->next->last = blk->last;
}

void MemPagTaskFree(CMemBlk *blk, CHeapCtrl *hc)
{
  if (blk->mb_signature != MBS_USED_SIGNATURE_VAL)
    SysBadFree(blk);

  MemPagTaskUnlink(blk, hc);
  uint64_t bytes = uint64_t{blk->pags} << MEM_PAG_BITS;
  hc->alloced_u8s -= bytes;

  auto *bp = hc->bp;
  CMemLock lock(bp->locked_flags);
  bp->used_u8s -= bytes;
  blk->mb_signature = MBS_UNUSED_SIGNATURE_VAL;
  CMemBlk **slot;
  if (blk->pags < MEM_FREE_PAG_HASH_SIZE)
    slot = &bp->free_pag_hash[blk->pags];
  else {
    unsigned i = 63 - __builtin_clzll(blk->pags - MEM_EXTRA_HASH2_PAGS);
    slot = &bp->free_pag_hash2[i];
  }
  blk->next = *slot;
  *slot = blk;
}

[[noreturn]] void SysBadFree(const void *ptr)
{
  SerialPutS("Bad Free: ");
  SerialPutHex(reinterpret_cast<uintptr_t>(ptr));
  SerialPutS("\r\n");
  for (;;)
    asm volatile("cli; hlt");
}
