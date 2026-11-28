#include "KernelB.hpp"
#include "../Compiler/CompilerA.hpp"

extern "C" void JobDelCpp(CJob *job)
{
  if (job) {
    Free(job->aux_str);
    Free(job);
  }
}

void JobQueDel(CJob *head)
{
  while (head->next != head) {
    auto *job = head->next;
    QueRem(*job);
    JobDelCpp(job);
  }
}

void JobCtrlInit(CJobCtrl *ctrl)
{
  QueInit(ctrl->next_waiting, ctrl->last_waiting);
  QueInit(ctrl->next_done, ctrl->last_done);
  ctrl->flags = 0;
}

extern "C" void TaskRstAwaitingMsgCpp(CTask *task)
{
  if (!task)
    task = CurrentTask();
  auto flags = GetRFlags();
  asm volatile("cli" : : : "memory");
  while (TaskValidateCpp(task)) {
    __atomic_fetch_and(&task->task_flags, ~TASKF_AWAITING_MSG, __ATOMIC_RELAXED);
    task = task->popup_task;
  }
  SetRFlags(flags);
}

static bool JobTaskValid(CTask *srv, CTask *master)
{
  return TaskValidateCpp(srv) && (!master || TaskValidateCpp(master)) &&
         (!srv->popup_task || (srv->task_flags & TASKF_FILTER_INPUT));
}

static CJob *JobNew(int64_t code, CTask *master, int64_t flags)
{
  auto *job = reinterpret_cast<CJob *>(CAlloc(sizeof(CJob), adam_task->data_heap));
  if (job) {
    job->job_code = code;
    job->master_task = master;
    job->flags = flags;
  }
  return job;
}

static bool JobPost(CJob *job, CTask *task)
{
  auto &ctrl = task->srv_ctrl;
  CMemLock lock(reinterpret_cast<uint64_t &>(ctrl.flags));
  if (!TaskValidateCpp(task))
    return false;
  job->ctrl = &ctrl;
  __atomic_fetch_and(&task->task_flags, ~TASKF_IDLE, __ATOMIC_RELAXED);
  TaskRstAwaitingMsgCpp(task);
  QueIns(*job, *ctrl.last_waiting);
  return true;
}

extern "C" CJob *TaskExeCpp(CTask *srv, CTask *master, const char *data, int64_t flags)
{
  if (!data || !JobTaskValid(srv, master))
    return nullptr;
  auto *job = JobNew(JOBT_EXE_STR, master, flags);
  if (!job)
    return nullptr;
  job->aux_str = reinterpret_cast<uint8_t *>(StrNew(data, adam_task->data_heap));
  if (!job->aux_str || !JobPost(job, srv)) {
    JobDelCpp(job);
    return nullptr;
  }
  if (flags & JOBF_WAKE_MASTER) {
    auto old_flags = GetRFlags();
    asm volatile("cli" : : : "memory");
    SuspendCpp(master, true);
    _YIELD();
    SetRFlags(old_flags);
  }
  return job;
}

extern "C" CJob *TaskTextCpp(CTask *srv, CTask *master, const char *data, int64_t flags)
{
  if (!data || !JobTaskValid(srv, master))
    return nullptr;
  auto *job = JobNew(JOBT_TEXT_INPUT, master, flags);
  if (!job)
    return nullptr;
  job->aux_str = reinterpret_cast<uint8_t *>(StrNew(data, adam_task->data_heap));
  if (!job->aux_str) {
    JobDelCpp(job);
    return nullptr;
  }

  auto old_flags = GetRFlags();
  auto *task = srv->last_input_filter_task;
  if ((flags & JOBF_HIGHEST_PRIORITY) || task == srv) {
    if (task != srv)
      TaskWaitCpp(srv, false);
    task = SpawnCpp(reinterpret_cast<uintptr_t>(InputFilterTaskHC), nullptr,
                    "Input Filter", -1, srv, 0, JOBF_ADD_TO_QUE);
    asm volatile("cli" : : : "memory");
    if (task) {
      task->next_input_filter_task = srv->next_input_filter_task;
      task->last_input_filter_task = srv;
      srv->next_input_filter_task = task;
      task->next_input_filter_task->last_input_filter_task = task;
    }
  } else
    task = srv->next_input_filter_task;
  bool posted = task && JobPost(job, task);
  SetRFlags(old_flags);
  if (!posted) {
    JobDelCpp(job);
    return nullptr;
  }
  return job;
}

extern "C" CJob *TaskMsgCpp(CTask *srv, CTask *master, int64_t msg_code,
                            int64_t arg1, int64_t arg2, int64_t flags)
{
  if (!JobTaskValid(srv, master))
    return nullptr;
  auto *job = JobNew(JOBT_MSG, master, flags);
  if (!job)
    return nullptr;
  job->msg_code = msg_code < 0 ? int64_t(uint64_t{0} - uint64_t(msg_code)) : msg_code;
  job->aux1 = arg1;
  job->aux2 = arg2;

  auto old_flags = GetRFlags();
  auto *original = srv;
  if ((__atomic_load_n(&sys_semas[SEMA_RECORD_MACRO].flags, __ATOMIC_RELAXED) & 1) &&
      srv != sys_macro_task && msg_code == MSG_KEY_DOWN) {
    auto *copy = reinterpret_cast<CJob *>(MAlloc(sizeof(CJob), adam_task->data_heap));
    if (copy) {
      MemCpy(copy, job, sizeof(CJob));
      asm volatile("cli" : : : "memory");
      QueIns(*copy, *sys_macro_head.last);
    }
  }
  asm volatile("cli" : : : "memory");
  while ((srv->task_flags & TASKF_FILTER_INPUT) && !(flags & JOBF_DONT_FILTER))
    srv = srv->next_input_filter_task;
  auto release = int64_t(uint64_t(job->msg_code) + 1);
  if (!JobPost(job, srv)) {
    JobDelCpp(job);
    job = nullptr;
  }
  SetRFlags(old_flags);
  if (msg_code < 0)
    TaskMsgCpp(original, master, release, arg1, arg2, flags);
  return job;
}

extern "C" uint64_t JobResScanCpp(CJob *rqst, int64_t *res)
{
  CJob *job = nullptr;
  if (!rqst || (__atomic_load_n(&rqst->flags, __ATOMIC_ACQUIRE) & JOBF_DONE)) {
    auto *ctrl = !rqst || rqst->master_task ? &CurrentTask()->srv_ctrl : rqst->ctrl;
    auto *head = reinterpret_cast<CJob *>(&ctrl->next_done);
    CMemLock lock(reinterpret_cast<uint64_t &>(ctrl->flags));
    job = ctrl->next_done;
    while (job != head && rqst && job != rqst)
      job = job->next;
    if (job == head)
      job = nullptr;
    else
      QueRem(*job);
  }
  if (res)
    *res = job ? job->res : 0;
  if (!job)
    return false;
  JobDelCpp(job);
  return true;
}

extern "C" int64_t JobResGetCpp(CJob *rqst)
{
  auto *task = CurrentTask();
  auto *head = reinterpret_cast<CJob *>(&task->srv_ctrl.next_done);
  while (rqst ? !(__atomic_load_n(&rqst->flags, __ATOMIC_ACQUIRE) & JOBF_DONE)
              : __atomic_load_n(&head->next, __ATOMIC_RELAXED) == head) {
    __atomic_fetch_or(&task->task_flags, TASKF_IDLE, __ATOMIC_RELAXED);
    _YIELD();
  }
  __atomic_fetch_and(&task->task_flags, ~TASKF_IDLE, __ATOMIC_RELAXED);
  int64_t res;
  JobResScanCpp(rqst, &res);
  return res;
}

extern "C" void TaskWaitCpp(CTask *task, uint8_t cmd_line_pmt)
{
  if (!task)
    task = CurrentTask();
  auto old_flags = GetRFlags();
  asm volatile("cli" : : : "memory");
  while (TaskValidateCpp(task)) {
    auto *filter = task->last_input_filter_task;
    if (filter == CurrentTask() || !TaskValidateCpp(filter) ||
        (filter->srv_ctrl.next_waiting ==
             reinterpret_cast<CJob *>(&filter->srv_ctrl.next_waiting) &&
         (filter->task_flags & TASKF_IDLE) &&
         (!cmd_line_pmt || (filter->task_flags & TASKF_CMD_LINE_PMT))))
      break;
    _YIELD();
  }
  SetRFlags(old_flags);
}

extern "C" void PostMsgCpp(CTask *task, int64_t msg_code, int64_t arg1,
                           int64_t arg2, int64_t flags)
{
  if (!TaskValidateCpp(task))
    return;
  if (task->task_flags & TASKF_INPUT_FILTER_TASK)
    TaskMsgCpp(task->last_input_filter_task, nullptr, msg_code, arg1, arg2,
               flags | JOBF_DONT_FILTER);
  else
    TaskMsgCpp(task, nullptr, msg_code, arg1, arg2, flags);
}

extern "C" void PostMsgWaitCpp(CTask *task, int64_t msg_code, int64_t arg1,
                               int64_t arg2, int64_t flags)
{
  PostMsgCpp(task, msg_code, arg1, arg2, flags);
  TaskWaitCpp(task, false);
}

extern "C" void MsgCpp(int64_t msg_code, int64_t arg1, int64_t arg2, int64_t flags)
{
  PostMsgCpp(CurrentTask(), msg_code, arg1, arg2, flags);
}

enum
{
  JOB_DONE, JOB_CONT, JOB_EXIT,
};

static int JobRunOne(uint64_t run_flags, CJobCtrl &ctrl)
{
  CJob *job;
  {
    CMemLock lock(reinterpret_cast<uint64_t &>(ctrl.flags));
    job = ctrl.next_waiting;
    if (job == reinterpret_cast<CJob *>(&ctrl.next_waiting) ||
        job->job_code != JOBT_EXE_STR)
      return JOB_DONE;
    QueRem(*job);
    job->flags |= JOBF_DISPATCHED;
  }

  auto flags = job->flags;
  auto old_flags = GetRFlags();
  SetRFlags(run_flags);
  __atomic_fetch_and(&CurrentTask()->task_flags, ~TASKF_IDLE, __ATOMIC_RELAXED);
  job->res = ExePutS(reinterpret_cast<const char *>(job->aux_str), nullptr, 0).value;
  SetRFlags(old_flags);

  auto *master = job->master_task;
  if (master && !TaskValidateCpp(master)) {
    JobDelCpp(job);
    return flags & JOBF_EXIT_ON_COMPLETE ? JOB_EXIT : JOB_CONT;
  }
  if (!(flags & JOBF_FREE_ON_COMPLETE)) {
    auto &done = master ? master->srv_ctrl : ctrl;
    CMemLock lock(reinterpret_cast<uint64_t &>(done.flags));
    QueIns(*job, *done.last_done);
    __atomic_fetch_or(&job->flags, JOBF_DONE, __ATOMIC_RELEASE);
  }
  if (master) {
    if ((flags & JOBF_FOCUS_MASTER) && !(master->win_inhibit & WIF_SELF_FOCUS))
      sys_focus_task = master;
    if (flags & JOBF_WAKE_MASTER)
      SuspendCpp(master, false);
  }
  if (flags & JOBF_FREE_ON_COMPLETE)
    JobDelCpp(job);
  return flags & JOBF_EXIT_ON_COMPLETE ? JOB_EXIT : JOB_CONT;
}

extern "C" int64_t JobsHndlrCpp(uint64_t run_flags, CTask *task)
{
  if (!task)
    task = CurrentTask();
  int64_t count = 0;
  auto old_flags = GetRFlags();
  asm volatile("cli" : : : "memory");
  for (;;) {
    auto action = JobRunOne(run_flags, task->srv_ctrl);
    if (action == JOB_DONE)
      break;
    if (action == JOB_EXIT)
      Exit();
    ++count;
  }
  SetRFlags(old_flags);
  return count;
}

extern "C" void SrvCmdLineHC();

extern "C" int64_t PopUpCpp(const char *src, CTask *parent, CTask **popup_task)
{
  if (popup_task)
    *popup_task = nullptr;
  if (!src)
    return 0;
  auto *task = SpawnCpp(reinterpret_cast<uintptr_t>(SrvCmdLineHC), nullptr,
                       "Servant", -1, parent, 0, JOBF_ADD_TO_QUE);
  if (!task)
    return 0;
  if (!parent) {
    if (TaskExeCpp(task, nullptr, src,
                   JOBF_EXIT_ON_COMPLETE | JOBF_FREE_ON_COMPLETE)) {
      if (popup_task)
        *popup_task = task;
    } else
      KillTask(task);
    return 0;
  }
  auto *caller = CurrentTask();
  caller->popup_task = task;
  auto *job = TaskExeCpp(task, parent, src, JOBF_WAKE_MASTER | JOBF_FOCUS_MASTER);
  if (popup_task)
    *popup_task = task;
  int64_t res = 0;
  if (job)
    JobResScanCpp(job, &res);
  caller->popup_task = nullptr;
  KillTask(task);
  if (popup_task)
    *popup_task = nullptr;
  return res;
}

extern "C" int64_t PopUpPrintCpp(const char *fmt, int64_t argc,
                                  const uint64_t *argv)
{
  auto *src = StrPrintJoin(nullptr, fmt, argc, argv);
  if (!src)
    return 0;
  auto res = PopUpCpp(src, CurrentTask(), nullptr);
  Free(src);
  return res;
}

int64_t AdamText(const char *src)
{
  auto *task = CurrentTask();
  auto *job = TaskExeCpp(adam_task, task, src, 0);
  if (!job)
    return 0;
  if (task == adam_task) {
    JobsHndlrCpp(GetRFlags(), task);
    int64_t res;
    JobResScanCpp(job, &res);
    return res;
  }
  return JobResGetCpp(job);
}

extern "C" int64_t AdamCpp(const char *fmt, int64_t argc, const uint64_t *argv)
{
  auto *src = StrPrintJoin(nullptr, fmt, argc, argv);
  if (!src)
    return 0;
  auto res = AdamText(src);
  Free(src);
  return res;
}

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

bool XTalk(CTask *task, const char *fmt, int64_t argc, const uint64_t *argv)
{
  auto *text = StrPrintJoin(nullptr, fmt, argc, argv);
  if (!text)
    return false;
  auto *shared = StrNew(text, adam_task->data_heap);
  Free(text);
  if (!shared)
    return false;
  const uint64_t addr = reinterpret_cast<uintptr_t>(shared);
  const uint64_t args[]{addr, addr};
  auto *source = MStrPrintCpp("\"%%s\",%d;Free(%d);", 2, args);
  bool posted = source && TaskTextCpp(task, nullptr, source, 0);
  Free(source);
  if (!posted)
    Free(shared);
  return posted;
}

extern "C" void XTalkCpp(CTask *task, const char *fmt, int64_t argc,
                         const uint64_t *argv)
{
  XTalk(task, fmt, argc, argv);
}
