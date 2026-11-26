#pragma once

#include "../Kernel/KernelB.hpp"

constexpr int64_t FONT_WIDTH = 8, FONT_HEIGHT = 8;

CTask *MenuTask();
int64_t MenuEntryWidth(const CMenuEntry *entry);
void DrawMenu(CDC *dc);
void WinMenuPointer(int64_t &x, int64_t &y);
void MenuSend(CTask *focus, CMenu *menu);

extern CMenuEntry *sys_cur_submenu_entry;
