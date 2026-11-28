#include "../Adam/DolDoc/DolDoc.hpp"
#include "../Compiler/CompilerA.hpp"

extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);
void UserStartUp();
void SrvStartUp();

bool CmpCall(CCmpCtrl &cc, const uint8_t *machine, uint64_t &result)
{
  if (CallHolyC0Checked(machine, &result))
    return true;
  cc.jit_exception = CurrentTask()->except_ch;
  return false;
}

bool ExeCmdLine(CCmpCtrl &cc, uint64_t &result)
{
  result = 0;
  while (cc.token != TK_EOF &&
         (cc.token != '}' || !(cc.flags & CCF_EXE_BLK))) {
    uint8_t *machine;
    CRawType type;
    if (!LexStmt2Bin(cc, machine, type))
      return false;
    if (cc.jit_exception) {
      Free(machine);
      return false;
    }
    if (machine) {
      bool okay = true;
      if (!(cc.flags & CCF_JUST_LOAD)) {
        const auto start = cc.flags & CCF_CMD_LINE ? GetTSC() : 0;
        okay = CmpCall(cc, machine, result);
        if (okay) {
          auto *task = CurrentTask();
          task->answer = static_cast<int64_t>(result);
          task->answer_type = type;
          if (start && cnts.time_stamp_freq > 0) {
            auto elapsed = F64DivBits(F64FromI64(GetTSC() - start),
                                      F64FromI64(cnts.time_stamp_freq));
            MemCpy(&task->answer_time, &elapsed, sizeof(elapsed));
          }
          task->new_answer = true;
        }
      }
      if (!(cc.flags & CCF_HAS_MISC_DATA))
        Free(machine);
      if (!okay)
        return false;
    }
  }
  if (cc.flags & CCF_JUST_LOAD)
    result = 1;
  return !cc.jit_exception;
}

static void UserTaskCont()
{
  auto *task = CurrentTask();
  for (;;) {
    const auto result = ExePutS(nullptr, nullptr, CCF_CMD_LINE | CCF_PMT);
    if (result.okay)
      break;
    auto *doc = DocPut();
    if (!doc)
      break;
    DocUnlock(*doc);
    DocBottomCpp(doc);
    if (result.exception) {
      uint64_t tag = result.exception;
      PrintCpp("Except:%c\n", 1, &tag);
    } else
      PutS("Syntax Error\n");
    task->new_answer = false;
  }
}

extern "C" void UserCmdLineCpp(int64_t)
{
  auto *task = CurrentTask();
  task->win_inhibit = WIG_USER_TASK_DFT;
  UserStartUp();
  if (DocPut())
    UserTaskCont();
}

extern "C" [[noreturn]] void SrvTaskContCpp()
{
  auto *task = CurrentTask();
  auto flags = GetRFlags();
  FlushMsgsCpp(task);
  for (;;) {
    asm volatile("cli" : : : "memory");
    if (JobsHndlrCpp(flags, task) && task->title_src == TTS_TASK_NAME)
      MemCpy(task->task_title, task->task_name, sizeof(task->task_name));
    FlushMsgsCpp(task);
    __atomic_fetch_or(&task->task_flags, TASKF_IDLE | TASKF_AWAITING_MSG,
                      __ATOMIC_RELAXED);
    _YIELD();
    SetRFlags(flags);
  }
}

extern "C" [[noreturn]] void SrvCmdLineCpp(int64_t)
{
  CurrentTask()->win_inhibit = WIG_USER_TASK_DFT;
  SrvStartUp();
  SrvTaskContCpp();
}

static int64_t sys_num_spawned_tasks;

static CFPU *TaskFPUInit(CHeapCtrl *hc, const CFPU *initial)
{
  auto *fpu = reinterpret_cast<CFPU *>(MAllocAligned(sizeof(CFPU), 16, hc));
  if (!fpu)
    return nullptr;
  MemCpy(fpu, initial, sizeof(CFPU));
  return fpu;
}

static CTaskStk *TaskStkNew(size_t size, CHeapCtrl *hc)
{
  if (size > SIZE_MAX - sizeof(CTaskStk))
    return nullptr;
  auto *ptr = MAlloc(size + sizeof(CTaskStk), hc);
  if (!ptr)
    return nullptr;
  auto *stk = reinterpret_cast<CTaskStk *>(ptr);
  stk->next_stk = nullptr;
  stk->stk_ptr = 0;
  stk->stk_size = MSize(ptr) - sizeof(CTaskStk);
  return stk;
}

extern "C" CTask *TaskInitCpp(CTask *task, size_t stk_size)
{
  auto *area = reinterpret_cast<CSysFixedArea *>(SYS_FIXED_AREA);
  auto *hc = task->code_heap;
  QueInit(hc->next_mem_blk, hc->last_mem_blk);
  hc->last_mergable = nullptr;
  if (task->data_heap != hc) {
    QueInit(task->data_heap->next_mem_blk, task->data_heap->last_mem_blk);
    task->data_heap->last_mergable = nullptr;
  }

  task->addr = task->next_task = task->last_task =
      task->next_input_filter_task = task->last_input_filter_task = task;
  task->task_num = sys_num_spawned_tasks++;
  task->rflags = 0x200;
  task->win_inhibit = WIG_TASK_DFT;
  task->next_child_task = task->last_child_task = reinterpret_cast<CTask *>(
      reinterpret_cast<uintptr_t>(&task->next_child_task) -
      offsetof(CTask, next_sibling_task));

  JobCtrlInit(&task->srv_ctrl);
  QueInit(task->next_cc, task->last_cc);
  QueInit(task->next_except, task->last_except);
  QueInit(task->next_ctrl, task->last_ctrl);
  QueInit(task->next_ode, task->last_ode);

  auto *data = task->data_heap;
  if (!(task->fpu_mmx = TaskFPUInit(data, &area->init_fpu_mmx)))
    return nullptr;
  if (!(task->hash_table = HashTableNew(1024, data)))
    return nullptr;
  if (!(task->stk = TaskStkNew(stk_size ? stk_size : MEM_DFT_STK, data)))
    return nullptr;
  task->rsp = reinterpret_cast<uintptr_t>(task->stk + 1) + task->stk->stk_size;

  // This task starts before the display and drive tables are initialized.
  task->text_attr = 0xF1;
  task->border_attr = 0x0F;
  task->border_src = task->title_src = 0;
  task->win_left = 1;
  task->win_top = 13;
  task->win_right = task->win_bottom = -2;

  const char *home = blkdev.home_dir;
  if (home)
    task->cur_dv = PathDrive(*task, *home);
  task->cur_dir = StrNew(home ? home + 2 : HOME_DIR, data);
  if (!task->cur_dir)
    return nullptr;

  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  task->rand_seed ^= (uint64_t{hi} << 32) | lo;
  return task;
}

extern "C" uint64_t TaskValidateCpp(CTask *task)
{
  auto addr = reinterpret_cast<uintptr_t>(task);
  return addr && addr <= INT32_MAX && task->addr == task &&
         task->task_signature == TASK_SIGNATURE_VAL;
}

bool KillTask(CTask *task, bool wait)
{
  if (!TaskValidateCpp(task) || task == sys_winmgr_task ||
      task == task->gs->seth_task)
    return false;
  __atomic_fetch_or(&task->task_flags, TASKF_KILL_TASK, __ATOMIC_RELAXED);
  if (wait) {
    do {
      _YIELD();
    } while (TaskValidateCpp(task) &&
             (__atomic_load_n(&task->task_flags, __ATOMIC_RELAXED) &
              TASKF_KILL_TASK));
  }
  return true;
}

extern "C" uint64_t KillCpp(CTask *task, uint8_t wait, uint8_t just_break)
{
  if (!TaskValidateCpp(task))
    return false;
  if (!just_break)
    return KillTask(task, wait);
  // The reference Break branch acts on the caller when the supplied task
  // differs from Fs. For Fs it schedules the Ctrl-Alt-C path instead.
  if (task != CurrentTask()) {
    BreakCpp();
    return false;
  }
  sys_focus_task = task;
  __atomic_fetch_or(&sys_ctrl_alt_flags, 1U << 1, __ATOMIC_RELAXED);
  return true;
}

extern "C" uint64_t SuspendCpp(CTask *task, uint8_t state)
{
  if (!task)
    task = CurrentTask();
  if (!TaskValidateCpp(task))
    return false;
  auto old = state
      ? __atomic_fetch_or(&task->task_flags, TASKF_SUSPENDED, __ATOMIC_RELAXED)
      : __atomic_fetch_and(&task->task_flags, ~TASKF_SUSPENDED, __ATOMIC_RELAXED);
  return (old & TASKF_SUSPENDED) != 0;
}

extern "C" uint64_t IsSuspendedCpp(CTask *task)
{
  if (!task)
    task = CurrentTask();
  return TaskValidateCpp(task) &&
         (__atomic_load_n(&task->task_flags, __ATOMIC_RELAXED) & TASKF_SUSPENDED);
}

static void TaskDel(CTask *task)
{
  auto *code = task->code_heap;
  auto *data = task->data_heap;
  HeapCtrlDel(code);
  if (data != code)
    HeapCtrlDel(data);
  Free(task);
}

extern "C" CTask *SpawnCpp(uintptr_t fp_start_addr, uint8_t *data,
                          const char *task_name, int64_t target_cpu,
                          CTask *parent, int64_t stk_size, uint64_t flags)
{
  auto *cpu = CurrentCPU();
  if (target_cpu > cpu->num || stk_size < 0 ||
      (!fp_start_addr && (flags & JOBF_ADD_TO_QUE)))
    return nullptr;
  if (!parent)
    parent = cpu->seth_task;
  if (!TaskValidateCpp(parent))
    return nullptr;
  auto *task = reinterpret_cast<CTask *>(CAlloc(sizeof(CTask), adam_task->code_heap));
  if (!task)
    return nullptr;
  task->task_signature = TASK_SIGNATURE_VAL;
  task->parent_task = parent;
  task->gs = cpu;
  task->code_heap = HeapCtrlInit(task, SYS_CODE_BP);
  task->data_heap = SYS_DATA_BP ? HeapCtrlInit(task, SYS_DATA_BP) : task->code_heap;
  if (!task->code_heap || !task->data_heap || !TaskInitCpp(task, stk_size)) {
    TaskDel(task);
    return nullptr;
  }

  task->rip = fp_start_addr;
  auto *rsp = reinterpret_cast<uintptr_t *>(task->rsp);
  *--rsp = reinterpret_cast<uintptr_t>(data);
  *--rsp = reinterpret_cast<uintptr_t>(Exit);
  task->rsp = reinterpret_cast<uintptr_t>(rsp);
  task->hash_table->next = parent->hash_table;

  if (!task_name)
    task_name = "Unnamed Task";
  size_t len = 0;
  while (len < sizeof(task->task_name) && task_name[len])
    ++len;
  MemCpy(task->task_name, task_name, len);
  MemCpy(task->task_title, task->task_name, len);
  task->title_src = TTS_TASK_NAME;

  if (flags & JOBF_ADD_TO_QUE) {
    uint64_t rflags;
    asm volatile("pushfq; popq %0; cli" : "=r"(rflags) : : "memory");
    TaskQueInsChild(task);
    TaskQueInsCpp(task, nullptr);
    asm volatile("pushq %0; popfq" : : "r"(rflags) : "cc", "memory");
  }
  return task;
}

extern "C" CTask *TaskEnd()
{
  auto *task = CurrentTask();
  auto *next = task->next_task;
  if (task == sys_task_being_scrn_updated) {
    __atomic_fetch_or(&task->task_flags, TASKF_KILL_TASK, __ATOMIC_RELAXED);
    return next;
  }
  if (task->task_end_cb) {
    task->wake_jiffy = 0;
    __atomic_fetch_and(&task->task_flags, ~TASKF_KILL_TASK, __ATOMIC_RELAXED);
    TaskRstAwaitingMsgCpp(task);
    SuspendCpp(task, false);
    task->rip = reinterpret_cast<uintptr_t>(task->task_end_cb);
    task->task_end_cb = nullptr;
    return task;
  }

  auto *parent = task->parent_task;
  if (parent && parent->popup_task == task) {
    parent->popup_task = nullptr;
    KillTask(parent, false);
    return parent;
  }

  DrvsReleaseCpp();
  BlkDevsReleaseCpp();
  auto *child_head = reinterpret_cast<CTask *>(
      reinterpret_cast<uintptr_t>(&task->next_child_task) -
      offsetof(CTask, next_sibling_task));
  if (task->next_child_task != child_head) {
    for (auto *child = task->next_child_task; child != child_head;
         child = child->next_sibling_task)
      child->task_flags |= TASKF_KILL_TASK;
    return next;
  }

  if (__atomic_fetch_and(&task->display_flags, ~DISPLAYF_SHOW,
                         __ATOMIC_RELAXED) &
      DISPLAYF_SHOW)
    __atomic_fetch_or(&sys_semas[SEMA_UPDATE_WIN_Z_BUF].flags, 1,
                      __ATOMIC_RELAXED);

  if (sys_focus_task == task) {
    SingleUserCpp(false);
    sys_focus_task = nullptr;
  }
  JobQueDel(reinterpret_cast<CJob *>(&task->srv_ctrl.next_waiting));
  JobQueDel(reinterpret_cast<CJob *>(&task->srv_ctrl.next_done));
  task->task_signature = 0;
  task->last_input_filter_task->next_input_filter_task = task->next_input_filter_task;
  task->next_input_filter_task->last_input_filter_task = task->last_input_filter_task;
  task->last_sibling_task->next_sibling_task = task->next_sibling_task;
  task->next_sibling_task->last_sibling_task = task->last_sibling_task;
  TaskQueRemCpp(task);

  task->wake_jiffy = __atomic_load_n(&cnts.jiffies, __ATOMIC_RELAXED) + DYING_JIFFIES;
  auto *cpu = CurrentCPU();
  QueIns(*reinterpret_cast<CTaskDying *>(task), *cpu->last_dying);
  return next;
}

extern "C" void TaskKillDying()
{
  auto *cpu = CurrentCPU();
  auto now = int64_t(__atomic_load_n(&cnts.jiffies, __ATOMIC_RELAXED));
  if (cpu->kill_jiffy >= now)
    return;
  auto *head = reinterpret_cast<CTaskDying *>(&cpu->next_dying);
  while (cpu->next_dying != head && cpu->next_dying->wake_jiffy < now) {
    auto *task = cpu->next_dying;
    QueRem(*task);
    TaskDel(reinterpret_cast<CTask *>(task));
  }
  cpu->kill_jiffy = now + DYING_JIFFIES;
}

extern "C" [[noreturn]] void TaskInitFailed()
{
  SerialPutS("UnholyOS: Out of memory starting Adam\r\n");
  for (;;)
    asm volatile("hlt");
}
