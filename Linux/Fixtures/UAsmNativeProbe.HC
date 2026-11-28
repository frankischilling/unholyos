// Undefined dictionary entries advance by one byte; valid code follows.
U8 ua_bad[16]={0x0F,0xFF,0x90,0x90,0xC3,0,0,0,0,0,0,0,0,0,0,0};
U8 ua_prefix[16]={0x66,0xB8,0x34,0x12,0x48,0xB8,0x78,0x56,0x34,0x12,0,0,0,0,0xC3,0};
I64 UANative()
{
  U8 buf[1024],*p=ua_bad;
  I64 dst,res=0;
  Ui(buf,&p,64,&dst,TRUE);
  if (p==ua_bad+1 && dst==-1 && !StrCmp(buf,"DU8\t0F\n")) res|=1;
  p=ua_bad+2;
  Ui(buf,&p,64,&dst,TRUE);
  if (p==ua_bad+3 && !StrCmp(buf,"NOP\n")) res|=2;
  p=ua_prefix;
  Ui(buf,&p,64,&dst,TRUE);
  if (p==ua_prefix+4 && !StrCmp(buf,"MOV\tU16 AX,I16 1234\n")) res|=4;
  Ui(buf,&p,64,&dst,TRUE);
  if (p==ua_prefix+14 && !StrCmp(buf,"MOV\tU64 RAX,I64 0000000012345678\n")) res|=8;
  if (U(ua_bad+2,3)==ua_bad+5 && U(ua_bad,0)==ua_bad) res|=16;
  if (Un(ua_bad+2,3)==3 && Un(ua_prefix,1)==1 && Un(ua_bad,0)==0) res|=32;
  return res;
}
"UAsm native %X\n",UANative;
