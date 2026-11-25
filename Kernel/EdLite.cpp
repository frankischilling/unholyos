#include "KernelB.hpp"

extern "C" void FixSetCpp(const char *filename, int64_t line)
{
  uint64_t args[]{reinterpret_cast<uintptr_t>(filename), uint64_t(line)};
  auto *link = MStrPrintCpp("FL:%s,%d", 2, args);
  if (!link)
    return;
  if (CurrentTask()->data_heap != adam_task->data_heap) {
    auto *copy = StrNew(link, adam_task->data_heap);
    Free(link);
    if (!copy)
      return;
    link = copy;
  }
  CMemLock lock(sys_semas[SEMA_FIX].flags);
  Free(dbg.fix_file_line);
  dbg.fix_file_line = link;
}
