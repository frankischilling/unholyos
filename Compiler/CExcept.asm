BITS 64
SECTION .text
GLOBAL UndefinedExternHC
EXTERN UndefinedExternCpp

UndefinedExternHC:
	MOV	RDI,[RSP]
	AND	RSP,-16
	CALL	UndefinedExternCpp

SECTION .note.GNU-stack noalloc noexec nowrite progbits
