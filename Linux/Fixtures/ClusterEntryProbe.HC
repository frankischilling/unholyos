// The public cluster and directory-entry interfaces in KernelC.HH.
Bool ClusterEntryUsed(CDrv *dv,I64 c)
{
  I64 bit=c-dv->data_area;
  return !!(dv->bd->RAM_dsk[dv->fat1*BLK_SIZE+bit/8] & (1<<(bit%8)));
}

U0 ClusterEntryProbe()
{
  CDrv *dv=Let2Drv('B',FALSE);
  CBlkDev *bd;
  CDirEntry de,found;
  I64 c,d,e,i,n,res=0;
  U8 *buf=MAlloc(2*BLK_SIZE),*out=MAlloc(2*BLK_SIZE),*name,*file;
  U0 (*free_clus)(CDrv *drive,I64 start,I64 count);
  free_clus=&RedSeaFreeClus;
  if (!dv) {
    bd=BlkDevNextFreeSlot('B',BDT_RAM);
    bd->max_blk=2047;
    BlkDevAdd(bd,,FALSE,TRUE);
    dv=Let2Drv('B');
  }
  if (dv->fs_type==FSt_REDSEA && dv->spc==1) res|=1;
  c=ClusAlloc(dv,,2,TRUE);
  if (c>0 && ClusterEntryUsed(dv,c) && ClusterEntryUsed(dv,c+1) &&
      Clus2Blk(dv,c)==c && ClusNumNext(dv,c)==c+1 &&
      ClusNumNext(dv,c,2)==c+2 && ClusNumNext(dv,c,0)==c &&
      ClusAlloc(dv,c,0)==c)
    res|=2;
  for (i=0;i<2*BLK_SIZE;i++) buf[i]=i*17+(i>>8);
  if (!ClusWrite(dv,buf,c,2) && ClusRead(dv,out,c,2)==c+2 &&
      !MemCmp(buf,out,2*BLK_SIZE)) res|=4;
  MemSet(out,0xA5,2*BLK_SIZE);
  if (ClusBlkRead(dv,out,c,1)==c+1 && !MemCmp(buf,out,BLK_SIZE) &&
      out[BLK_SIZE]==0xA5 && ClusBlkRead(dv,out,c,0)==c &&
      ClusBlkWrite(dv,buf,c,0)==c) res|=8;
  MemSet(&de,0,sizeof(CDirEntry));
  StrCpy(de.name,"Cluster.BIN");
  de.clus=c;
  de.size=2*BLK_SIZE;
  de.datetime.date=740000;
  de.datetime.time=123456;
  if (!DirNew(dv,"/",&de) && de.attr&RS_ATTR_CONTIGUOUS &&
      FileFind("B:/Cluster.BIN",&found)) {
    if (found.clus==c && found.size==de.size && found.datetime==de.datetime &&
        found.attr==de.attr) res|=16;
    Free(found.full_name);
  }
  file=FileRead("B:/Cluster.BIN",&n);
  if (file && n==de.size && !MemCmp(file,buf,n)) res|=32;
  Free(file);
  d=ClusAlloc(dv,0,1);
  ClusBlkWrite(dv,buf,d,1);
  de.clus=d;
  de.size=BLK_SIZE;
  DirNew(dv,"/",&de,FALSE);
  if (ClusterEntryUsed(dv,c) && ClusterEntryUsed(dv,c+1) &&
      ClusterEntryUsed(dv,d) && FileFind("B:/Cluster.BIN",&found)) {
    if (found.clus==d && found.size==BLK_SIZE) res|=64;
    Free(found.full_name);
  }
  (*free_clus)(dv,c,2);
  if (!ClusterEntryUsed(dv,c) && !ClusterEntryUsed(dv,c+1)) res|=128;
  e=ClusAlloc(dv,0,2);
  ClusWrite(dv,buf,e,2);
  de.clus=e;
  de.size=2*BLK_SIZE;
  DirNew(dv,"/",&de);
  if (!ClusterEntryUsed(dv,d) && ClusterEntryUsed(dv,e) &&
      ClusterEntryUsed(dv,e+1) && FileFind("B:/Cluster.BIN",&found)) {
    if (found.clus==e && found.size==de.size) res|=256;
    Free(found.full_name);
  }
  if (Del("B:/Cluster.BIN",,,FALSE)==1 && !ClusterEntryUsed(dv,e) &&
      !ClusterEntryUsed(dv,e+1)) res|=512;
  if (DirMk("B:/ClusterDir")) {
    for (i=0;i<10;i++) {
      name=MStrPrint("E%02d.TXT",i);
      StrCpy(de.name,name);
      Free(name);
      de.clus=-1;
      de.size=0;
      DirNew(dv,"/ClusterDir",&de);
    }
    if (FileFind("B:/ClusterDir",&found,FUF_JUST_DIRS)) {
      if (found.size>=2*BLK_SIZE && FileFind("B:/ClusterDir/E09.TXT"))
        res|=1024;
      Free(found.full_name);
    }
    if (DelTree("B:/ClusterDir")==10 && !IsDir("B:/ClusterDir"))
      res|=2048;
  }
  Free(buf);
  Free(out);
  "Cluster entry %03X\n",res;
}
ClusterEntryProbe;
