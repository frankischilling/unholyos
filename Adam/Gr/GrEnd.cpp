#include "../../Kernel/KernelB.hpp"

#include <bit>

void CtrlAltZ(int64_t sc)
{
  GrScaleZoomCpp(std::bit_cast<uint64_t>(sc & SCF_SHIFT ? 0.5 : 2.0));
}

extern "C" void CtrlAltZHC();

extern "C" void CtrlAltZCpp(int64_t sc)
{
  if (sys_winmgr_task)
    CtrlAltZ(sc);
}

void GrEnd()
{
  CtrlAltCBSetCpp('Z', reinterpret_cast<uintptr_t>(CtrlAltZHC),
                  "Cmd /Zoom In", "Cmd /Zoom Out", false);
}
