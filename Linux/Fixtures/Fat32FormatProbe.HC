Bool FatFormatAll(U8 *buf,I64 value)
{
  I64 i;
  for (i=0;i<BLK_SIZE;i++)
    if (buf[i]!=value) return FALSE;
  return TRUE;
}

Bool FatFormatEmpty(CDrv *dv,U8 *buf)
{
  if (dv->fs_type!=FSt_FAT32 || dv->spc!=1 || dv->root_clus!=2 ||
      dv->fat1!=2080 || dv->fat2!=3113 || dv->data_area!=4144 ||
      dv->file_system_info_sect!=2049 || FileFind("C:/FRAG.BIN") ||
      !BlkRead(dv,buf,dv->data_area+2,1) || !FatFormatAll(buf,0))
    return FALSE;
  if (!BlkRead(dv,buf,dv->fat1,1) || buf(U32 *)[0] ||
      buf(U32 *)[1]!=0x0FFFFFFF || buf(U32 *)[2]!=0x0FFFFFFF ||
      buf(U32 *)[3]) return FALSE;
  if (!BlkRead(dv,buf,dv->fat2,1) || buf(U32 *)[0] ||
      buf(U32 *)[1]!=0x0FFFFFFF || buf(U32 *)[2]!=0x0FFFFFFF ||
      buf(U32 *)[3]) return FALSE;
  return TRUE;
}

U0 Fat32FormatProbe()
{
  CDrv *dv=Let2Drv('C',FALSE);
  I64 res=0,flags,tail,size,i;
  U8 *buf=MAlloc(BLK_SIZE),*file;
  CDate date=Str2Date("2/29/24");
  if (!dv || dv->fs_type!=FSt_FAT32 || dv->drv_offset!=2048 ||
      dv->size!=132130) goto done;
  flags=dv->bd->flags;
  tail=dv->drv_offset+dv->size-1;
  if (!FileFind("C:/FormatDone.BIN")) {
    dv->bd->flags&=~BDF_READ_ONLY;
    MemSet(buf,0xA5,BLK_SIZE);
    if (BlkWrite(dv,buf,tail,1)) res|=1;
    Fmt('C',TRUE,FALSE);
    if (FatFormatEmpty(dv,buf)) res|=2;
    if (BlkRead(dv,buf,tail,1) && FatFormatAll(buf,0xA5)) res|=4;
    MemSet(buf,0x5A,BLK_SIZE);
    if (FileWrite("C:/Quick.BIN",buf,BLK_SIZE,date)) res|=8;
    Fmt('C',FALSE,FALSE);
    if (FatFormatEmpty(dv,buf) && !FileFind("C:/Quick.BIN") &&
        BlkRead(dv,buf,tail,1) && FatFormatAll(buf,0)) res|=16;
    for (i=0;i<BLK_SIZE;i++) buf[i]=i*37+(i>>8)+90;
    if (FileWrite("C:/FormatDone.BIN",buf,BLK_SIZE,date)) res|=32;
    "FAT32 format mutation %02X\n",res;
    Cd("T:/Home");
  } else
    "FAT32 persisted format\n";
  dv->bd->flags=flags;
  res=0;
  if (dv->spc==1 && dv->root_clus==2 && dv->fat2==3113 &&
      dv->data_area==4144) res|=1;
  if (!FileFind("C:/FRAG.BIN") && !FileFind("C:/Quick.BIN")) res|=2;
  if (BlkRead(dv,buf,tail,1) && FatFormatAll(buf,0)) res|=4;
  file=FileRead("C:/FormatDone.BIN",&size);
  if (file && size==BLK_SIZE) {
    for (i=0;i<BLK_SIZE;i++) buf[i]=i*37+(i>>8)+90;
    if (!MemCmp(buf,file,BLK_SIZE)) res|=8;
  }
  Free(file);
done:
  Free(buf);
  "FAT32 format probe %02X\n",res;
}
Fat32FormatProbe;
