#include "DolDoc.hpp"

void DocUpdateTaskDocs(CTask *task)
{
  if (task->title_src == TTS_TASK_NAME) {
    MemSet(task->task_title, 0, sizeof(task->task_title));
    MemCpy(task->task_title, task->task_name, sizeof(task->task_name));
  }
  if (auto *doc = DocDisplayCpp(task); doc && !(doc->flags & DOCF_DONT_SHOW))
    DocRecalc(*doc, RECALCt_TO_SCRN | RECALCF_HAS_CURSOR);
  if (auto *doc = DocBorderCpp(task); doc && !(doc->flags & DOCF_DONT_SHOW)) {
    auto x = task->scroll_x, y = task->scroll_y, z = task->scroll_z;
    task->scroll_x = task->scroll_y = task->scroll_z = 0;
    DocRecalc(*doc, RECALCt_TO_SCRN);
    task->scroll_x = x;
    task->scroll_y = y;
    task->scroll_z = z;
  }
}
