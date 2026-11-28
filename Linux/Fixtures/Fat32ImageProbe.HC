// Contiguous file-backed images on the ATA FAT32 filesystem.
Bool FatImagePayload(U8 letter,U8 *expected,I64 size)
{
  U8 path[STR_LEN],*data;
  I64 n;
  StrPrint(path,"%C:/Payload.BIN",letter);
  data=FileRead(path,&n);
  Bool okay=data && n==size && !MemCmp(data,expected,n);
  Free(data);
  return okay;
}

U0 Fat32ImageProbe()
{
  CDrv *backing=Let2Drv('C',FALSE),*dv;
  CBlkDev *bd,*disk;
  CDirEntry de;
  CFile *f=NULL;
  U8 *saved=DirCur,*buf=MAlloc(2*BLK_SIZE),*out=MAlloc(2*BLK_SIZE),
      *name="C:/FatImage.ISO.C",path[STR_LEN],letter=0;
  I64 i,j,c,flags=0,res=0,blocks=192;
  Bool reboot=FALSE,changed=FALSE;
  if (!backing) {
    disk=BlkDevNextFreeSlot('C',BDT_ATA);
    disk->base0=0x1F0;
    disk->base1=0x3F4;
    disk->unit=0;
    BlkDevAdd(disk,,FALSE,FALSE);
    backing=Let2Drv('C',FALSE);
  }
  if (!backing || backing->fs_type!=FSt_FAT32 || !saved || !buf || !out)
    goto done;
  disk=backing->bd;
  flags=disk->flags;
  disk->flags&=~BDF_READ_ONLY;
  changed=TRUE;
  for (i=0;i<2*BLK_SIZE;i++) buf[i]=i*41+(i>>8)+9;
  reboot=FileFind(name);
  if (reboot) {
    letter=MountFile(name);
    dv=Let2Drv(letter,FALSE);
    if (letter && dv && dv->size==blocks-88 && dv->bd->file_dsk->dv==backing)
      res|=1;
    if (letter && FatImagePayload(letter,buf,777)) res|=2;
    f=FOpen(name,"rc");
    if (f && FSize(f)==blocks*BLK_SIZE && FBlkRead(f,out,64,1) &&
        out[0]==1 && !MemCmp(out+1,"CD001",5)) res|=4;
    goto cleanup;
  }
  letter=DrvNextFreeLet('Q');
  bd=BlkDevNextFreeSlot(letter,BDT_ISO_FILE_WRITE);
  bd->drv_offset=88;
  bd->max_blk=blocks-1;
  bd->init_root_dir_blks=2;
  bd->file_dsk_name=StrNew(name);
  if (BlkDevAdd(bd,,TRUE,TRUE)!=1) goto cleanup;
  dv=Let2Drv(letter,FALSE);
  if (!dv) goto cleanup;
  if (dv->fs_type==FSt_REDSEA && dv->size==blocks-88 &&
      bd->file_dsk && bd->file_dsk->dv==backing &&
      FSize(bd->file_dsk)==blocks*BLK_SIZE && bd->lock_fwding==disk)
    res|=1;
  if (FileFind(name,&de,FUF_JUST_FILES)) {
    c=de.clus;
    j=(blocks+backing->spc-1)/backing->spc;
    for (i=1;i<j;i++)
      if (ClusNumNext(backing,c+i-1)!=c+i) break;
    if (de.size==blocks*BLK_SIZE && i==j &&
        ClusNumNext(backing,c+j-1)==-1 &&
        (FileAttr(de.name,de.attr)&RS_ATTR_CONTIGUOUS)) res|=2;
    Free(de.full_name);
  }
  f=FOpen(name,"rc");
  if (f && FBlkRead(f,out,64,1) && out[0]==1 &&
      !MemCmp(out+1,"CD001",5) && FBlkRead(f,out,88,1) &&
      out[3]==0x88 && out[510]==0x55 && out[511]==0xAA) res|=4;
  StrPrint(path,"%C:/Payload.BIN",letter);
  if (FileWrite(path,buf,777) && FatImagePayload(letter,buf,777)) res|=8;
  if (FileFind(path,&de,FUF_JUST_FILES)) {
    c=de.clus;
    Free(de.full_name);
    // Change and restore a partial file through absolute device blocks.
    MemSet(out,0x6B,BLK_SIZE);
    if (BlkWrite(dv,out,c,1) && f && FBlkRead(f,out,c,1) &&
        out[0]==0x6B && out[BLK_SIZE-1]==0x6B &&
        BlkWrite(dv,buf,c,2) && FatImagePayload(letter,buf,777)) res|=16;
  }
  FClose(f);
  f=NULL;
  Cd(saved);
  Unmount(letter);
  if (!Let2Drv(letter,FALSE) && FileFind(name)) res|=32;
  letter=MountFile(name);
  if (letter && FatImagePayload(letter,buf,777) && !DrvIsWritable(letter))
    res|=64;
cleanup:
  FClose(f);
  if (saved) Cd(saved);
  if (letter && Let2Drv(letter,FALSE)) Unmount(letter);
  if (changed) disk->flags=flags;
done:
  Free(saved);
  Free(buf);
  Free(out);
  if (reboot) "FAT32 image reboot %X\n",res;
  else "FAT32 image %X\n",res;
}

Fat32ImageProbe;
