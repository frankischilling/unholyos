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

// Mounted image handles are owned by the device, not by the mounting task.
static bool FImageMounted(const CFile *f)
{
  if (blkdev.blkdevs)
    for (size_t i=0;i<BLKDEVS_NUM;i++)
      if (blkdev.blkdevs[i].bd_signature==BD_SIGNATURE_VAL &&
          blkdev.blkdevs[i].file_dsk==f)
        return true;
  return false;
}

bool BlkDevSameBacking(const CBlkDev &a,const CBlkDev &b)
{
  if (&a==&b) return true;
  if (a.type==BDT_RAM && b.type==BDT_RAM)
    return a.RAM_dsk && a.RAM_dsk==b.RAM_dsk;
  return a.type==BDT_ATA && b.type==BDT_ATA &&
         a.base0==b.base0 && a.base1==b.base1 && a.unit==b.unit;
}

bool BlkDevImageBacking(const CBlkDev *bd)
{
  if (bd && blkdev.blkdevs)
    for (size_t i=0;i<BLKDEVS_NUM;i++) {
      const auto &image=blkdev.blkdevs[i];
      if (image.bd_signature==BD_SIGNATURE_VAL && image.file_dsk &&
          image.file_dsk->dv && image.file_dsk->dv->bd &&
          BlkDevSameBacking(*image.file_dsk->dv->bd,*bd))
        return true;
    }
  return false;
}

bool DrvHasImageDependents(const CDrv &dv)
{
  return BlkDevImageBacking(dv.bd);
}

// A validated contiguous image is a bounded byte extent in its backing
// drive. The backing filesystem is kept immutable while the image is mounted.
bool FImageByteRange(const CFile &f,uint64_t pos,size_t bytes,uint64_t &physical)
{
  const auto *dv=f.dv;
  if (!dv || dv->dv_signature!=DRV_SIGNATURE_VAL || !ReadableFS(*dv) ||
      !BlkDevChkCpp(dv->bd,false) || !(f.flags&FF_CONTIGUOUS) ||
      f.de.size<int64_t(BLK_SIZE) ||
      f.de.clus<0 || f.max_blk<0 || dv->drv_offset<0 || dv->size<=0 ||
      dv->data_area<0 || dv->spc<=0 || dv->bd->max_blk<0 ||
      (dv->bd->type!=BDT_RAM && dv->bd->type!=BDT_ATA &&
       dv->bd->type!=BDT_ATAPI))
    return false;
  uint64_t blocks=uint64_t(f.de.size)/BLK_SIZE;
  if (uint64_t(f.max_blk)+1!=blocks)
    return false;
  uint64_t size=blocks*BLK_SIZE;
  if (pos>size || bytes>size-pos)
    return false;
  uint64_t end=uint64_t(dv->drv_offset)+dv->size;
  if (end>uint64_t(dv->bd->max_blk)+1 || end>UINT64_MAX/BLK_SIZE)
    return false;
  uint64_t start=f.de.clus;
  if (dv->fs_type==FSt_FAT32) {
    if (start<2 || start>=0x0FFFFFF0 || start>(UINT64_MAX-dv->data_area)/dv->spc)
      return false;
    start=uint64_t(dv->data_area)+start*dv->spc;
  } else if (start<uint64_t(dv->data_area))
    return false;
  if (start<uint64_t(dv->drv_offset) || start>=end || blocks>end-start)
    return false;
  physical=start*BLK_SIZE+pos;
  return true;
}

static bool FImageRedSeaAllocated(const CFile &f)
{
  const auto &dv=*f.dv;
  uint64_t first=f.de.clus;
  uint64_t count=uint64_t(f.de.size)/BLK_SIZE+
                 (uint64_t(f.de.size)%BLK_SIZE!=0);
  if (first<=uint64_t(dv.data_area) ||
      count>uint64_t(dv.drv_offset)+dv.size-first ||
      (uint64_t(dv.root_clus)>=first && uint64_t(dv.root_clus)-first<count))
    return false;
  uint64_t bit=first-dv.data_area;
  uint8_t map[BLK_SIZE];
  while (count) {
    uint64_t off=bit%(BLK_SIZE*8);
    uint64_t n=BLK_SIZE*8-off;
    if (n>count) n=count;
    if (!DrvFATRead(dv,bit/(BLK_SIZE*8),map,0,sizeof(map)))
      return false;
    for (uint64_t i=off;i<off+n;i++)
      if (!(map[i/8]&(1u<<(i%8)))) return false;
    bit+=n;
    count-=n;
  }
  return true;
}

static CFile *FImageNew(const char *absolute,CDrv *backing)
{
  if (!absolute || !backing || !adam_task || !ReadableFS(*backing) ||
      !BlkDevChkCpp(backing->bd,false) || backing->spc<=0 ||
      backing->spc>128 || !absolute[0] || absolute[1]!=':' ||
      absolute[2]!='/' || (backing->fat_blk_dirty&1))
    return nullptr;
  auto *hc=adam_task->data_heap;
  auto *f=reinterpret_cast<CFile *>(CAlloc(sizeof(CFile),hc));
  if (!f) return nullptr;
  f->dv=backing;
  f->flags=FF_CONTIGUOUS;
  f->clus=f->file_clus_num=INVALID_CLUS;
  f->de.full_name=StrNew(absolute,hc);
  f->clus_buf=CAlloc(backing->spc*BLK_SIZE,hc);
  if (!f->de.full_name || !f->clus_buf) {
    FDel(f);
    return nullptr;
  }
  return f;
}

static bool FImageValidate(CFile &f)
{
  uint64_t physical;
  return FImageByteRange(f,0,0,physical) &&
      (f.dv->fs_type!=FSt_REDSEA || FImageRedSeaAllocated(f)) &&
      (f.dv->fs_type!=FSt_FAT32 ||
       FAT32StreamEnsure(f,uint64_t(f.max_blk)+1,false));
}

CFile *FOpenImageRead(const char *absolute,CDrv *backing)
{
  auto *f=FImageNew(absolute,backing);
  if (!f) return nullptr;
  CRedSeaDirEntry de;
  size_t prefix;
  if (!FsFileLookup(*backing,absolute+2,false,false,de,prefix) ||
      de.size<BLK_SIZE || de.size>INT64_MAX || de.clus>INT64_MAX ||
      (FileAttrCpp(de.name,de.attr)&RS_ATTR_COMPRESSED) ||
      (backing->fs_type==FSt_FAT32 &&
       !(FileAttrCpp(de.name,de.attr)&RS_ATTR_CONTIGUOUS))) {
    FDel(f);
    return nullptr;
  }
  f->max_blk=de.size/BLK_SIZE-1;
  f->de.attr=de.attr;
  MemCpy(f->de.name,de.name,sizeof(de.name));
  f->de.clus=de.clus;
  f->de.size=de.size;
  f->de.datetime=DateFromBits(de.datetime);
  if (!FImageValidate(*f)) {
    FDel(f);
    return nullptr;
  }
  return f;
}

CFile *FOpenImageWrite(const char *absolute,CDrv *backing,int64_t cnt)
{
  if (!absolute || !backing || !WritableFS(*backing) || cnt<=0 ||
      cnt>backing->size || cnt>int64_t(INT64_MAX/BLK_SIZE) ||
      (FileAttrCpp(absolute,0)&RS_ATTR_COMPRESSED) ||
      (backing->fs_type==FSt_FAT32 &&
       !(FileAttrCpp(absolute,0)&RS_ATTR_CONTIGUOUS)))
    return nullptr;
  auto *f=FImageNew(absolute,backing);
  if (!f) return nullptr;
  f->flags|=FF_WRITE;
  f->max_blk=cnt-1;
  bool okay=backing->fs_type==FSt_FAT32?
      FAT32StreamPrepare(*f,absolute+2,cnt):
      RedSeaStreamPrepare(*backing,absolute+2,f->de,cnt);
  if (okay) {
    f->de.datetime=DateFromBits(Now());
    okay=(backing->fs_type==FSt_FAT32?
          FAT32StreamCommit(*f,absolute+2):
          RedSeaStreamCommit(*backing,absolute+2,f->de,false)) &&
         FImageValidate(*f);
  }
  if (!okay) {
    FDel(f);
    return nullptr;
  }
  return f;
}

void FCloseImage(CFile *f)
{
  // The fixed-size extent was committed before publication. Device I/O
  // writes through to it without dirty stream buffers or deferred metadata.
  if (f) FDel(f);
}

bool FImageReadBytes(CFile *f,uint8_t *buf,uint64_t pos,size_t bytes)
{
  uint64_t physical;
  if (!f || (bytes && !buf) || !FImageByteRange(*f,pos,bytes,physical))
    return false;
  if (!bytes) return true;
  if (!DrvReadBytes(*f->dv,buf,physical,bytes)) return false;
  f->fblk_num=(pos+bytes+BLK_SIZE-1)/BLK_SIZE;
  return true;
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
  if (write && (!WritableFS(*f->dv) || DrvHasImageDependents(*f->dv))) {
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
  if (FImageMounted(f))
    RaiseHolyC(0x656C6946); // File: the block device owns this handle.
  if (f->dv && (f->flags&(FF_WRITE|FF_BUF_DIRTY|FF_NEEDS_WRITE)) &&
      DrvHasImageDependents(*f->dv))
    RaiseHolyC(0x656C6946); // File: preserve the handle until unmount.
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
  if (!FBlkRange(f, blk, cnt) || !(f->flags & FF_WRITE) ||
      DrvHasImageDependents(*f->dv))
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
