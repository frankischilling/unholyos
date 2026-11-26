#include "../../Kernel/KernelB.hpp"

#include <bit>

void CtrlAltZ(int64_t sc)
{
  GrScaleZoomCpp(std::bit_cast<uint64_t>(sc & SCF_SHIFT ? 0.5 : 2.0));
}
