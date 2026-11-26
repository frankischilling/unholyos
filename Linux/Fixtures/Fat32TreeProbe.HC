I64 FatTreeLink(CDrv *dv,I64 c)
{
  U32 *buf=MAlloc(BLK_SIZE);
  I64 res=-1;
  if (BlkRead(dv,buf,dv->fat1+c/128,1))
    res=buf[c%128]&0x0FFFFFFF;
  Free(buf);
  return res;
}

Bool FatTreeFile(U8 *name,U8 *buf,I64 n,CDate date)
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

U0 Fat32TreeProbe()
{
  CDrv *dv=Let2Drv('C',FALSE);
  CFile *f;
  CDirEntry de;
  I64 i,c,next,last,blocker,flags,res=0,n=777;
  U8 *buf=MAlloc(5*BLK_SIZE),*tmp=MAlloc(BLK_SIZE),*saved=DirCur,*cwd;
  CDate date=Str2Date("2/29/24");
  if (!dv || dv->fs_type!=FSt_FAT32 || dv->spc!=2) goto done;
  for (i=0;i<5*BLK_SIZE;i++) buf[i]=i*19+(i>>8);
  flags=dv->bd->flags;
  if (!FileFind("C:/FSTREAM.BIN")) {
    dv->bd->flags&=~BDF_READ_ONLY;
    f=FOpen("C:/FSTREAM.BIN","w");
    if (f) {
      if (FBlkWrite(f,buf,0,2)) {
        c=f->de.clus;
        blocker=FileWrite("C:/BLOCKER.BIN",buf,1024,date);
        if (blocker==c+1 && FBlkWrite(f,buf+1024,2,3) && FSize(f)==2560) {
          next=FatTreeLink(dv,c);
          last=FatTreeLink(dv,next);
          if (next>1 && next!=blocker && last>1 &&
              FatTreeLink(dv,last)>=0x0FFFFFF8) res|=1;
        }
      }
      f->flags|=FF_USE_OLD_DATETIME;
      f->de.datetime=date;
      FClose(f);
    }
    if (DirMk("C:/OPS") && DirMk("C:/OPS/SUB") &&
        DirMk("C:/OPS/SUB/DEEP") && DirMk("C:/OPS/EMPTY") && DirMk("C:/PICK") &&
        FileWrite("C:/OPS/A.TXT",buf,n,date) && FileWrite("C:/OPS/B.BIN",buf,n,date) &&
        FileWrite("C:/OPS/SUB/C.TXT",buf,n,date) && FileWrite("C:/OPS/SUB/DEEP/D.TXT",buf,n,date) &&
        FileWrite("C:/OPS/SUB/E.BIN.Z",buf,n,date)) res|=2;
    if (Copy("C:/OPS/*.TXT;*.BIN;!B.BIN","C:/PICK") &&
        Move("C:/PICK/A.TXT","C:/PICK/R.TXT") && !FileFind("C:/PICK/A.TXT") &&
        !FileFind("C:/PICK/B.BIN") && FatTreeFile("C:/PICK/R.TXT",buf,n,date)) res|=4;
    if (!CopyTree("C:/OPS","C:/OPS/SUB/BAD") && !IsDir("C:/OPS/SUB/BAD")) res|=8;
    if (CopyTree("C:/OPS","C:/CLONE")==9 && IsDir("C:/CLONE/EMPTY") &&
        FatTreeFile("C:/CLONE/A.TXT",buf,n,date) && FatTreeFile("C:/CLONE/B.BIN",buf,n,date) &&
        FatTreeFile("C:/CLONE/SUB/C.TXT",buf,n,date) && FatTreeFile("C:/CLONE/SUB/DEEP/D.TXT",buf,n,date) &&
        FatTreeFile("C:/CLONE/SUB/E.BIN.Z",buf,n,date)) res|=16;
    if (CopyTree("C:/OPS/*.TXT","C:/TXT",FALSE)==7 && IsDir("C:/TXT/EMPTY") &&
        FatTreeFile("C:/TXT/A.TXT",buf,n,date) && FatTreeFile("C:/TXT/SUB/C.TXT",buf,n,date) &&
        FatTreeFile("C:/TXT/SUB/DEEP/D.TXT",buf,n,date) &&
        !FileFind("C:/TXT/B.BIN") && !FileFind("C:/TXT/SUB/E.BIN.Z")) res|=32;
    if (Del("C:/CLONE/*.TXT",,,FALSE)==1 && DelTree("C:/CLONE/*.TXT")==2 &&
        !FileFind("C:/CLONE/SUB/C.TXT") && !FileFind("C:/CLONE/SUB/DEEP/D.TXT") &&
        FatTreeFile("C:/CLONE/SUB/E.BIN.Z",buf,n,date)) res|=64;
    if (DelTree("C:/CLONE")==2 && DelTree("C:/TXT")==3 &&
        DelTree("C:/PICK")==1 && DelTree("C:/OPS")==5) res|=128;
    "FAT32 tree mutation %02X\n",res;
  } else
    "FAT32 persisted tree\n";
  dv->bd->flags=flags;
  res=0;
  if (FatTreeFile("C:/FSTREAM.BIN",buf,2560,date)) res|=1;
  if (FatTreeFile("C:/BLOCKER.BIN",buf,1024,date)) res|=2;
  if (FileFind("C:/FSTREAM.BIN",&de,FUF_JUST_FILES)) {
    c=de.clus;
    next=FatTreeLink(dv,c);
    if (next>1 && next!=c+1) res|=4;
    Free(de.full_name);
  }
  f=FOpen("C:/FSTREAM.BIN","r");
  if (f) {
    if (FBlkRead(f,tmp,4,1) && !MemCmp(tmp,buf+2048,512) &&
        FBlkRead(f,tmp,1,1) && !MemCmp(tmp,buf+512,512) &&
        FBlkRead(f,tmp) && !MemCmp(tmp,buf+1024,512)) res|=8;
    FClose(f);
  }
  if (!IsDir("C:/OPS") && !IsDir("C:/PICK") && !IsDir("C:/CLONE") && !IsDir("C:/TXT")) res|=16;
  cwd=DirCur;
  if (!StrCmp(cwd,saved)) res|=32;
  Free(cwd);
done:
  Free(saved);
  Free(tmp);
  Free(buf);
  "FAT32 tree probe %02X\n",res;
}
Fat32TreeProbe;
