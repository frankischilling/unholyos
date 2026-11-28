// Exercise Terry A. Davis's Adam/God/GodBible.HC and HolySpirit.HC.
Bool GodBibleName(I64 line,I64 sep,U8 *name)
{
  U8 *val=BibleLine2Verse(line,sep);
  Bool res=val && !StrCmp(val,name);
  if (!res)
    "Bible line %d got [%s]\n",line,val;
  Free(val);
  return res;
}

U0 GodBibleProbe()
{
  I64 bits=0,num,trees=0,key_time,latched_time,mouse_time;
  CDoc *doc=DocNew,*bible;
  CDocEntry *entry;
  CBlkDev *bd;
  U8 *file=NULL,*needle=NULL,*text="One\nTwo\nThree\nFour\n";
  if (!StrCmp(Define("ST_BIBLE_LINES"),"100110") &&
      DefineCnt("ST_BIBLE_BOOKS")==66 && DefineCnt("ST_BIBLE_BOOK_LINES")==67 &&
      FileFind(BIBLE_FILENAME))
    bits|=1;
  if (!BibleLine2Verse(296) && !BibleLine2Verse(100111) &&
      GodBibleName(297,' ',"Genesis") && GodBibleName(300,',',"Genesis,1:1"))
    bits|=2;
  if (GodBibleName(303,'%',"Genesis%201:2") &&
      GodBibleName(5075,' ',"Exodus 1:3"))
    bits|=4;
  if (GodBibleName(100110,' ',"Revelation 22:21"))
    bits|=8;

  BibleVerse(doc,"exodus,1:4",1);
  if (DocFind(doc,,"Benjamin, 1:4 Dan"))
    bits|=16;
  DocClear(doc,TRUE);
  BookLines(doc,300,2);
  if (DocFind(doc,,"In the beginning") && !DocFind(doc,,"without form"))
    bits|=32;
  DocClear(doc,TRUE);
  if (!Let2Drv('B',FALSE)) {
    bd=BlkDevNextFreeSlot('B',BDT_RAM);
    bd->max_blk=511;
    BlkDevAdd(bd,,FALSE,TRUE);
  }
  if (DrvIsWritable('B')) {
    FileWrite("B:/BibleProbe.TXT",text,StrLen(text));
    BookLines(doc,2,2,"B:/BibleProbe.TXT");
    if (DocFind(doc,,"Two") && DocFind(doc,,"Three") &&
        !DocFind(doc,,"One") && !DocFind(doc,,"Four"))
      bits|=64;
    Del("B:/BibleProbe.TXT",,,FALSE);
  }
  if (EdLinkCvt("BF:eXodus,1:4-1:8",&file,&needle,&num)==LK_BIBLE_FIND &&
      num==5068 && StrMatch("/Misc/Bible.TXT.Z",file) && !StrCmp(needle,"1:4"))
    bits|=128;
  Free(file);
  Free(needle);

  bible=BibleDoc;
  if (bible) {
    for (entry=bible->head.next;entry!=bible;entry=entry->next)
      if (entry->type_u8==DOCT_TREE)
        trees++;
    if (DocTreeFind(bible,"Genesis/1") && DocTreeFind(bible,"Revelation/22"))
      bits|=256;
    DocDel(bible);
  }
  DocDel(doc);

  FifoU8Flush(god.fifo);
  GodHexIns("000958");
  DocClear(,TRUE);
  GodBiblePassage;
  if (FifoU8Cnt(god.fifo)==3 && DocFind(DocPut,,"Genesis 1:1") &&
      DocFind(DocPut,,"In the beginning God created"))
    bits|=512;

  DocPutKey(DocPut,0,SC_F7|SCF_SHIFT|SCF_KEY_DESC);
  if (!StrCmp(keydev.desc,"Cmd /God Passage"))
    bits|=1024;
  DocClear(,TRUE);
  key_time=kbd.timestamp;
  latched_time=kbd.new_key_timestamp;
  mouse_time=ms_hard.timestamp;
  kbd.timestamp=kbd.new_key_timestamp=ms_hard.timestamp=0x1A90000;
  DocPutKey(DocPut,0,SC_F7|SCF_SHIFT);
  kbd.timestamp=key_time;
  kbd.new_key_timestamp=latched_time;
  ms_hard.timestamp=mouse_time;
  if (FifoU8Cnt(god.fifo)==3 && DocFind(DocPut,,"Genesis 1:1") &&
      DocFind(DocPut,,"In the beginning God created"))
    bits|=2048;
  FifoU8Flush(god.fifo);
  DocBottom;
  "\nBible probe %X %d\n",bits,trees;
}

GodBibleProbe;
