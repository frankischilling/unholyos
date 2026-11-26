#include "KernelB.hpp"

extern "C" [[noreturn]] void SysHlt();

bool ChkPtr(const void *ptr)
{
  auto addr = reinterpret_cast<uintptr_t>(ptr);
  if (addr >= MEM_HEAP_BASE && addr < MEM_MAPPED_SPACE)
    return (*MemPageTable(addr) & 1) != 0;
  // Core zero and Adam retain their real records in the fixed boot area.
  if (addr >= SYS_FIXED_AREA && addr < SYS_FIXED_AREA + sizeof(CSysFixedArea))
    return true;
  return MEM_BOOT_BASE <= addr && addr < 0xA0000;
}

bool ChkPtrRange(const void *ptr, size_t size)
{
  auto addr = reinterpret_cast<uintptr_t>(ptr);
  if (!ChkPtr(ptr) || size > UINTPTR_MAX - addr)
    return false;
  if (!size)
    return true;
  auto last = addr + size - 1;
  if (addr < MEM_HEAP_BASE)
    return addr < 0xA0000 ? last < 0xA0000 :
           last < SYS_FIXED_AREA + sizeof(CSysFixedArea);
  if (last >= MEM_MAPPED_SPACE)
    return false;
  for (auto page = addr & ~(MEM_PAGE_SIZE - 1);;) {
    if (!(*MemPageTable(page) & 1))
      return false;
    if (last - page < MEM_PAGE_SIZE)
      return true;
    page += MEM_PAGE_SIZE;
  }
}

extern "C" uint64_t ChkPtrCpp(const void *ptr)
{
  return ChkPtr(ptr);
}

uint8_t *Caller(const uintptr_t *frame, int64_t depth, const CTask &task)
{
  if (!task.stk || task.stk->stk_size < 16 || depth < 0)
    return nullptr;
  uintptr_t base = reinterpret_cast<uintptr_t>(task.stk + 1);
  uintptr_t last = base + task.stk->stk_size - 2 * sizeof(uintptr_t);
  auto addr = reinterpret_cast<uintptr_t>(frame);
  while (base <= addr && addr <= last) {
    uintptr_t words[2];
    MemCpy(words, reinterpret_cast<const void *>(addr), sizeof(words));
    if (!depth--)
      return reinterpret_cast<uint8_t *>(words[1]);
    if (words[0] <= addr)
      break;
    addr = words[0];
  }
  return nullptr;
}

extern "C" uint8_t *CallerCpp(const uintptr_t *frame, int64_t depth)
{
  return Caller(frame, depth, *CurrentTask());
}

extern "C" [[noreturn]] void BootFault(const CTask *task)
{
  SerialPutS("\r\nUnholyOS: fault ");
  SerialPutHex(task->fault_num);
  SerialPutS(", error ");
  SerialPutHex(task->fault_err_code);
  SerialPutS(", RIP ");
  SerialPutHex(task->rip);
  SerialPutS("\r\n");
  SysHlt();
}
