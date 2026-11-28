// Exercises Terry A. Davis's vocabulary commands in Adam/God/HolySpirit.HC.
I64 GodVocabularyProbe()
{
  I64 bits=0,boot_count=god.num_words,i;
  U8 **saved=god.words,*p,*old;
  U64 hash=0xCBF29CE484222325;
  U8 *a="first's Z_7",*b="Second, Second. 42";
  I64 keyboard,mouse,latched;
  CBlkDev *bd;
  if (boot_count && !StrCmp(god.word_file_mask,"/Adam/God/Vocab.DD*") &&
      god.word_fuf_flags==0x281801)
    bits|=1;
  for (i=0;i<boot_count;i++) {
    p=god.words[i];
    do
      hash=(hash^*p)*0x100000001B3;
    while (*p++);
  }
  if (!boot_count) {
    "God vocabulary %d %016X\n",boot_count,hash;
    return bits;
  }
  FifoU8Flush(god.fifo);
  GodHexIns("00000");
  if (GodWordStr==saved[0] && FifoU8Cnt(god.fifo)==3)
    bits|=2;
  old=saved[0];

  try
    GodInit(,"+p");
  catch {
    if (Fs->except_ch=='FUF' && god.words==saved && god.num_words==boot_count)
      bits|=4;
    Fs->catch_except=TRUE;
  }

  if (!Let2Drv('B',FALSE)) {
    bd=BlkDevNextFreeSlot('B',BDT_RAM);
    bd->max_blk=511;
    BlkDevAdd(bd,,FALSE,TRUE);
  }
  if (FileWrite("B:/GVTwo.TXT",b,StrLen(b)) &&
      FileWrite("B:/GVOne.TXT",a,StrLen(a))) {
    if (GodInit("B:/GV*.TXT")==5 && !StrCmp(god.words[0],"Second") &&
        !StrCmp(god.words[1],"Second") && !StrCmp(god.words[2],"42") &&
        !StrCmp(god.words[3],"first's") && !StrCmp(god.words[4],"Z_7"))
      bits|=8;
    if (GodInit("B:/GV*.TXT","-O")==5 && !StrCmp(god.words[0],"first's") &&
        !StrCmp(god.words[4],"42") && StrLen(old))
      bits|=16;
    FifoU8Flush(god.fifo);
    GodHexIns("E");
    if (GodWordStr(4)==god.words[4] && !FifoU8Cnt(god.fifo))
      bits|=32;

    DocClear(,TRUE);
    DocPutKey(DocPut,0,SC_F7|SCF_KEY_DESC);
    if (!StrCmp(keydev.desc,"Cmd /God Word")) {
      PUSHFD
      CLI
      keyboard=kbd.timestamp;
      latched=kbd.new_key_timestamp;
      mouse=ms_hard.timestamp;
      kbd.timestamp=ms_hard.timestamp=kbd.new_key_timestamp=0;
      DocPutKey(DocPut,0,SC_F7);
      kbd.timestamp=keyboard;
      kbd.new_key_timestamp=latched;
      ms_hard.timestamp=mouse;
      POPFD
      if (DocFind(DocPut,,"first's ") && FifoU8Cnt(god.fifo)==7)
        bits|=64;
    }
    Del("B:/GVOne.TXT",,,FALSE);
    Del("B:/GVTwo.TXT",,,FALSE);
  }
  if (!GodInit("B:/GVDoesNotExist.TXT") && !GodWordStr && GodInit==boot_count)
    bits|=128;
  FifoU8Flush(god.fifo);

  DocBottom;
  "\nGod vocabulary %d %016X\n",boot_count,hash;
  return bits;
}

"God vocabulary checks %X\n",GodVocabularyProbe();
