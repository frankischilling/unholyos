#pragma once

#include <stdint.h>

struct CTask;

void KeyMapFamily(CTask *task, int64_t scf, bool no_shift, bool shift);
void KeyMapCtrlAltFamily(bool no_shift, bool shift);
