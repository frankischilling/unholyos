%include "Kernel/KernelA.inc"

BITS 64
SECTION .text
GLOBAL StreamPrintHC,StreamDirHC
EXTERN StreamPrintCpp,StreamDirCpp
GLOBAL OptionHC,GetOptionHC
EXTERN OptionCpp,GetOptionCpp

HOLYC_CPP OptionHC,OptionCpp,2
HOLYC_CPP GetOptionHC,GetOptionCpp,1
HOLYC_PRINT StreamPrintHC,StreamPrintCpp,1,1
HOLYC_CPP StreamDirHC,StreamDirCpp,0

SECTION .note.GNU-stack noalloc noexec nowrite progbits
