#include "../KernelB.hpp"

CHeapCtrl *MemTaskHeap(void *mem_task)
{
  if (!mem_task)
    mem_task = CurrentTask();
  static_assert(offsetof(CTask, task_signature) ==
                offsetof(CHeapCtrl, hc_signature));
  uint32_t signature;
  MemCpy(&signature,
         static_cast<uint8_t *>(mem_task) + offsetof(CTask, task_signature),
         sizeof(signature));
  if (signature == TASK_SIGNATURE_VAL)
    return static_cast<CTask *>(mem_task)->data_heap;
  return static_cast<CHeapCtrl *>(mem_task);
}

static CMemUnused *MAllocSmallChunk(size_t size, CHeapCtrl *hc)
{
  auto *blk =
      MemPagTaskAlloc((size + 16 * MEM_PAG_SIZE - 1) >> MEM_PAG_BITS, hc);
  if (!blk)
    return nullptr;

  size_t bytes = uint64_t{blk->pags} << MEM_PAG_BITS;
  auto *merge = hc->last_mergable;
  if (reinterpret_cast<uintptr_t>(blk) + bytes ==
      reinterpret_cast<uintptr_t>(merge)) {
    blk->pags += merge->pags;
    MemPagTaskUnlink(merge, hc);
  }
  hc->last_mergable = blk;

  auto *unused = reinterpret_cast<CMemUnused *>(blk + 1);
  unused->size = bytes - sizeof(CMemBlk);
  unused->next = hc->malloc_free_lst;
  hc->malloc_free_lst = unused;
  return unused;
}

static CMemUsed *MAllocSmall(size_t size, CHeapCtrl *hc)
{
  auto &slot = hc->heap_hash[size / sizeof(void *)];
  if (auto *unused = slot) {
    slot = unused->next;
    return reinterpret_cast<CMemUsed *>(unused);
  }

  auto **link = &hc->malloc_free_lst;
  while (auto *unused = *link) {
    size_t available = unused->size;
    if (available == size) {
      *link = unused->next;
      return reinterpret_cast<CMemUsed *>(unused);
    }
    if (available >= size + sizeof(CMemUnused)) {
      unused->size -= size;
      return reinterpret_cast<CMemUsed *>(reinterpret_cast<uintptr_t>(unused) +
                                          unused->size);
    }
    link = &unused->next;
  }

  auto *unused = MAllocSmallChunk(size, hc);
  if (!unused)
    return nullptr;
  unused->size -= size;
  return reinterpret_cast<CMemUsed *>(reinterpret_cast<uintptr_t>(unused) +
                                      unused->size);
}

uint8_t *MAlloc(size_t size, CHeapCtrl *hc)
{
  if (!hc || hc->hc_signature != HEAP_CTRL_SIGNATURE_VAL ||
      size > SIZE_MAX - sizeof(CMemUsed) - sizeof(CMemBlk) - MEM_PAG_SIZE - 7)
    return nullptr;
  size = (size + sizeof(CMemUsed) + 7) & ~size_t{7};

  CMemLock lock(hc->locked_flags);
  CMemUsed *used;
  if (size >= MEM_HEAP_HASH_SIZE) {
    auto *blk = MemPagTaskAlloc(
        (size + sizeof(CMemBlk) + MEM_PAG_SIZE - 1) >> MEM_PAG_BITS, hc);
    if (!blk)
      return nullptr;
    used = reinterpret_cast<CMemUsed *>(blk + 1);
    size = (uint64_t{blk->pags} << MEM_PAG_BITS) - sizeof(CMemBlk);
  } else if (!(used = MAllocSmall(size, hc))) {
    return nullptr;
  }

  hc->used_u8s += size;
  used->size = size;
  used->hc = hc;
  return reinterpret_cast<uint8_t *>(used + 1);
}

uint8_t *CAlloc(size_t size, CHeapCtrl *hc)
{
  auto *ptr = MAlloc(size, hc);
  if (ptr)
    MemSet(ptr, 0, size);
  return ptr;
}

uint8_t *MAllocAligned(size_t size, size_t alignment, CHeapCtrl *hc,
                       size_t misalignment)
{
  if (!alignment || (alignment & (alignment - 1)) || misalignment >= alignment)
    return nullptr;
  size_t mask = alignment - 1;
  if (mask > SIZE_MAX - sizeof(int64_t) - misalignment)
    return nullptr;
  size_t extra = mask + sizeof(int64_t) + misalignment;
  if (extra > INT64_MAX || size > SIZE_MAX - extra)
    return nullptr;
  auto *ptr = MAlloc(size + extra, hc);
  if (!ptr)
    return nullptr;
  uintptr_t base = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t addr = ((base + sizeof(int64_t) + mask) & ~mask) + misalignment;
  int64_t offset = -static_cast<int64_t>(addr - base);
  MemCpy(reinterpret_cast<void *>(addr - sizeof(offset)), &offset,
         sizeof(offset));
  return reinterpret_cast<uint8_t *>(addr);
}

static CMemUsed *MemUsed(const void *ptr)
{
  if (!ptr)
    return nullptr;

  auto addr = reinterpret_cast<uintptr_t>(ptr);
  int64_t size_field;
  MemCpy(&size_field, reinterpret_cast<void *>(addr - sizeof(size_field)),
         sizeof(size_field));
  if (size_field < 0)
    addr += size_field;
  return reinterpret_cast<CMemUsed *>(addr - sizeof(CMemUsed));
}

size_t MSize(const void *ptr)
{
  return ptr ? MSize2(ptr) - sizeof(CMemUsed) : 0;
}

size_t MSize2(const void *ptr)
{
  auto *used = MemUsed(ptr);
  return used ? used->size : 0;
}

void Free(void *ptr)
{
  auto *used = MemUsed(ptr);
  if (!used)
    return;
  auto *hc = used->hc;
  if (!hc || hc->hc_signature != HEAP_CTRL_SIGNATURE_VAL)
    SysBadFree(ptr);

  CMemLock lock(hc->locked_flags);
  size_t size = used->size;
  hc->used_u8s -= size;
  if (size < MEM_HEAP_HASH_SIZE) {
    auto *unused = reinterpret_cast<CMemUnused *>(used);
    auto &slot = hc->heap_hash[size / sizeof(void *)];
    unused->next = slot;
    slot = unused;
  } else {
    auto *blk = reinterpret_cast<CMemBlk *>(reinterpret_cast<uintptr_t>(used) -
                                            sizeof(CMemBlk));
    MemPagTaskFree(blk, hc);
  }
}

extern "C" uint8_t *MAllocCpp(size_t size, void *mem_task)
{
  return MAlloc(size, MemTaskHeap(mem_task));
}

extern "C" uint8_t *CAllocCpp(size_t size, void *mem_task)
{
  return CAlloc(size, MemTaskHeap(mem_task));
}

extern "C" void FreeCpp(void *ptr)
{
  Free(ptr);
}

extern "C" size_t MSizeCpp(const void *ptr)
{
  return MSize(ptr);
}

extern "C" size_t MSize2Cpp(const void *ptr)
{
  return MSize2(ptr);
}

extern "C" CHeapCtrl *MHeapCtrlCpp(const void *ptr)
{
  auto *used = MemUsed(ptr);
  return used ? used->hc : nullptr;
}

extern "C" uint8_t *MAllocIdentCpp(const void *src, void *mem_task)
{
  if (!src)
    return nullptr;
  size_t size = MSize(src);
  auto *ptr = MAlloc(size, MemTaskHeap(mem_task));
  if (ptr)
    MemCpy(ptr, src, size);
  return ptr;
}

char *StrNew(const char *src, CHeapCtrl *hc)
{
  if (!src)
    src = "";
  size_t size = StrLen(src) + 1;
  auto *ptr = MAlloc(size, hc);
  if (ptr)
    MemCpy(ptr, src, size);
  return reinterpret_cast<char *>(ptr);
}

extern "C" char *StrNewCpp(const char *src, void *mem_task)
{
  return StrNew(src, MemTaskHeap(mem_task));
}

extern "C" char *AStrNewCpp(const char *src)
{
  return StrNew(src, adam_task->data_heap);
}
