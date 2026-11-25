#include "KernelB.hpp"

extern "C" void AdamErrCpp(const char *fmt, int64_t argc, const uint64_t *argv)
{
  auto *task = CurrentTask();
  if (task != adam_task &&
      (__atomic_load_n(&sys_semas[SEMA_SINGLE_USER].flags, __ATOMIC_RELAXED) &
       1))
    return;
  auto *text = StrPrintJoin(nullptr, fmt, argc, argv);
  uint64_t arg = reinterpret_cast<uintptr_t>(text);
  auto *message = MStrPrintCpp("$LTRED$$BK,1$ERROR:$FG$$BK,0$ %s", 1, &arg);
  Free(text);
  PutTaskS(*adam_task, message);
  Free(message);
}
