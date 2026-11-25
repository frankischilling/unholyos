#include "KernelB.hpp"

extern "C" [[noreturn]] void SysHlt();

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
