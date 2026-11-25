asm {
_BRANCH_LITERAL::
  JMP 4
  DU8 0xCC,0xCC;
  MOV RAX,41
  RET

_BRANCH_FORWARD::
  JMP @@done
  DU8 127 DUP (0xCC);
@@done:
  MOV RAX,42
  RET

_BRANCH_BACKWARD::
  JMP @@jump
@@done:
  MOV RAX,43
  RET
  DU8 120 DUP (0xCC);
@@jump:
  JMP @@done

_BRANCH_DWORD::
  JMP I32 @@done
  DU8 256 DUP (0xCC);
@@done:
  MOV RAX,44
  RET

_BRANCH_COND::
  XOR RAX,RAX
  JZ @@done
  DU8 0xCC,0xCC;
@@done:
  MOV RAX,45
  RET

  USE16
_BRANCH_WORD::
  JMP I16 @@done
  DU8 256 DUP (0xCC);
@@done:
  RET
}
