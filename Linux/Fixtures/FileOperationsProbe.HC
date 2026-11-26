Bool OpsFile(U8 *name,U8 *buf,I64 n,CDate date)
{
  I64 size;
  CDirEntry de;
  U8 *out=FileRead(name,&size);
  Bool res=FALSE;
  if (out && size==n && !MemCmp(buf,out,n) &&
	FileFind(name,&de,FUF_JUST_FILES)) {
    res=de.datetime==date;
    Free(de.full_name);
  }
  Free(out);
  return res;
}

Bool OpsClusterUsed(CDrv *dv,I64 c)
{
  I64 bit=c-dv->data_area;
  return !!(dv->bd->RAM_dsk[dv->fat1*BLK_SIZE+bit/8] & (1<<(bit%8)));
}

U0 FileOperationsProbe()
{
  I64 res=0,i,n=1537,c;
  U8 *buf=MAlloc(n),*saved=DirCur,*cwd;
  CDate date;
  CDirEntry de;
  CDrv *dv;
  date.date=740000;
  date.time=123456;
  for (i=0;i<n;i++)
    buf[i]=i*17+(i>>8);
  if (DirMk("B:/Ops") && DirMk("B:/Ops/Sub",10) &&
	DirMk("B:/Ops/Sub/Deep") && DirMk("B:/Ops/Empty") &&
	DirMk("B:/Pick") && FileFind("B:/Ops/Sub",&de,FUF_JUST_DIRS)) {
    if (de.size==2*BLK_SIZE)
      res|=1;
    Free(de.full_name);
  }
  FileWrite("B:/Ops/A.TXT",buf,n,date);
  FileWrite("B:/Ops/B.BIN",buf,n,date);
  FileWrite("B:/Ops/Sub/C.TXT",buf,n,date);
  FileWrite("B:/Ops/Sub/Deep/D.TXT",buf,n,date);
  FileWrite("B:/Ops/Sub/E.BIN.Z",buf,n,date);
  if (Copy("B:/Ops/*.TXT;*.BIN;!B.BIN","B:/Pick") &&
	OpsFile("B:/Pick/A.TXT",buf,n,date) && !FileFind("B:/Pick/B.BIN"))
    res|=2;
  if (Move("B:/Pick/A.TXT","B:/Pick/R.TXT") &&
	!FileFind("B:/Pick/A.TXT") && OpsFile("B:/Pick/R.TXT",buf,n,date))
    res|=4;
  if (!CopyTree("B:/Ops","B:/Ops/Sub/Bad") && !IsDir("B:/Ops/Sub/Bad"))
    res|=8;
  c=CopyTree("B:/Ops","B:/Clone");
  "tree count %d\n",c;
  if (c==9 && IsDir("B:/Clone/Empty") &&
	OpsFile("B:/Clone/A.TXT",buf,n,date) &&
	OpsFile("B:/Clone/Sub/Deep/D.TXT",buf,n,date) &&
	OpsFile("B:/Clone/Sub/E.BIN.Z",buf,n,date))
    res|=16;
  c=CopyTree("B:/Ops/*.TXT","B:/Txt",FALSE);
  "masked tree count %d\n",c;
  if (c==7 && IsDir("B:/Txt/Empty") &&
	OpsFile("B:/Txt/Sub/C.TXT",buf,n,date) &&
	!FileFind("B:/Txt/Sub/E.BIN.Z") && !FileFind("B:/Txt/B.BIN"))
    res|=32;
  if (Del("B:/Clone/*.TXT",,,FALSE)==1 &&
	!Del("B:/Clone/Sub") && IsDir("B:/Clone/Sub") &&
	DelTree("B:/Clone/*.TXT")==2 && !FileFind("B:/Clone/Sub/C.TXT") &&
	!FileFind("B:/Clone/Sub/Deep/D.TXT") &&
	OpsFile("B:/Clone/Sub/E.BIN.Z",buf,n,date))
    res|=64;
  if (DelTree("B:/Clone")==2 && !IsDir("B:/Clone") &&
	DelTree("B:/Txt")==3 && !IsDir("B:/Txt") &&
	DelTree("B:/Pick")==1 && !IsDir("B:/Pick"))
    res|=128;
  if (Cd("B:/") && FileWrite("Reclaim.BIN",buf,n,date) &&
	FileFind("Reclaim.BIN",&de,FUF_JUST_FILES)) {
    dv=Fs->cur_dv;
    c=de.clus;
    Free(de.full_name);
    if (OpsClusterUsed(dv,c) && OpsClusterUsed(dv,c+3) &&
	Del("Reclaim.BIN")==1 && !FileFind("Reclaim.BIN") &&
	!OpsClusterUsed(dv,c) && !OpsClusterUsed(dv,c+3))
      res|=256;
    if (FileWrite("Guard.BIN",buf,n,date)) {
      dv->bd->flags|=BDF_READ_ONLY;
      try {
	Del("Guard.BIN",,,FALSE);
      } catch
	Fs->catch_except=TRUE;
      dv->bd->flags&=~BDF_READ_ONLY;
      if (OpsFile("Guard.BIN",buf,n,date) && Del("Guard.BIN")==1)
	res|=512;
    }
  }
  Cd(saved);
  if (DelTree("B:/Ops")==5 && !IsDir("B:/Ops"))
    res|=1024;
  cwd=DirCur;
  if (!StrCmp(cwd,saved))
    res|=2048;
  Free(cwd);
  Free(saved);
  Free(buf);
  "File operations probe %03X\n",res;
}

FileOperationsProbe;
