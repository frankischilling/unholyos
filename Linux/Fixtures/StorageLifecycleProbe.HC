// Drive and device lifecycle from Terry A. Davis's DskAddDev.HC,
// DskDrv.HC and DskBlkDev.HC.
U0 StorageLifecycleProbe()
{
  CDrv *dv,*slot;
  CBlkDev *bd,*again;
  U8 letter='B',*saved=DirCur,*buf=CAlloc(BLK_SIZE),
      *out=MAlloc(BLK_SIZE),*p,*name,*data;
  I64 res=0,blk,n,size,old_flags;
  Bool a,b,c,d;
  U0 (*drv_del)(CDrv *drive);
  U0 (*drvs_del)(CBlkDev *device);
  U0 (*device_del)(CBlkDev *device);
  U0 (*cache_add)(CDrv *drive,U8 *data,I64 block,I64 count);
  U0 (*cache_fetch)(CDrv *drive,U8 **data,I64 *block,I64 *cnt);
  U0 (*invalidate)(CDrv *drive);
  drv_del=&DrvDel;
  drvs_del=&DrvBlkDevDel;
  device_del=&BlkDevDel;
  cache_add=&DskCacheAdd;
  cache_fetch=&RCache;
  invalidate=&DskCacheInvalidate;

  if (Let2Drv(letter,FALSE)) letter='A';
  if (Let2Drv(letter,FALSE) || !saved || !buf || !out) goto done;
  bd=BlkDevNextFreeSlot(letter,BDT_RAM);
  bd->max_blk=127;
  if (BlkDevAdd(bd,,FALSE,TRUE)!=1) goto done;
  dv=Let2Drv(letter,FALSE);
  if (!dv || dv->bd!=bd || dv->fs_type!=FSt_REDSEA) goto done;
  res|=1;
  slot=dv;
  size=dv->size;
  a=DrvEnable(letter,FALSE);
  b=DrvEnable(letter,FALSE);
  if (a && !b && Let2Drv(letter,FALSE)==dv &&
      dv->fs_type==(FSt_REDSEA|(1<<FStf_DISABLE)) && dv->size==size &&
      BlkRead(dv,out,dv->drv_offset,1)) res|=2;
  c=DrvEnable(letter,TRUE);
  d=DrvEnable(letter,TRUE);
  if (!c && d && dv->fs_type==FSt_REDSEA && dv->size==size &&
      !DrvEnable('?',TRUE)) res|=4;
  if (!Drv(letter)) goto restore;
  MemCpy(buf,"storage lifecycle",18);
  FileWrite("/Life.DD",buf,18);
  data=FileRead("/Life.DD",&n);
  if (data && n==18 && !MemCmp(buf,data,18)) res|=8;
  Free(data);
  old_flags=bd->flags;
  bd->flags|=BDF_READ_CACHE;
  blk=dv->fat1;
  (*cache_add)(dv,buf,blk,1);
  p=out;n=1;
  (*cache_fetch)(dv,&p,&blk,&n);
  if (!n && p==out+BLK_SIZE && !MemCmp(out,buf,BLK_SIZE)) res|=16;
  (*invalidate)(dv);
  blk=dv->fat1;p=out;n=1;
  (*cache_fetch)(dv,&p,&blk,&n);
  if (n==1 && p==out && blk==dv->fat1) res|=32;
  bd->flags=old_flags;
  DirMk("/LifeDir");
  if (Cd("/LifeDir")) {
    DskChg(letter);
    name=DirCur;
    if (Let2Drv()==dv && name && name[0]==letter && !StrCmp(name+1,":/"))
      res|=64;
    Free(name);
  }
  data=FileRead("/Life.DD",&n);
  if (data && n==18 && !MemCmp(buf,data,18)) res|=128;
  Free(data);
restore:
  Cd(saved);
  (*drv_del)(dv);
  if (!Let2Drv(letter,FALSE) && !slot->dv_signature && !slot->bd &&
      !slot->cur_fat_blk && bd->bd_signature==BD_SIGNATURE_VAL) res|=256;
  (*drvs_del)(bd);
  (*device_del)(bd);
  if (!bd->bd_signature && !bd->RAM_dsk && !bd->type) res|=512;
  again=BlkDevNextFreeSlot(letter,BDT_RAM);
  if (again==bd && again->first_drv_let==letter && again->type==BDT_RAM &&
      DrvNextFreeLet(letter)==letter) res|=1024;
  again->max_blk=127;
  if (BlkDevAdd(again,,FALSE,TRUE)==1 && Let2Drv(letter,FALSE)==slot) {
    dv=Let2Drv(letter,FALSE);
    if (dv->cur_fat_blk && !dv->fat_blk_dirty) res|=2048;
    (*device_del)(again);
    if (!Let2Drv(letter,FALSE) && !again->bd_signature) res|=4096;
  }
done:
  Free(saved);
  Free(buf);
  Free(out);
  "Storage lifecycle %04X\n",res;
}

StorageLifecycleProbe;
