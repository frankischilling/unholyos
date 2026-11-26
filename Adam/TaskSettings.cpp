#include "DolDoc/DolDoc.hpp"

extern "C" CTaskSettings *SettingsPushCpp(CTask *task, int64_t flags)
{
  if (!task)
    task = CurrentTask();
  if (!TaskValidateCpp(task))
    return nullptr;
  auto *tmpse =
      reinterpret_cast<CTaskSettings *>(CAllocCpp(sizeof(CTaskSettings), task));
  if (!tmpse)
    return nullptr;
  tmpse->cur_dir = DirCurCpp(task, task);
  if (!tmpse->cur_dir && task->cur_dv && task->cur_dir) {
    Free(tmpse);
    return nullptr;
  }
  tmpse->draw_it = task->draw_it;
  GrPaletteGetCpp(tmpse->palette);
  tmpse->task_end_cb = task->task_end_cb;

  if (!(flags & TSF_SAME_SONG)) {
    if ((tmpse->song_task = task->song_task)) {
      SuspendCpp(task->song_task, true);
      SndCpp(0);
    }
    task->song_task = nullptr;
  }

  if ((tmpse->animate_task = task->animate_task))
    SuspendCpp(task->animate_task, true);
  task->animate_task = nullptr;

  if (auto *doc = DocPutCpp(task)) {
    tmpse->hide_cursor = !(doc->flags & DOCF_HIDE_CURSOR);
    tmpse->highlight_cursor = !(doc->flags & DOCF_DONT_HIGHLIGHT_CURSOR);
    tmpse->scroll = !(doc->flags & DOCF_NO_SCROLL_BARS);
  }

  tmpse->left = task->win_left;
  tmpse->right = task->win_right;
  tmpse->top = task->win_top;
  tmpse->bottom = task->win_bottom;

  tmpse->scroll_x = task->scroll_x;
  tmpse->scroll_y = task->scroll_y;
  tmpse->scroll_z = task->scroll_z;

  tmpse->win_inhibit = task->win_inhibit;
  tmpse->text_attr = task->text_attr;
  StrCpyCpp(reinterpret_cast<char *>(tmpse->task_title),
            reinterpret_cast<const char *>(task->task_title));
  tmpse->title_src = task->title_src;
  tmpse->border_attr = task->border_attr;
  tmpse->border_src = task->border_src;
  tmpse->border = !(task->display_flags & DISPLAYF_NO_BORDER);
  tmpse->autocomplete = TaskValidateCpp(ac.task);

  tmpse->next = task->next_settings;
  task->next_settings = tmpse;
  return tmpse;
}

static void SettingsPop2(CTask *task, const CTaskSettings *tmpse)
{
  if (auto *doc = DocPutCpp(task)) {
    constexpr auto mask =
        DOCF_HIDE_CURSOR | DOCF_DONT_HIGHLIGHT_CURSOR | DOCF_NO_SCROLL_BARS;
    const uint64_t flags =
        (!tmpse->hide_cursor ? DOCF_HIDE_CURSOR : 0) |
        (!tmpse->highlight_cursor ? DOCF_DONT_HIGHLIGHT_CURSOR : 0) |
        (!tmpse->scroll ? DOCF_NO_SCROLL_BARS : 0);
    __atomic_fetch_and(&doc->flags, ~mask, __ATOMIC_RELAXED);
    __atomic_fetch_or(&doc->flags, flags, __ATOMIC_RELAXED);
  }

  WinBorderCpp(tmpse->border, task);
  WinHorzCpp(tmpse->left, tmpse->right, task);
  WinVertCpp(tmpse->top, tmpse->bottom, task);
  task->scroll_x = tmpse->scroll_x;
  task->scroll_y = tmpse->scroll_y;
  task->scroll_z = tmpse->scroll_z;
  task->win_inhibit = tmpse->win_inhibit;
  task->text_attr = tmpse->text_attr;
  task->border_attr = tmpse->border_attr;
  task->border_src = tmpse->border_src;
  task->title_src = tmpse->title_src;
  StrCpyCpp(reinterpret_cast<char *>(task->task_title),
            reinterpret_cast<const char *>(tmpse->task_title));
  AutoCompleteCpp(tmpse->autocomplete);
  GrPaletteSetCpp(tmpse->palette);
  SndCpp(0);
}

extern "C" void SettingsPopCpp(CTask *task, int64_t flags)
{
  if (!task)
    task = CurrentTask();
  if (!TaskValidateCpp(task))
    return;
  if (auto *tmpse = task->next_settings) {
    task->next_settings = tmpse->next;
    CdCpp(tmpse->cur_dir, false);
    Free(tmpse->cur_dir);
    task->draw_it = tmpse->draw_it;
    task->task_end_cb = tmpse->task_end_cb;

    if (task->animate_task)
      KillTask(task->animate_task);
    if ((task->animate_task = tmpse->animate_task))
      SuspendCpp(task->animate_task, false);

    if (!(flags & TSF_SAME_SONG)) {
      if (task->song_task)
        KillTask(task->song_task);
      if ((task->song_task = tmpse->song_task))
        SuspendCpp(task->song_task, false);
    }

    SettingsPop2(task, tmpse);
    RefreshCpp(1, true);
    SettingsPop2(task, tmpse);

    Free(tmpse);
  }
}
