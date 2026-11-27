BITS 64
DEFAULT REL

SECTION .text
GLOBAL HostModuleMain:function
GLOBAL HostModuleCompareDirect:function
GLOBAL HostModuleCompareIndirect:function
GLOBAL HostModuleStackAlign:function
EXTERN HostModuleRun
EXTERN _STRCMP

; IET_MAIN calls HolyC code; the C++ body uses the System V ABI.
HostModuleMain:
	PUSH	RBP
	MOV	RBP,RSP
	PUSH	RSI
	PUSH	RDI
	PUSH	R10
	PUSH	R11
	AND	RSP,-16
	CLD
	CALL	HostModuleRun
	LEA	RSP,[RBP-32]
	POP	R11
	POP	R10
	POP	RDI
	POP	RSI
	POP	RBP
	RET

; HolyC callees pop their arguments and may use RBX as a scratch register.
HostModuleCompareDirect:
	PUSH	RBX
	PUSH	RSI
	PUSH	RDI
	CALL	_STRCMP
	CLD
	POP	RBX
	RET

HostModuleCompareIndirect:
	MOV	RAX,RDI
	PUSH	RBX
	PUSH	RDX
	PUSH	RSI
	CALL	RAX
	CLD
	POP	RBX
	RET

HostModuleStackAlign:
	MOV	RAX,RSP
	AND	RAX,15
	RET

SECTION .note.GNU-stack noalloc noexec nowrite progbits
