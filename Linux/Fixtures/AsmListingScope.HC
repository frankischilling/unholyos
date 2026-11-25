asm {
  DU8 65532 DUP (0);
  LIST
  DU32 0x11223344;
  DU64 0x1122334455667788;
  NOLIST
}
asm {
  LIST
  NOP
}
asm {
  DU8 0xDE;
}
I64 AsmListingInline()
{
  asm {
    LIST
    MOV RAX,47
    NOP
    NOLIST
  }
  return 47;
}
"inline listing %d\n", AsmListingInline;
