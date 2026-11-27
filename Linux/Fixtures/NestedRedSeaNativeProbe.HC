// Failed image allocation preserves the original file; live child mounts
// retain the parent's metadata and reject mutation and teardown.
U0 NestedRedSeaNativeProbe()
{
  CDrv *backing,*outer,*inner;
  CBlkDev *a=NULL,*b,*bad;
  U8 *saved=DirCur,*buf=MAlloc(2*BLK_SIZE),*data,
      q=0,r=0,path[STR_LEN];
  I64 i,n,res=0;
  Bool caught;
  if (!saved || !buf || Let2Drv('B',FALSE)) goto done;
  a=BlkDevNextFreeSlot('B',BDT_RAM);
  a->max_blk=1023;
  if (BlkDevAdd(a,,FALSE,TRUE)!=1) goto done;
  backing=Let2Drv('B',FALSE);
  for (i=0;i<777;i++) buf[i]=i*23+(i>>8)+11;
  if (!FileWrite("B:/Recover.ISO.C",buf,777)) goto cleanup;
  bad=BlkDevNextFreeSlot('Q',BDT_ISO_FILE_WRITE);
  bad->file_dsk_name=StrNew("B:/Recover.ISO.C");
  bad->drv_offset=88;
  bad->max_blk=1023;
  if (BlkDevAdd(bad,,TRUE,TRUE)) {
    q='Q';
    goto cleanup;
  }
  if (!Let2Drv('Q',FALSE)) {
    data=FileRead("B:/Recover.ISO.C",&n);
    if (data && n==777 && !MemCmp(data,buf,n)) res|=1;
    Free(data);
  }
  Free(bad->file_dsk_name);
  bad->file_dsk_name=NULL;
  bad->max_blk=511;
  bad->file_dsk_name=StrNew("B:/Recover.ISO.C");
  if (BlkDevAdd(bad,,TRUE,TRUE)!=1) goto cleanup;
  q='Q';
  outer=Let2Drv(q);
  b=BlkDevNextFreeSlot('R',BDT_ISO_FILE_WRITE);
  b->file_dsk_name=StrNew("Q:/Nested.ISO.C");
  b->drv_offset=88;
  b->max_blk=191;
  if (BlkDevAdd(b,,TRUE,TRUE)!=1) goto cleanup;
  r='R';
  inner=Let2Drv(r);
  if (FileWrite("R:/Payload.BIN",buf,777) && !FileWrite("Q:/Unexpected.BIN",buf,777) &&
      !Del("Q:/Nested.ISO.C",,,FALSE) && !DrvIsWritable(q)) res|=2;
  caught=FALSE;
  try {
    Unmount(q);
  } catch {
    caught=Fs->except_ch=='BlkDev';
    Fs->catch_except=TRUE;
  }
  if (caught && Let2Drv(q,FALSE)==outer && Let2Drv(r,FALSE)==inner) res|=4;
  caught=FALSE;
  try {
    DrvEnable(q,FALSE);
  } catch {
    caught=Fs->except_ch=='Drv';
    Fs->catch_except=TRUE;
  }
  if (caught && outer->fs_type==FSt_REDSEA) res|=8;
  data=FileRead("R:/Payload.BIN",&n);
  if (data && n==777 && !MemCmp(data,buf,n)) res|=16;
  Free(data);
  Cd(saved);
  Unmount(r);
  r=0;
  if (DrvIsWritable(q) && FileWrite("Q:/After.TXT",buf,333)) res|=32;
  Unmount(q);
  q=0;
  if (DrvIsWritable('B') && Del("B:/Recover.ISO.C",,,FALSE)==1) res|=64;
cleanup:
  Cd(saved);
  if (r && Let2Drv(r,FALSE)) Unmount(r);
  if (q && Let2Drv(q,FALSE)) Unmount(q);
  if (a && a->bd_signature==BD_SIGNATURE_VAL) BlkDevDel(a);
done:
  Free(saved);
  Free(buf);
  "Nested RedSea native %X\n",res;
}

NestedRedSeaNativeProbe;
