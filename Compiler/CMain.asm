%include "Kernel/KernelA.inc"

BITS 64
SECTION .text
GLOBAL CmpHC,ExePutSHC,ExeFileHC,StreamExePrintHC
EXTERN CmpCpp,ExePutSCpp,ExeFileCpp,StreamExePrintCpp

HOLYC_CPP CmpHC,CmpCpp,4
HOLYC_CPP ExePutSHC,ExePutSCpp,4
HOLYC_CPP ExeFileHC,ExeFileCpp,2
HOLYC_PRINT StreamExePrintHC,StreamExePrintCpp,1,1

SECTION .note.GNU-stack noalloc noexec nowrite progbits
