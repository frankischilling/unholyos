BITS 64
DEFAULT REL

SECTION .text
GLOBAL LateImportProviderMain:function
GLOBAL LateImportTarget:function

LateImportProviderMain:
	RET

LateImportTarget:
	MOV	RAX,[RSP+8]
	ADD	RAX,35
	RET	8

SECTION .note.GNU-stack noalloc noexec nowrite progbits
