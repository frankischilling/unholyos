#include "../KernelB.hpp"

extern "C" int64_t FSizeCpp(CFile *f)
{
  return f ? f->de.size : 0;
}

static bool FFlag(const char *flags, char ch)
{
  for (; *flags; flags++)
    if (*flags == ch)
      return true;
  return false;
}

static void FDel(CFile *f)
{
  Free(f->clus_buf);
  Free(f->de.full_name);
  Free(f);
}

extern "C" CFile *FOpenCpp(const char *filename, const char *flags, int64_t cnt)
{
  if (!filename || !flags)
    return nullptr;
  auto &task = *CurrentTask();
  auto *f = reinterpret_cast<CFile *>(CAlloc(sizeof(CFile), task.data_heap));
  if (!f)
    return nullptr;
  f->clus = f->file_clus_num = INVALID_CLUS;
  f->max_blk = cnt > 0 ? cnt - 1 : INT64_MAX;
  auto *full_name = FileNameAbs(filename, task);
  if (full_name && *full_name && full_name[1] == ':')
    f->dv = PathDrive(task, *full_name);
  if (!f->dv || !ReadableFS(*f->dv) || f->dv->spc<=0) {
    Free(full_name);
    FDel(f);
    return nullptr;
  }
  if (f->dv->fs_type==FSt_REDSEA || FFlag(flags,'c'))
    f->flags=FF_CONTIGUOUS;
  if (f->dv->fs_type==FSt_FAT32 && (f->flags&FF_CONTIGUOUS) &&
      !(FileAttrCpp(filename,0)&RS_ATTR_CONTIGUOUS)) {
    Free(full_name);
    FDel(f);
    return nullptr;
  }
  f->clus_buf = CAlloc(f->dv->spc*BLK_SIZE, task.data_heap);
  if (!f->clus_buf) {
    Free(full_name);
    FDel(f);
    return nullptr;
  }

  bool write = FFlag(flags, 'w');
  if (write && !WritableFS(*f->dv)) {
    Free(full_name);
    FDel(f);
    return nullptr;
  }
  if (write)
    f->flags |= FF_WRITE | FF_NEEDS_WRITE;
  if ((!write || FFlag(flags, '+')) &&
      FileFindCpp(full_name, &f->de, FUF_JUST_FILES)) {
    Free(full_name);
    if (f->de.size < 0 || (write && f->dv->fs_type==FSt_FAT32 &&
        ((f->de.attr&1) || !FAT32StreamEnsure(*f,0,false)))) {
      FDel(f);
      return nullptr;
    }
    if (!write || (f->flags&FF_CONTIGUOUS))
      f->max_blk = f->de.size / int64_t(BLK_SIZE) +
                   (f->de.size % BLK_SIZE != 0) - 1;
    return f;
  }
  f->de.full_name = full_name;
  if (write && (f->dv->fs_type==FSt_FAT32?
      FAT32StreamPrepare(*f,full_name+2,cnt):
      RedSeaStreamPrepare(*f->dv,full_name+2,f->de,cnt))) {
    if (cnt <= 0)
      f->flags |= FF_NEW_FILE;
    return f;
  }
  FDel(f);
  return nullptr;
}

static bool FFlush(CFile *f,bool closing)
{
  if (!(f->flags&FF_BUF_DIRTY)) return true;
  int64_t blocks=f->dv->spc;
  if (!closing && f->max_blk!=INT64_MAX) {
    int64_t remaining=f->max_blk+1-f->file_clus_num*f->dv->spc;
    if (remaining<blocks) blocks=remaining;
  }
  if (blocks<=0 || f->clus<0 ||
      !FAT32ClusWrite(*f->dv,f->clus,f->clus_buf,blocks)) return false;
  f->flags&=~FF_BUF_DIRTY;
  return true;
}

static bool FSetClus(CFile *f,uint32_t c,int64_t blk,bool read)
{
  if (f->clus==c) return true;
  if (!FFlush(f,false)) return false;
  f->clus=f->file_clus_num=INVALID_CLUS;
  int64_t index=blk/f->dv->spc,blocks=f->dv->spc;
  if (f->max_blk!=INT64_MAX) {
    int64_t remaining=f->max_blk+1-index*f->dv->spc;
    if (remaining<blocks) blocks=remaining;
  }
  // Read the rest of the allocated cluster as well, so closing a dirty
  // partial cluster preserves sectors outside a caller's finite limit.
  if (read && (blocks<=0 || !FAT32ClusRead(*f->dv,c,f->clus_buf,f->dv->spc)))
    return false;
  f->clus=c;
  f->file_clus_num=index;
  return true;
}

extern "C" void FCloseCpp(CFile *f)
{
  if (!f)
    return;
  bool okay = FFlush(f,true);
  if (okay && (f->flags & FF_NEEDS_WRITE)) {
    if (!(f->flags & FF_USE_OLD_DATETIME)) {
      auto date = Now();
      MemCpy(&f->de.datetime, &date, sizeof(date));
    }
    okay = f->dv->fs_type==FSt_FAT32?
        FAT32StreamCommit(*f,f->de.full_name+2):
        RedSeaStreamCommit(*f->dv,f->de.full_name+2,f->de,f->flags&FF_NEW_FILE);
  }
  FDel(f);
  if (!okay)
    RaiseHolyC(0x656C6946); //File
}

static bool FBlkRange(CFile *f, int64_t &blk, int64_t cnt)
{
  if (!f || !f->dv || !ReadableFS(*f->dv))
    return false;
  if (blk == FFB_NEXT_BLK)
    blk = f->fblk_num;
  return blk >= 0 && __int128(blk) + cnt - 1 <= f->max_blk;
}

static bool FBlkExtent(CFile *f, int64_t blk, int64_t cnt)
{
  return f->de.clus >= f->dv->data_area &&
         __int128(f->de.clus) + blk + cnt <=
             __int128(f->dv->drv_offset) + f->dv->size;
}

extern "C" uint64_t FBlkReadCpp(CFile *f, uint8_t *buf, int64_t blk, int64_t cnt)
{
  if (!FBlkRange(f, blk, cnt))
    return false;
  if (cnt <= 0)
    return true;
  if (!buf) return false;
  if (f->dv->fs_type==FSt_FAT32) {
    if (f->de.size<0 || __int128(blk)+cnt>(__int128(f->de.size)+BLK_SIZE-1)/BLK_SIZE ||
        !FAT32StreamEnsure(*f,uint64_t(blk)+cnt,false)) return false;
    if (f->flags&FF_CONTIGUOUS) {
      if (!FAT32ReadBlocks(*f,buf,blk,cnt)) return false;
    } else {
      int64_t spc=f->dv->spc;
      for (int64_t pos=blk,left=cnt;left;) {
        uint32_t c;
        if (!FAT32StreamCluster(*f,pos/spc,c) || !FSetClus(f,c,pos,true))
          return false;
        int64_t offset=pos%spc,n=spc-offset;
        if (n>left) n=left;
        MemCpy(buf,f->clus_buf+offset*BLK_SIZE,n*BLK_SIZE);
        buf+=n*BLK_SIZE;pos+=n;left-=n;
      }
    }
  } else if (!FBlkExtent(f,blk,cnt) || !BlkReadCpp(f->dv,buf,f->de.clus+blk,cnt))
    return false;
  f->fblk_num = blk + cnt;
  return true;
}

extern "C" uint64_t FBlkWriteCpp(CFile *f, const uint8_t *buf, int64_t blk,
                                 int64_t cnt)
{
  if (!FBlkRange(f, blk, cnt) || !(f->flags & FF_WRITE))
    return false;
  if (cnt <= 0)
    return true;
  if (!buf) return false;
  if (f->dv->fs_type==FSt_FAT32) {
    if (!WritableFS(*f->dv) || __int128(blk)+cnt>UINT32_MAX/BLK_SIZE)
      return false;
    if (f->flags&FF_CONTIGUOUS) {
      if (__int128(blk)+cnt>(__int128(f->de.size)+BLK_SIZE-1)/BLK_SIZE ||
          !FAT32WriteBlocks(*f,buf,blk,cnt)) return false;
    } else {
      if (!FAT32StreamEnsure(*f,uint64_t(blk)+cnt,true)) return false;
      int64_t spc=f->dv->spc;
      for (int64_t pos=blk,left=cnt;left;) {
        uint32_t c;
        int64_t offset=pos%spc,n=spc-offset;
        if (n>left) n=left;
        if (!FAT32StreamCluster(*f,pos/spc,c) || !FSetClus(f,c,pos,n!=spc))
          return false;
        MemCpy(f->clus_buf+offset*BLK_SIZE,buf,n*BLK_SIZE);
        f->flags|=FF_BUF_DIRTY;
        buf+=n*BLK_SIZE;pos+=n;left-=n;
        if (pos*int64_t(BLK_SIZE)>f->de.size) f->de.size=pos*BLK_SIZE;
      }
    }
  } else if (!FBlkExtent(f,blk,cnt) || !BlkWriteCpp(f->dv,buf,f->de.clus+blk,cnt))
    return false;
  f->fblk_num = blk + cnt;
  return true;
}
