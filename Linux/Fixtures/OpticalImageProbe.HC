// Read-only image dependencies keep the boot optical medium pinned.
U0 OpticalImageProbe()
{
  CDrv *source=Let2Drv('T',FALSE),*first,*second;
  U8 *saved=DirCur,*a=NULL,*b=NULL,path[STR_LEN],m=0,n=0;
  I64 asz,bsz,res=0;
  Bool caught;
  if (!saved || !source || source->bd->type!=BDT_ATAPI) goto done;
  m=MountFile("T:/Embedded.ISO.C");
  if (!m) goto done;
  first=Let2Drv(m,FALSE);
  if (first && first->bd->file_dsk->dv==source &&
      first->bd->lock_fwding==source->bd && !DrvIsWritable(m)) res|=1;
  n=MountFile("T:/Embedded.ISO.C");
  if (!n || n==m) goto done;
  second=Let2Drv(n,FALSE);
  if (second && second->bd->file_dsk->dv==source && !DrvIsWritable(n)) res|=2;
  StrPrint(path,"%C:/FileBackedRedSeaProbe.HC",m);
  a=FileRead(path,&asz);
  StrPrint(path,"%C:/FileBackedRedSeaProbe.HC",n);
  b=FileRead(path,&bsz);
  if (a && b && asz>0 && asz==bsz && !MemCmp(a,b,asz)) res|=4;
  caught=FALSE;
  try { DskChg('T'); } catch {
    caught=Fs->except_ch=='Drv';
    Fs->catch_except=TRUE;
  }
  if (caught) res|=8;
  Cd(saved);
  Unmount(m);
  m=0;
  caught=FALSE;
  try { DskChg('T'); } catch {
    caught=Fs->except_ch=='Drv';
    Fs->catch_except=TRUE;
  }
  if (caught) res|=16;
  Unmount(n);
  n=0;
  DskChg('T');
  if (Let2Drv('T',FALSE)==source && RedSeaValidate('T')) res|=32;
done:
  if (saved) Cd(saved);
  if (n && Let2Drv(n,FALSE)) Unmount(n);
  if (m && Let2Drv(m,FALSE)) Unmount(m);
  Free(saved);
  Free(a);
  Free(b);
  "Optical image %X\n",res;
}

OpticalImageProbe;
