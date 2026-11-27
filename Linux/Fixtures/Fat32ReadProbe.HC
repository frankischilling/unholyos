Bool FatBytes(U8 *buf,I64 n,I64 pos,I64 tag)
{
  I64 i;
  if (!buf)
    return FALSE;
  for (i=0;i<n;i++,pos++)
    if (buf[i]!=((pos*37+(pos>>8)+tag)&255))
      return FALSE;
  return TRUE;
}

U0 Fat32ReadProbe()
{
  I64 res=0,size,attr,cnt=0;
  CDirEntry de,*tree,*entry,*child;
  CDate date=Str2Date("2/29/24");
  U8 *saved=DirCur,*cwd,*buf,*model,*serial,*blocks=MAlloc(5*BLK_SIZE);
  CDrv *dv;
  CFile *f;
  if (Cd("C:/")) {
    dv=Fs->cur_dv;
    "fat geometry %d %d %d %d %d %d %d\n",dv->fs_type,dv->drv_offset,dv->size,
	dv->spc,dv->root_clus,dv->fat1,dv->data_area;
    if (dv->fs_type==FSt_FAT32 && dv->spc==2 && dv->root_clus==2 &&
	dv->drv_offset==2048 && dv->size==132130 && dv->fat1==2080 &&
	dv->fat2==2593 && dv->data_area==3102)
      res|=1;
    model=DrvModelNum('C');
    serial=DrvSerialNum('C');
    if (dv->bd->dev_id_record && model && *model && serial && *serial)
      res|=4096;
    Free(model);
    Free(serial);
  }
  if (FileFind("C:/FRAG.BIN",&de,FUF_JUST_FILES)) {
    "fat metadata %s %d %d %X %D %T\n",de.name,de.clus,de.size,de.attr,
	de.datetime,de.datetime;
    if (de.clus==5 && de.size==2501 && de.attr==0x20 &&
	de.datetime.date==date.date)
      res|=2;
    Free(de.full_name);
  }
  buf=FileRead("C:/FRAG.BIN",&size,&attr);
  if (size==2501 && attr==0x20 && FatBytes(buf,size,0,1) && !buf[size])
    res|=4;
  Free(buf);
  buf=FileRead("C:/LongNameExample.txt",&size,&attr);
  if (size==333 && FatBytes(buf,size,0,3) && !FileFind("C:/LONGNA~1.TXT"))
    res|=8;
  Free(buf);
  buf=FileRead("C:/EMPTY.TXT",&size);
  if (buf && !size && !*buf)
    res|=16;
  Free(buf);
  tree=FilesFind("C:/*TXT;*.txt;!EMPTY.TXT",FUF_RECURSE);
  for (entry=tree;entry;entry=entry->next) {
    "fat entry %s %d\n",entry->name,entry->size;
    if (!StrCmp(entry->name,"SUB") && entry->size==1024) {
      child=entry->sub;
      if (child && !StrCmp(child->name,"NEST.TXT") && child->parent==entry && !child->next)
	res|=32;
    } else if (!StrCmp(entry->name,"LongNameExample.txt"))
      cnt++;
  }
  if (cnt==1 && tree && tree->next && !tree->next->next)
    res|=64;
  DirTreeDel(tree);
  if (Cd("C:/SUB") && IsDir(".") && FileFind("NEST.TXT")) {
    buf=FileRead("NEST.TXT",&size);
    if (size==611 && FatBytes(buf,size,0,2))
      res|=128;
    Free(buf);
    buf=FileRead("FRAG.BIN",&size);
    if (size==2501 && FatBytes(buf,size,0,1))
      res|=256;
    Free(buf);
  }
  f=FOpen("C:/FRAG.BIN","r");
  if (f) {
    if (FSize(f)==2501 && !f->flags && f->max_blk==4 &&
	FBlkRead(f,blocks,1,3) && f->fblk_num==4 && FatBytes(blocks,1536,512,1) &&
	FBlkRead(f,blocks) && f->fblk_num==5 && FatBytes(blocks,453,2048,1) &&
	!FBlkRead(f,blocks) && FBlkRead(f,blocks,0,1) && FatBytes(blocks,512,0,1))
      res|=512;
    FClose(f);
  }
  f=FOpen("C:/FILE.BIN.C","rc");
  if (f) {
    if (FSize(f)==1200 && (f->flags&FF_CONTIGUOUS) &&
	FBlkRead(f,blocks,0,3) && FatBytes(blocks,1200,0,4))
      res|=1024;
    FClose(f);
  }
  Cd(saved);
  cwd=DirCur;
  if (!StrCmp(cwd,saved))
    res|=2048;
  Free(cwd);
  Free(saved);
  Free(blocks);
  "FAT32 read probe %03X\n",res;
}

Fat32ReadProbe;
