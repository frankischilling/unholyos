#include "../KernelB.hpp"

extern "C" void PrintCpp(const char *fmt,int64_t argc,const uint64_t *argv);
extern "C" void PrintErrCpp(const char *fmt,int64_t argc,const uint64_t *argv,
                            const uintptr_t *frame);

static void CopyPrint(const char *f1,const char *f2)
{
  uint64_t args[]{reinterpret_cast<uintptr_t>(f1),
                  reinterpret_cast<uintptr_t>(f2)};
  PrintCpp("Copying %s to %s\n",2,args);
}

static void CopyError(const char *name)
{
  uint64_t arg=reinterpret_cast<uintptr_t>(name);
  PrintErrCpp("File not found: \"%s\".\n",1,&arg,nullptr);
}

static bool CopySingleZ(const char *f1,const char *f2)
{
  size_t size;
  uint64_t attr=0,date;
  CDirEntry de;
  if (FileFindCpp(f1,&de,FUF_JUST_FILES)) {
    Free(de.full_name);
    auto *file_buf=FileReadCpp(f1,&size,&attr);
    attr=FileAttrCpp(f2,attr);
    if (file_buf) {
      CopyPrint(f1,f2);
      MemCpy(&date,&de.datetime,sizeof(date));
      auto c=FileWriteCpp(f2,file_buf,size,date,attr);
      Free(file_buf);
      return c!=0;
    }
  }
  CopyError(f1);
  return false;
}

constexpr int64_t COPY_BUF_BLKS=0x80;

extern "C" uint64_t CopySingleCpp(const char *f1,const char *f2)
{
  if (!f1 || !f2)
    return false;
  auto &task=*CurrentTask();
  auto *absf1=FileNameAbs(f1,task),*absf2=FileNameAbs(f2,task);
  bool valid=absf1 && absf2 &&
      StrCmpCpp(reinterpret_cast<uint8_t *>(absf1),
                reinterpret_cast<uint8_t *>(absf2));
  Free(absf1);
  Free(absf2);
  if (!valid)
    return false;
  auto attr1=FileAttrCpp(f1,0),attr2=FileAttrCpp(f2,0);
  if (attr1!=attr2)
    return CopySingleZ(f1,f2);
  auto *buf=MAlloc(COPY_BUF_BLKS*BLK_SIZE,task.data_heap);
  if (!buf)
    return false;
  auto *in_file=FOpenCpp(f1,attr1 & RS_ATTR_CONTIGUOUS ? "rc" : "r",0);
  bool res=false;
  if (in_file) {
    auto size=FSizeCpp(in_file);
    int64_t cnt=size/BLK_SIZE+(size%BLK_SIZE!=0);
    auto *out_file=FOpenCpp(f2,attr2 & RS_ATTR_CONTIGUOUS ? "wc" : "w",cnt);
    if (out_file) {
      CopyPrint(f1,f2);
      while (cnt>0) {
        auto n=cnt>COPY_BUF_BLKS ? COPY_BUF_BLKS : cnt;
        if (!FBlkReadCpp(in_file,buf,FFB_NEXT_BLK,n) ||
            !FBlkWriteCpp(out_file,buf,FFB_NEXT_BLK,n))
          break;
        cnt-=n;
      }
      res=!cnt;
      if (res) {
        out_file->flags|=FF_USE_OLD_DATETIME;
        out_file->de.datetime=in_file->de.datetime;
        out_file->de.size=size;
        out_file->de.attr=FileAttrCpp(f2,in_file->de.attr);
      }
      FCloseCpp(out_file);
    } else
      CopyError(f2);
    FCloseCpp(in_file);
  } else
    CopyError(f1);
  Free(buf);
  return res;
}

extern "C" int64_t DelCpp(const char *mask, uint8_t make_mask,
                           uint8_t del_dir, uint8_t print_msg)
{
  int64_t res = 0;
  if (auto *dirc = DirContextNewCpp(mask, make_mask, false, false)) {
    res = RedSeaFilesDel(*dirc->dv, CurrentTask()->cur_dir, dirc->mask, 0,
                         del_dir, print_msg);
    DirContextDelCpp(dirc, true);
  }
  return res;
}
