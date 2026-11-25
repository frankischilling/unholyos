#include "KernelA.hpp"

extern "C" CCPU *CPUStructInitCpp(int64_t num, CCPU *cpu, CTask *seth_task)
{
  // SYS_INIT_PAGE_TABLES clears the fixed area before Core 0 starts.
  cpu->addr = cpu;
  cpu->num = num;
  // The pinned HolyC build rounded 0.01 to these bits.
  cpu->idle_factor = 0x1.47AE147AE147Cp-7;
  cpu->next_dying = cpu->last_dying =
      reinterpret_cast<CTaskDying *>(&cpu->next_dying);
  cpu->seth_task = seth_task;
  return cpu;
}
