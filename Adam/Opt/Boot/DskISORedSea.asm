SECTION .rodata
GLOBAL BDVD_START,BDVD_END

BDVD_START:
	INCBIN	BDVD_FILE
BDVD_END:

SECTION .note.GNU-stack noalloc noexec nowrite progbits
