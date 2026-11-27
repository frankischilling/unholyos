BITS 64
DEFAULT REL

SECTION .text
GLOBAL LateImportConsumerMain:function
GLOBAL LateImportCall:function
EXTERN LateImportTarget

; MAIN is safe before the provider exists. The separate call needs its import.
LateImportConsumerMain:
	INC	QWORD [REL LateImportCount]
	RET

LateImportCall:
	PUSH	7
	CALL	LateImportTarget
	RET

SECTION .bss ALIGN=8
GLOBAL LateImportCount:data
LateImportCount:
	RESQ	1

SECTION .note.GNU-stack noalloc noexec nowrite progbits
