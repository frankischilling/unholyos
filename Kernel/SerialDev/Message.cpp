#include "../KernelB.hpp"
#include "../../Compiler/CompilerA.hpp"
#include "../../Adam/DolDoc/DolDoc.hpp"

extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);

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
  KbdMsService();
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

extern "C" int64_t ScanCharCpp()
{
  int64_t down, up;
  if (!ScanMsgCpp(&down, nullptr, 1 << MSG_KEY_DOWN, nullptr) || !down)
    return 0;
  do
    GetMsgCpp(&up, nullptr, 1 << MSG_KEY_UP, nullptr);
  while (!up);
  return down;
}

extern "C" uint64_t ScanKeyCpp(int64_t *ch, int64_t *scan_code, uint8_t echo)
{
  int64_t c, sc;
  bool found = ScanMsgCpp(&c, &sc, 1 << MSG_KEY_DOWN, nullptr);
  if (ch)
    *ch = c;
  if (scan_code)
    *scan_code = sc;
  if (found && echo)
    PutKeyCpp(c, sc);
  return found;
}

extern "C" int64_t GetKeyCpp(int64_t *scan_code, uint8_t echo, uint8_t raw_cursor)
{
  auto *task = CurrentTask();
  auto *head = reinterpret_cast<CJob *>(&task->srv_ctrl.next_waiting);
  int64_t ch, sc;
  bool cursor_on = false;
  while (!ScanKeyCpp(&ch, &sc, false)) {
    if (raw_cursor && IsRawCpp() && cnts.time_stamp_freq) {
      bool cursor = (GetTSC() * 5 / uint64_t(cnts.time_stamp_freq)) & 1;
      if (cursor != cursor_on) {
        PutKeyCpp(cursor ? CH_CURSOR : CH_BACKSPACE, 0);
        cursor_on = cursor;
      }
    }
    __atomic_fetch_or(&task->task_flags, TASKF_IDLE, __ATOMIC_RELAXED);
    if (IsDbgModeCpp()) {
      KbdPoll();
    } else {
      auto flags = GetRFlags();
      asm volatile("cli" : : : "memory");
      KbdMsService();
      if (head->next == head)
        __atomic_fetch_or(&task->task_flags, TASKF_AWAITING_MSG, __ATOMIC_RELAXED);
      _YIELD();
      SetRFlags(flags);
    }
    __atomic_fetch_and(&task->task_flags, ~TASKF_IDLE, __ATOMIC_RELAXED);
  }
  if (cursor_on)
    PutKeyCpp(CH_BACKSPACE, 0);
  if (echo)
    PutKeyCpp(ch, sc);
  if (scan_code)
    *scan_code = sc;
  return ch;
}

extern "C" int64_t GetCharCpp(int64_t *scan_code, uint8_t echo, uint8_t raw_cursor)
{
  int64_t ch;
  do
    ch = GetKeyCpp(scan_code, false, raw_cursor);
  while (!ch);
  if (echo)
    PutKeyCpp(ch, 0);
  return ch;
}

extern "C" int64_t GetSCpp(char *buf, int64_t size, uint8_t allow_ext)
{
  if (!buf || size <= 0)
    return 0;
  int64_t len = 0;
  if (allow_ext) {
    auto *st = GetStrCpp(nullptr, nullptr, 0);
    if (st) {
      len = StrLen(st);
      if (len >= size)
        len = size - 1;
      MemCpy(buf, st, len);
      Free(st);
    }
  } else {
    while (true) {
      const auto ch = GetCharCpp(nullptr, false, IsDbgModeCpp());
      if (ch == '\n') {
        PutCharsCpp(ch);
        break;
      }
      if (ch == CH_BACKSPACE) {
        if (len) {
          --len;
          PutCharsCpp(ch);
        }
      } else if (len < size - 1) {
        buf[len++] = ch;
        PutCharsCpp(ch);
      }
    }
  }
  buf[len] = 0;
  return len;
}

extern "C" char *GetStrCpp(const char *msg, const char *dft, int64_t flags)
{
  auto *task = CurrentTask();
  if (msg) {
    uint64_t arg = reinterpret_cast<uintptr_t>(dft);
    PrintCpp(msg, 1, &arg);
  }
  char *st;
  if (!IsRawCpp() && DocPut())
    st = DocGetStr2(flags);
  else {
    char buf[512];
    GetSCpp(buf, sizeof(buf), false);
    st = StrNew(buf, task->data_heap);
  }
  if (st && !*st && dft) {
    Free(st);
    st = StrNew(dft, task->data_heap);
  }
  return st;
}
