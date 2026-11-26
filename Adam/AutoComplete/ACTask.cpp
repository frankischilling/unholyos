#include "../../Kernel/KernelB.hpp"

extern "C" uint64_t AutoCompleteCpp(uint8_t val)
{
  while (__atomic_load_n(&ac.flags, __ATOMIC_RELAXED) & ACF_INIT_IN_PROGRESS)
    SleepCpp(10);
  auto *task = ac.task;
  if (!TaskValidateCpp(task))
    return false;

  bool old_val = SYS_RUN_LEVEL & RLF_AUTO_COMPLETE;
  if (!val) {
    KillTask(task);
    if (ac.task == task && !TaskValidateCpp(task))
      ac.task = nullptr;
  } else if (old_val)
    WinToTopCpp(task, true);
  return old_val;
}
