// Execute actual DolDoc branches, including a saved and reopened tree.
public U0 DocTreeProbe()
{
  I64 res=0,size,v1,v2;
  CDoc *doc=DocNew,*copy;
  CDocEntry *tree,*start,*end;
  CBlkDev *bd;
  U8 *saved;
  Bool fnew,found,appended;
  doc->flags|=DOCF_NO_CURSOR;
  if (DocTreeMake(doc,"Root/First") &&
      !DocTreeMake(doc,"Root/First") &&
      DocTreeFind(doc,"Root/First",&tree,&start,&end) &&
      tree->type_u8==DOCT_TREE && !StrCmp(tree->tag+3,"First") &&
      start->attr==2 && end->attr==-2) res|=1;
  if (!DocTreeFind(doc,"Root/Missing",&tree,&start,&end) &&
      tree==doc && start==doc && end==doc &&
      DocTreeMake(doc,"Root/Second")) res|=2;
  if (DocTreeWrite(doc,"Root/First",FALSE,"%d+%d;\n",12,30) &&
      DocTreeExe(doc,"Root/First")==42 &&
      DocTreeWrite(doc,"Root/Second",FALSE,"19;\n") &&
      DocTreeExe(doc,"Root/Second")==19) res|=4;
  if (DocTreeAppend(doc,"Root/First",FALSE,"%d*%d;\n",7,8) &&
      DocTreeExe(doc,"Root/First")==56 &&
      DocTreeExe(doc,"Root/Second")==19) res|=8;
  if (DocTreeWrite(doc,"Root/First",FALSE,"23;\n") &&
      DocTreeFind(doc,"Root/First",&tree,&start,&end)) {
    copy=DocCopy(doc,tree,end);
    if (copy && ExeDoc(copy)==23 &&
        DocTreeExe(doc,"Root/Second")==19) res|=16;
    DocDel(copy);
    DocCut(doc,start->next,end->last);
    if (DocTreeFind(doc,"Root/First",&tree,&start,&end) &&
        start->next==end && DocTreeExe(doc,"Root/Second")==19) res|=32;
  }
  if (DocLock(doc)) {
    if (DocTreeFind(doc,"Root") && !DocLock(doc)) res|=64;
    DocUnlock(doc);
  }
  copy=DocNew;
  DocPrint(copy,"throw('Break');");
  if (!ExeDoc(copy) && DocLock(copy)) {
    DocUnlock(copy);
    DocDel(copy);
    copy=DocNew;
    DocPrint(copy,"41+1;");
    if (ExeDoc(copy)==42) res|=256;
  }
  DocDel(copy);
  copy=DocNew("::/TreeReadOnly.HC");
  if (!DocTreeWrite(copy,"Volatile/Value",TRUE,"17;\n") &&
      DocTreeExe(copy,"Volatile/Value")==17 &&
      !FileFind("::/TreeReadOnly.HC")) res|=512;
  DocDel(copy);
  if (!Let2Drv('B',FALSE)) {
    bd=BlkDevNextFreeSlot('B',BDT_RAM);
    bd->max_blk=511;
    BlkDevAdd(bd,,FALSE,TRUE);
  }
  if (DrvIsWritable('B')) {
    Del("B:/TreeTest.DD",,,FALSE);
    fnew=DocTreeFWrite("B:/TreeTest.DD","Config/Value","%d;\n",61);
    found=DocTreeFFind("B:/TreeTest.DD","Config/Value");
    v1=DocTreeFExe("B:/TreeTest.DD","Config/Value");
    appended=DocTreeFAppend("B:/TreeTest.DD","Config/Value","%d;\n",73);
    v2=DocTreeFExe("B:/TreeTest.DD","Config/Value");
    "Tree disk %d %d %d %d %d\n",fnew,found,v1,appended,v2;
    if (!fnew && found && v1==61 && appended && v2==73) {
      saved=FileRead("B:/TreeTest.DD",&size);
      if (saved && size>0 && StrMatch("Config",saved) &&
          StrMatch("61;",saved) && StrMatch("73;",saved)) res|=128;
      Free(saved);
    }
    Del("B:/TreeTest.DD",,,FALSE);
  } else "Tree disk unavailable\n";
  DocDel(doc);
  "Doc tree %X\n",res;
}

DocTreeProbe;
