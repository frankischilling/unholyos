#include "Adam/DolDoc/DolDoc.hpp"

extern "C" void UserCmdLineHC();

void UserStartUp()
{
  auto *task = CurrentTask();
  if (!task->cur_dv)
    task->cur_dv = adam_task->cur_dv;
  if (task->cur_dv && !FsIsDir(*task->cur_dv, task->cur_dir)) {
    auto *path = StrNew("/", task->data_heap);
    if (path) {
      Free(task->cur_dir);
      task->cur_dir = path;
    }
  }
  DocTermNewCpp();
  if (!DocPut())
    return;
  WinHorzCpp(1, TEXT_COLS - 2, task);
  WinVertCpp(2, TEXT_ROWS - 2, task);
  TypeCpp("::/Doc/Start.DD", 1);
  task->display_flags |= DISPLAYF_SHOW;
  WinToTopCpp(task, false);
  WinZBufUpdateCpp();
  DirCpp("*", false);
}

void SrvStartUp()
{
  auto *task = CurrentTask();
  DocTermNewCpp();
  WinHorzCpp(task->win_left, TEXT_COLS - 2, task);
  WinVertCpp(task->win_top, TEXT_ROWS - 2, task);
  task->display_flags |= DISPLAYF_SHOW;
  WinToTopCpp(task, false);
  WinZBufUpdateCpp();
}

bool StartUpTasks()
{
  auto *user1 = SpawnCpp(reinterpret_cast<uintptr_t>(UserCmdLineHC), nullptr,
                         nullptr, -1, nullptr, 0, JOBF_ADD_TO_QUE);
  if (!user1)
    return false;
  TaskWaitCpp(user1, true);
  if (!TaskValidateCpp(user1))
    return false;
  auto *user2 = SpawnCpp(reinterpret_cast<uintptr_t>(UserCmdLineHC), nullptr,
                         nullptr, -1, nullptr, 0, JOBF_ADD_TO_QUE);
  if (!user2)
    return false;
  TaskWaitCpp(user2, true);
  if (!TaskValidateCpp(user2))
    return false;
  WinToTopCpp(user1, false);
  WinTileVertCpp();
  WinZBufUpdateCpp();
  auto *path = FileNameAbs("Once.HC.Z", *user1);
  auto *dv = path && *path && path[1] == ':' ? PathDrive(*user1, path[0])
                                             : nullptr;
  CRedSeaDirEntry entry;
  size_t prefix;
  const bool found = dv && FsFileLookup(*dv, path + 2, true, true, entry, prefix);
  Free(path);
  if (found)
    return XTalk(user1, "Cd;#include \"Once\";\n", 0, nullptr);
  return true;
}
