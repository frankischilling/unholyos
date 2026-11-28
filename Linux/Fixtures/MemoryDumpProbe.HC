// Dump layout from Terry A. Davis's Kernel/KDbg.HC,
// TempleOS c26482bb6ad3f80106d28504ec5db3c6a360732c.
U8 DumpBytes[18]={0,8,0x20,0x24,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
                  0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E};
U8 *DumpPointers[1];

I64 DumpMark()
{
  return 42;
}

U0 MemoryDumpProbe()
{
  U8 *doc,*line,*expected;
  I64 size,checks=0;
  DocClear(,TRUE);
  D(DumpBytes,18);
  Dm(DumpBytes,1);
  DumpPointers[0]=&DumpMark;
  Da(DumpPointers,1);
  doc=DocSave(DocPut,&size);
  if (StrMatch("00000000 00 08 20 24 41 42 43 44 45 46 47 48 49 4A 4B 4C ",doc))
    checks|=1;
  line=StrMatch("00000010 4D 4E ",doc);
  if (line && line[57]=='M' && line[58]=='N' && line[59]=='\n')
    checks|=2;
  expected=MStrPrint("%010X 00 ",DumpBytes);
  if (StrMatch(expected,doc))
    checks|=4;
  Free(expected);
  expected=MStrPrint("%08X:%08X,",DumpPointers,DumpPointers[0]);
  line=StrMatch(expected,doc);
  if (line && StrMatch("DumpMark",line))
    checks|=8;
  Free(expected);
  Free(doc);
  "Memory dump probe %X\n",checks;
}

MemoryDumpProbe;
