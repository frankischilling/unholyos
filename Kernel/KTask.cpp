#include "KernelB.hpp"
#include "../Compiler/CompilerA.hpp"

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
        okay = CmpCall(cc, machine, result);
        if (okay) {
          auto *task = CurrentTask();
          task->answer = static_cast<int64_t>(result);
          task->answer_type = type;
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
  task->win_inhibit = 0x52F3FF;
  task->next_child_task = task->last_child_task = reinterpret_cast<CTask *>(
      reinterpret_cast<uintptr_t>(&task->next_child_task) -
      offsetof(CTask, next_sibling_task));

  auto &srv = task->srv_ctrl;
  QueInit(srv.next_waiting, srv.last_waiting);
  QueInit(srv.next_done, srv.last_done);
  srv.flags = 0;
  QueInit(task->next_cc, task->last_cc);
  QueInit(task->next_except, task->last_except);
  QueInit(task->next_ctrl, task->last_ctrl);
  QueInit(task->next_ode, task->last_ode);

  auto *data = task->data_heap;
  if (!(task->fpu_mmx = TaskFPUInit(data, &area->init_fpu_mmx)))
    return nullptr;
  if (!(task->hash_table = HashTableNew(1024, data)))
    return nullptr;
  if (!(task->stk = TaskStkNew(stk_size, data)))
    return nullptr;
  task->rsp = reinterpret_cast<uintptr_t>(task->stk + 1) + task->stk->stk_size;

  // This task starts before the display and drive tables are initialized.
  task->text_attr = 0xF1;
  task->border_attr = 0x0F;
  task->border_src = task->title_src = 0;
  task->win_left = 1;
  task->win_top = 13;
  task->win_right = task->win_bottom = -2;

  task->cur_dir = reinterpret_cast<char *>(MAlloc(sizeof(HOME_DIR), data));
  if (!task->cur_dir)
    return nullptr;
  MemCpy(task->cur_dir, HOME_DIR, sizeof(HOME_DIR));

  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  task->rand_seed ^= (uint64_t{hi} << 32) | lo;
  return task;
}

extern "C" [[noreturn]] void TaskInitFailed()
{
  SerialPutS("UnholyOS: Out of memory starting Adam\r\n");
  for (;;)
    asm volatile("hlt");
}
