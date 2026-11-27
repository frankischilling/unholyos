// File-backed RedSea inside a writable image, followed by two read mounts.
U0 NestedRedSeaProbe()
{
  CDrv *backing=Let2Drv('B',FALSE),*outer,*inner;
  CBlkDev *created=NULL,*a,*b;
  U8 *saved=DirCur,*ram=NULL,*buf=MAlloc(2*BLK_SIZE),*data,
      q=0,r=0,m=0,n=0,path[STR_LEN];
  I64 i,sz,res=0;
  if (!saved || !buf) goto done;
  if (!backing) {
    ram=CAlloc(1024*BLK_SIZE);
    if (!ram) goto done;
    created=BlkDevNextFreeSlot('B',BDT_RAM);
    created->RAM_dsk=ram;
    created->max_blk=1023;
    if (BlkDevAdd(created,,FALSE,TRUE)!=1) goto done;
    backing=Let2Drv('B',FALSE);
  }
  if (!backing || backing->fs_type!=FSt_REDSEA) goto done;
  q=DrvNextFreeLet('Q');
  a=BlkDevNextFreeSlot(q,BDT_ISO_FILE_WRITE);
  a->file_dsk_name=StrNew("B:/NestedOuter.ISO.C");
  a->drv_offset=88;
  a->max_blk=511;
  a->init_root_dir_blks=2;
  if (BlkDevAdd(a,,TRUE,TRUE)!=1) goto cleanup;
  outer=Let2Drv(q,FALSE);
  if (!outer || outer->bd!=a) goto cleanup;
  res|=1;
  r=DrvNextFreeLet('Q');
  b=BlkDevNextFreeSlot(r,BDT_ISO_FILE_WRITE);
  StrPrint(path,"%C:/NestedInner.ISO.C",q);
  b->file_dsk_name=StrNew(path);
  b->drv_offset=88;
  b->max_blk=191;
  b->init_root_dir_blks=2;
  if (BlkDevAdd(b,,TRUE,TRUE)!=1) goto cleanup;
  inner=Let2Drv(r,FALSE);
  if (inner && inner->bd==b && b->file_dsk->dv==outer &&
      b->lock_fwding==a && a->lock_fwding==backing->bd) res|=2;
  if (BlkDevLock(backing->bd)) {
    if (!BlkDevLock(a) && !BlkDevLock(b)) res|=4;
    BlkDevUnlock(backing->bd);
  }
  for (i=0;i<777;i++) buf[i]=i*37+(i>>8)+17;
  StrPrint(path,"%C:/Payload.BIN",r);
  if (FileWrite(path,buf,777)) {
    data=FileRead(path,&sz);
    if (data && sz==777 && !MemCmp(buf,data,sz)) res|=8;
    Free(data);
  }
  Cd(saved);
  DskCacheInvalidate(inner);
  Unmount(r);
  r=0;
  StrPrint(path,"%C:/NestedInner.ISO.C",q);
  m=MountFile(path);
  if (m && Let2Drv(m,FALSE)->bd->file_dsk->dv==outer) {
    StrPrint(path,"%C:/Payload.BIN",m);
    data=FileRead(path,&sz);
    if (data && sz==777 && !MemCmp(buf,data,sz)) res|=16;
    Free(data);
    DskCacheInvalidate(Let2Drv(m));
    Unmount(m);
    m=0;
  }
  DskCacheInvalidate(outer);
  Unmount(q);
  q=0;
  m=MountFile("B:/NestedOuter.ISO.C");
  if (!m) goto cleanup;
  StrPrint(path,"%C:/NestedInner.ISO.C",m);
  n=MountFile(path);
  if (n && n!=m && !DrvIsWritable(m) && !DrvIsWritable(n) &&
      Let2Drv(n)->bd->lock_fwding==Let2Drv(m)->bd) res|=32;
  if (n) {
    StrPrint(path,"%C:/Payload.BIN",n);
    data=FileRead(path,&sz);
    if (data && sz==777 && !MemCmp(buf,data,sz)) res|=64;
    Free(data);
  }
cleanup:
  if (saved) Cd(saved);
  if (n && Let2Drv(n,FALSE)) Unmount(n);
  if (m && Let2Drv(m,FALSE)) Unmount(m);
  if (r && Let2Drv(r,FALSE)) Unmount(r);
  if (q && Let2Drv(q,FALSE)) Unmount(q);
  if (backing) Del("B:/NestedOuter.ISO.C",,,FALSE);
  if (created && created->bd_signature==BD_SIGNATURE_VAL) BlkDevDel(created);
done:
  Free(ram);
  Free(saved);
  Free(buf);
  "Nested RedSea %X\n",res;
}

NestedRedSeaProbe;
