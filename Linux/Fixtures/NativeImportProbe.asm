BITS 64
DEFAULT REL

SECTION .text
GLOBAL NativeImportMain:function
GLOBAL NativeImportPutHoly:function
EXTERN NativeImportRun
EXTERN NativeImportPublicPutS

NativeImportMain:
		PUSH	RBP
		MOV	RBP,RSP
		PUSH	RSI
		PUSH	RDI
		PUSH	R10
		PUSH	R11
		AND	RSP,-16
		CLD
		CALL	NativeImportRun
		LEA	RSP,[RBP-32]
		POP	R11
		POP	R10
		POP	RDI
		POP	RSI
		POP	RBP
		RET

; This import remains the public HolyC entry beside the native PutS import.
NativeImportPutHoly:
		PUSH	RBX
		PUSH	RBP
		PUSH	R12
		PUSH	R13
		PUSH	R14
		PUSH	R15
		PUSH	RDI
		CALL	NativeImportPublicPutS
		CLD
		POP	R15
		POP	R14
		POP	R13
		POP	R12
		POP	RBP
		POP	RBX
		RET

SECTION .note.GNU-stack noalloc noexec nowrite progbits
