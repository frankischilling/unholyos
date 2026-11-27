%include "Kernel/KernelA.inc"

BITS 64
SECTION .text
GLOBAL CmpHC,ExePutSHC,ExeFileHC,StreamExePrintHC
EXTERN CmpCpp,ExePutSCpp,ExeFileCpp,StreamExePrintCpp
GLOBAL CInitHC
EXTERN CInitCpp
GLOBAL UHC,UiHC,UnHC
EXTERN UCpp,UiCpp,UnCpp

HOLYC_CPP CmpHC,CmpCpp,4
HOLYC_CPP ExePutSHC,ExePutSCpp,4
HOLYC_CPP ExeFileHC,ExeFileCpp,2
HOLYC_PRINT StreamExePrintHC,StreamExePrintCpp,1,1
HOLYC_CPP CInitHC,CInitCpp,0
HOLYC_CPP UHC,UCpp,3
HOLYC_CPP UiHC,UiCpp,5
HOLYC_CPP UnHC,UnCpp,3

SECTION .note.GNU-stack noalloc noexec nowrite progbits
