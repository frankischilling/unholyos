#include "DolDoc/DolDoc.hpp"
#include "Gr/GrGlbls.hpp"

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

static bool WinInside(const CTask &task, int64_t x, int64_t y)
{
  return task.pix_left <= x && x <= task.pix_right && task.pix_top <= y &&
         y <= task.pix_bottom;
}

static CTask *WinTaskAt(int64_t x, int64_t y)
{
  for (auto *task = sys_winmgr_task->last_task; task != sys_winmgr_task;
       task = task->last_task) {
    if (!TaskValidateCpp(task))
      break;
    if (!(task->display_flags & DISPLAYF_SHOW))
      continue;
    int64_t border = !(task->display_flags & DISPLAYF_NO_BORDER);
    auto col = x >> 3, row = y >> 3;
    if (col + border >= task->win_left && col - border <= task->win_right &&
        row + border >= task->win_top && row - border <= task->win_bottom)
      return task;
  }
  return nullptr;
}

static CDoc *WinCursorPosSet(CTask &task, int64_t x, int64_t y)
{
  auto *doc = DocDisplayCpp(&task);
  if (!doc || (doc->flags & DOCF_DONT_SHOW) || !DocCanLock(*doc) ||
      !WinInside(task, x, y))
    return nullptr;
  CDocLock lock(*doc);
  doc->x = (x - task.pix_left - task.scroll_x) / 8 + doc->line_start_col;
  doc->y = (y - task.pix_top - task.scroll_y) / 8 + doc->top_line_num;
  if (!DocRecalc(*doc, RECALCt_FIND_CURSOR))
    return nullptr;
  task.scroll_x = task.scroll_y = task.scroll_z = 0;
  WinToTopCpp(&task, true);
  return doc;
}

struct CWinDocMouse
{
  CTask *pressed_task;
  CDoc *pressed_doc;
  CDocEntryBase *pressed_entry;
  bool was_down;
};

static void WinDocMouse(bool left, CWinDocMouse &button)
{
  auto &[pressed_task, pressed_doc, pressed_entry, was_down] = button;
  bool down = left ? ms.lb : ms.rb;
  bool press = down && !was_down;
  was_down = down;
  auto x = ms.pos.x, y = ms.pos.y;

  if (pressed_task && !down && !(left ? ms.left_dbl_time : ms.right_dbl_time)) {
    auto *task = pressed_task;
    auto *doc = TaskValidateCpp(task) ? DocDisplayCpp(task) : nullptr;
    if (!doc || doc != pressed_doc) {
      pressed_task = nullptr;
      return;
    }
    if (!DocCanLock(*doc))
      return;
    pressed_task = nullptr;
    CDocLock lock(*doc);
    for (auto *entry = doc->head.next; entry != &doc->head;
         entry = entry->next) {
      if (entry != pressed_entry)
        continue;
      if (left)
        entry->de_flags &= ~DOCEF_SOLID_BORDER;
      if (!(doc->flags & DOCF_DONT_SHOW) && WinTaskAt(x, y) == task &&
          WinInside(*task, x, y) && doc->cur_entry == entry) {
        auto actions = left ? DOCEF_LEFT_EXP | DOCEF_LEFT_CB
                            : DOCEF_RIGHT_EXP | DOCEF_RIGHT_CB;
        if (left ? ms.left_dbl : ms.right_dbl)
          TaskMsgCpp(task, nullptr, MSG_KEY_DOWN, left ? CH_ESC : CH_SHIFT_ESC,
                     0, 0);
        else if (entry->de_flags & (actions | DOCEF_LINK))
          TaskMsgCpp(task, nullptr, MSG_KEY_DOWN, left ? ' ' : '\n', 0, 0);
      }
      break;
    }
  }
  if (!press || pressed_task)
    return;

  auto *task = WinTaskAt(x, y);
  auto inhibit = left ? WIF_SELF_MS_L : WIF_SELF_MS_R;
  auto focus_inhibit = left ? WIF_FOCUS_TASK_MS_L : WIF_FOCUS_TASK_MS_R;
  if (!task || (task->win_inhibit & inhibit) ||
      (TaskValidateCpp(sys_focus_task) &&
       (sys_focus_task->win_inhibit & focus_inhibit)))
    return;
  auto *doc = WinCursorPosSet(*task, x, y);
  if (!doc)
    return;
  CDocLock lock(*doc);
  auto *entry = doc->cur_entry;
  if (entry == &doc->head)
    return;
  if (left && (entry->de_flags & DOCEF_HAS_BORDER))
    entry->de_flags |= DOCEF_SOLID_BORDER;
  pressed_task = task;
  pressed_doc = doc;
  pressed_entry = entry;
}

extern "C" void WinMgrTaskEntry();

static uint64_t WinMgrSleep(uint64_t ideal)
{
  static CWinDocMouse left, right;
  WinRefocusCpp(sys_focus_task);
  WinMsUpdate();
  WinDocMouse(true, left);
  WinDocMouse(false, right);
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
  auto *task = sys_winmgr_task;
  if (!task)
    return false;
  task->win_left = task->win_top = 0;
  task->win_right = TEXT_COLS - 1;
  task->win_bottom = TEXT_ROWS - 1;
  WinDerivedValsUpdate(task);
  task->display_flags |= DISPLAYF_NO_BORDER | DISPLAYF_SHOW;
  gr.dc->win_task = task;
  return true;
}
