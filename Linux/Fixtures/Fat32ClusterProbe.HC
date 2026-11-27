I64 FatClusterLink(CDrv *dv,I64 c,I64 copy=0)
{
  U32 *buf=MAlloc(BLK_SIZE);
  I64 res=-1,fat=dv->fat1;
  if (copy) fat=dv->fat2;
  if (BlkRead(dv,buf,fat+c/128,1)) res=buf[c%128]&0x0FFFFFFF;
  Free(buf);
  return res;
}

U0 Fat32ClusterProbe()
{
  CDrv *dv=Let2Drv('C',FALSE);
  CBlkDev *bd;
  CDirEntry de,found;
  I64 c,d,e,i,res=0,flags,n;
  U8 *buf=MAlloc(4*BLK_SIZE),*out=MAlloc(4*BLK_SIZE),*file;
  U0 (*free_clus)(CDrv *drive,I64 start);
  free_clus=&FAT32FreeClus;
  if (!dv) {
    bd=BlkDevNextFreeSlot('C',BDT_ATA);
    bd->base0=0x1F0;
    bd->base1=0x3F4;
    bd->unit=0;
    BlkDevAdd(bd,,FALSE,FALSE);
    dv=Let2Drv('C',FALSE);
  }
  if (!dv || dv->fs_type!=FSt_FAT32) goto done;
  if (Clus2Blk(dv,5)==dv->data_area+5*dv->spc &&
      ClusNumNext(dv,5)==12 && ClusNumNext(dv,5,2)==7 &&
      ClusNumNext(dv,5,3)==-1 && ClusNumNext(dv,5,0)==5)
    res|=1;
  MemSet(out,0xA5,4*BLK_SIZE);
  if (ClusBlkRead(dv,out,5,3)==7 && out[3*BLK_SIZE]==0xA5) {
    for (i=0;i<3*BLK_SIZE;i++)
      if (out[i]!=((i*37+(i>>8)+1)&255)) break;
    if (i==3*BLK_SIZE) res|=2;
  }
  flags=dv->bd->flags;
  dv->bd->flags&=~BDF_READ_ONLY;
  c=ClusAlloc(dv,,2,TRUE);
  d=ClusNumNext(dv,c);
  if (c>1 && d==c+1 && ClusNumNext(dv,d)==-1 &&
      FatClusterLink(dv,c,1)==d && ClusAlloc(dv,c,0)==c)
    res|=4;
  for (i=0;i<4*BLK_SIZE;i++) buf[i]=i*13+(i>>8);
  if (ClusWrite(dv,buf,c,2)==-1 && ClusRead(dv,out,c,2)==-1 &&
      !MemCmp(buf,out,4*BLK_SIZE)) res|=8;
  e=ClusAlloc(dv,d,1);
  if (e>1 && ClusNumNext(dv,d)==e && ClusNumNext(dv,e)==-1 &&
      FatClusterLink(dv,d,1)==e) res|=16;
  MemSet(&de,0,sizeof(CDirEntry));
  StrCpy(de.name,"ClusterEntryLong.BIN");
  de.clus=c;
  de.size=4*BLK_SIZE;
  de.datetime=Str2Date("2/29/24");
  if (!DirNew(dv,"/SUB",&de) &&
      FileFind("C:/SUB/ClusterEntryLong.BIN",&found)) {
    if (found.clus==c && found.size==de.size && found.datetime==de.datetime)
      res|=32;
    Free(found.full_name);
  }
  file=FileRead("C:/SUB/ClusterEntryLong.BIN",&n);
  if (file && n==de.size && !MemCmp(file,buf,n)) res|=64;
  Free(file);
  de.size=BLK_SIZE;
  DirNew(dv,"/SUB",&de,FALSE);
  if (FileFind("C:/SUB/ClusterEntryLong.BIN",&found)) {
    if (found.clus==c && found.size==BLK_SIZE && ClusNumNext(dv,c)==d &&
        ClusNumNext(dv,d)==e) res|=128;
    Free(found.full_name);
  }
  i=ClusAlloc(dv,,1);
  ClusBlkWrite(dv,buf,i,1);
  de.clus=i;
  DirNew(dv,"/SUB",&de);
  if (!FatClusterLink(dv,c) && !FatClusterLink(dv,d) &&
      !FatClusterLink(dv,e) && !FatClusterLink(dv,c,1) &&
      !FatClusterLink(dv,d,1) && !FatClusterLink(dv,e,1) &&
      FileFind("C:/SUB/ClusterEntryLong.BIN",&found)) {
    if (found.clus==i && found.size==BLK_SIZE) res|=256;
    Free(found.full_name);
  }
  if (Del("C:/SUB/ClusterEntryLong.BIN",,,FALSE)==1 &&
      !FatClusterLink(dv,i) && !FatClusterLink(dv,i,1)) res|=512;
  c=ClusAlloc(dv,,2);
  d=ClusNumNext(dv,c);
  (*free_clus)(dv,c);
  if (!FatClusterLink(dv,c) && !FatClusterLink(dv,d) &&
      !FatClusterLink(dv,c,1) && !FatClusterLink(dv,d,1)) res|=1024;
  dv->bd->flags=flags;
done:
  Free(buf);
  Free(out);
  "FAT32 cluster entry %03X\n",res;
}
Fat32ClusterProbe;
