U0 FileEditorProbe()
{
  I64 bits=0,num,size;
  U8 *filename,*needle,*data;
  CDoc *doc=DocNew;
  CBlkDev *bd;
  U8 *path="B:/EditTest.TXT";
  DocPrint(doc,"First word\nSecond word\n$$AN,\"Anchor\",A=\"Marker\"$$Last row\n");
  if (DocGoToLine(doc,2) && doc->cur_entry->y==1)
    bits|=1;
  if (DocFind(doc,,"WORD",2) && doc->cur_entry->y==1)
    bits|=2;
  if (DocAnchorFind(doc,"Marker") && !DocAnchorFind(doc,"marker"))
    bits|=4;
  DocDel(doc);
  if (EdLinkCvt("pF:::/Doc/HelpIndex.DD,Ctrl:2",&filename,&needle,&num)==LK_PLAIN_FIND &&
      FileFind(filename) && !StrCmp(needle,"Ctrl") && num==2)
    bits|=8;
  Free(filename);
  Free(needle);
  if (Ed("FL:::/Doc/HelpIndex.DD,2",EDF_BAIL) &&
      !Ed("FF:::/Doc/HelpIndex.DD,MissingFileEditorMarker",EDF_BAIL))
    bits|=16;
  if (!Let2Drv('B',FALSE)) {
    bd=BlkDevNextFreeSlot('B',BDT_RAM);
    bd->max_blk=511;
    BlkDevAdd(bd,,FALSE,TRUE);
  }
  if (FileWrite(path,"EditorSave",10)) {
    if (Ed("PI:B:/EditTest.TXT",EDF_WAS_WRITE)) {
      data=FileRead(path,&size);
      if (data && size==11 && !StrCmp(data,"EditorSaveq"))
        bits|=32;
      Free(data);
    }
    "Editor canceled write test\n";
    Sleep(250);
    if (!Ed("PI:B:/EditTest.TXT",EDF_WAS_WRITE)) {
      data=FileRead(path,&size);
      if (data && size==11 && !StrCmp(data,"EditorSaveq"))
        bits|=64;
      Free(data);
    }
    Del(path,,,FALSE);
  }
  doc=DocNew;
  DocPrint(doc,"$$LK,\"Open editor help\",A=\"FI:::/Doc/HelpIndex.DD\"$$\n");
  if (DocGoToLine(doc,1)) {
    filename=DocEntryLink(doc,doc->cur_entry);
    if (filename && !StrCmp(filename,"FI:::/Doc/HelpIndex.DD"))
      bits|=128;
    Free(filename);
  }
  DocEd(doc);
  DocDel(doc);
  if (!Ed("B:/EditTest.BIN") && !FileFind("B:/EditTest.BIN"))
    bits|=256;
  "File editor %X\n",bits;
}
FileEditorProbe;
