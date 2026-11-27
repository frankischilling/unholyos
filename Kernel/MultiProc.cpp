#include "KernelB.hpp"

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
  if (!num) {
    cpu_structs = cpu;
    mp_cnt = 1;
  }
  return cpu;
}

bool Core0TaskInit()
{
  auto *cpu = CurrentCPU();
  cpu->idle_task = SpawnCpp(0, nullptr, "Idle Task", -1, adam_task, 0, 0);
  if (!cpu->idle_task)
    return false;
  cpu->idle_task->task_flags |= TASKF_IDLE;
  adam_task->gs = cpu;
  return true;
}
