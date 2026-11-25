SECTION .rodata
GLOBAL CompilerOpCodes

CompilerOpCodes:
  INCBIN "Compiler/OpCodes.DD"
  DB 0

SECTION .note.GNU-stack noalloc noexec nowrite progbits
