%include "Kernel/KernelA.inc"

BITS 64
SECTION .text
GLOBAL CmpHC,ExePutSHC,ExeFileHC,StreamExePrintHC
EXTERN CmpCpp,ExePutSCpp,ExeFileCpp,StreamExePrintCpp
GLOBAL ExePrintHC,RunFileHC,ExePutS2HC,ExePrint2HC,ExeFile2HC,RunFile2HC
EXTERN ExePrintCpp,RunFileCpp,ExePutS2Cpp,ExePrint2Cpp,ExeFile2Cpp,RunFile2Cpp
GLOBAL CInitHC
EXTERN CInitCpp
GLOBAL UHC,UiHC,UnHC
EXTERN UCpp,UiCpp,UnCpp

HOLYC_CPP CmpHC,CmpCpp,4
HOLYC_CPP ExePutSHC,ExePutSCpp,4
HOLYC_PRINT ExePrintHC,ExePrintCpp,1
HOLYC_CPP ExeFileHC,ExeFileCpp,2
HOLYC_PRINT RunFileHC,RunFileCpp,2
HOLYC_CPP ExePutS2HC,ExePutS2Cpp,3
HOLYC_PRINT ExePrint2HC,ExePrint2Cpp,1
HOLYC_CPP ExeFile2HC,ExeFile2Cpp,2
HOLYC_PRINT RunFile2HC,RunFile2Cpp,2
HOLYC_PRINT StreamExePrintHC,StreamExePrintCpp,1,1
HOLYC_CPP CInitHC,CInitCpp,0
HOLYC_CPP UHC,UCpp,3
HOLYC_CPP UiHC,UiCpp,5
HOLYC_CPP UnHC,UnCpp,3

SECTION .note.GNU-stack noalloc noexec nowrite progbits
