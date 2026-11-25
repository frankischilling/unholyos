#include "T:/Kernel/KernelA.HH"

import I64 HashTargetFn(I64 value);
import I64 HashTargetVar;

public I64 HashCallTarget(I64 value)
{
  return HashTargetFn(value) + HashTargetVar;
}
