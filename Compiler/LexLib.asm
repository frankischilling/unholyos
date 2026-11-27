%include "Kernel/KernelA.inc"

BITS 64
SECTION .text
GLOBAL MemberMetaDataHC,MemberMetaFindHC,MemberFindHC
EXTERN MemberMetaDataCpp,MemberMetaFindCpp,MemberFindCpp

HOLYC_CPP MemberMetaDataHC,MemberMetaDataCpp,2
HOLYC_CPP MemberMetaFindHC,MemberMetaFindCpp,2
HOLYC_CPP MemberFindHC,MemberFindCpp,2

SECTION .note.GNU-stack noalloc noexec nowrite progbits
