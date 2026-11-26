#include "Adam/DolDoc/DolDoc.hpp"

extern "C" void UserCmdLineHC();

void UserStartUp()
{
  auto *task = CurrentTask();
  if (!task->cur_dv)
    task->cur_dv = adam_task->cur_dv;
  if (task->cur_dv && !RedSeaIsDir(*task->cur_dv, task->cur_dir)) {
    auto *path = StrNew("/", task->data_heap);
    if (path) {
      Free(task->cur_dir);
      task->cur_dir = path;
    }
  }
  DocTermNewCpp();
  if (!DocPut())
    return;
  WinHorzCpp(1, TEXT_COLS / 2 - 1, task);
  WinVertCpp(2, TEXT_ROWS - 2, task);
  task->display_flags |= DISPLAYF_SHOW;
  sys_focus_task = task;
  WinZBufUpdateCpp();
}

bool StartUpTasks()
{
  return SpawnCpp(reinterpret_cast<uintptr_t>(UserCmdLineHC), nullptr, nullptr,
                  0, adam_task, 0, JOBF_ADD_TO_QUE);
}
