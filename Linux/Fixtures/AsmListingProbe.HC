asm {
  LIST
_ASM_LIST_VALUES::
	MOV RAX,42
  RET

  // Values use both byte and scalar columns.
  DU8 0xAB,0;
  DU16 0x1234;
  DU32 0x11223344;
  DU64 0x1122334455667788;
  NOLIST
  DU8 0xCC;
  LIST
_ASM_LIST_TAIL::
  DU8 10 DUP (0x90);
  JMP @@done
  DU8 0;
@@done:
  RET
}
