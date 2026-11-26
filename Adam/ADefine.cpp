#include "../Kernel/KernelB.hpp"

bool LoadDocDefines()
{
  return DefineLstLoadCpp("DD_OS_NAME_VERSION", "UnholyOS\0", nullptr);
}
