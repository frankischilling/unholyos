%include "Kernel/KernelA.inc"

BITS 64
SECTION .text
GLOBAL StreamPrintHC,StreamDirHC
EXTERN StreamPrintCpp,StreamDirCpp
GLOBAL OptionHC,GetOptionHC
EXTERN OptionCpp,GetOptionCpp
GLOBAL _LAST_FUN,LastFunHC,LastFunCpp

HOLYC_CPP OptionHC,OptionCpp,2
HOLYC_CPP GetOptionHC,GetOptionCpp,1
LastFunHC:
HOLYC_CPP _LAST_FUN,LastFunCpp,2
HOLYC_PRINT StreamPrintHC,StreamPrintCpp,1,1
HOLYC_CPP StreamDirHC,StreamDirCpp,0

; _LAST_FUN in CMisc.HC, with SysV arguments for RunFile and RunFile2.
LastFunCpp:
	PUSH	RBP
	MOV	RBP,RSP
	MOV	RAX,[FS:CTask_LAST_FUN]
	TEST	RAX,RAX
	JZ	.empty
	MOV	RDX,[RAX+CHashFun_EXE_ADDR]
	TEST	RDX,RDX
	JZ	.empty
	MOV	RCX,RDI
	SHL	RCX,3
	LEA	RAX,[RCX+15]
	AND	RAX,-16
	SUB	RSP,RAX
	MOV	RDI,RSP
	REP MOVSB
	CALL	RDX
	LEAVE
	RET
.empty:
	XOR	EAX,EAX
	LEAVE
	RET

SECTION .note.GNU-stack noalloc noexec nowrite progbits
