#include "../../Kernel/KernelB.hpp"

extern "C" uint64_t CopyCpp(const char *src,const char *dst)
{
  auto *first=FilesFindCpp(src,FUF_CLUS_ORDER);
  if (!first)
    return false;
  if (!IsDirCpp(dst)) {
    DirTreeDelCpp(first);
    return CopySingleCpp(src,dst);
  }
  bool res=true;
  if (auto *dirc=DirContextNewCpp(dst,true,false,false)) {
    for (auto *de=first;de;de=de->next)
      if (!(de->attr & RS_ATTR_DIR)) {
        auto *st=FileNameAbs(de->name,*CurrentTask());
        if (!st || !CopySingleCpp(de->full_name,st))
          res=false;
        Free(st);
      }
    DirContextDelCpp(dirc,true);
  } else
    res=false;
  DirTreeDelCpp(first);
  return res;
}

extern "C" uint64_t MoveCpp(const char *f1,const char *f2)
{
  if (!CopyCpp(f1,f2))
    return false;
  DelCpp(f1,false,false,true);
  return true;
}

static int64_t CopyTree2(CDirEntry *de,size_t src_dir_len,size_t dst_dir_len,
                          const char *dst_dir)
{
  int64_t res=1;
  auto *heap=CurrentTask()->data_heap;
  for (;de;de=de->next) {
    size_t len=StrLen(de->full_name);
    if (len<src_dir_len)
      continue;
    auto *st=reinterpret_cast<char *>(MAlloc(len+dst_dir_len+1,heap));
    if (!st)
      continue;
    MemCpy(st,dst_dir,dst_dir_len);
    MemCpy(st+dst_dir_len,de->full_name+src_dir_len,len-src_dir_len+1);
    if (de->attr & RS_ATTR_DIR) {
      int64_t cnt=0;
      for (auto *child=de->sub;child;child=child->next)
        cnt++;
      if (DirMkCpp(st,cnt) || IsDirCpp(st))
        res+=CopyTree2(de->sub,src_dir_len,dst_dir_len,dst_dir);
    } else if (CopySingleCpp(de->full_name,st))
      res++;
    Free(st);
  }
  return res;
}

static bool CopyTreeContains(const char *src,const char *dst)
{
  if (!src || !dst || !*src || !*dst || src[1]!=':' || dst[1]!=':')
    return false;
  auto &task=*CurrentTask();
  if (PathDrive(task,*src)!=PathDrive(task,*dst))
    return false;
  src+=2;
  dst+=2;
  while (*src && *src==*dst) {
    src++;
    dst++;
  }
  return !*src && (!*dst || *dst=='/' || dst[-1]=='/');
}

extern "C" int64_t CopyTreeCpp(const char *src,const char *dst,uint8_t no_mask)
{
  auto &task=*CurrentTask();
  auto *st1=DirNameAbs(src,task),*st2=DirNameAbs(dst,task);
  bool valid=st1 && st2 && !CopyTreeContains(st1,st2);
  Free(st1);
  Free(st2);
  if (!valid)
    return 0;
  auto *dirc=DirContextNewCpp(src,true,false,no_mask);
  if (!dirc)
    return 0;
  auto *de=FilesFindCpp(dirc->mask,FUF_RECURSE);
  st1=DirCurCpp(nullptr,nullptr);
  DirContextDelCpp(dirc,true);
  st2=DirNameAbs(dst,task);
  valid=st1 && st2 && !CopyTreeContains(st1,st2);
  Free(st2);
  int64_t res=0;
  if (valid && (dirc=DirContextNewCpp(dst,true,true,false))) {
    st2=DirCurCpp(nullptr,nullptr);
    if (st2 && WritableFS(*dirc->dv) &&
        dirc->mask[0]=='*' && !dirc->mask[1]) {
      size_t i1=StrLen(st1),i2=StrLen(st2);
      res=CopyTree2(de,i1==3?2:i1,i2==3?2:i2,st2);
    }
    DirContextDelCpp(dirc,true);
    Free(st2);
  }
  DirTreeDelCpp(de);
  Free(st1);
  return res;
}

static int64_t DelTreeDirs(CDirEntry *de)
{
  int64_t res=0;
  while (de) {
    auto *next=de->next;
    if (de->attr & RS_ATTR_DIR) {
      res+=DelTreeDirs(de->sub);
      res+=DelCpp(de->full_name,true,true,true);
    }
    DirEntryDelCpp(de);
    de=next;
  }
  return res;
}

static int64_t DelTreeFiles(CDirEntry *de)
{
  int64_t res=0;
  while (de) {
    auto *next=de->next;
    if (de->attr & RS_ATTR_DIR)
      res+=DelTreeFiles(de->sub);
    else
      res+=DelCpp(de->full_name,false,true,true);
    DirEntryDelCpp(de);
    de=next;
  }
  return res;
}

extern "C" int64_t DelTreeCpp(const char *mask,const char *fu_flags)
{
  uint64_t fuf_flags=0;
  bool defined;
  auto *list=reinterpret_cast<const uint8_t *>(DefineText("ST_FILE_UTIL_FLAGS",defined));
  if (!defined)
    UndefinedDefine("ST_FILE_UTIL_FLAGS");
  ScanFlagsCpp(reinterpret_cast<uint8_t *>(&fuf_flags),list,"+r");
  ScanFlagsCpp(reinterpret_cast<uint8_t *>(&fuf_flags),list,fu_flags);
  if (!IsDirCpp(mask))
    return DelTreeFiles(FilesFindCpp(mask,fuf_flags));
  int64_t res=DelTreeDirs(FilesFindCpp(mask,fuf_flags));
  res+=DelCpp(mask,true,true,true);
  res+=DelCpp(mask,false,true,true);
  return res;
}
