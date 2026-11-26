#include "KernelB.hpp"

extern "C" void TestExcept(const CExcept *entry);
extern "C" [[noreturn]] void SysHlt();
extern "C" void BreakHC();
extern "C" void PostMsgCpp(CTask *, int64_t, int64_t, int64_t, int64_t);

static CExcept *ExceptQueue(CTask &task)
{
  return reinterpret_cast<CExcept *>(&task.next_except);
}

extern "C" bool SysTryCpp(const CExcept *saved)
{
  auto &task = *CurrentTask();
  auto *entry =
      reinterpret_cast<CExcept *>(MAlloc(sizeof(CExcept), task.data_heap));
  if (!entry)
    return false;
  constexpr size_t state = offsetof(CExcept, hndlr_catch);
  MemCpy(&entry->hndlr_catch, &saved->hndlr_catch, sizeof(CExcept) - state);
  auto *head = ExceptQueue(task);
  entry->last = task.last_except;
  entry->next = head;
  if (entry->last == head)
    task.next_except = entry;
  else
    entry->last->next = entry;
  task.last_except = entry;
  return true;
}

extern "C" void SysUntryCpp()
{
  auto &task = *CurrentTask();
  auto *entry = task.last_except;
  auto *head = ExceptQueue(task);
  if (entry == head)
    return;
  task.last_except = entry->last;
  if (entry->last == head)
    task.next_except = head;
  else
    entry->last->next = head;
  Free(entry);
}

static void ReportException(const CTask &task)
{
  char tag[9]{};
  MemCpy(tag, &task.except_ch, sizeof(task.except_ch));
  SerialPutS("Except:");
  SerialPutS(tag);
  SerialPutS(":");
  for (auto *caller : task.except_callers) {
    SerialPutHex(reinterpret_cast<uintptr_t>(caller));
    SerialPutS(":");
  }
  SerialPutS("\r\n");
}

extern "C" void PutExceptCpp(uint8_t catch_it)
{
  auto &task = *CurrentTask();
  ReportException(task);
  task.catch_except = catch_it != 0;
}

[[noreturn]] void RethrowHolyC()
{
  auto &task = *CurrentTask();
  task.catch_except = false;
  const auto *head = ExceptQueue(task);
  while (task.next_except != head) {
    TestExcept(task.last_except);
    SysUntryCpp();
  }
  ReportException(task);
  SerialPutS("Unhandled Exception\r\n");
  SysHlt();
}

extern "C" [[noreturn]] void ThrowCpp(int64_t tag, uint8_t no_log,
                                      const uintptr_t *frame)
{
  auto &task = *CurrentTask();
  task.except_ch = tag;
  for (size_t i = 0;
       i < sizeof(task.except_callers) / sizeof(*task.except_callers); i++)
    task.except_callers[i] = Caller(frame, i, task);
  task.except_rbp = reinterpret_cast<uintptr_t>(frame);
  if (!no_log)
    ReportException(task);
  RethrowHolyC();
}

extern "C" void BreakCpp()
{
  auto *task = CurrentTask();
  if (task->task_flags & TASKF_BREAK_TO_SHIFT_ESC) {
    MsgCpp(MSG_KEY_DOWN, CH_SHIFT_ESC, 0x20100000201, 0);
    return;
  }
  task->wake_jiffy = 0;
  TaskRstAwaitingMsgCpp(task);
  // Drive and block locks in this kernel never yield while held.
  FlushMsgsCpp(task);
  RaiseHolyC(0x6B61657242); // 'Break'
}

static void BreakTask(CTask *task)
{
  if (task == CurrentTask()) {
    BreakCpp();
    return;
  }
  if (task->task_flags & TASKF_BREAK_TO_SHIFT_ESC) {
    PostMsgCpp(task, MSG_KEY_DOWN, CH_SHIFT_ESC, 0x20100000201, 0);
    return;
  }
  task->rip = reinterpret_cast<uintptr_t>(BreakHC);
  task->wake_jiffy = 0;
  TaskRstAwaitingMsgCpp(task);
}

extern "C" CTask *BreakFocusUser(CTask *next)
{
  if (!(__atomic_fetch_and(&sys_ctrl_alt_flags, ~(1U << 1), __ATOMIC_RELAXED) &
        (1U << 1)))
    return next;
  auto *task = sys_focus_task;
  if (!TaskValidateCpp(task) || (task->win_inhibit & WIF_SELF_FOCUS))
    return next;
  if (__atomic_fetch_and(&task->task_flags, ~TASKF_BREAK_LOCKED, __ATOMIC_RELAXED) &
      TASKF_BREAK_LOCKED) {
    __atomic_fetch_or(&task->task_flags, TASKF_PENDING_BREAK, __ATOMIC_RELAXED);
    return next;
  }
  // The scheduler has already saved the running task, even if it is focused.
  if (task->task_flags & TASKF_BREAK_TO_SHIFT_ESC) {
    PostMsgCpp(task, MSG_KEY_DOWN, CH_SHIFT_ESC, 0x20100000201, 0);
    return next;
  }
  task->rip = reinterpret_cast<uintptr_t>(BreakHC);
  task->wake_jiffy = 0;
  TaskRstAwaitingMsgCpp(task);
  return task;
}

extern "C" bool BreakLockCpp(CTask *task)
{
  if (!task)
    task = CurrentTask();
  return !(__atomic_fetch_or(&task->task_flags, TASKF_BREAK_LOCKED, __ATOMIC_RELAXED) &
           TASKF_BREAK_LOCKED);
}

extern "C" bool BreakUnlockCpp(CTask *task)
{
  if (!task)
    task = CurrentTask();
  auto old = __atomic_fetch_and(&task->task_flags,
      ~(TASKF_BREAK_LOCKED | TASKF_PENDING_BREAK), __ATOMIC_RELAXED);
  if (old & TASKF_PENDING_BREAK)
    BreakTask(task);
  return (old & TASKF_BREAK_LOCKED) != 0;
}
