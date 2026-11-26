U0 AtaWriteRejectProbe()
{
  CDrv *dv=Let2Drv('C',FALSE),*dvd=Let2Drv(':',FALSE);
  U8 *buf=MAlloc(BLK_SIZE),*before=MAlloc(BLK_SIZE);
  I64 blk=dv->drv_offset+dv->size-132,i,res=0,flags=dv->bd->flags;
  if (BlkRead(dv,before,blk,1)) {
    MemSet(buf,0xA5,BLK_SIZE);
    if (!BlkWrite(dv,buf,blk,1) && BlkRead(dv,buf,blk,1)) {
      for (i=0;i<BLK_SIZE;i++) if (buf[i]!=before[i]) break;
      if (i==BLK_SIZE) res|=1;
    }
    dv->bd->flags|=BDF_READ_ONLY_OVERRIDE;
    if (!BlkWrite(dv,buf,dv->drv_offset-1,1)) res|=2;
    if (!BlkWrite(dv,buf,dv->drv_offset+dv->size,1)) res|=4;
    if (!BlkWrite(dv,buf,dv->drv_offset+dv->size-1,2)) res|=8;
    dv->bd->flags=flags;
    if (!BlkWrite(dvd,buf,dvd->drv_offset,1)) res|=16;
  }
  Free(buf);
  Free(before);
  "ATA reject probe %02X\n",res;
}
AtaWriteRejectProbe;
