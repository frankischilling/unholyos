#include "../DolDoc/DolDoc.hpp"
#include "../Training.hpp"
#include "AutoComplete.hpp"

extern "C" void ACTaskHC(), ACTaskEndCBHC();
extern "C" int64_t ScanMsgCpp(int64_t *arg1, int64_t *arg2, uint64_t mask,
                               CTask *task);
extern "C" void DocClearCpp(CDoc *doc, uint8_t clear_holds);
extern "C" bool DocCursorCpp(bool show, CDoc *doc);

static void ACDocRst(int64_t left, int64_t top)
{
  auto *task = CurrentTask();
  auto *doc = DocPut();
  DocClearCpp(doc, true);
  doc->flags |= DOCF_SIZE_MIN;
  task->border_src = 0; // BDS_CONST
  task->border_attr = (LTGRAY << 4) | (DrvTextAttrGetCpp(':') & 15);
  task->text_attr = (LTGRAY << 4) | BLUE;
  task->display_flags &= ~DISPLAYF_SHOW;
  WinHorzCpp(left, TEXT_COLS - 2, task);
  WinVertCpp(top, TEXT_ROWS - 2, task);
  DocCursorCpp(true, doc);
}

static void ACDocShow()
{
  auto *task = CurrentTask();
  auto *doc = DocPut();
  if (task->display_flags & DISPLAYF_SHOW)
    return;
  DocRecalc(*doc);
  auto right = task->win_left + doc->max_x - doc->min_x;
  auto bottom = task->win_top + doc->max_y - doc->min_y;
  WinHorzCpp(task->win_left, right < TEXT_COLS - 2 ? right : TEXT_COLS - 2, task);
  WinVertCpp(task->win_top, bottom < TEXT_ROWS - 2 ? bottom : TEXT_ROWS - 2, task);
  task->display_flags |= DISPLAYF_SHOW;
  WinZBufUpdateCpp();
}

static void ACTaskNormal(int64_t sc, int64_t last_sc, CTask *focus_task)
{
  if (DocPutCpp(focus_task) && (focus_task->display_flags & DISPLAYF_SHOW) &&
      (!(CurrentTask()->display_flags & DISPLAYF_SHOW) ||
       ((sc ^ last_sc) & (SCF_CTRL | SCF_ALT)))) {
    ACDocRst(51, 13);
    TypeCpp(AC_STANDBY, 0);
  }
  ACDocShow();
}

static void ACTaskCtrl(int64_t sc, int64_t last_sc, CTask *focus_task,
                      CTask *original_focus_task)
{
  if (TaskValidateCpp(focus_task) && (focus_task->scroll_x || focus_task->scroll_y)) {
    auto *task = CurrentTask();
    if (__atomic_fetch_and(&task->display_flags, ~DISPLAYF_SHOW, __ATOMIC_RELAXED) &
        DISPLAYF_SHOW)
      WinZBufUpdateCpp();
    return;
  }
  if (sc != last_sc) {
    bool shift = sc & SCF_SHIFT;
    bool key_desc = TaskValidateCpp(original_focus_task) &&
                    !(original_focus_task->win_inhibit & WIF_SELF_KEY_DESC);
    if (sc & SCF_ALT) {
      ACDocRst(27, 3);
      if (key_desc)
        KeyMapFamily(original_focus_task, 0, !shift, shift);
      KeyMapCtrlAltFamily(!shift, shift);
    } else if (key_desc) {
      ACDocRst(27, 3);
      KeyMapFamily(original_focus_task, SCF_CTRL, !shift, shift);
    }
  }
  ACDocShow();
}

static void ACTaskAlt(int64_t sc, int64_t last_sc, CTask *original_focus_task)
{
  if (sc != last_sc && TaskValidateCpp(original_focus_task) &&
      !(original_focus_task->win_inhibit & WIF_SELF_KEY_DESC)) {
    bool shift = sc & SCF_SHIFT;
    ACDocRst(27, 3);
    KeyMapFamily(original_focus_task, SCF_ALT, !shift, shift);
  }
  ACDocShow();
}

extern "C" [[noreturn]] void ACTaskEndCBCpp()
{
  ac.task = nullptr;
  Exit();
}

extern "C" void ACTaskCpp()
{
  auto *task = CurrentTask();
  task->task_end_cb = ACTaskEndCBHC;
  task->win_inhibit = WIG_NO_FOCUS_TASK_DFT;
  DocTermNewCpp();
  if (!DocPut())
    return;
  task->display_flags |= DISPLAYF_WIN_ON_TOP;
  Free(ac.cur_word);
  ac.cur_word = nullptr;

  int64_t scan_code = 0, last_scan_code = 0;
  for (;;) {
    auto last_event = ms_hard.timestamp > kbd.timestamp ? ms_hard.timestamp
                                                       : kbd.new_key_timestamp;
    if ((scan_code & (SCF_CTRL | SCF_ALT)) ||
        GetTSC() > uint64_t(last_event) + (uint64_t(cnts.time_stamp_freq) >> 1)) {
      last_scan_code = scan_code;
      scan_code = kbd.scan_code;
    }
    auto *original_focus_task = sys_focus_task;
    auto *focus_task = original_focus_task;
    while (TaskValidateCpp(focus_task) && (focus_task->task_flags & TASKF_INPUT_FILTER_TASK))
      focus_task = focus_task->parent_task;
    if (scan_code & SCF_CTRL)
      ACTaskCtrl(scan_code, last_scan_code, focus_task, original_focus_task);
    else if (TaskValidateCpp(focus_task)) {
      if (scan_code & SCF_ALT)
        ACTaskAlt(scan_code, last_scan_code, original_focus_task);
      else
        ACTaskNormal(scan_code, last_scan_code, focus_task);
    }
    SleepCpp(333);
    int64_t ch;
    if (ScanMsgCpp(&ch, nullptr, 1 << MSG_KEY_DOWN, task) &&
        (ch == CH_ESC || ch == CH_SHIFT_ESC))
      break;
  }
}

extern "C" uint64_t AutoCompleteCpp(uint8_t val)
{
  while (__atomic_load_n(&ac.flags, __ATOMIC_RELAXED) & ACF_INIT_IN_PROGRESS)
    SleepCpp(10);
  auto *task = ac.task;
  bool running = TaskValidateCpp(task);
  bool enabled = SYS_RUN_LEVEL & RLF_AUTO_COMPLETE;
  bool old_val = running && enabled;
  if (val) {
    if (enabled) {
      if (!running) {
        ac.task = SpawnCpp(reinterpret_cast<uintptr_t>(ACTaskHC), nullptr,
                           "AutoComplete", -1, nullptr, 0, JOBF_ADD_TO_QUE);
        if (ac.task)
          TaskWaitCpp(ac.task, false);
      }
      if (ac.task)
        WinToTopCpp(ac.task, true);
    }
  } else if (running) {
    KillTask(task, false);
    while (ac.task == task && TaskValidateCpp(task))
      _YIELD();
  }
  return old_val;
}
