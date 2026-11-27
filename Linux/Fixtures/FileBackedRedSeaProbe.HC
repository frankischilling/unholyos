// MountFile, BlkDevInit and FOpen semantics from Terry A. Davis's
// Adam/ABlkDev/Mount.HC and Kernel/BlkDev/Dsk*.HC.
U0 FileBackedRedSeaProbe()
{
  CDrv *backing,*dv,*slot;
  CBlkDev *bd,*source_bd,*created=NULL;
  CFile *image,*payload;
  CDirEntry de;
  U8 *saved=DirCur,*ram=NULL,*data,*buf=MAlloc(2*BLK_SIZE),
      *out=MAlloc(2*BLK_SIZE),name[STR_LEN],letter=0,again;
  I64 i,n,blocks,res=0;

  if (!saved || !buf || !out) goto done;
  backing=Let2Drv('B',FALSE);
  if (!backing) {
    ram=CAlloc(512*BLK_SIZE);
    if (!ram) goto done;
    created=BlkDevNextFreeSlot('B',BDT_RAM);
    created->RAM_dsk=ram;
    created->max_blk=511;
    if (BlkDevAdd(created,,FALSE,TRUE)!=1) goto done;
    backing=Let2Drv('B',FALSE);
  }
  if (!backing || backing->bd->type!=BDT_RAM ||
      backing->fs_type!=FSt_REDSEA) goto done;
  source_bd=backing->bd;
  for (i=0;i<777;i++) buf[i]=i*37+(i>>8)+19;
  if (!DirMk("B:/FbReadSrc") ||
      !FileWrite("B:/FbReadSrc/Payload.BIN",buf,777)) goto cleanup;
  if (RedSeaISO("B:/FbRead.ISO.C","B:/FbReadSrc")<=0 ||
      !FileFind("B:/FbRead.ISO.C",&de,FUF_JUST_FILES)) goto cleanup;
  blocks=de.size/BLK_SIZE;
  if (de.size%BLK_SIZE || blocks<=88 ||
      !(FileAttr(de.name,de.attr)&RS_ATTR_CONTIGUOUS)) {
    Free(de.full_name);
    goto cleanup;
  }
  Free(de.full_name);
  res|=1;
  letter=MountFile("B:/FbRead");
  if (!(letter>='M' && letter<='P')) goto cleanup;
  dv=Let2Drv(letter,FALSE);
  if (!dv) goto cleanup;
  bd=dv->bd;
  slot=dv;
  if (dv->fs_type==FSt_REDSEA && dv->drv_offset==88 &&
      dv->size==blocks-88 && dv->spc==1 && bd->type==BDT_ISO_FILE_READ &&
      bd->drv_offset==88 && bd->max_blk==blocks-1 && bd->blk_size==BLK_SIZE &&
      (bd->flags&(BDF_READ_ONLY|BDF_INITIALIZED))==
          (BDF_READ_ONLY|BDF_INITIALIZED) && !DrvIsWritable(letter)) res|=2;
  image=bd->file_dsk;
  if (image && image->dv==backing && FSize(image)==blocks*BLK_SIZE &&
      image->max_blk==blocks-1 && (image->flags&FF_CONTIGUOUS) &&
      !(image->flags&FF_WRITE) && bd->file_dsk_name &&
      !StrCmp(bd->file_dsk_name,"B:/FbRead.ISO.C")) res|=4;
  if (bd->lock_fwding==source_bd && BlkDevLock(source_bd)) {
    if (!BlkDevLock(bd) && BlkDevUnlock(source_bd)) res|=8;
    else BlkDevUnlock(source_bd);
  }
  image=FOpen("B:/FbRead.ISO.C","rc");
  if (image) {
    if (FBlkRead(image,buf,88,2) && BlkRead(dv,out,88,2) &&
        !MemCmp(buf,out,2*BLK_SIZE) && out[3]==0x88 &&
        out[510]==0x55 && out[511]==0xAA &&
        FBlkRead(image,buf,blocks-1,1) && BlkRead(dv,out,blocks-1,1) &&
        !MemCmp(buf,out,BLK_SIZE) && !FBlkRead(image,out,blocks,1)) res|=16;
    FClose(image);
  }
  StrPrint(name,"%C:/Payload.BIN",letter);
  for (i=0;i<777;i++) buf[i]=i*37+(i>>8)+19;
  data=FileRead(name,&n);
  if (data && n==777 && !MemCmp(data,buf,n)) res|=32;
  Free(data);
  payload=FOpen(name,"r");
  if (payload) {
    if (FSize(payload)==777 && FBlkRead(payload,out,0,2) &&
        !MemCmp(out,buf,777) && !FBlkWrite(payload,buf,0,1)) res|=64;
    FClose(payload);
  }
  if (Cd(saved)) {
    Unmount(letter);
    if (!Let2Drv(letter,FALSE) && !slot->dv_signature &&
        !bd->bd_signature && !bd->file_dsk && !bd->file_dsk_name &&
        Let2Drv('B',FALSE)==backing && FileFind("B:/FbRead.ISO.C")) res|=128;
    again=MountFile("B:/FbRead.ISO.C");
    if (again==letter) {
      dv=Let2Drv(again,FALSE);
      data=FileRead(name,&n);
      if (dv && dv->bd->file_dsk && data && n==777 &&
          !MemCmp(data,buf,n)) res|=256;
      Free(data);
    }
    letter=again;
  }
cleanup:
  Cd(saved);
  if (letter && Let2Drv(letter,FALSE)) Unmount(letter);
  Del("B:/FbRead.ISO.C",,,FALSE);
  DelTree("B:/FbReadSrc");
  if (created && created->bd_signature==BD_SIGNATURE_VAL) BlkDevDel(created);
done:
  Free(ram);
  Free(saved);
  Free(buf);
  Free(out);
  "File backed RedSea %03X\n",res;
}

FileBackedRedSeaProbe;
