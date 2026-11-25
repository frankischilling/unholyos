#include "KernelB.hpp"

extern "C" void TestExcept(const CExcept *entry);
extern "C" [[noreturn]] void SysHlt();

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
