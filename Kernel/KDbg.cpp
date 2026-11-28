#include "KernelB.hpp"

extern "C" [[noreturn]] void SysHlt();
extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);

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

extern "C" void DCpp(const uint8_t *addr, int64_t cnt, uint8_t show_offset)
{
  auto *ptr = addr;
  while (cnt) {
    const uint64_t address = reinterpret_cast<uintptr_t>(ptr);
    const uint64_t pos = show_offset ? address-reinterpret_cast<uintptr_t>(addr)
                                     : address;
    PrintCpp(show_offset ? "%08X " : "%010X ", 1, &pos);
    const int64_t n = cnt > 16 ? 16 : cnt;
    int64_t i = 0;
    for (; i<n; i++) {
      const uint64_t byte = ptr[i];
      PrintCpp("%02X ", 1, &byte);
    }
    for (; i<16; i++)
      PutS("   ");
    for (i=0; i<n; i++) {
      auto ch = ptr[i];
      if (ch < CH_SHIFT_SPACE || ch == CH_BACKSPACE)
        ch = '.';
      PutCharsCpp(ch);
      if (ch == '$')
        PutCharsCpp(ch);
    }
    PutCharsCpp('\n');
    if (cnt <= 16)
      break;
    cnt -= 16;
    ptr += 16;
  }
}

extern "C" void DmCpp(const uint8_t *addr, int64_t cnt)
{
  DCpp(addr, cnt, false);
}

extern "C" void DaCpp(const uint8_t *const *addr, int64_t cnt)
{
  for (; cnt>0; cnt--, addr++) {
    const auto value = reinterpret_cast<uintptr_t>(*addr);
    const uint64_t args[]{reinterpret_cast<uintptr_t>(addr), value, value};
    PrintCpp("%08X:%08X,%P\n", 3, args);
  }
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
