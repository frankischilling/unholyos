#include "../Kernel/KernelB.hpp"

static int64_t WinQueButton(CTask *task, bool que, bool left, uint64_t time,
                            int64_t x, int64_t y, CD3I64 &single_ms)
{
  bool down = left ? ms.lb : ms.rb;
  bool was_down = left ? ms_last.lb : ms_last.rb;
  auto &deadline = left ? ms.left_dbl_time : ms.right_dbl_time;
  auto &dbl = left ? ms.left_dbl : ms.right_dbl;
  auto &sent = left ? ms.left_down_sent : ms.right_down_sent;
  auto code = left ? MSG_MS_L_DOWN : MSG_MS_R_DOWN;
  int64_t msg_code = MSG_NULL;
  auto post = [&](int64_t msg, int64_t px, int64_t py) {
    if (que)
      TaskMsgCpp(task, nullptr, msg, px, py, 0);
    msg_code = msg;
  };
  if (deadline) {
    if (time > deadline || (deadline >> 63)) {
      if (!sent) {
        auto sx = single_ms.x - task->pix_left - task->scroll_x;
        auto sy = single_ms.y - task->pix_top - task->scroll_y;
        post(code + (dbl ? 2 : 0), dbl ? x : sx, dbl ? y : sy);
        sent = true;
      }
      if (!down) {
        post(code + (dbl ? 3 : 1), x, y);
        deadline = 0;
      }
    } else if (down && !was_down) {
      deadline = time;
      dbl = true;
    }
  } else if (task->win_inhibit & (left ? WIF_FOCUS_TASK_MS_L_D :
                                        WIF_FOCUS_TASK_MS_R_D)) {
    if (down != was_down)
      post(code + !down, x, y);
  } else if (down && !was_down) {
    dbl = sent = false;
    deadline = F64AddBits(time, ms.dbl_time);
    single_ms.x = ms.pos.x;
    single_ms.y = ms.pos.y;
  }
  return msg_code;
}

int64_t WinQueIPMsgs(bool que)
{
  static CD3I64 single_ms;
  auto *task = sys_focus_task;
  if (!TaskValidateCpp(task))
    return MSG_NULL;
  bool moved = old_ms.presnap.x != ms.presnap.x || old_ms.presnap.y != ms.presnap.y;
  if (!moved && ms.lb == ms_last.lb && ms.rb == ms_last.rb &&
      !ms.left_dbl_time && !ms.right_dbl_time)
    return MSG_NULL;

  auto x = ms.pos.x - task->pix_left - task->scroll_x;
  auto y = ms.pos.y - task->pix_top - task->scroll_y;
  if (moved && que)
    TaskMsgCpp(task, nullptr, MSG_MS_MOVE, x, y, 0);
  auto time = tSCpp();
  auto left = WinQueButton(task, que, true, time, x, y, single_ms);
  auto right = WinQueButton(task, que, false, time, x, y, single_ms);
  MemCpy(&ms_last, &ms, sizeof(ms));
  MemCpy(&old_ms, &ms, sizeof(ms));
  return right ? right : left ? left : moved ? MSG_MS_MOVE : MSG_NULL;
}

extern "C" void WinMgrTaskEntry();

static uint64_t WinMgrSleep(uint64_t ideal)
{
  // WINMGR_PERIOD is 1001/30000 seconds. Keep 1/30-jiffy units here.
  auto now = __atomic_load_n(&cnts.jiffies, __ATOMIC_RELAXED) * 30 + 1001 / 8;
  if (ideal < now)
    ideal += ((now - ideal + 1000) / 1001) * 1001;
  auto &refresh = sys_semas[SEMA_REFRESH_IN_PROGRESS].flags;
  __atomic_fetch_or(&refresh, 1, __ATOMIC_RELAXED);
  GrUpdateScrn();
  __atomic_fetch_and(&refresh, ~uint64_t{1}, __ATOMIC_RELAXED);
  SleepUntilCpp(ideal / 30);
  return ideal;
}

extern "C" [[noreturn]] void WinMgrTask(int64_t)
{
  auto ideal = __atomic_load_n(&cnts.jiffies, __ATOMIC_RELAXED) * 30;
  for (;;)
    ideal = WinMgrSleep(ideal);
}

bool WinMgrInit()
{
  sys_winmgr_task =
      SpawnCpp(reinterpret_cast<uintptr_t>(WinMgrTaskEntry), nullptr,
               "Window Manager", 0, adam_task, 0, JOBF_ADD_TO_QUE);
  return sys_winmgr_task != nullptr;
}
