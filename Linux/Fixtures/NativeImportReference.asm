BITS 64
DEFAULT REL

SECTION .text
GLOBAL NativeImportReferenceMain:function
GLOBAL NativeImportReferencePutS:function
GLOBAL NativeImportReferenceStrCmp:function
EXTERN PutS
EXTERN _STRCMP

; Test-only System V adapters for the pinned guest's HolyC implementations.
%macro SAVE_SYSV 0
		PUSH	RBX
		PUSH	RBP
		PUSH	R12
		PUSH	R13
		PUSH	R14
		PUSH	R15
%endmacro

%macro RESTORE_SYSV 0
		CLD
		POP	R15
		POP	R14
		POP	R13
		POP	R12
		POP	RBP
		POP	RBX
		RET
%endmacro

NativeImportReferenceMain:
		RET

NativeImportReferencePutS:
		SAVE_SYSV
		PUSH	RDI
		CALL	PutS
		RESTORE_SYSV

NativeImportReferenceStrCmp:
		SAVE_SYSV
		PUSH	RSI
		PUSH	RDI
		CALL	_STRCMP
		RESTORE_SYSV

SECTION .note.GNU-stack noalloc noexec nowrite progbits
