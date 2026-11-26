Bool FatStreamCheck(U8 *name,U8 *buf,I64 n,CDate date)
{
  I64 size;
  CDirEntry de;
  U8 *out=FileRead(name,&size);
  Bool okay=FALSE;
  if (out && size==n && !MemCmp(out,buf,n) &&
      FileFind(name,&de,FUF_JUST_FILES)) {
    okay=de.datetime==date;
    Free(de.full_name);
  }
  Free(out);
  return okay;
}

U0 Fat32StreamProbe()
{
  CDrv *dv=Let2Drv('C',FALSE);
  CFile *f;
  I64 i,flags,res=0;
  U8 *buf=MAlloc(7*BLK_SIZE),*tmp=MAlloc(BLK_SIZE);
  CDate date=Str2Date("2/29/24");
  if (!dv || dv->fs_type!=FSt_FAT32 || dv->spc!=2 ||
      dv->drv_offset!=2048 || dv->size!=132130) goto done;
  for (i=0;i<7*BLK_SIZE;i++) buf[i]=i*19+(i>>8);
  flags=dv->bd->flags;
  if (!FileFind("C:/STREAM.BIN")) {
    dv->bd->flags&=~BDF_READ_ONLY;
    f=FOpen("C:/STREAM.BIN","w");
    if (f) {
      if (FBlkWrite(f,buf,0,3) && FSize(f)==1536 &&
          f->fblk_num==3 && f->file_clus_num==1 &&
          (f->flags&FF_BUF_DIRTY) && FBlkRead(f,tmp,2,1) &&
          !MemCmp(tmp,buf+1024,512)) res|=1;
      if (FBlkWrite(f,buf+1536,3,1) && FBlkWrite(f,buf+2048,4,1) &&
          FSize(f)==2560) res|=2;
      f->flags|=FF_USE_OLD_DATETIME;
      f->de.datetime=date;
      FClose(f);
    }
    f=FOpen("C:/STREAM.BIN","w+");
    if (f) {
      if (FSize(f)==2560 && FBlkWrite(f,buf+2560,5,2) &&
          FSize(f)==3584 && FBlkRead(f,tmp,6,1) &&
          !MemCmp(tmp,buf+3072,512) && FBlkRead(f,tmp,0,1) &&
          !MemCmp(tmp,buf,512)) res|=4;
      f->flags|=FF_USE_OLD_DATETIME;
      f->de.datetime=date;
      FClose(f);
    }
    f=FOpen("C:/FIXED.BIN","w",5);
    if (f) {
      if (FSize(f)==2560 && f->max_blk==4 &&
          !(f->flags&FF_NEW_FILE) && FBlkWrite(f,buf,0,5) &&
          !FBlkWrite(f,buf,5,1) && FBlkRead(f,tmp,4,1) &&
          !MemCmp(tmp,buf+2048,512)) res|=8;
      f->flags|=FF_USE_OLD_DATETIME;
      f->de.datetime=date;
      FClose(f);
    }
    f=FOpen("C:/FIXED.BIN.C","wc",4);
    if (f) {
      if (FSize(f)==2048 && f->max_blk==3 &&
          (f->flags&FF_CONTIGUOUS) && FBlkWrite(f,buf,0,4) &&
          FBlkRead(f,tmp,3,1) && !MemCmp(tmp,buf+1536,512) &&
          !FBlkWrite(f,buf,4,1)) res|=16;
      f->flags|=FF_USE_OLD_DATETIME;
      f->de.datetime=date;
      FClose(f);
    }
    if (FileWrite("C:/ZIP.BIN.Z",buf,2501,date) &&
        Copy("C:/ZIP.BIN.Z","C:/ZIP2.BIN.Z")) res|=32;
    "FAT32 stream mutation %02X\n",res;
  } else
    "FAT32 persisted streams\n";
  dv->bd->flags=flags;
  res=0;
  if (FatStreamCheck("C:/STREAM.BIN",buf,3584,date)) res|=1;
  if (FatStreamCheck("C:/FIXED.BIN",buf,2560,date)) res|=2;
  if (FatStreamCheck("C:/FIXED.BIN.C",buf,2048,date)) res|=4;
  if (FatStreamCheck("C:/ZIP.BIN.Z",buf,2501,date)) res|=8;
  if (FatStreamCheck("C:/ZIP2.BIN.Z",buf,2501,date)) res|=16;
  f=FOpen("C:/STREAM.BIN","r");
  if (f) {
    if (f->max_blk==6 && FBlkRead(f,tmp,6,1) &&
        !MemCmp(tmp,buf+3072,512) && FBlkRead(f,tmp,1,1) &&
        !MemCmp(tmp,buf+512,512) && FBlkRead(f,tmp) &&
        !MemCmp(tmp,buf+1024,512) && !FBlkRead(f,tmp,7,1)) res|=32;
    FClose(f);
  }
done:
  Free(tmp);
  Free(buf);
  "FAT32 stream probe %02X\n",res;
}
Fat32StreamProbe;
