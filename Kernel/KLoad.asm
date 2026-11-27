%include "Kernel/KernelA.inc"

BITS 64
SECTION .text
GLOBAL LoadHC
EXTERN LoadCpp

HOLYC_CPP LoadHC,LoadCpp,3

SECTION .note.GNU-stack noalloc noexec nowrite progbits
