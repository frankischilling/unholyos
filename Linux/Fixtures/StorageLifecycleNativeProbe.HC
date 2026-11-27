// Native guards around reference drive/device teardown.
U0 StorageLifecycleNativeProbe()
{
  CDrv *dv,*alias;
  CBlkDev *bd;
  U8 letter='B',*ram=CAlloc(128*BLK_SIZE),old_let;
  I64 res=0;
  Bool caught=FALSE;
  if (Let2Drv(letter,FALSE)) letter='A';
  if (Let2Drv(letter,FALSE) || !ram) goto done;
  bd=BlkDevNextFreeSlot(letter,BDT_RAM);
  bd->max_blk=127;
  bd->RAM_dsk=ram;
  if (BlkDevAdd(bd,,FALSE,TRUE)!=1) goto done;
  dv=Let2Drv(letter,FALSE);
  if (!dv || !dv->cur_fat_blk) goto done;
  if (DrvLock(dv)) {
    try {
      BlkDevDel(bd);
    } catch {
      caught=Fs->except_ch=='BlkDev';
      Fs->catch_except=TRUE;
    }
    if (caught && dv->bd==bd && bd->owning_task==Fs && DrvUnlock(dv)) res|=1;
  }
  caught=FALSE;
  dv->fat_blk_dirty|=1;
  try {
    DrvDel(dv);
  } catch {
    caught=Fs->except_ch=='Drv';
    Fs->catch_except=TRUE;
  }
  if (caught && dv->fat_blk_dirty==1 && Let2Drv(letter,FALSE)==dv) res|=2;
  caught=FALSE;
  try {
    DrvEnable(letter,FALSE);
  } catch {
    caught=Fs->except_ch=='Drv';
    Fs->catch_except=TRUE;
  }
  if (caught && dv->fs_type==FSt_REDSEA) res|=4;
  dv->fat_blk_dirty=0;
  alias=blkdev.let_to_drv['S'-'A'];
  old_let=Drv2Let(dv);
  if (!Let2Drv('S',FALSE) && DrvMap('S',dv)) {
    BlkDevDel(bd);
    if (blkdev.let_to_drv['S'-'A']==alias && !Let2Drv('S',FALSE) &&
        !Let2Drv(old_let,FALSE)) res|=8;
  } else BlkDevDel(bd);
  if (!bd->bd_signature && !dv->dv_signature && MSize(ram)>=128*BLK_SIZE) {
    MemSet(ram,0x5A,BLK_SIZE);
    if (ram[0]==0x5A && ram[BLK_SIZE-1]==0x5A) res|=16;
  }
done:
  Free(ram);
  "Storage lifecycle native %02X\n",res;
}

StorageLifecycleNativeProbe;
