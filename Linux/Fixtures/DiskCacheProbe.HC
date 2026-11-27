// Exercise the cache and task-owned storage locks from Terry A. Davis's
// Kernel/BlkDev/DskCache.HC, DskDrv.HC and DskBlkDev.HC.
I64 disk_cache_child_state;

Bool DiskCacheHit(CDrv *dv,I64 blk,U8 *out,U8 expected)
{
  U0 (*fetch)(CDrv *drive,U8 **buf,I64 *block,I64 *cnt);
  U8 *p=out;
  I64 c=blk,n=1,i;
  fetch=&RCache;
  MemSet(out,0xA5,BLK_SIZE);
  (*fetch)(dv,&p,&c,&n);
  if (n || p!=out+BLK_SIZE || c!=blk+1)
    return FALSE;
  for (i=0;i<BLK_SIZE;i++)
    if (out[i]!=expected)
      return FALSE;
  return TRUE;
}

Bool DiskCacheMiss(CDrv *dv,I64 blk,U8 *out)
{
  U0 (*fetch)(CDrv *drive,U8 **buf,I64 *block,I64 *cnt);
  U8 *p=out;
  I64 c=blk,n=1;
  fetch=&RCache;
  MemSet(out,0xA5,BLK_SIZE);
  (*fetch)(dv,&p,&c,&n);
  return n==1 && p==out && c==blk && out[0]==0xA5 &&
      out[BLK_SIZE-1]==0xA5;
}

U0 DiskCacheChild(U8 *opaque)
{
  CDrv *dv=opaque;
  disk_cache_child_state=1;
  if (DrvUnlock(dv) || BlkDevUnlock(dv->bd)) {
    disk_cache_child_state=-1;
    return;
  }
  disk_cache_child_state=2;
  if (DrvLock(dv) && dv->owning_task==Fs &&
      dv->bd->owning_task==Fs)
    disk_cache_child_state=3;
  else
    disk_cache_child_state=-2;
  // Task termination must release the acquired drive and device.
}

Bool DiskCacheContention(CDrv *dv)
{
  CTask *child;
  I64 i;
  Bool waited,unlocked;
  disk_cache_child_state=0;
  if (!DrvLock(dv))
    return FALSE;
  child=Spawn(&DiskCacheChild,dv,"Cache lock",,Fs);
  if (!child) {
    DrvUnlock(dv);
    return FALSE;
  }
  for (i=0;i<500 && disk_cache_child_state>=0 &&
      disk_cache_child_state<2;i++)
    Sleep(1);
  Sleep(1);
  waited=disk_cache_child_state==2 && dv->owning_task==Fs &&
      dv->bd->owning_task==Fs;
  unlocked=DrvUnlock(dv);
  for (i=0;i<500 && TaskValidate(child);i++)
    Sleep(1);
  if (TaskValidate(child))
    return FALSE;
  return waited && unlocked && disk_cache_child_state==3 &&
      !dv->owning_task && !dv->bd->owning_task &&
      !(dv->locked_flags & (1<<DVlf_LOCKED)) &&
      !(dv->bd->locked_flags & (1<<BDlf_LOCKED)) &&
      !(dv->bd->flags & (BDF_INITIALIZED|BDF_INIT_IN_PROGRESS));
}

U0 DiskCacheProbe()
{
  CDrv *dv=Let2Drv('B',FALSE),*dvd=Let2Drv(':');
  CBlkDev *bd;
  CCacheBlk *ctrl,*oldest,*newest;
  U8 *buf=MAlloc(4*BLK_SIZE),*out=MAlloc(4*BLK_SIZE),*p;
  I64 i,c,n,cap,flags,old_cache_size=0,res=0,blk=31;
  Bool acquired,reentered,owned,unlocked;
  U0 (*cache_init)(I64 size_in_U8s);
  U0 (*cache_add)(CDrv *drive,U8 *data,I64 block,I64 count);
  U0 (*cache_invalidate)(CDrv *drive);
  U0 (*cache_fetch)(CDrv *drive,U8 **data,I64 *block,I64 *count);
  U0 (*free_clus)(CDrv *drive,I64 block,I64 count);
  cache_init=&DskCacheInit;
  cache_add=&DskCacheAdd;
  cache_invalidate=&DskCacheInvalidate2;
  cache_fetch=&RCache;
  free_clus=&RedSeaFreeClus;
  if (!dv) {
    bd=BlkDevNextFreeSlot('B',BDT_RAM);
    bd->max_blk=2047;
    BlkDevAdd(bd,,FALSE,TRUE);
    dv=Let2Drv('B',FALSE);
  }
  if (!dv || !dvd || !buf || !out) {
    Free(buf);
    Free(out);
    "Disk cache %04X\n",res;
    return;
  }
  bd=dv->bd;
  flags=bd->flags;
  if (blkdev.cache_base)
    old_cache_size=MSize(blkdev.cache_base);
  (*cache_init)(0x1FFF);
  MemSet(buf,0x11,BLK_SIZE);
  (*cache_add)(dv,buf,blk,1);
  if (!blkdev.cache_base && !blkdev.cache_ctrl &&
      !blkdev.cache_hash_table && DiskCacheMiss(dv,blk,out))
    res|=1;

  (*cache_init)(0x2000);
  ctrl=blkdev.cache_ctrl;
  cap=MSize(blkdev.cache_base)/sizeof(CCacheBlk);
  if (ctrl && blkdev.cache_hash_table && cap>=4 && cap<=128 &&
      sizeof(CCacheBlk)==560 && offset(CCacheBlk.next_hash)==16 &&
      offset(CCacheBlk.body)==48 &&
      blkdev.cache_size==cap*BLK_SIZE &&
      ctrl->next_lru==blkdev.cache_base &&
      ctrl->last_lru==blkdev.cache_base+cap-1 &&
      !ctrl->next_lru->dv &&
      ctrl->next_lru->next_hash==ctrl->next_lru)
    res|=2;
  if (!(res&2)) {
    (*cache_init)(old_cache_size);
    Free(buf);
    Free(out);
    "Disk cache %04X\n",res;
    return;
  }

  (*cache_add)(dv,buf,blk,1);
  MemSet(buf,0x22,BLK_SIZE);
  (*cache_add)(dv,buf,blk+DSK_CACHE_HASH_SIZE,1);
  MemSet(buf,0x33,BLK_SIZE);
  (*cache_add)(dvd,buf,blk,1);
  newest=ctrl->last_lru;
  if (newest->dv==dvd && newest->blk==blk &&
      blkdev.cache_hash_table[2*(blk&(DSK_CACHE_HASH_SIZE-1))]==newest &&
      newest->next_hash->blk==blk+DSK_CACHE_HASH_SIZE &&
      DiskCacheHit(dv,blk,out,0x11) &&
      DiskCacheHit(dv,blk+DSK_CACHE_HASH_SIZE,out,0x22) &&
      DiskCacheHit(dvd,blk,out,0x33))
    res|=4;
  (*cache_invalidate)(dv);
  if (DiskCacheMiss(dv,blk,out) &&
      DiskCacheMiss(dv,blk+DSK_CACHE_HASH_SIZE,out) &&
      DiskCacheHit(dvd,blk,out,0x33))
    res|=8;

  (*cache_init)(0x2000);
  ctrl=blkdev.cache_ctrl;
  MemSet(buf,0x41,BLK_SIZE);
  MemSet(buf+BLK_SIZE,0x42,BLK_SIZE);
  (*cache_add)(dv,buf,blk,2);
  MemSet(buf,0x44,BLK_SIZE);
  (*cache_add)(dv,buf,blk+3,1);
  MemSet(out,0xA5,4*BLK_SIZE);
  p=out;
  c=blk;
  n=4;
  (*cache_fetch)(dv,&p,&c,&n);
  if (p==out+2*BLK_SIZE && c==blk+2 && n==1 &&
      out[0]==0x41 && out[BLK_SIZE-1]==0x41 &&
      out[BLK_SIZE]==0x42 && out[2*BLK_SIZE-1]==0x42 &&
      out[2*BLK_SIZE]==0xA5 && out[3*BLK_SIZE-1]==0xA5 &&
      out[3*BLK_SIZE]==0x44 && out[4*BLK_SIZE-1]==0x44)
    res|=16;

  (*cache_init)(0x2000);
  ctrl=blkdev.cache_ctrl;
  cap=blkdev.cache_size/BLK_SIZE;
  for (i=0;i<cap && i<128;i++) {
    MemSet(buf,i,BLK_SIZE);
    (*cache_add)(dv,buf,blk+i,1);
  }
  oldest=ctrl->next_lru;
  if (oldest->blk==blk && ctrl->last_lru->blk==blk+cap-1 &&
      DiskCacheHit(dv,blk,out,0) && ctrl->next_lru==oldest)
    res|=32;
  MemSet(buf,0x61,BLK_SIZE);
  (*cache_add)(dv,buf,blk,1);
  if (ctrl->last_lru==oldest && ctrl->next_lru->blk==blk+1 &&
      DiskCacheHit(dv,blk,out,0x61))
    res|=64;
  MemSet(buf,0x62,BLK_SIZE);
  (*cache_add)(dv,buf,blk+cap,1);
  if (DiskCacheMiss(dv,blk+1,out) &&
      DiskCacheHit(dv,blk,out,0x61) &&
      DiskCacheHit(dv,blk+cap,out,0x62))
    res|=128;

  (*cache_init)(0x2000);
  bd->flags|=BDF_READ_CACHE;
  c=ClusAlloc(dv,0,2);
  if (c>0) {
    MemSet(buf,0x71,BLK_SIZE);
    MemSet(buf+BLK_SIZE,0x72,BLK_SIZE);
    if (BlkWrite(dv,buf,c,2) &&
        DiskCacheHit(dv,c,out,0x71) &&
        DiskCacheHit(dv,c+1,out,0x72))
      res|=256;
    (*cache_invalidate)(dv);
    if (DiskCacheMiss(dv,c,out) && BlkRead(dv,out,c,2) &&
        !MemCmp(buf,out,2*BLK_SIZE) &&
        DiskCacheHit(dv,c,out,0x71) &&
        DiskCacheHit(dv,c+1,out,0x72))
      res|=512;
    MemSet(buf,0x73,BLK_SIZE);
    if (BlkWrite(dv,buf,c,1) && BlkRead(dv,out,c,1) &&
        !MemCmp(buf,out,BLK_SIZE))
      res|=1024;
    acquired=DrvLock(dv);
    MemSet(buf,0x74,BLK_SIZE);
    owned=BlkWrite(dv,buf,c,1) && BlkRead(dv,out,c,1) &&
        !MemCmp(buf,out,BLK_SIZE) &&
        dv->owning_task==Fs && bd->owning_task==Fs &&
        (dv->locked_flags & (1<<DVlf_LOCKED)) &&
        (bd->locked_flags & (1<<BDlf_LOCKED));
    unlocked=DrvUnlock(dv);
    if (acquired && owned && unlocked)
      res|=65536;
  }
  (*cache_init)(0);
  if (!blkdev.cache_base && !blkdev.cache_ctrl &&
      !blkdev.cache_hash_table && DiskCacheMiss(dv,blk,out))
    res|=2048;
  if (c>0) {
    acquired=DrvLock(dv);
    unlocked=DrvUnlock(dv,TRUE);
    if (acquired && unlocked &&
        !(bd->flags & (BDF_INITIALIZED|BDF_INIT_IN_PROGRESS)) &&
        BlkRead(dv,out,c,1) && !MemCmp(buf,out,BLK_SIZE) &&
        (bd->flags & BDF_INITIALIZED) && !dv->owning_task &&
        !bd->owning_task)
      res|=131072;
    (*free_clus)(dv,c,2);
  }
  acquired=BlkDevLock(bd);
  reentered=BlkDevLock(bd);
  owned=bd->owning_task==Fs && (bd->locked_flags & (1<<BDlf_LOCKED));
  unlocked=BlkDevUnlock(bd);
  if (acquired && !reentered && owned && unlocked &&
      !BlkDevUnlock(bd) && !bd->owning_task &&
      !(bd->locked_flags & (1<<BDlf_LOCKED)))
    res|=4096;
  acquired=DrvLock(dv);
  reentered=DrvLock(dv);
  owned=dv->owning_task==Fs && bd->owning_task==Fs &&
      (dv->locked_flags & (1<<DVlf_LOCKED)) &&
      (bd->locked_flags & (1<<BDlf_LOCKED));
  unlocked=DrvUnlock(dv);
  if (acquired && !reentered && owned && unlocked &&
      !DrvUnlock(dv) && !dv->owning_task && !bd->owning_task &&
      !(dv->locked_flags & (1<<DVlf_LOCKED)) &&
      !(bd->locked_flags & (1<<BDlf_LOCKED)))
    res|=8192;
  acquired=DrvLock(dv);
  DrvsRelease;
  owned=!dv->owning_task && !bd->owning_task &&
      !(dv->locked_flags & (1<<DVlf_LOCKED)) &&
      !(bd->locked_flags & (1<<BDlf_LOCKED)) &&
      !(bd->flags & (BDF_INITIALIZED|BDF_INIT_IN_PROGRESS));
  bd->flags=flags;
  reentered=BlkDevLock(bd);
  BlkDevsRelease;
  if (acquired && owned && reentered && !bd->owning_task &&
      !(bd->locked_flags & (1<<BDlf_LOCKED)) &&
      !(bd->flags & (BDF_INITIALIZED|BDF_INIT_IN_PROGRESS)))
    res|=32768;
  bd->flags=flags;
  if (DiskCacheContention(dv))
    res|=16384;
  bd->flags=flags;
  (*cache_init)(old_cache_size);
  Free(buf);
  Free(out);
  "Disk cache %04X\n",res;
}

DiskCacheProbe;
