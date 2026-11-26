U0 FileCopyProbe()
{
  I64 res=0,i,size,attr,n=0x10025;
  CDate date;
  CDirEntry de;
  U8 *buf=MAlloc(n),*out;
  CFile *f,*g;

  date.date=740000;
  date.time=123456;
  if (FileAttr("A.B.Z")==RS_ATTR_COMPRESSED &&
	FileAttr("A.B.C")==RS_ATTR_CONTIGUOUS && !FileAttr("A.Z") &&
	FileAttr("A.B",RS_ATTR_CONTIGUOUS|RS_ATTR_COMPRESSED)==RS_ATTR_CONTIGUOUS)
    res|=1;
  for (i=0;i<n;i++)
    buf[i]=i*37+(i>>8);
  if (FileWrite("B:/CopySrc.BIN",buf,n,date) &&
	CopySingle("B:/CopySrc.BIN","B:/CopyDst.BIN")) {
    out=FileRead("B:/CopyDst.BIN",&size,&attr);
    if (out && size==n && !MemCmp(buf,out,n) &&
	  FileFind("B:/CopyDst.BIN",&de,FUF_JUST_FILES)) {
      if (de.datetime==date && de.size==n)
	res|=2;
      Free(de.full_name);
    }
    Free(out);
  }
  if (!CopySingle("B:/CopySrc.BIN","B:/CopySrc.BIN"))
    res|=4;

  if (CopySingle("B:/CopySrc.BIN","B:/CopyDst.BIN.Z")) {
    out=FileRead("B:/CopyDst.BIN.Z",&size,&attr);
    if (out && size==n && (attr & RS_ATTR_COMPRESSED) &&
	  !MemCmp(buf,out,n) && FileFind("B:/CopyDst.BIN.Z",&de,FUF_JUST_FILES)) {
      if (de.datetime==date && de.size<n)
	res|=8;
      Free(de.full_name);
    }
    Free(out);
  }
  if (CopySingle("B:/CopyDst.BIN.Z","B:/CopyDst.BIN")) {
    out=FileRead("B:/CopyDst.BIN",&size,&attr);
    if (out && size==n && !(attr & RS_ATTR_COMPRESSED) && !MemCmp(buf,out,n))
      res|=16;
    Free(out);
  }
  if (CopySingle("B:/CopySrc.BIN","B:/CopyDst.BIN.C")) {
    out=FileRead("B:/CopyDst.BIN.C",&size,&attr);
    if (out && size==n && (attr & RS_ATTR_CONTIGUOUS) && !MemCmp(buf,out,n))
      res|=32;
    Free(out);
  }
  if (CopySingle("::/StartOS.HC.Z","B:/CopyArchive.HC.Z")) {
    f=FOpen("::/StartOS.HC.Z","r");
    g=FOpen("B:/CopyArchive.HC.Z","r");
    if (f && g && FSize(f)==FSize(g) && f->de.datetime==g->de.datetime &&
	  FBlkRead(f,buf,0,2) && FBlkRead(g,buf+2*BLK_SIZE,0,2) &&
	  !MemCmp(buf,buf+2*BLK_SIZE,2*BLK_SIZE))
      res|=64;
    FClose(g);
    FClose(f);
  }
  if (FileWrite("B:/CopyEmpty.BIN",buf,0,date) &&
	CopySingle("B:/CopyEmpty.BIN","B:/CopyEmpty2.BIN")) {
    f=FOpen("B:/CopyEmpty2.BIN","r");
    if (f && !FSize(f) && f->de.datetime==date)
      res|=128;
    FClose(f);
  }
  Free(buf);
  "File copy probe %02X\n",res;
}

FileCopyProbe;
