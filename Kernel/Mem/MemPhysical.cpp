#include "../KernelB.hpp"

constexpr uint32_t MRT_UNUSED = 0;

static void Mem32LinkBefore(CMemRange *range, CMemRange *next)
{
  range->next = next;
  range->last = next->last;
  next->last->next = range;
  next->last = range;
}

enum class CMem32Insert
{
  Rejected,
  Inserted,
  OutOfMemory
};

static CMem32Insert Mem32DevIns(CMemRange *range, CHeapCtrl *hc)
{
  auto *head = &dev.mem32_head;
  for (auto *free = head->next; free != head; free = free->next) {
    if (free->type != MRT_UNUSED || range->base < free->base)
      continue;
    uintptr_t offset = range->base - free->base;
    if (offset > free->size || range->size > free->size - offset)
      continue;

    if (offset) {
      auto *prefix =
          reinterpret_cast<CMemRange *>(MAlloc(sizeof(CMemRange), hc));
      if (!prefix)
        return CMem32Insert::OutOfMemory;
      prefix->type = MRT_UNUSED;
      prefix->flags = 0;
      prefix->base = free->base;
      prefix->size = offset;
      Mem32LinkBefore(prefix, free);
    }

    Mem32LinkBefore(range, free);
    free->base = range->base + range->size;
    free->size -= offset + range->size;
    if (!free->size) {
      free->last->next = free->next;
      free->next->last = free->last;
      Free(free);
    }
    return CMem32Insert::Inserted;
  }
  return CMem32Insert::Rejected;
}

bool Mem32DevInit(CHeapCtrl *hc)
{
  auto *head = &dev.mem32_head;
  QueInit(head->next, head->last);
  auto *range = reinterpret_cast<CMemRange *>(MAlloc(sizeof(CMemRange), hc));
  if (!range)
    return false;
  range->type = MRT_UNUSED;
  range->flags = 0;
  range->base = 0xE0000000;
  range->size = 0x10000000;
  Mem32LinkBefore(range, head);

  for (const auto &entry : MEM_E820) {
    if (!entry.type)
      break;
    range = reinterpret_cast<CMemRange *>(MAlloc(sizeof(CMemRange), hc));
    if (!range)
      return false;
    range->type = entry.type;
    range->flags = 0;
    range->base = entry.base;
    range->size = entry.len;
    auto result = Mem32DevIns(range, hc);
    if (result != CMem32Insert::Inserted)
      Free(range);
    if (result == CMem32Insert::OutOfMemory)
      return false;
  }
  return true;
}

uint64_t BIOSTotalMem()
{
  uint64_t e801 =
      0x100000 + (uint64_t{MEM_E801[0]} << 10) + (uint64_t{MEM_E801[1]} << 16);
  uint64_t e820 = 0;
  for (const auto &range : MEM_E820) {
    if (!range.type)
      break;
    if (range.type == 1)
      e820 += range.len;
  }
  return e801 > e820 ? e801 : e820;
}

static void InvlPg(uintptr_t addr)
{
  asm volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

static uintptr_t Mem64DevAlloc(unsigned gigs)
{
  constexpr uintptr_t gig = uintptr_t{1} << 30;
  for (unsigned i = 0; i < gigs; i++) {
    uintptr_t base = (dev.mem64_ptr -= gig);
    for (uintptr_t offset = 0; offset < gig; offset += MEM_PAGE_SIZE) {
      auto *pte = MemPageTable(base + offset);
      *pte = (*pte & ~uint64_t{0x18}) | 0x11;
      InvlPg(base);
    }
  }
  return dev.mem64_ptr;
}

void UncachedAliasAlloc()
{
  constexpr uintptr_t bytes = uintptr_t{1} << 32;
  dev.uncached_alias = Mem64DevAlloc(4);
  for (uintptr_t offset = 0; offset < bytes; offset += MEM_PAGE_SIZE) {
    uintptr_t addr = dev.uncached_alias + offset;
    *MemPageTable(addr) = offset | 0x197;
    InvlPg(addr);
  }
}
