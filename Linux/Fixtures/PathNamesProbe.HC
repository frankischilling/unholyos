I64 PathDotOffset(U8 *name)
{
  U8 *p=FileExtDot(name);
  if (p)
    return p-name;
  return -1;
}

Bool PathAllocEquals(U8 *name,U8 *expected)
{
  Bool ok=name && !StrCmp(name,expected);
  Free(name);
  return ok;
}

Bool PathDirEntryProbe()
{
  CDirEntry root,child,sibling;
  CDirEntry *(*find)(CDirEntry *tmpde,U8 *abs_name);
  find=&Cd2DirEntry;
  MemSet(&root,0,sizeof(CDirEntry));
  MemSet(&child,0,sizeof(CDirEntry));
  MemSet(&sibling,0,sizeof(CDirEntry));
  root.full_name="B:/P";
  root.sub=&child;
  root.next=&sibling;
  child.full_name="B:/P/C";
  sibling.full_name="B:/PP";
  return (*find)(&root,"B:/P")==&root &&
      (*find)(&root,"B:/P/C")==&child &&
      (*find)(&root,"B:/PP")==&sibling &&
      !(*find)(&root,"B:/P/Missing") &&
      !(*find)(&root,"B:/PX") &&
      !(*find)(NULL,"B:/P");
}

Bool PathClusterProbe()
{
  CDrv *dv=Let2Drv('B',FALSE);
  CDirEntry de;
  I64 outer,inner;
  I64 (*cluster)(CDrv *dv,U8 *dirname);
  I64 (*parent)(CDrv *dv,U8 *dirname);
  cluster=&Name2DirClus;
  parent=&Name2ParentDirClus;
  if (!dv || !DirMk("B:/PathNamesProbe") ||
      !DirMk("B:/PathNamesProbe/Sub") ||
      !FileFind("B:/PathNamesProbe",&de,FUF_JUST_DIRS))
    return FALSE;
  outer=de.clus;
  Free(de.full_name);
  if (!FileFind("B:/PathNamesProbe/Sub",&de,FUF_JUST_DIRS))
    return FALSE;
  inner=de.clus;
  Free(de.full_name);
  return outer && inner && outer!=inner &&
      (*cluster)(dv,"/")==dv->root_clus &&
      (*cluster)(dv,"")==dv->root_clus &&
      (*cluster)(dv,"/PathNamesProbe")==outer &&
      (*cluster)(dv,"PathNamesProbe/Sub")==inner &&
      (*cluster)(dv,"/PathNamesProbe//Sub/")==inner &&
      (*parent)(dv,"/PathNamesProbe")==dv->root_clus &&
      (*parent)(dv,"/PathNamesProbe/Sub")==outer &&
      (*parent)(dv,"/PathNamesProbe/Sub//")==inner;
}

U0 PathNamesProbe()
{
  U8 a[64],b[64],*tmp;
  I64 i,res=0;
  Bool (*copy_name)(U8 *dst,U8 *src);
  I64 (*attr)(U8 *name,I64 old_attr);
  copy_name=&CFileNameTo;
  attr=&FileAttr;

  if (PathDotOffset("../x.HC.Z")==4 &&
      PathDotOffset("a.b/c")==1 &&
      PathDotOffset("...")==2 &&
      PathDotOffset("a.")==1 &&
      PathDotOffset("./../x")==-1 &&
      PathDotOffset("")==-1)
    res|=1;
  StrCpy(a,"../a.HC.Z");
  if (FileExtRem(a,b)==b && !StrCmp(a,"../a") && !StrCmp(b,"HC.Z"))
    res|=2;
  StrCpy(a,"../abc");
  StrCpy(b,"old");
  if (FileExtRem(a,b)==b && !StrCmp(a,"../abc") && !*b)
    res|=4;
  StrCpy(a,"a.");
  if (!FileExtRem(a) && !StrCmp(a,"a"))
    res|=8;
  if (IsDotZ("a.HC.Z") && IsDotZ("../a.Z") && !IsDotZ("a.Z") &&
      !IsDotZ("a.HC.z") && !IsDotZ("") &&
      IsDotC("a.BIN.C") && IsDotC("../a.C") && !IsDotC("a.C") &&
      !IsDotC("a.BIN.c") && !IsDotC(""))
    res|=16;
  if (PathAllocEquals(ToggleZorNotZ("a.HC.Z"),"a.HC") &&
      PathAllocEquals(ToggleZorNotZ("a.HC"),"a.HC.Z") &&
      PathAllocEquals(ToggleZorNotZ("a.Z"),"a.Z.Z") &&
      PathAllocEquals(ToggleZorNotZ("a.HC.z"),"a.HC.z.Z"))
    res|=32;
  if (FileNameChk(".") && FileNameChk("..") && FileNameChk("a b.HC") &&
      !FileNameChk(NULL) && !FileNameChk("") && !FileNameChk("a/b") &&
      !FileNameChk("a?b") && !FileNameChk("a:b") && !FileNameChk("a\n"))
    res|=64;
  for (i=0;i<CDIR_FILENAME_LEN-1;i++)
    a[i]='a';
  a[i]=0;
  if (FileNameChk(a)) {
    a[i]='a';
    a[i+1]=0;
    if (!FileNameChk(a))
      res|=128;
  }
  if (((*attr)("a.HC.Z",0) & RS_ATTR_COMPRESSED) &&
      !((*attr)("a.HC",RS_ATTR_COMPRESSED) & RS_ATTR_COMPRESSED) &&
      ((*attr)("a.BIN.C",0) & RS_ATTR_CONTIGUOUS) &&
      ((*attr)("a.BIN",RS_ATTR_CONTIGUOUS) & RS_ATTR_CONTIGUOUS))
    res|=256;
  tmp=FileNameTmpTxt;
  if (tmp && !StrNCmp(tmp,"~/SysTmp",8)) {
    i=8;
    while (('0'<=tmp[i] && tmp[i]<='9') ||
        ('A'<=tmp[i] && tmp[i]<='F'))
      i++;
    if (i>8 && i<=16 && !StrCmp(tmp+i,".DD.Z"))
      res|=512;
  }
  Free(tmp);
  MemSet(b,0xA5,sizeof(b));
  if ((*copy_name)(b,"ok.HC") && !StrCmp(b,"ok.HC")) {
    for (i=6;i<CDIR_FILENAME_LEN && !b[i];i++);
    if (i==CDIR_FILENAME_LEN && b[i]==0xA5 &&
        !(*copy_name)(b,"bad/name")) {
      for (i=0;i<CDIR_FILENAME_LEN && !b[i];i++);
      if (i==CDIR_FILENAME_LEN && b[i]==0xA5)
        res|=1024;
    }
  }
  if (PathDirEntryProbe())
    res|=2048;
  if (PathClusterProbe())
    res|=4096;
  "Path names %X\n",res;
}

PathNamesProbe;
