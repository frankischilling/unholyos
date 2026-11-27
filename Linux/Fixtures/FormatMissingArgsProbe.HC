// Supplied-buffer error timing from Terry A. Davis's Kernel/StrPrint.HC,
// TempleOS c26482bb6ad3f80106d28504ec5db3c6a360732c.
asm {
_FORMAT_MISSING_ARGS_RSP::
  LEA RAX,8[RSP]
  RET
}

_extern _FORMAT_MISSING_ARGS_RSP U64 FormatMissingArgsRSP();

U0 FormatMissingArgsProbe()
{
  U8 buf[128],expected[128];
  CExcept *first,*last;
  I64 i;
  U64 before,after,errors=0,bytes=0,stack=0,queue=0;
  Bool returned,caught;
  for (i=0;i<8;i++) {
    MemSet(buf,0x5A,sizeof(buf));
    MemSet(expected,0x5A,sizeof(expected));
    switch (i) {
      case 1: MemCpy(expected,"pre",3); break;
      case 2: MemCpy(expected,"ok=7;bad=",9); break;
      case 3:
      case 4:
      case 5: MemCpy(expected,"left",4); break;
      case 6:
        StrCpy(buf,"saved");
        MemCpy(expected,"saved/",6);
        break;
      case 7: MemCpy(expected,"list=",5); break;
    }
    returned=caught=FALSE;
    first=Fs->next_except;
    last=Fs->last_except;
    before=FormatMissingArgsRSP;
    try {
      switch (i) {
        case 0: StrPrint(buf,"%d"); break;
        case 1: StrPrint(buf,"pre%d"); break;
        case 2: StrPrint(buf,"ok=%d;bad=%d",7); break;
        case 3: StrPrint(buf,"left%*d"); break;
        case 4: StrPrint(buf,"left%.*f"); break;
        case 5: StrPrint(buf,"left%*.*f",8,2); break;
        case 6: CatPrint(buf,"/%d"); break;
        case 7: StrPrint(buf,"list=%Z",0); break;
      }
      returned=TRUE;
    } catch {
      caught=Fs->except_ch=='StrPrint';
      Fs->catch_except=TRUE;
    }
    after=FormatMissingArgsRSP;
    if (caught && !returned)
      errors|=1<<i;
    if (!MemCmp(buf,expected,sizeof(buf)))
      bytes|=1<<i;
    if (before==after)
      stack|=1<<i;
    if (Fs->next_except==first && Fs->last_except==last)
      queue|=1<<i;
  }
  DocClear(,TRUE);
  "FM errors %X\n",errors;
  "FM bytes %X\n",bytes;
  "FM stack %X\n",stack;
  "FM queue %X\n",queue;
  "FM DONE\n";
}

FormatMissingArgsProbe;
