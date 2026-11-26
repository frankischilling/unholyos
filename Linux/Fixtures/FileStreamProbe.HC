U0 FileStreamProbe()
{
  I64 res=0,i,size;
  U8 buf[3*BLK_SIZE],out[3*BLK_SIZE];
  CFile *f,*g;
  CDirEntry de;

  if (sizeof(CFile)==168 && offset(CFile.de)==8 &&
	offset(CFile.dv)==120 && offset(CFile.fblk_num)==128 &&
	offset(CFile.clus)==136 && offset(CFile.file_clus_num)==144 &&
	offset(CFile.max_blk)==152 && offset(CFile.clus_buf)==160 && !FSize(NULL))
    res|=1;
  FClose(NULL);

  f=FOpen("::/StartOS.HC.Z","rc");
  if (f) {
    size=FSize(f);
    if (size>BLK_SIZE && size<=2*BLK_SIZE && f->flags==FF_CONTIGUOUS &&
	f->max_blk==1 && f->fblk_num==0 && FBlkRead(f,buf) &&
	f->fblk_num==1 && FBlkRead(f,buf+BLK_SIZE) && f->fblk_num==2 &&
	!FBlkRead(f,out) && f->fblk_num==2 && !FBlkWrite(f,out,0) &&
	FBlkRead(f,out,0,2) && !MemCmp(buf,out,2*BLK_SIZE) &&
	*(buf+0)(U64 *)==size && *(buf+8)(U64 *)>size)
      res|=2;
    FClose(f);
  }

  f=FOpen("B:/StreamProbe.BIN","w",3);
  if (f) {
    if (FSize(f)==3*BLK_SIZE && f->max_blk==2 &&
	(f->flags & (FF_WRITE|FF_NEEDS_WRITE|FF_CONTIGUOUS))==25 &&
	!(f->flags & FF_NEW_FILE) &&
	FileFind("B:/StreamProbe.BIN",&de,FUF_JUST_FILES)) {
      if (de.size==3*BLK_SIZE && de.clus==f->de.clus)
	res|=4;
      Free(de.full_name);
    }
    for (i=0;i<3*BLK_SIZE;i++)
      buf[i]=i*37+(i>>8);
    if (FBlkWrite(f,buf) && f->fblk_num==1 &&
	FBlkWrite(f,buf+2*BLK_SIZE,2) && f->fblk_num==3 &&
	!FBlkWrite(f,buf) && f->fblk_num==3 &&
	FBlkWrite(f,buf+BLK_SIZE,1) && f->fblk_num==2 &&
	FBlkRead(f,out,0,3) && !MemCmp(buf,out,3*BLK_SIZE))
      res|=8;
    f->flags|=FF_USE_OLD_DATETIME;
    f->de.datetime.date=740000;
    f->de.datetime.time=123456;
    f->de.size=1200;
    FClose(f);
  }

  f=FOpen("B:/StreamProbe.BIN","w+",1);
  if (f) {
    if (FSize(f)==1200 && f->max_blk==2 &&
	f->de.datetime.date==740000 && f->de.datetime.time==123456 &&
	FBlkRead(f,out,0,3) && !MemCmp(buf,out,3*BLK_SIZE))
      res|=16;
    MemSet(buf+BLK_SIZE,0xA5,BLK_SIZE);
    if (FBlkWrite(f,buf+BLK_SIZE,1) && f->fblk_num==2 &&
	FBlkRead(f,out,0,3) && !MemCmp(buf,out,3*BLK_SIZE))
      res|=32;
    FClose(f);
  }

  f=FOpen("B:/StreamProbe.BIN","wc",1);
  if (f) {
    if (FSize(f)==BLK_SIZE && FBlkWrite(f,buf) && !FBlkWrite(f,buf))
      res|=64;
    FClose(f);
  }
  f=FOpen("B:/StreamProbe.BIN","r");
  if (f) {
    if (FSize(f)==BLK_SIZE && FBlkRead(f,out) &&
	!MemCmp(buf,out,BLK_SIZE) && !FBlkRead(f,out))
      res|=128;
    FClose(f);
  }

  f=FOpen("B:/StreamEmpty.BIN","w");
  if (f) {
    FClose(f);
    f=FOpen("B:/StreamEmpty.BIN","r");
    if (f && !FSize(f) && !FBlkRead(f,out))
      res|=256;
    FClose(f);
  }

  f=FOpen("::/StartOS.HC.Z","r");
  if (f) {
    size=FSize(f);
    g=FOpen("B:/StreamArchive.HC.Z","w",2);
    if (g) {
      if (FBlkRead(f,buf,0,2) && FBlkWrite(g,buf,0,2)) {
	g->flags|=FF_USE_OLD_DATETIME;
	g->de.datetime=f->de.datetime;
	g->de.size=size;
	g->de.attr=f->de.attr;
	FClose(g);
	g=FOpen("B:/StreamArchive.HC.Z","r");
	if (g && FSize(g)==size && FBlkRead(g,out,0,2) &&
	    !MemCmp(buf,out,2*BLK_SIZE))
	  res|=512;
      }
      FClose(g);
    }
    FClose(f);
  }
  "File stream probe %03X\n",res;
}

FileStreamProbe;
