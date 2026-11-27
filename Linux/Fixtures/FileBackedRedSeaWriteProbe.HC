// Writable ISO device and RedSea filesystem operations from Terry A. Davis's
// Kernel/BlkDev/DskBlkDev.HC and Adam/Opt/Boot/DskISORedSea.HC.
Bool FbWriteMatches(U8 *name,U8 *buf,I64 size)
{
  I64 n;
  U8 *data=FileRead(name,&n);
  Bool res=data && n==size && !MemCmp(buf,data,n);
  Free(data);
  return res;
}

U0 FileBackedRedSeaWriteProbe()
{
  CDrv *backing,*dv,*slot;
  CBlkDev *bd,*created=NULL;
  CFile *f,*image=NULL;
  CDirEntry de;
  U8 *saved=DirCur,*ram=NULL,*buf=MAlloc(2*BLK_SIZE),
      *out=MAlloc(2*BLK_SIZE),*requested,letter=0,reader=0,
      dir[STR_LEN],payload[STR_LEN],stream[STR_LEN],gone[STR_LEN];
  I64 i,c=0,root=0,blocks=192,res=0;

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
  if (!backing || backing->fs_type!=FSt_REDSEA ||
      backing->bd->type!=BDT_RAM) goto done;
  letter=DrvNextFreeLet('Q');
  bd=BlkDevNextFreeSlot(letter,BDT_ISO_FILE_WRITE);
  requested=StrNew("B:/FbWrite.ISO.C");
  bd->file_dsk_name=requested;
  bd->drv_offset=88;
  bd->max_blk=blocks-1;
  bd->init_root_dir_blks=2;
  if (BlkDevAdd(bd,,TRUE,TRUE)!=1) {
    if (bd->file_dsk_name==requested) {
      Free(requested);
      bd->file_dsk_name=NULL;
    }
    goto cleanup;
  }
  dv=Let2Drv(letter,FALSE);
  if (!dv) goto cleanup;
  slot=dv;
  root=dv->root_clus;
  if (dv->bd==bd && dv->fs_type==FSt_REDSEA && dv->drv_offset==88 &&
      dv->size==blocks-88 && dv->spc==1 && bd->max_blk==blocks-1 &&
      bd->type==BDT_ISO_FILE_WRITE && bd->blk_size==BLK_SIZE &&
      (bd->flags&BDF_INITIALIZED) && !(bd->flags&BDF_READ_ONLY) &&
      DrvIsWritable(letter)==bd) res|=1;
  f=bd->file_dsk;
  if (f && f->dv==backing && FSize(f)==blocks*BLK_SIZE &&
      f->max_blk==blocks-1 &&
      (f->flags&(FF_WRITE|FF_CONTIGUOUS))==(FF_WRITE|FF_CONTIGUOUS) &&
      bd->lock_fwding==backing->bd &&
      !StrCmp(bd->file_dsk_name,"B:/FbWrite.ISO.C")) res|=2;
  image=FOpen("B:/FbWrite.ISO.C","rc");
  if (image && FSize(image)==blocks*BLK_SIZE &&
      FBlkRead(image,out,64,1) && out[0]==1 &&
      !MemCmp(out+1,"CD001",5) && out[6]==1 &&
      *(out+152)(U32 *)==root &&
      BlkRead(dv,buf,88,1) && FBlkRead(image,out,88,1) &&
      !MemCmp(buf,out,BLK_SIZE) && out[3]==0x88 &&
      *(out+8)(U64 *)==88 && *(out+16)(U64 *)==blocks-88 &&
      *(out+24)(U64 *)==root && out[510]==0x55 && out[511]==0xAA) res|=4;

  StrPrint(dir,"%C:/Nested",letter);
  StrPrint(payload,"%C:/Nested/Payload.BIN",letter);
  StrPrint(stream,"%C:/Stream.BIN.C",letter);
  StrPrint(gone,"%C:/Gone.TXT",letter);
  for (i=0;i<2*BLK_SIZE;i++) buf[i]=i*17+(i>>8)+23;
  if (DirMk(dir) && FileWrite(payload,buf,777) &&
      FbWriteMatches(payload,buf,777) &&
      FileFind(payload,&de,FUF_JUST_FILES)) {
    c=de.clus;
    if (de.size==777 && c>root && c+1<blocks) res|=8;
    Free(de.full_name);
  }
  // Overwrite the first allocated data block after reading it into cache.
  if (c>root && c+1<blocks && BlkRead(dv,out,c,2) &&
      !MemCmp(buf,out,777)) {
    for (i=0;i<BLK_SIZE;i++) buf[i]=i*29+(i>>8)+31;
    if (BlkWrite(dv,buf,c,1) && BlkRead(dv,out,c,2) &&
        !MemCmp(buf,out,777) && FbWriteMatches(payload,buf,777)) res|=16;
    if (image && FBlkRead(image,out,c,2) && !MemCmp(buf,out,777)) res|=32;
  }
  for (i=0;i<2*BLK_SIZE;i++) buf[i]=i*11+(i>>8)+47;
  f=FOpen(stream,"wc",2);
  if (f) {
    if (FSize(f)==2*BLK_SIZE && FBlkWrite(f,buf,0,2) &&
        f->fblk_num==2 && !FBlkWrite(f,buf,2,1) &&
        FBlkRead(f,out,0,2) && !MemCmp(buf,out,2*BLK_SIZE)) res|=64;
    FClose(f);
    if (FbWriteMatches(stream,buf,2*BLK_SIZE)) res|=128;
  }
  if (FileWrite(gone,"delete",6) && Del(gone,,,FALSE)==1 &&
      !FileFind(gone)) res|=256;

  FClose(image);
  image=NULL;
  Cd(saved);
  Unmount(letter);
  if (!Let2Drv(letter,FALSE) && !slot->dv_signature &&
      !bd->bd_signature && !bd->file_dsk && !bd->file_dsk_name &&
      FileFind("B:/FbWrite.ISO.C",&de,FUF_JUST_FILES)) {
    if (de.size==blocks*BLK_SIZE) res|=512;
    Free(de.full_name);
  }
  letter=0;
  reader=MountFile("B:/FbWrite.ISO.C");
  if (reader>='M' && reader<='P' && Let2Drv(reader,FALSE)) {
    StrPrint(stream,"%C:/Stream.BIN.C",reader);
    StrPrint(payload,"%C:/Nested/Payload.BIN",reader);
    StrPrint(gone,"%C:/Gone.TXT",reader);
    if (FbWriteMatches(stream,buf,2*BLK_SIZE)) {
      for (i=0;i<777;i++) buf[i]=i*17+(i>>8)+23;
      for (i=0;i<BLK_SIZE;i++) buf[i]=i*29+(i>>8)+31;
      if (FbWriteMatches(payload,buf,777) && !FileFind(gone) &&
          !DrvIsWritable(reader)) res|=1024;
    }
  }
cleanup:
  FClose(image);
  Cd(saved);
  if (reader && Let2Drv(reader,FALSE)) Unmount(reader);
  if (letter && Let2Drv(letter,FALSE)) Unmount(letter);
  Del("B:/FbWrite.ISO.C",,,FALSE);
  if (created && created->bd_signature==BD_SIGNATURE_VAL) BlkDevDel(created);
done:
  Free(ram);
  Free(saved);
  Free(buf);
  Free(out);
  "File backed RedSea write %03X\n",res;
}

FileBackedRedSeaWriteProbe;
