#pragma once

#include "../../Kernel/KernelB.hpp"

inline CCtrl *CtrlHead(CTask *task)
{
  return reinterpret_cast<CCtrl *>(&task->next_ctrl);
}

extern "C" CCtrl *CtrlFindUniqueCpp(CTask *task, int64_t type);
extern "C" uint64_t CtrlInsideCpp(CCtrl *c, int64_t x, int64_t y);
extern "C" void WinScrollsInitCpp(CTask *task);

void CtrlsUpdate(CTask *task);
bool DrawCtrls(CTask *task);
bool WinCtrlsCaptured();
