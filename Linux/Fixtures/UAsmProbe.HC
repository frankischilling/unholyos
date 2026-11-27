// Fixed encodings, with padding for the reference Ui's byte display.
U8 ua_code[56]={
  0x48,0x89,0xD8,0x48,0x8B,0x44,0x8D,0xF0,0x49,0x89,0xC1,
  0xB8,0x78,0x56,0x34,0x12,0x48,0x83,0xC0,0xFF,
  0x41,0x51,0x41,0x59,0x48,0x0F,0xBC,0xC0,
  0xD9,0xE1,0xD9,0xF2,0xD9,0xF3,
  0x48,0x8B,0x05,0x20,0,0,0,0xEB,0xFE,0xC3,
  0,0,0,0,0,0,0,0,0,0,0,0};

Bool UACheck(I64 offset,I64 length,U8 *expected)
{
  U8 buf[1024],*p=ua_code+offset;
  I64 dst=0;
  Ui(buf,&p,64,&dst,TRUE);
  return p==ua_code+offset+length && dst==-1 && !StrCmp(buf,expected);
}

I64 UAProbe()
{
  I64 res=0,dst;
  U8 buf[1024],*p,*expected;
  if (UACheck(0,3,"MOV\tU64 RAX,U64 RBX\n")) res|=1;
  if (UACheck(3,5,"MOV\tU64 RAX,U64 F0[RBP+RCX*4]\n")) res|=2;
  if (UACheck(8,3,"MOV\tU64 R9,U64 RAX\n")) res|=4;
  if (UACheck(11,5,"MOV\tU32 EAX,I32 12345678\n")) res|=8;
  if (UACheck(16,4,"ADD\tU64 RAX,I8 FF\n")) res|=16;
  if (UACheck(20,2,"PUSH\tU64 R9\n") &&
      UACheck(22,2,"POP\tU64 R9\n")) res|=32;
  if (UACheck(24,4,"BSF\tU64 RAX,U64 RAX\n")) res|=64;
  if (UACheck(28,2,"FABS\n") && UACheck(30,2,"FPTAN\n") &&
      UACheck(32,2,"FPATAN\n")) res|=128;
  p=ua_code+34;
  Ui(buf,&p,64,&dst,TRUE);
  expected=MStrPrint("MOV\tU64 RAX,U64 [%X]\n",ua_code+73);
  if (p==ua_code+41 && dst==-1 && !StrCmp(buf,expected)) res|=256;
  Free(expected);
  Ui(buf,&p,64,&dst,TRUE);
  if (p==ua_code+43 && dst==ua_code+41 && !StrNCmp(buf,"JMP\tI8 ",7)) res|=512;
  if (UACheck(43,1,"RET\n")) res|=1024;
  p=ua_code;
  Ui(buf,&p);
  if (p==ua_code+3 && StrMatch("4889D8",buf) &&
      StrMatch("MOV",buf) && StrMatch("RAX",buf)) res|=2048;
  return res;
}

"UAsm probe %X\n",UAProbe;
