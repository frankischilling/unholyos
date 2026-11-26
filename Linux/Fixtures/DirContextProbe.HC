Bool DirCtxEqu(U8 *a,U8 *b)
{
  while (*a && *a==*b) {
    a++;
    b++;
  }
  return *a==*b;
}

U0 DirContextProbe()
{
  I64 res=0;
  CDirContext *outer,*inner,*ram,*missing;
  CDrv *old=Fs->cur_dv;
  U8 *saved=DirCur,*end;

  if (sizeof(CDirContext)==32 && offset(CDirContext.mask)==24 &&
	Str2I64(" --0x2a!",,&end)==42 && *end=='!')
    res|=1;

  outer=DirContextNew("::/Doc/Start.DD*;!old.DD",TRUE);
  if (outer) {
    if (outer->old_dv==old && outer->dv==Fs->cur_dv &&
	DirCtxEqu(outer->old_dir,"/Home") &&
	DirCtxEqu(Fs->cur_dir,"/Doc") &&
	DirCtxEqu(outer->mask,"Start.DD*;!old.DD"))
      res|=2;

    inner=DirContextNew("::/Home",,,TRUE);
    if (inner && !*inner->mask && DirCtxEqu(Fs->cur_dir,"/Home"))
      res|=4;
    DirContextDel(inner);
    if (DirCtxEqu(Fs->cur_dir,"/Doc"))
      res|=8;
    DirContextDel(outer);
    if (Fs->cur_dv==old && DirCtxEqu(Fs->cur_dir,"/Home"))
      res|=16;
  }

  ram=DirContextNew("B:/DirScope/Nested/new.DD",,TRUE);
  if (ram && ram->dv->drv_let=='B' && DirCtxEqu(ram->mask,"new.DD"))
    res|=32;
  DirContextDel(ram,FALSE);
  missing=DirContextNew("B:/DirScope/Missing/new.DD");
  if (!missing && Fs->cur_dv->drv_let=='B' &&
	DirCtxEqu(Fs->cur_dir,"/DirScope/Nested"))
    res|=64;
  DirContextDel(missing);
  if (Cd(saved) && Fs->cur_dv==old && DirCtxEqu(Fs->cur_dir,"/Home"))
    res|=128;
  Free(saved);
  "Dir context %02X\n",res;
}

DirContextProbe;
