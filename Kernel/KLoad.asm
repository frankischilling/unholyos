%include "Kernel/KernelA.inc"

BITS 64
SECTION .text
GLOBAL LoadPass2HC,LoadHC
EXTERN LoadPass2Cpp,LoadCpp

HOLYC_CPP LoadPass2HC,LoadPass2Cpp,3
HOLYC_CPP LoadHC,LoadCpp,3

SECTION .note.GNU-stack noalloc noexec nowrite progbits
