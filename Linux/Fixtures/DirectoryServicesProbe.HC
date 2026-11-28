Bool DirSvcClusOrder(CDirEntry *entry,CDirEntry *parent)
{
  while (entry) {
    if (entry->parent!=parent || (entry->next &&
	entry->clus>entry->next->clus) || !DirSvcClusOrder(entry->sub,entry))
      return FALSE;
    entry=entry->next;
  }
  return TRUE;
}

Bool DirSvcTreeOrder(CDirEntry *tree)
{
  CDirEntry *sub,*deep;
  if (!tree || StrCmp(tree->name,"ZDir") || !tree->sub ||
      !tree->next || StrCmp(tree->next->name,"Z.BIN") ||
      !tree->next->next || StrCmp(tree->next->next->name,"A.TXT") ||
      tree->next->next->next)
    return FALSE;
  sub=tree->sub;
  if (StrCmp(sub->name,"Deep") || !sub->sub || sub->parent!=tree ||
      !sub->next || StrCmp(sub->next->name,"A.BIN") ||
      sub->next->parent!=tree || !sub->next->next ||
      StrCmp(sub->next->next->name,"B.TXT") ||
      sub->next->next->parent!=tree || sub->next->next->next)
    return FALSE;
  deep=sub->sub;
  return !StrCmp(deep->name,"Q.TXT") && deep->parent==sub && !deep->next;
}

U8 *DirSvcExpected(I64 i)
{
  switch (i) {
    case 0: return "ZDir";
    case 1: return "Deep";
    case 2: return "Q.TXT";
    case 3: return "A.BIN";
    case 4: return "B.TXT";
    case 5: return "Z.BIN";
    case 6: return "A.TXT";
  }
  return "";
}

U0 DirectoryServicesProbe()
{
  I64 res=0,i;
  I64 (*cmp_name)(CDirEntry *e1,CDirEntry *e2);
  I64 (*cmp_clus)(CDirEntry *e1,CDirEntry *e2);
  U0 (*sort)(CDirEntry **head,I64 key);
  CDirEntry *(*flatten)(CDirEntry *node,CDirEntry **out,I64 flags);
  CDirEntry *(*find2)(U8 *mask,I64 flags);
  CDirEntry a,b,*tree,*entry,*last,*tail,*parent,*sub;
  U8 *saved,*cwd,*data,*user;
  Bool okay;

  cmp_name=&DirEntryCompareName;
  cmp_clus=&DirEntryCompareClus;
  sort=&DirFilesSort;
  flatten=&DirFilesFlatten;
  find2=&FilesFind2;
  if (!cmp_name || !cmp_clus || !sort || !flatten || !find2) {
    "Directory services 000\n";
    return;
  }
  saved=DirCur;
  data=MAlloc(BLK_SIZE);
  user=MAlloc(8);
  MemSet(&a,0,sizeof(CDirEntry));
  MemSet(&b,0,sizeof(CDirEntry));
  StrCpy(a.name,"A.TXT");
  StrCpy(b.name,"Z.TXT");
  if ((*cmp_name)(&a,&b)==-1 && (*cmp_name)(&b,&a)==1 && !(*cmp_name)(&a,&a)) {
    StrCpy(b.name,"Z.BIN");
    if ((*cmp_name)(&a,&b)==1) {
      a.attr=RS_ATTR_DIR;
      if ((*cmp_name)(&a,&b)==-1 && (*cmp_name)(&b,&a)==1)
	res|=1;
    }
  }
  a.clus=9;
  b.clus=44;
  if ((*cmp_clus)(&a,&b)==-35 && (*cmp_clus)(&b,&a)==35 && !(*cmp_clus)(&a,&a))
    res|=2;

  MemSet(data,0xA5,BLK_SIZE);
  if (DirMk("B:/DirSvc") && DirMk("B:/DirSvc/ZDir") &&
      DirMk("B:/DirSvc/ZDir/Deep") &&
      FileWrite("B:/DirSvc/A.TXT",data,8) &&
      FileWrite("B:/DirSvc/Z.BIN",data,8) &&
      FileWrite("B:/DirSvc/ZDir/B.TXT",data,8) &&
      FileWrite("B:/DirSvc/ZDir/A.BIN",data,8) &&
      FileWrite("B:/DirSvc/ZDir/Deep/Q.TXT",data,8) && Cd("B:/DirSvc")) {
    res|=4;
    tree=(*find2)("*",FUF_RECURSE);
    if (DirSvcTreeOrder(tree)) {
      res|=8;
      (*sort)(&tree,1); // SK_CLUS in DskDirA.HC
      if (DirSvcClusOrder(tree,NULL))
	res|=16;
      (*sort)(&tree,0); // SK_NAME in DskDirA.HC
      if (DirSvcTreeOrder(tree))
	res|=32;
      parent=tree;
      sub=parent->sub;
      tail=(*flatten)(tree,&tree,0);
      okay=TRUE;
      last=NULL;
      i=0;
      for (entry=tree;entry;entry=entry->next) {
	if (i>=7 || StrCmp(entry->name,DirSvcExpected(i)) || entry->sub)
	  okay=FALSE;
	if ((i==1 || i==3 || i==4) && entry->parent!=parent ||
	    i==2 && entry->parent!=sub)
	  okay=FALSE;
	i++;
	last=entry;
      }
      if (okay && i==7 && tail==last)
	res|=64;
    }
    DirTreeDel(tree);

    tree=(*find2)("*",FUF_RECURSE|FUF_JUST_FILES);
    okay=TRUE;
    i=2;
    for (entry=tree;entry;entry=entry->next) {
      if (i>=7 || StrCmp(entry->name,DirSvcExpected(i)) || entry->sub ||
	  entry->attr&RS_ATTR_DIR)
	okay=FALSE;
      i++;
      entry->user_data=MAlloc(8);
    }
    if (okay && i==7)
      res|=128;
    DirTreeDel2(tree);

    tree=FilesFind("B:/DirSvc/*",FUF_RECURSE|FUF_JUST_FILES|FUF_CLUS_ORDER);
    okay=TRUE;
    i=0;
    for (entry=tree;entry;entry=entry->next) {
      if (entry->sub || entry->attr&RS_ATTR_DIR ||
	  (entry->next && entry->clus>entry->next->clus))
	okay=FALSE;
      i++;
    }
    if (okay && i==5)
      res|=256;
    if (tree) {
      MemSet(user,0x5A,8);
      tree->user_data=user;
      DirTreeDel(tree);
      if (user[0]==0x5A && user[7]==0x5A)
	res|=512;
    }
    cwd=DirCur;
    if (!StrCmp(cwd,"B:/DirSvc"))
      res|=1024;
    Free(cwd);
  }
  if (Cd(saved) && DelTree("B:/DirSvc")==5 && !IsDir("B:/DirSvc"))
    res|=2048;
  Free(saved);
  Free(data);
  Free(user);
  "Directory services %03X\n",res;
}

DirectoryServicesProbe;
