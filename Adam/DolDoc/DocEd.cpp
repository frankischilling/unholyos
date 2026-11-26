#include "DolDoc.hpp"

extern "C" uint64_t ViewCpp()
{
  int64_t ch;
  do
    ch = DocGetKeyCpp(nullptr);
  while (ch != CH_ESC && ch != CH_SHIFT_ESC);
  return ch == CH_ESC;
}

extern "C" uint64_t DocEdCpp(CDoc *doc, int64_t dof_flags)
{
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL)
    return false;
  auto *task = CurrentTask();
  auto *border = DocNewCpp(nullptr, task);
  if (!border)
    return false;
  border->flags |= DOCF_BORDER_DOC;
  constexpr char controls[] = "$CM+H+TY+NC,0,-1$$TX+H+RX+BD,\"[X]\"$"
                              "$BK,1$$TX+H+LX+BD,\"MENU\"$$BK,0$";
  DocPutS(*border, controls, sizeof(controls) - 1);
  auto *old_put = task->put_doc;
  auto *old_display = task->display_doc;
  auto *old_border = task->border_doc;
  auto old_attr = task->text_attr, old_title_src = task->title_src;
  auto old_display_flags = task->display_flags;
  uint8_t old_title[sizeof(task->task_title)];
  MemCpy(old_title, task->task_title, sizeof(old_title));
  auto old_left = task->win_left, old_right = task->win_right;
  auto old_top = task->win_top, old_bottom = task->win_bottom;
  if (dof_flags & DOF_WIN_MAX)
    WinMaxCpp(task);
  bool unlock = DocLock(*doc);
  doc->win_task = task;
  if (task->title_src != 1) { // TTS_LOCKED_CONST
    task->title_src = 3; // TTS_ED_FILENAME
    MemCpy(task->task_title, doc->filename.name, sizeof(task->task_title) - 1);
    task->task_title[sizeof(task->task_title) - 1] = 0;
  }
  if (!(dof_flags & DOF_DONT_HOME)) {
    doc->cur_entry = doc->head.next;
    doc->cur_col = doc->cur_entry->min_col;
    doc->top_line_num = doc->line_start_col = 0;
    DocFormFwd(*doc);
  }
  task->border_doc = border;
  if (doc != old_display)
    doc->parent_doc = old_display;
  task->put_doc = task->display_doc = doc;
  if (!(dof_flags & DOF_DONT_TEXT_ATTR))
    task->text_attr = 0xF0;
  task->display_flags |= DISPLAYF_NOT_RAW;
  if (!(dof_flags & DOF_DONT_SHOW))
    task->display_flags |= DISPLAYF_SHOW;
  if (dof_flags & DOF_SIZE_MIN)
    doc->flags |= DOCF_SIZE_MIN;
  DocRecalc(*doc);
  if (doc->flags & DOCF_SIZE_MIN) {
    auto right = task->win_left + doc->max_x - doc->min_x + 1;
    auto bottom = task->win_top + doc->max_y - doc->min_y + 1;
    if (right < task->win_right)
      WinHorzCpp(task->win_left, right, task);
    if (bottom < task->win_bottom)
      WinVertCpp(task->win_top, bottom, task);
  }
  doc->flags |= DOCF_DO_FULL_REFRESH;
  DocUnlock(*doc);
  WinZBufUpdateCpp();
  if (!(dof_flags & DOF_DONT_WINMGR_SYNC))
    RefreshCpp(2, true);
  bool res = ViewCpp();
  DocLock(*doc);
  if (res)
    for (auto *entry = doc->head.next; entry != &doc->head; entry = entry->next)
      if (entry->Type() == DOCT_DATA || entry->Type() == DOCT_CHECK_BOX)
        DocDataScan(*doc, *reinterpret_cast<CDocEntry *>(entry));
  task->border_doc = old_border;
  task->display_doc = old_display;
  task->put_doc = old_put;
  task->text_attr = old_attr;
  task->display_flags = (task->display_flags & ~DISPLAYF_NOT_RAW) |
                        (old_display_flags & DISPLAYF_NOT_RAW);
  if (task->title_src != 1) {
    task->title_src = old_title_src;
    MemCpy(task->task_title, old_title, sizeof(old_title));
  }
  if (doc->flags & DOCF_SIZE_MIN) {
    WinHorzCpp(old_left, old_right, task);
    WinVertCpp(old_top, old_bottom, task);
  }
  if (unlock)
    DocUnlock(*doc);
  DocDelCpp(border);
  WinZBufUpdateCpp();
  return res;
}
