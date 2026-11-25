#include "T:/Kernel/KernelA.HH"

import I64 LoadProbe(I64 value);

public I64 LoadCallerValue;
LoadCallerValue = LoadProbe(7);
