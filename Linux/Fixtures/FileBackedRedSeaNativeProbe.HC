// Native guards for persistent image ownership and bounded device I/O.
_extern _MHEAP_CTRL CHeapCtrl *FileBackedOwner(U8 *addr);
extern CTask *adam_task;

U0 FileBackedMountChild(U8 *opaque)
{
  *opaque=MountFile("B:/FbNative.ISO.C");
}

U0 FileBackedRedSeaNativeProbe()
{
  CDrv *backing,*dv;
  CBlkDev *bd,*created=NULL;
  CFile *f,*w;
  CTask *child;
  U8 *saved=DirCur,*ram=NULL,*buf=MAlloc(BLK_SIZE),*raw,*data,
      *out=MAlloc(2*BLK_SIZE),letter=0,name[STR_LEN];
  I64 res=0,i,n,size,flags;
  Bool caught;

  if (!saved || !buf || !out) goto done;
  backing=Let2Drv('B',FALSE);
  if (!backing) {
    ram=CAlloc(1024*BLK_SIZE);
    if (!ram) goto done;
    created=BlkDevNextFreeSlot('B',BDT_RAM);
    created->RAM_dsk=ram;
    created->max_blk=1023;
    if (BlkDevAdd(created,,FALSE,TRUE)!=1) goto done;
    backing=Let2Drv('B',FALSE);
  }
  if (!backing || backing->bd->type!=BDT_RAM ||
      backing->fs_type!=FSt_REDSEA) goto done;
  for (i=0;i<BLK_SIZE;i++) buf[i]=i*13+7;
  if (!DirMk("B:/FbNativeSrc") ||
      !FileWrite("B:/FbNativeSrc/Payload.BIN",buf,333) ||
      RedSeaISO("B:/FbNative.ISO.C","B:/FbNativeSrc")<=0) goto cleanup;
  raw=FileRead("B:/FbNative.ISO.C",&size);
  if (!raw || size<=89*BLK_SIZE) {
    Free(raw);
    goto cleanup;
  }
  if (FileWrite("B:/FbShort.ISO.C",raw,88*BLK_SIZE)) {
    raw[88*BLK_SIZE+3]=0;
    if (FileWrite("B:/FbBad.ISO.C",raw,size) &&
        !MountFile("B:/FbShort.ISO.C") && !MountFile("B:/FbBad.ISO.C") &&
        !MountFile("B:/FbAbsent.ISO.C") && !Let2Drv('M',FALSE)) res|=1;
  }
  Free(raw);

  child=Spawn(&FileBackedMountChild,&letter,,,Fs);
  if (!child) goto cleanup;
  while (TaskValidate(child)) Yield;
  dv=Let2Drv(letter,FALSE);
  if (!letter || !dv) goto cleanup;
  bd=dv->bd;
  f=bd->file_dsk;
  if (!f) goto cleanup;
  if (FileBackedOwner(f)==adam_task->data_heap &&
      FileBackedOwner(bd->file_dsk_name)==adam_task->data_heap &&
      FileBackedOwner(f->de.full_name)==adam_task->data_heap &&
      FileBackedOwner(f->clus_buf)==adam_task->data_heap &&
      f->dv==backing && FSize(f)==size) res|=2;
  MemSet(out,0xA5,2*BLK_SIZE);
  if (!BlkRead(dv,out,87,1) && !BlkRead(dv,out,bd->max_blk+1,1) &&
      !BlkRead(dv,out,bd->max_blk,2) && !FBlkRead(f,out,f->max_blk+1,1) &&
      BlkRead(dv,out,-1,0) && out[0]==0xA5 && out[2*BLK_SIZE-1]==0xA5) res|=4;
  flags=bd->flags;
  if (!BlkWrite(dv,buf,88,1) && !FBlkWrite(f,buf,88,1)) {
    bd->flags|=BDF_READ_ONLY_OVERRIDE;
    if (!BlkWrite(dv,buf,88,1)) res|=8;
    bd->flags=flags;
  }
  w=FOpen("B:/FbNative.ISO.C","w+");
  if (!w && !FileWrite("B:/FbNative.ISO.C",buf,BLK_SIZE) &&
      !Del("B:/FbNative.ISO.C",,,FALSE) &&
      !BlkWrite(backing,buf,backing->drv_offset,1)) res|=16;
  if (w) FClose(w);
  caught=FALSE;
  try {
    FClose(f);
  } catch {
    caught=Fs->except_ch=='File';
    Fs->catch_except=TRUE;
  }
  if (caught && bd->file_dsk==f) res|=32;
  caught=FALSE;
  try {
    DrvDel(backing);
  } catch {
    caught=Fs->except_ch=='Drv';
    Fs->catch_except=TRUE;
  }
  if (caught && f->dv==backing && Let2Drv('B',FALSE)==backing) res|=64;
  caught=FALSE;
  try {
    BlkDevDel(backing->bd);
  } catch {
    caught=Fs->except_ch=='BlkDev';
    Fs->catch_except=TRUE;
  }
  if (caught && bd->lock_fwding==backing->bd) res|=128;
  caught=FALSE;
  try {
    DrvEnable('B',FALSE);
  } catch {
    caught=Fs->except_ch=='Drv';
    Fs->catch_except=TRUE;
  }
  if (caught && backing->fs_type==FSt_REDSEA) res|=256;
  StrPrint(name,"%C:/Payload.BIN",letter);
  data=FileRead(name,&n);
  if (data && n==333 && !MemCmp(buf,data,n) &&
      BlkRead(dv,out,88,1) && out[3]==0x88) res|=512;
  Free(data);
  Cd(saved);
  Unmount(letter);
  letter=0;
  if (!bd->bd_signature && !bd->file_dsk &&
      FileWrite("B:/FbNative.ISO.C",buf,BLK_SIZE) &&
      Del("B:/FbNative.ISO.C",,,FALSE)==1) res|=1024;
cleanup:
  Cd(saved);
  if (letter && Let2Drv(letter,FALSE)) Unmount(letter);
  Del("B:/FbNative.ISO.C",,,FALSE);
  Del("B:/FbShort.ISO.C",,,FALSE);
  Del("B:/FbBad.ISO.C",,,FALSE);
  DelTree("B:/FbNativeSrc");
  if (created && created->bd_signature==BD_SIGNATURE_VAL) BlkDevDel(created);
done:
  Free(ram);
  Free(saved);
  Free(buf);
  Free(out);
  "File backed RedSea native %03X\n",res;
}

FileBackedRedSeaNativeProbe;
