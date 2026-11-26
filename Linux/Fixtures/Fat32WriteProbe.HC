I64 FatWriteLink(CDrv *dv,I64 c,I64 copy=0)
{
  U32 *buf=MAlloc(BLK_SIZE);
  I64 res=-1,fat=dv->fat1;
  if (copy) fat=dv->fat2;
  if (BlkRead(dv,buf,fat+c/128,1))
    res=buf[c%128]&0x0FFFFFFF;
  Free(buf);
  return res;
}

Bool FatWriteFile(U8 *name,U8 *buf,I64 n,CDate date)
{
  I64 size;
  CDirEntry de;
  U8 *out=FileRead(name,&size);
  Bool res=FALSE;
  if (out && size==n && !MemCmp(out,buf,n) &&
      FileFind(name,&de,FUF_JUST_FILES)) {
    res=de.datetime==date;
    Free(de.full_name);
  }
  Free(out);
  return res;
}

U0 Fat32WriteProbe()
{
  CDrv *dv=Let2Drv('C',FALSE);
  I64 i,c,next,last,res=0,flags,sz;
  U8 *buf=MAlloc(2501),*name,*out;
  CDirEntry de;
  CDate date=Str2Date("2/29/24");
  if (!dv || dv->fs_type!=FSt_FAT32 || dv->drv_offset!=2048 ||
      dv->size!=132130) goto done;
  for (i=0;i<2501;i++) buf[i]=i*17+(i>>8);
  flags=dv->bd->flags;
  dv->bd->flags&=~BDF_READ_ONLY;
  if (!FileFind("C:/Work/Keep.BIN")) {
    if (DirMk("C:/Work",40) && DirMk("C:/Work/Grow") &&
        DirMk("C:/Work/Empty")) res|=1;
    c=FileWrite("C:/Work/LongRecordExample.BIN",buf,2501,date);
    if (c>1 && c<0x0FFFFFF8 &&
        FatWriteFile("C:/Work/LongRecordExample.BIN",buf,2501,date)) {
      next=FatWriteLink(dv,c);
      last=FatWriteLink(dv,next);
      if (next>1 && next<0x0FFFFFF8 && last>1 && last<0x0FFFFFF8 &&
          FatWriteLink(dv,last)>=0x0FFFFFF8 &&
          FatWriteLink(dv,c,1)==next && FatWriteLink(dv,next,1)==last &&
          FileWrite("C:/Work/LongRecordExample.BIN",buf,1537,date) &&
          !FatWriteLink(dv,c) && !FatWriteLink(dv,c,1) &&
          !FatWriteLink(dv,next) && !FatWriteLink(dv,next,1) &&
          !FatWriteLink(dv,last) && !FatWriteLink(dv,last,1)) res|=2;
    }
    if (FileWrite("C:/Work/Zip.BIN.Z",buf,2501,date) &&
        FileWrite("C:/Work/Keep.BIN",buf,2501,date) &&
        FileWrite("C:/Work/Zero.TXT",buf,0,date)) res|=4;
    c=FileWrite("C:/Work/SEQ.BIN.C",buf,2501,date);
    if (c>1 && c<0x0FFFFFF8 && FatWriteLink(dv,c)==c+1 &&
        FatWriteLink(dv,c+1)==c+2 && FatWriteLink(dv,c+2)>=0x0FFFFFF8) res|=8;
    for (i=0;i<36;i++) {
      name=MStrPrint("C:/Work/Grow/E%02d.TXT",i);
      c=FileWrite(name,buf,1,date);
      Free(name);
      if (!c) break;
    }
    if (i==36) res|=16;
    c=FileWrite("C:/Work/RemoveMe.TXT",buf,2501,date);
    if (c>1 && c<0x0FFFFFF8 && Del("C:/Work/RemoveMe.TXT",,,FALSE)==1 &&
        !FatWriteLink(dv,c) && !FatWriteLink(dv,c,1)) {
      Del("C:/Work/Empty",,TRUE,FALSE);
      if (!IsDir("C:/Work/Empty")) res|=32;
    }
    "FAT32 mutation %02X\n",res;
  } else
    "FAT32 persisted files\n";
  res=0;
  if (FatWriteFile("C:/Work/Keep.BIN",buf,2501,date)) res|=1;
  if (FatWriteFile("C:/Work/LongRecordExample.BIN",buf,1537,date)) res|=2;
  if (FatWriteFile("C:/Work/Zip.BIN.Z",buf,2501,date)) res|=4;
  if (FatWriteFile("C:/Work/SEQ.BIN.C",buf,2501,date)) res|=8;
  out=FileRead("C:/Work/Zero.TXT",&sz);
  if (out && !sz) res|=16;
  Free(out);
  for (i=0;i<36;i++) {
    name=MStrPrint("C:/Work/Grow/E%02d.TXT",i);
    c=FatWriteFile(name,buf,1,date);
    Free(name);
    if (!c) break;
  }
  if (i==36 && FileFind("C:/Work/Grow",&de,FUF_JUST_DIRS)) {
    c=de.clus;
    next=FatWriteLink(dv,c);
    if (next>1 && next<0x0FFFFFF8 && FatWriteLink(dv,c,1)==next &&
        FatWriteLink(dv,next)>=0x0FFFFFF8 &&
        FatWriteLink(dv,next)==FatWriteLink(dv,next,1)) res|=32;
    Free(de.full_name);
  }
  if (!FileFind("C:/Work/RemoveMe.TXT") && !IsDir("C:/Work/Empty")) res|=64;
  out=FileRead("C:/FRAG.BIN",&sz);
  if (out && sz==2501) {
    for (i=0;i<sz;i++) if (out[i]!=((i*37+(i>>8)+1)&255)) break;
    if (i==sz) res|=128;
  }
  Free(out);
  dv->bd->flags=flags;
done:
  Free(buf);
  "FAT32 file probe %02X\n",res;
}
Fat32WriteProbe;
