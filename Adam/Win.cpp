#include "../Kernel/KernelB.hpp"
#include "Gr/GrGlbls.hpp"

#include <bit>

CWinMgrGlbls winmgr{0, 0, 0, std::bit_cast<uint64_t>(30000.0 / 1001),
                   0, 0, nullptr, false, false, false};

extern "C" CTask *WinRefocusCpp(CTask *task)
{
  auto flags = GetRFlags();
  asm volatile("cli" : : : "memory");
  if (!task && sys_winmgr_task)
    for (task = sys_winmgr_task->last_task;
         task != sys_winmgr_task && TaskValidateCpp(task);
         task = task->last_task)
      if (!(task->win_inhibit & WIF_SELF_FOCUS)) {
        sys_focus_task = task;
        break;
      }
  SetRFlags(flags);
  return sys_focus_task;
}

static int64_t WinOnTopWindows()
{
  auto flags = GetRFlags();
  asm volatile("cli" : : : "memory");
  int64_t res = 0;
  CTask *first_moved_fwd = nullptr;
  for (auto *task = sys_winmgr_task->next_task;
       task != sys_winmgr_task && task != first_moved_fwd &&
       TaskValidateCpp(task);) {
    auto *next = task->next_task;
    if ((task->display_flags & DISPLAYF_WIN_ON_TOP) &&
        task != sys_winmgr_task->last_task) {
      TaskQueRemCpp(task);
      TaskQueInsCpp(task, sys_winmgr_task);
      ++res;
      if (!first_moved_fwd)
        first_moved_fwd = task;
    }
    task = next;
  }
  SetRFlags(flags);
  return res;
}

extern "C" int64_t WinToTopCpp(CTask *task, uint8_t update_z_buf)
{
  if (!task)
    task = CurrentTask();
  if (!TaskValidateCpp(task) || task->gs->num)
    return 0;
  WinDerivedValsUpdate(task);
  if (!sys_winmgr_task || task == sys_winmgr_task)
    return 0;
  auto flags = GetRFlags();
  asm volatile("cli" : : : "memory");
  int64_t res = 0;
  if (task != sys_winmgr_task->last_task) {
    TaskQueRemCpp(task);
    TaskQueInsCpp(task, sys_winmgr_task);
    ++res;
  }
  if (!(task->win_inhibit & WIF_SELF_FOCUS))
    sys_focus_task = task;
  if (res && !(task->display_flags & DISPLAYF_CHILDREN_NOT_ON_TOP)) {
    auto *head = reinterpret_cast<CTask *>(
        reinterpret_cast<uintptr_t>(&task->next_child_task) -
        offsetof(CTask, next_sibling_task));
    for (auto *child = task->next_child_task;
         child != head && TaskValidateCpp(child);
         child = child->next_sibling_task)
      res += WinToTopCpp(child, false);
    if (task->popup_task && task->popup_task->parent_task == task)
      res += WinToTopCpp(task->popup_task, false);
  }
  SetRFlags(flags);
  res += WinOnTopWindows();
  if (res && update_z_buf)
    WinZBufUpdateCpp();
  return res;
}

extern "C" CTask *WinFocusCpp(CTask *task)
{
  if (!task)
    task = CurrentTask();
  auto flags = GetRFlags();
  asm volatile("cli" : : : "memory");
  if (!TaskValidateCpp(task) || (task->win_inhibit & WIF_SELF_FOCUS))
    task = WinRefocusCpp(sys_focus_task);
  if (task) {
    sys_focus_task = task;
    WinToTopCpp(task, true);
  }
  SetRFlags(flags);
  return sys_focus_task;
}

extern "C" bool WinHorzCpp(int64_t left, int64_t right, CTask *task)
{
  auto width = static_cast<int64_t>(uint64_t(right) - left);
  if (!task)
    task = CurrentTask();
  if (!TaskValidateCpp(task))
    return false;
  if (width < 0)
    width = 0;
  if (left >= TEXT_COLS) {
    left = TEXT_COLS - 1;
    right = uint64_t(left) + width;
  }
  if (right < 0) {
    right = 0;
    left = -width;
  }
  if (left > right) {
    if (left > 0)
      right = left;
    else
      left = right;
  }
  auto flags = GetRFlags();
  asm volatile("cli" : : : "memory");
  bool changed = task->win_left != left || task->win_right != right;
  if (changed) {
    task->win_left = left;
    task->win_right = right;
    WinDerivedValsUpdate(task);
    if (task->display_flags & DISPLAYF_SHOW)
      __atomic_fetch_or(&sys_semas[SEMA_UPDATE_WIN_Z_BUF].flags, 1,
                        __ATOMIC_RELAXED);
  }
  SetRFlags(flags);
  return changed;
}

extern "C" bool WinVertCpp(int64_t top, int64_t bottom, CTask *task)
{
  auto height = static_cast<int64_t>(uint64_t(bottom) - top);
  if (!task)
    task = CurrentTask();
  if (!TaskValidateCpp(task))
    return false;
  if (height < 0)
    height = 0;
  if (top >= TEXT_ROWS) {
    top = TEXT_ROWS - 1;
    bottom = uint64_t(top) + height;
  }
  if (bottom <= 0) {
    bottom = 1;
    top = bottom - height;
  }
  if (top > bottom) {
    if (top >= 0)
      bottom = top;
    else
      top = bottom;
  }
  auto flags = GetRFlags();
  asm volatile("cli" : : : "memory");
  bool changed = task->win_top != top || task->win_bottom != bottom;
  if (changed) {
    task->win_top = top;
    task->win_bottom = bottom;
    WinDerivedValsUpdate(task);
    if (task->display_flags & DISPLAYF_SHOW)
      __atomic_fetch_or(&sys_semas[SEMA_UPDATE_WIN_Z_BUF].flags, 1,
                        __ATOMIC_RELAXED);
  }
  SetRFlags(flags);
  return changed;
}

static void WinTile(bool vert)
{
  if (!sys_winmgr_task)
    return;
  auto flags = GetRFlags();
  asm volatile("cli" : : : "memory");
  auto *task = sys_winmgr_task;
  int64_t cnt = 0;
  do {
    if (!(task->win_inhibit & WIF_SELF_FOCUS))
      ++cnt;
    task = task->last_task;
  } while (task != sys_winmgr_task);

  int64_t i = 0;
  CTask *last_task = nullptr;
  do {
    if (!(task->win_inhibit & WIF_SELF_FOCUS)) {
      int64_t no_border = !!(task->display_flags & DISPLAYF_NO_BORDER);
      int64_t c = cnt - (i & ~3);
      if (c > 4)
        c = 4;
      if (vert) {
        auto horz_size = TEXT_COLS / c;
        WinHorzCpp((i & 3) * horz_size + 1 - no_border,
                   (i & 3) == 3 ? TEXT_COLS - 2 :
                                  ((i & 3) + 1) * horz_size - 1 + no_border,
                   task);
        WinVertCpp(2 - no_border, TEXT_ROWS - 2 + no_border, task);
      } else {
        auto vert_size = (TEXT_ROWS - 1) / c;
        WinHorzCpp(1 - no_border, TEXT_COLS - 2 + no_border, task);
        WinVertCpp((i & 3) * vert_size + 2 - no_border,
                   (i & 3) == 3 ? TEXT_ROWS - 2 :
                                  ((i & 3) + 1) * vert_size + no_border,
                   task);
      }
      last_task = task;
      ++i;
    }
    task = task->last_task;
  } while (task != sys_winmgr_task);
  if (last_task) {
    if (vert)
      WinHorzCpp(last_task->win_left, TEXT_COLS - 2, last_task);
    else
      WinVertCpp(last_task->win_top, TEXT_ROWS - 2, last_task);
  }
  SetRFlags(flags);
}

extern "C" void WinTileHorzCpp()
{
  WinTile(false);
}

extern "C" void WinTileVertCpp()
{
  WinTile(true);
}

extern "C" void WinMaxCpp(CTask *task)
{
  if (!task)
    task = CurrentTask();
  if (!TaskValidateCpp(task))
    return;
  auto flags = GetRFlags();
  asm volatile("cli" : : : "memory");
  int64_t no_border = !!(task->display_flags & DISPLAYF_NO_BORDER);
  WinHorzCpp(1 - no_border, TEXT_COLS - 2 + no_border, task);
  WinVertCpp(2 - no_border, TEXT_ROWS - 2 + no_border, task);
  WinToTopCpp(task, true);
  SetRFlags(flags);
}

bool WinMsCursorVisible()
{
  return ms.show && ms_hard.installed;
}

static void DrawMs(CDC *dc)
{
  auto flags = GetRFlags();
  asm volatile("cli" : : : "memory");
  auto x = ms.pos.x, y = ms.pos.y;
  auto color = ms.lb ? LTPURPLE : ms.rb ? LTCYAN : BLACK;
  SetRFlags(flags);
  if (!WinMsCursorVisible() || !(SYS_RUN_LEVEL & RLF_VGA) || !gr.fp_draw_ms)
    return;

  dc->color.c0.rop = ROPB_XOR;
  dc->color.c0.color = color ^ TRANSPARENT;
  auto addr = reinterpret_cast<uintptr_t>(dc);
  if (winmgr.grab_scroll && gr.fp_draw_grab_ms)
    CallHolyC4(gr.fp_draw_grab_ms, addr, x, y, winmgr.grab_scroll_closed);
  else
    CallHolyC3(gr.fp_draw_ms, addr, x, y);
}

extern "C" void WinFinalUpdateCpp(CDC *dc)
{
  DrawMs(dc);
}

static bool ms_input_updated;

void WinMsInput()
{
  if (!ms_hard.installed)
    return;
  ms.has_wheel = ms_hard.has_wheel;
  if (ms_hard.evt) {
    MsUpdate(ms_hard.pos.x, ms_hard.pos.y, ms_hard.pos.z, ms_hard.bttns[0],
             ms_hard.bttns[1]);
    ms_hard.evt = false;
    ms.speed = ms_hard.speed;
    ms.timestamp = ms_hard.timestamp;
    ms_input_updated = true;
  }
}

void WinMsUpdate()
{
  if (!ms_input_updated)
    ms.speed = F64MulBits(ms.speed, std::bit_cast<uint64_t>(0.95));
  ms_input_updated = false;
  if (gr.scrn_zoom != 1) {
    auto x = (uint64_t(ms.pos.x) - gr.sx) * gr.scrn_zoom;
    auto y = (uint64_t(ms.pos.y) - gr.sy) * gr.scrn_zoom;
    if (gr.continuous_scroll || x < 8 || x >= GR_WIDTH - 8 || y < 8 ||
        y >= GR_HEIGHT - 8)
      GrScaleZoomCpp(std::bit_cast<uint64_t>(1.0));
  }
}
