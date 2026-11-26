#include "KernelB.hpp"

extern "C" void TaskQueInsCpp(CTask *task, CTask *pred)
{
  uint64_t rflags;
  asm volatile("pushfq; popq %0; cli" : "=r"(rflags) : : "memory");
  if (!pred)
    pred = CurrentTask();
  auto *last = pred->last_task;
  last->next_task = pred->last_task = task;
  task->last_task = last;
  task->next_task = pred;
  if (task->display_flags & DISPLAYF_SHOW)
    __atomic_fetch_or(&sys_semas[SEMA_UPDATE_WIN_Z_BUF].flags, 1, __ATOMIC_RELAXED);
  asm volatile("pushq %0; popfq" : : "r"(rflags) : "cc", "memory");
}

extern "C" void TaskQueRemCpp(CTask *task)
{
  uint64_t rflags;
  asm volatile("pushfq; popq %0; cli" : "=r"(rflags) : : "memory");
  auto *next = task->next_task;
  auto *last = task->last_task;
  last->next_task = next;
  next->last_task = last;
  if (task->display_flags & DISPLAYF_SHOW)
    __atomic_fetch_or(&sys_semas[SEMA_UPDATE_WIN_Z_BUF].flags, 1, __ATOMIC_RELAXED);
  asm volatile("pushq %0; popfq" : : "r"(rflags) : "cc", "memory");
}

void TaskQueInsChild(CTask *task)
{
  uint64_t rflags;
  asm volatile("pushfq; popq %0; cli" : "=r"(rflags) : : "memory");
  auto *pred = task->parent_task->last_child_task;
  auto *last = pred->last_sibling_task;
  last->next_sibling_task = pred->last_sibling_task = task;
  task->last_sibling_task = last;
  task->next_sibling_task = pred;
  asm volatile("pushq %0; popfq" : : "r"(rflags) : "cc", "memory");
}
