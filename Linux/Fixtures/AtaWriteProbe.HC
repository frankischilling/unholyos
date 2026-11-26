Bool AtaPattern(U8 *buf,I64 size,I64 start,I64 tag)
{
  I64 i;
  for (i=0;i<size;i++,start++)
    if (buf[i]!=((start*37+(start>>8)+tag)&255))
      return FALSE;
  return TRUE;
}

U0 AtaWriteProbe()
{
  CDrv *dv=Let2Drv('C',FALSE);
  I64 i,blk,bytes=130*BLK_SIZE,res=0,old_flags;
  U8 *buf,*guard;
  if (!dv || dv->fs_type!=FSt_FAT32 || dv->drv_offset!=2048 ||
      dv->size!=132130) {
    "ATA write fixture missing\n";
    return;
  }
  blk=dv->drv_offset+dv->size-131;
  buf=MAlloc(bytes);
  guard=MAlloc(BLK_SIZE);
  if (!buf || !guard) goto done;
  if (!BlkRead(dv,buf,blk,130)) goto done;
  if (AtaPattern(buf,bytes,0,90)) {
    "ATA persisted pattern\n";
    res|=1;
  } else {
    for (i=0;i<bytes;i++)
      if (buf[i]) goto done;
    for (i=0;i<bytes;i++)
      buf[i]=i*37+(i>>8)+90;
    old_flags=dv->bd->flags;
    dv->bd->flags|=BDF_READ_ONLY|BDF_READ_ONLY_OVERRIDE;
    if (BlkWrite(dv,buf,blk,129) &&
        BlkWrite(dv,buf+129*BLK_SIZE,blk+129,1))
      res|=1;
    dv->bd->flags=old_flags;
    "ATA wrote pattern\n";
  }
  MemSet(buf,0,bytes);
  if (BlkRead(dv,buf,blk,130) && AtaPattern(buf,bytes,0,90)) res|=2;
  if (BlkRead(dv,guard,blk+128,1) &&
      AtaPattern(guard,BLK_SIZE,128*BLK_SIZE,90)) res|=4;
  if (BlkRead(dv,guard,blk-1,1)) {
    for (i=0;i<BLK_SIZE;i++) if (guard[i]) break;
    if (i==BLK_SIZE) res|=8;
  }
  if (BlkRead(dv,guard,blk+130,1)) {
    for (i=0;i<BLK_SIZE;i++) if (guard[i]) break;
    if (i==BLK_SIZE) res|=16;
  }
  if (BlkWrite(dv,NULL,blk,0)) res|=32;
done:
  Free(guard);
  Free(buf);
  "ATA write probe %02X\n",res;
}
AtaWriteProbe;
