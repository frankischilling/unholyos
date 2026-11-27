BITS 64
DEFAULT REL

SECTION .text
GLOBAL LoadApiMain:function
GLOBAL LoadApiCall:function
EXTERN LateImportTarget

LoadApiMain:
	INC	QWORD [REL LoadApiCount]
	RET

LoadApiCall:
	PUSH	7
	CALL	LateImportTarget
	RET

SECTION .data ALIGN=64
GLOBAL LoadApiAnchor:data
LoadApiAnchor:
	DQ	LoadApiAnchor

SECTION .bss ALIGN=8
GLOBAL LoadApiCount:data
LoadApiCount:
	RESQ	1

SECTION .note.GNU-stack noalloc noexec nowrite progbits
