I64 InlineAsmProbe(I64 n)
{
  I64 noreg result=0;
  asm {
    MOV RAX,&n[RBP]
    JMP INLINE_ASM_ADD
  }
  result=99;
  asm {
INLINE_ASM_ADD:
    IMUL2 RAX,2
    ADD RAX,2
    MOV &result[RBP],RAX
    IMPORT PutS;
    MOV RAX,INLINE_ASM_MSG
    PUSH RAX
    CALL PutS
    JMP INLINE_ASM_DONE
INLINE_ASM_MSG:
    DU8 "inline import\n",0;
INLINE_ASM_DONE:
  }
  return result;
}
"inline asm %d\n",InlineAsmProbe(20);
"inline counters %d\n",cnts.time_stamp_freq>0 && sizeof(CCntsGlbls)==65;
