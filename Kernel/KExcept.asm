%include "Kernel/KernelA.inc"

GLOBAL SysTryHC,ThrowHC,CallerHC,TestExcept,RaiseHolyC
GLOBAL CallHolyC0Checked,CallHolyC1Checked,CallHolyC2Checked,CallHolyC3Checked,CallHolyC4Checked
EXTERN SysTryCpp,SysUntryCpp,ThrowCpp,CallerCpp,CallHolyC0,CallHolyC1,CallHolyC2,CallHolyC3,CallHolyC4

SECTION .text
BITS 64

SysTryHC:
	PUSH	RBP
	MOV	RBP,RSP
	PUSHFQ
	SUB	RSP,CExcept_SIZE
	AND	RSP,-16
	MOV	RAX,[RBP+16]
	MOV	[RSP+CExcept_HNDLR_CATCH],RAX
	MOV	RAX,[RBP+24]
	MOV	[RSP+CExcept_HNDLR_UNTRY],RAX
	LEA	RAX,[RBP+32]
	MOV	[RSP+CExcept_RSP],RAX
	MOV	RAX,[RBP]
	MOV	[RSP+CExcept_RBP],RAX
	MOV	RAX,[RBP-8]
	MOV	[RSP+CExcept_RFLAGS],RAX
	MOV	[RSP+CExcept_RSI],RSI
	MOV	[RSP+CExcept_RDI],RDI
	MOV	[RSP+CExcept_R10],R10
	MOV	[RSP+CExcept_R11],R11
	MOV	[RSP+CExcept_R12],R12
	MOV	[RSP+CExcept_R13],R13
	MOV	[RSP+CExcept_R14],R14
	MOV	[RSP+CExcept_R15],R15
	MOV	RDI,RSP
	CALL	SysTryCpp
	TEST	AL,AL
	JZ	.oom
	MOV	RSI,[RSP+CExcept_RSI]
	MOV	RDI,[RSP+CExcept_RDI]
	MOV	R10,[RSP+CExcept_R10]
	MOV	R11,[RSP+CExcept_R11]
	MOV	RSP,RBP
	POP	RBP
	RET	16
.oom:
	MOV	RAX,'OutMem'
	PUSH	0
	PUSH	RAX
	CALL	ThrowHC

ThrowHC:
	PUSH	RBP
	MOV	RBP,RSP
	MOV	RDI,[RBP+16]
	MOV	RSI,[RBP+24]
	MOV	RDX,RBP
	AND	RSP,-16
	CALL	ThrowCpp

RaiseHolyC:
	PUSH	0
	PUSH	RDI
	CALL	ThrowHC

CallerHC:
	PUSH	RBP
	MOV	RBP,RSP
	PUSH	RSI
	PUSH	RDI
	PUSH	R10
	PUSH	R11
	MOV	RDI,RBP
	MOV	RSI,[RBP+16]
	AND	RSP,-16
	CALL	CallerCpp
	LEA	RSP,[RBP-32]
	POP	R11
	POP	R10
	POP	RDI
	POP	RSI
	POP	RBP
	RET	8

TestExcept:
	; Run the HolyC catch on the throw stack and return here for its decision.
	PUSH	RBP
	PUSH	RBX
	PUSH	R12
	PUSH	R13
	PUSH	R14
	PUSH	R15
	MOV	RAX,RDI
	LEA	RDX,[REL .returned]
	PUSH	RDX
	MOV	RBP,[RAX+CExcept_RBP]
	MOV	RSI,[RAX+CExcept_RSI]
	MOV	RDI,[RAX+CExcept_RDI]
	MOV	R10,[RAX+CExcept_R10]
	MOV	R11,[RAX+CExcept_R11]
	MOV	R12,[RAX+CExcept_R12]
	MOV	R13,[RAX+CExcept_R13]
	MOV	R14,[RAX+CExcept_R14]
	MOV	R15,[RAX+CExcept_R15]
	PUSH	QWORD [RAX+CExcept_RFLAGS]
	POPFQ
	JMP	QWORD [RAX+CExcept_HNDLR_CATCH]
.returned:
	MOV	RAX,[FS:0]
	CMP	BYTE [RAX+CTask_CATCH_EXCEPT],0
	JNE	TakeExcept
	CLD
	POP	R15
	POP	R14
	POP	R13
	POP	R12
	POP	RBX
	POP	RBP
	RET

TakeExcept:
	; The compiler's untry label frees the accepted frame on the saved stack.
	MOV	RAX,[RAX+CTask_LAST_EXCEPT]
	MOV	RSP,[RAX+CExcept_RSP]
	JMP	QWORD [RAX+CExcept_HNDLR_UNTRY]

%macro CALL_HOLYC_CHECKED 3
%1:
	PUSH	RBP
	MOV	RBP,RSP
	PUSH	RBX
	PUSH	R12
	PUSH	R13
	PUSH	R14
	PUSH	R15
	SUB	RSP,8
	MOV	R12,RDI
	MOV	R13,RSI
%if %3 >= 2
	MOV	R14,RDX
%endif
%if %3 = 2
	MOV	RBX,RCX
%elif %3 = 3
	MOV	R15,RCX
	MOV	RBX,R8
%elif %3 = 4
	MOV	R15,RCX
	MOV	[RBP-48],R8
	MOV	RBX,R9
%endif
	LEA	RAX,[REL .accepted]
	PUSH	RAX
	LEA	RAX,[REL .catch]
	PUSH	RAX
	CALL	SysTryHC
	MOV	RDI,R12
%if %3 >= 1
	MOV	RSI,R13
%endif
%if %3 >= 2
	MOV	RDX,R14
%endif
%if %3 >= 3
	MOV	RCX,R15
%endif
%if %3 = 4
	MOV	R8,[RBP-48]
%endif
	CALL	%2
%if %3 = 0
	MOV	[R13],RAX
%elif %3 >= 2
	MOV	[RBX],RAX
%endif
	MOV	R12D,1
	JMP	.cleanup
.catch:
	MOV	RAX,[FS:0]
	MOV	BYTE [RAX+CTask_CATCH_EXCEPT],1
	RET
.accepted:
	CLD
	XOR	R12D,R12D
.cleanup:
	CALL	SysUntryCpp
	MOV	EAX,R12D
	ADD	RSP,8
	POP	R15
	POP	R14
	POP	R13
	POP	R12
	POP	RBX
	POP	RBP
	RET
%endmacro

CALL_HOLYC_CHECKED CallHolyC0Checked,CallHolyC0,0
CALL_HOLYC_CHECKED CallHolyC1Checked,CallHolyC1,1
CALL_HOLYC_CHECKED CallHolyC2Checked,CallHolyC2,2
CALL_HOLYC_CHECKED CallHolyC3Checked,CallHolyC3,3
CALL_HOLYC_CHECKED CallHolyC4Checked,CallHolyC4,4

SECTION .note.GNU-stack noalloc noexec nowrite progbits
