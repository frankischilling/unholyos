#include "../KernelB.hpp"
#include "../../Compiler/CompilerA.hpp"

extern "C" void InputFilterTaskCpp()
{
  auto *task = CurrentTask();
  auto old_flags = GetRFlags();
  auto *parent = task->last_input_filter_task;
  task->win_inhibit = WIF_SELF_KEY_DESC;
  task->task_flags |= TASKF_INPUT_FILTER_TASK;
  bool old_filter = parent->task_flags & TASKF_FILTER_INPUT;
  parent->task_flags |= TASKF_FILTER_INPUT;
  if (old_filter)
    task->task_flags |= TASKF_FILTER_INPUT;
  auto *head = reinterpret_cast<CJob *>(&task->srv_ctrl.next_waiting);
  for (;;) {
    asm volatile("cli" : : : "memory");
    JobsHndlrCpp(old_flags, task);
    auto *job = head->next;
    if (job == head || job->job_code != JOBT_TEXT_INPUT)
      break;
    QueRem(*job);
    SetRFlags(old_flags);
    ExePutS(reinterpret_cast<const char *>(job->aux_str), nullptr, 0);
    JobDelCpp(job);
  }
  task->next_input_filter_task->last_input_filter_task = task->last_input_filter_task;
  task->last_input_filter_task->next_input_filter_task = task->next_input_filter_task;
  if (!old_filter)
    task->last_input_filter_task->task_flags &= ~TASKF_FILTER_INPUT;
  task->next_input_filter_task = task->last_input_filter_task = task;
  SetRFlags(old_flags);
}

extern "C" int64_t ScanMsgCpp(int64_t *arg1, int64_t *arg2, uint64_t mask,
                              CTask *task)
{
  if (!task)
    task = CurrentTask();
  auto old_flags = GetRFlags();
  while (TaskValidateCpp(task)) {
    auto *head = reinterpret_cast<CJob *>(&task->srv_ctrl.next_waiting);
    for (;;) {
      asm volatile("cli" : : : "memory");
      if (task == CurrentTask())
        JobsHndlrCpp(old_flags, task);
      auto *job = head->next;
      if (job == head || job->job_code != JOBT_MSG)
        break;
      QueRem(*job);
      SetRFlags(old_flags);
      auto code = job->msg_code;
      auto b = job->aux2;
      if (arg1)
        *arg1 = job->aux1;
      if (arg2)
        *arg2 = b;
      JobDelCpp(job);
      if ((mask & (uint64_t{1} << (uint64_t(code) & 63))) &&
          (code != MSG_KEY_DOWN || !(uint64_t(b) & SCF_KEY_DESC) ||
           !(task->win_inhibit & WIF_SELF_KEY_DESC)))
        return code;
    }
    auto *parent = task->parent_task;
    if (!parent || parent->popup_task != task)
      break;
    task = parent;
  }
  SetRFlags(old_flags);
  if (arg1)
    *arg1 = 0;
  if (arg2)
    *arg2 = 0;
  return MSG_NULL;
}

extern "C" int64_t FlushMsgsCpp(CTask *task)
{
  int64_t count = 0;
  while (ScanMsgCpp(nullptr, nullptr, ~uint64_t{1}, task))
    ++count;
  return count;
}

extern "C" int64_t GetMsgCpp(int64_t *arg1, int64_t *arg2, uint64_t mask,
                             CTask *task)
{
  if (!task)
    task = CurrentTask();
  __atomic_fetch_and(&task->task_flags, ~TASKF_IDLE, __ATOMIC_RELAXED);
  int64_t code;
  while (!(code = ScanMsgCpp(arg1, arg2, mask, task))) {
    __atomic_fetch_or(&task->task_flags, TASKF_IDLE, __ATOMIC_RELAXED);
    _YIELD();
  }
  __atomic_fetch_and(&task->task_flags, ~TASKF_IDLE, __ATOMIC_RELAXED);
  return code;
}
