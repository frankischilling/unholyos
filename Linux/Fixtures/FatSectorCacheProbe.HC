// The single-sector allocation cache in Terry A. Davis's DskDrv.HC.
Bool FatSectorEqual(CDrv *dv,I64 sector,U8 *expected,U8 *scratch)
{
  return BlkRead(dv,scratch,sector,1) &&
      !MemCmp(expected,scratch,BLK_SIZE);
}

I64 FatSectorRedSea()
{
  CDrv *dv=Let2Drv('B',FALSE);
  CBlkDev *bd;
  U8 letter='B',*base,*a=MAlloc(BLK_SIZE),*b=MAlloc(BLK_SIZE),
      *want=MAlloc(BLK_SIZE),*scratch=MAlloc(BLK_SIZE);
  I64 c,d,flags,res=0,i,bit;
  Bool mounted;
  U0 (*alloc_cache)(CDrv *drive);
  U0 (*clean_cache)(CDrv *drive,I64 selector);
  U0 (*set_cache)(CDrv *drive,I64 cluster,I64 selector);
  U0 (*free_clus)(CDrv *drive,I64 cluster,I64 count);
  alloc_cache=&DrvFATBlkAlloc;
  clean_cache=&DrvFATBlkClean;
  set_cache=&DrvFATBlkSet;
  free_clus=&RedSeaFreeClus;
  if (dv && dv->data_area+4096>=dv->drv_offset+dv->size) {
    letter='A';
    dv=Let2Drv(letter,FALSE);
  }
  if (!dv) {
    bd=BlkDevNextFreeSlot(letter,BDT_RAM);
    bd->max_blk=8191;
    BlkDevAdd(bd,,FALSE,TRUE);
    dv=Let2Drv(letter,FALSE);
  }
  if (!dv || dv->fs_type!=FSt_REDSEA || dv->fat1!=dv->fat2 ||
      dv->data_area+4096>=dv->drv_offset+dv->size ||
      !a || !b || !want || !scratch)
    goto done;
  bd=dv->bd;
  flags=bd->flags;
  bd->flags&=~BDF_READ_CACHE;
  c=dv->data_area+4095;
  d=c+1;
  if (!BlkRead(dv,a,dv->fat1,1) || !BlkRead(dv,b,dv->fat1+1,1) ||
      (a[511]&0x80) || (b[0]&1))
    goto restore_flags;
  mounted=dv->cur_fat_blk && MSize(dv->cur_fat_blk)>=BLK_SIZE;
  (*alloc_cache)(dv);
  base=dv->cur_fat_blk;
  if (mounted && base && MSize(base)>=BLK_SIZE && !dv->cur_fat_blk_num &&
      !dv->fat_blk_dirty && !MemCmp(base,a,BLK_SIZE))
    res|=1;
  if (!base) goto restore_flags;
  (*set_cache)(dv,c,1);
  if (dv->cur_fat_blk==base && !dv->cur_fat_blk_num &&
      !MemCmp(base,a,BLK_SIZE))
    res|=2;
  base[511]|=0x80;
  dv->fat_blk_dirty|=1;
  if (FatSectorEqual(dv,dv->fat1,a,scratch))
    res|=4;
  MemCpy(want,a,BLK_SIZE);
  want[511]|=0x80;
  (*set_cache)(dv,d,0);
  if (dv->cur_fat_blk==base && dv->cur_fat_blk_num==1 &&
      !dv->fat_blk_dirty && !MemCmp(base,b,BLK_SIZE) &&
      FatSectorEqual(dv,dv->fat1,want,scratch))
    res|=8;
  base[0]|=1;
  dv->fat_blk_dirty|=1;
  (*clean_cache)(dv,1);
  MemCpy(want,b,BLK_SIZE);
  want[0]|=1;
  if (!dv->fat_blk_dirty && FatSectorEqual(dv,dv->fat1+1,want,scratch))
    res|=16;
  MemCpy(base,b,BLK_SIZE);
  dv->fat_blk_dirty|=1;
  (*clean_cache)(dv,3);
  (*set_cache)(dv,c,3);
  MemCpy(base,a,BLK_SIZE);
  dv->fat_blk_dirty|=1;
  (*clean_cache)(dv,0);
  if (!dv->fat_blk_dirty && FatSectorEqual(dv,dv->fat1,a,scratch) &&
      FatSectorEqual(dv,dv->fat1+1,b,scratch))
    res|=32;
  c=ClusAlloc(dv,0,2);
  if (c>=dv->data_area && c+1<dv->drv_offset+dv->size) {
    for (i=0;i<2;i++) {
      bit=c+i-dv->data_area;
      if (!BlkRead(dv,scratch,dv->fat1+bit/4096,1) ||
          !(scratch[(bit%4096)/8] & (1<<(bit%8)))) break;
    }
    if (i==2 && !dv->fat_blk_dirty && dv->cur_fat_blk==base)
      res|=64;
    (*free_clus)(dv,c,2);
    for (i=0;i<2;i++) {
      bit=c+i-dv->data_area;
      if (!BlkRead(dv,scratch,dv->fat1+bit/4096,1) ||
          scratch[(bit%4096)/8] & (1<<(bit%8))) break;
    }
    if (i==2 && !dv->fat_blk_dirty && dv->cur_fat_blk==base)
      res|=128;
  }
restore_flags:
  bd->flags=flags;
done:
  Free(a); Free(b); Free(want); Free(scratch);
  return res;
}

I64 FatSectorFAT32()
{
  CDrv *dv=Let2Drv('C',FALSE);
  CBlkDev *bd;
  U32 *base,*a=MAlloc(BLK_SIZE),*b=MAlloc(BLK_SIZE),
      *want=MAlloc(BLK_SIZE),*scratch=MAlloc(BLK_SIZE);
  I64 res=0,flags,c,d;
  Bool mounted;
  U0 (*alloc_cache)(CDrv *drive);
  U0 (*clean_cache)(CDrv *drive,I64 selector);
  U0 (*set_cache)(CDrv *drive,I64 cluster,I64 selector);
  U0 (*free_clus)(CDrv *drive,I64 cluster);
  alloc_cache=&DrvFATBlkAlloc;
  clean_cache=&DrvFATBlkClean;
  set_cache=&DrvFATBlkSet;
  free_clus=&FAT32FreeClus;
  if (!dv) {
    bd=BlkDevNextFreeSlot('C',BDT_ATA);
    bd->base0=0x1F0;
    bd->base1=0x3F4;
    bd->unit=0;
    BlkDevAdd(bd,,FALSE,FALSE);
    dv=Let2Drv('C',FALSE);
  }
  if (!dv || dv->fs_type!=FSt_FAT32 || dv->fat1==dv->fat2 ||
      !a || !b || !want || !scratch)
    goto done;
  bd=dv->bd;
  flags=bd->flags;
  bd->flags&=~(BDF_READ_ONLY|BDF_READ_CACHE);
  // Entries 127 and 128 are free in both copies of the writable fixture.
  if (!BlkRead(dv,a,dv->fat1,1) || !BlkRead(dv,b,dv->fat1+1,1) ||
      a[127] || b[0] ||
      !FatSectorEqual(dv,dv->fat2,a,scratch) ||
      !FatSectorEqual(dv,dv->fat2+1,b,scratch))
    goto restore_flags;
  mounted=dv->cur_fat_blk && MSize(dv->cur_fat_blk)>=BLK_SIZE;
  (*alloc_cache)(dv);
  base=dv->cur_fat_blk;
  if (mounted && base && MSize(base)>=BLK_SIZE && !dv->cur_fat_blk_num &&
      !dv->fat_blk_dirty && !MemCmp(base,a,BLK_SIZE))
    res|=1;
  if (!base) goto restore_flags;
  (*set_cache)(dv,127,0);
  base[127]=0x0FFFFFFF;
  dv->fat_blk_dirty|=1;
  if (FatSectorEqual(dv,dv->fat1,a,scratch) &&
      FatSectorEqual(dv,dv->fat2,a,scratch))
    res|=2;
  MemCpy(want,a,BLK_SIZE);
  want[127]=0x0FFFFFFF;
  (*clean_cache)(dv,0);
  if ((dv->fat_blk_dirty&1) && FatSectorEqual(dv,dv->fat1,want,scratch) &&
      FatSectorEqual(dv,dv->fat2,a,scratch))
    res|=4;
  (*set_cache)(dv,127,1);
  if (dv->cur_fat_blk==base && !dv->cur_fat_blk_num &&
      (dv->fat_blk_dirty&1) && !MemCmp(base,want,BLK_SIZE))
    res|=8;
  // Give the two copies different valid EOC markers.
  base[127]=0x0FFFFFF8;
  (*clean_cache)(dv,1);
  if (!dv->fat_blk_dirty && FatSectorEqual(dv,dv->fat1,want,scratch)) {
    want[127]=0x0FFFFFF8;
    if (FatSectorEqual(dv,dv->fat2,want,scratch))
      res|=16;
  }
  (*set_cache)(dv,128,1);
  if (dv->cur_fat_blk==base && dv->cur_fat_blk_num==1 &&
      !MemCmp(base,b,BLK_SIZE)) {
    (*set_cache)(dv,127,1);
    if (base[127]==0x0FFFFFF8) {
      (*set_cache)(dv,127,0);
      if (base[127]==0x0FFFFFF8) res|=32;
    }
  }
  // Restore both copies, then exercise a cross-sector dirty flush.
  (*set_cache)(dv,127,3);
  MemCpy(base,a,BLK_SIZE);
  dv->fat_blk_dirty|=1;
  (*clean_cache)(dv,3);
  (*set_cache)(dv,128,3);
  base[0]=0x0FFFFFFF;
  dv->fat_blk_dirty|=1;
  (*set_cache)(dv,127,3);
  MemCpy(want,b,BLK_SIZE);
  want[0]=0x0FFFFFFF;
  if (!dv->fat_blk_dirty && !dv->cur_fat_blk_num &&
      dv->cur_fat_blk==base && !MemCmp(base,a,BLK_SIZE) &&
      FatSectorEqual(dv,dv->fat1+1,want,scratch) &&
      FatSectorEqual(dv,dv->fat2+1,want,scratch))
    res|=64;
  (*set_cache)(dv,128,3);
  MemCpy(base,b,BLK_SIZE);
  dv->fat_blk_dirty|=1;
  (*clean_cache)(dv,3);
  if (!dv->fat_blk_dirty &&
      FatSectorEqual(dv,dv->fat1,a,scratch) &&
      FatSectorEqual(dv,dv->fat2,a,scratch) &&
      FatSectorEqual(dv,dv->fat1+1,b,scratch) &&
      FatSectorEqual(dv,dv->fat2+1,b,scratch))
    res|=128;
  c=ClusAlloc(dv,0,2,TRUE);
  if (c>1 && c<0x0FFFFFF8) {
    d=ClusNumNext(dv,c);
    if (d==c+1 && ClusNumNext(dv,d)==-1 &&
        BlkRead(dv,scratch,dv->fat1+c/128,1) &&
        scratch[c%128]==d &&
        BlkRead(dv,scratch,dv->fat2+c/128,1) &&
        scratch[c%128]==d &&
        BlkRead(dv,scratch,dv->fat1+d/128,1) &&
        scratch[d%128]>=0x0FFFFFF8 &&
        BlkRead(dv,scratch,dv->fat2+d/128,1) &&
        scratch[d%128]>=0x0FFFFFF8 &&
        !dv->fat_blk_dirty && dv->cur_fat_blk==base)
      res|=256;
    (*free_clus)(dv,c);
    if (d==c+1 &&
        BlkRead(dv,scratch,dv->fat1+c/128,1) && !scratch[c%128] &&
        BlkRead(dv,scratch,dv->fat2+c/128,1) && !scratch[c%128] &&
        BlkRead(dv,scratch,dv->fat1+d/128,1) && !scratch[d%128] &&
        BlkRead(dv,scratch,dv->fat2+d/128,1) && !scratch[d%128] &&
        !dv->fat_blk_dirty && dv->cur_fat_blk==base)
      res|=512;
  }
restore_flags:
  bd->flags=flags;
done:
  Free(a); Free(b); Free(want); Free(scratch);
  return res;
}

U0 FatSectorCacheProbe()
{
  I64 red=FatSectorRedSea, fat=FatSectorFAT32;
  "FAT sector cache %X %X\n",red,fat;
}
FatSectorCacheProbe;
