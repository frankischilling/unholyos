// Native safety and coherence checks; the reference does not maintain the
// sector cache when callers write directly to disk.
U0 FatSectorCacheNativeProbe()
{
  CDrv *dv=Let2Drv('C',FALSE);
  U32 *base,*saved=MAlloc(BLK_SIZE),*want=MAlloc(BLK_SIZE),
      *out=MAlloc(BLK_SIZE);
  I64 flags,res=0;
  U0 (*alloc_cache)(CDrv *drive);
  U0 (*clean_cache)(CDrv *drive,I64 selector);
  U0 (*set_cache)(CDrv *drive,I64 cluster,I64 selector);
  alloc_cache=&DrvFATBlkAlloc;
  clean_cache=&DrvFATBlkClean;
  set_cache=&DrvFATBlkSet;
  if (!dv || dv->fs_type!=FSt_FAT32 || dv->fat1==dv->fat2 ||
      !saved || !want || !out)
    goto done;
  flags=dv->bd->flags;
  dv->bd->flags&=~(BDF_READ_ONLY|BDF_READ_CACHE);
  if (!BlkRead(dv,saved,dv->fat1+1,1) || saved[0] ||
      !BlkRead(dv,out,dv->fat2+1,1) || MemCmp(saved,out,BLK_SIZE))
    goto restore_flags;
  base=dv->cur_fat_blk;
  (*alloc_cache)(dv);
  if (base && dv->cur_fat_blk==base && !dv->cur_fat_blk_num &&
      !dv->fat_blk_dirty)
    res|=1;
  if (!dv->cur_fat_blk) goto restore_flags;
  base=dv->cur_fat_blk;
  (*set_cache)(dv,128,0);
  MemCpy(want,saved,BLK_SIZE);
  want[0]=0x0FFFFFFF;
  if (BlkWrite(dv,want,dv->fat1+1,1) &&
      !MemCmp(base,want,BLK_SIZE) && !dv->fat_blk_dirty &&
      BlkRead(dv,out,dv->fat2+1,1) && !MemCmp(saved,out,BLK_SIZE))
    res|=2;
  (*set_cache)(dv,0,1);
  (*set_cache)(dv,128,1);
  if (!MemCmp(base,saved,BLK_SIZE) &&
      BlkWrite(dv,want,dv->fat1+1,1) && !MemCmp(base,saved,BLK_SIZE) &&
      BlkWrite(dv,saved,dv->fat1+1,1) && !MemCmp(base,saved,BLK_SIZE))
    res|=4;
  if (BlkWrite(dv,want,dv->fat2+1,1) &&
      !MemCmp(base,want,BLK_SIZE))
    res|=8;
  base[0]=0x0FFFFFF8;
  dv->fat_blk_dirty|=1;
  if (!BlkWrite(dv,saved,dv->fat1+1,1) &&
      !BlkWrite(dv,saved,dv->fat2+1,1) &&
      (dv->fat_blk_dirty&1) && base[0]==0x0FFFFFF8 &&
      BlkRead(dv,out,dv->fat2+1,1) && !MemCmp(out,want,BLK_SIZE))
    res|=16;
  MemCpy(base,saved,BLK_SIZE);
  dv->fat_blk_dirty|=1;
  (*clean_cache)(dv,3);
  if (!dv->fat_blk_dirty && dv->cur_fat_blk==base &&
      BlkRead(dv,out,dv->fat1+1,1) && !MemCmp(out,saved,BLK_SIZE) &&
      BlkRead(dv,out,dv->fat2+1,1) && !MemCmp(out,saved,BLK_SIZE))
    res|=32;
restore_flags:
  dv->bd->flags=flags;
done:
  Free(saved); Free(want); Free(out);
  "FAT sector native %X\n",res;
}
FatSectorCacheNativeProbe;
