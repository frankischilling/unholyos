#include "T:/Kernel/KernelA.HH"

_import SYS_COMPILE_TIME I64 ProbeCompileTime;
_import _MEMCPY U8 *ProbeCopy(U8 *dst, U8 *src, I64 count);

asm {
  ALIGN 128,OC_NOP
}

public I64 LoadProbe(I64 value)
{
  return value * 3 + 7;
}

public I64 LoadProbeValue, LoadProbeAddress, LoadProbeDate, LoadProbeCopyEnd;
#exe {Option(OPTf_GLBLS_ON_DATA_HEAP,ON);};
I64 LoadProbeHeap;
#exe {Option(OPTf_GLBLS_ON_DATA_HEAP,OFF);};
LoadProbeHeap = LoadProbe(11);
LoadProbeValue = LoadProbeHeap;
LoadProbeAddress = &LoadProbe;
LoadProbeCopyEnd = ProbeCopy(&LoadProbeDate, &ProbeCompileTime, 8);
