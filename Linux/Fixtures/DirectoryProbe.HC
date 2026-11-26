Bool DirProbeEqu(U8 *a,U8 *b)
{
  while (*a && *a==*b) {
    a++;
    b++;
  }
  return *a==*b;
}

U0 DirectoryProbe()
{
  I64 res=0;
  CDirEntry *tree,*entry,*child,de;
  U8 *saved_dir=DirCur,*path;

  if (sizeof(CDirEntry)==112 && offset(CDirEntry.start)==48 &&
	CDIR_SIZE==64 && sizeof(de.name)==38)
    res|=1;

  tree=FilesFind("::/Home");
  if (tree && DirProbeEqu(tree->name,".") && tree->size==512 &&
	tree->next && DirProbeEqu(tree->next->name,"..") &&
	tree->next->size==0 && !tree->next->next)
    res|=2;
  DirTreeDel(tree);

  tree=FilesFind("::/Doc/St?rt.DD*;*.HC;!*.HC",FUF_JUST_TXT|FUF_JUST_FILES);
  if (tree && !tree->next && tree->size==200 &&
	DirProbeEqu(tree->full_name,"T:/Doc/Start.DD.Z") &&
	(tree->attr&RS_ATTR_COMPRESSED))
    res|=4;
  DirTreeDel(tree);

  tree=FilesFind("::/Doc/Start.DD.Z",FUF_SINGLE);
  if (tree && !tree->next && tree->size==200 &&
	DirProbeEqu(tree->name,"Start.DD.Z") &&
	!FilesFindMatch(tree->full_name,"*;!*/Start.DD*"))
    res|=8;
  DirEntryDel(tree);

  tree=FilesFind("::/St?rt.DD*",FUF_RECURSE);
  for (entry=tree;entry;entry=entry->next)
    if (DirProbeEqu(entry->name,"Doc"))
      for (child=entry->sub;child;child=child->next)
	if (DirProbeEqu(child->name,"Start.DD.Z") && child->parent==entry)
	  res|=16;
  DirTreeDel(tree);

  tree=FilesFind("::/St?rt.DD*",FUF_RECURSE|FUF_JUST_FILES|FUF_CLUS_ORDER);
  for (entry=tree;entry;entry=entry->next)
    if (DirProbeEqu(entry->full_name,"T:/Doc/Start.DD.Z") && !entry->sub &&
	!(entry->attr&RS_ATTR_DIR))
      res|=32;
  DirTreeDel(tree);

  if (IsDir("::/Home") && Cd("::/Doc")) {
    path=DirCur;
    if (DirProbeEqu(path,"T:/Doc") &&
	FileFind("Start.DD",&de,FUF_JUST_FILES|FUF_Z_OR_NOT_Z)) {
      if (de.size==200 && DirProbeEqu(de.full_name,"T:/Doc/Start.DD.Z"))
	res|=64;
      Free(de.full_name);
    }
    Free(path);
  }
  if (!Cd("::/Doc/NoSuchDir/..")) {
    path=DirCur;
    if (DirProbeEqu(path,"T:/Doc") &&
	FileFind("::/Home",&de,FUF_JUST_DIRS)) {
      if (de.size==512 && DirProbeEqu(de.full_name,"T:/Home"))
	res|=128;
      Free(de.full_name);
    }
    Free(path);
  }
  if (Cd("B:/DirProbe/Sub",TRUE)) {
    path=DirCur;
    if (DirProbeEqu(path,"B:/DirProbe/Sub") && IsDir("B:/DirProbe/Sub"))
      res|=256;
    Free(path);
  }
  Cd(saved_dir);
  Free(saved_dir);
  "Directory probe %03X\n",res;
}

DirectoryProbe;
