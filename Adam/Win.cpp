#include "../Kernel/KernelB.hpp"

void WinMsUpdate()
{
  if (!ms_hard.installed)
    return;
  ms.has_wheel = ms_hard.has_wheel;
  if (ms_hard.evt) {
    MsUpdate(ms_hard.pos.x, ms_hard.pos.y, ms_hard.pos.z,
             ms_hard.bttns[0], ms_hard.bttns[1]);
    ms_hard.evt = false;
    ms.speed = ms_hard.speed;
    ms.timestamp = ms_hard.timestamp;
  }
}
