#include "../KernelB.hpp"

bool BlkWriteZero(CDrv &dv, uint64_t blk, uint64_t cnt)
{
  constexpr uint64_t zero_buf_size = 2048;
  if (!dv.bd || dv.drv_offset < 0 || dv.size <= 0 ||
      blk < uint64_t(dv.drv_offset) || cnt > uint64_t(dv.size) ||
      blk - dv.drv_offset > uint64_t(dv.size) - cnt)
    return false;
  if (!cnt)
    return true;
  auto *z = CAlloc((cnt < zero_buf_size ? cnt : zero_buf_size) * BLK_SIZE,
                   CurrentTask()->data_heap);
  if (!z)
    return false;
  bool res = true;
  while (cnt) {
    uint64_t n = cnt < zero_buf_size ? cnt : zero_buf_size;
    if (!BlkDevWriteBytes(*dv.bd, z, blk * BLK_SIZE, n * BLK_SIZE)) {
      res = false;
      break;
    }
    blk += n;
    cnt -= n;
  }
  Free(z);
  return res;
}

bool BlkDevReadBytes(CBlkDev &bd, uint8_t *buf, uint64_t pos,
                     size_t bytes)
{
  CDiskLock lock(bd);
  if (bd.type == BDT_ATAPI)
    return ATAPIReadBytes(bd, buf, pos, bytes);
  if (bd.type == BDT_ATA)
    return ATAReadBytes(bd, buf, pos, bytes);
  if (bd.type==BDT_ISO_FILE_READ || bd.type==BDT_ISO_FILE_WRITE)
    return FImageReadBytes(bd.file_dsk,buf,pos,bytes);
  if (bd.type != BDT_RAM || !bd.RAM_dsk || bd.max_blk < 0 ||
      uint64_t(bd.max_blk) >= SIZE_MAX / BLK_SIZE)
    return false;
  size_t size = (size_t(bd.max_blk) + 1) * BLK_SIZE;
  if (pos > size || bytes > size - pos)
    return false;
  MemCpy(buf, bd.RAM_dsk + pos, bytes);
  return true;
}

static bool ImageBackingWriteAllowed(const CBlkDev &bd,const CFile *origin)
{
  if (blkdev.blkdevs)
    for (size_t i=0;i<BLKDEVS_NUM;i++) {
      const auto &image=blkdev.blkdevs[i];
      if (image.bd_signature==BD_SIGNATURE_VAL && image.file_dsk &&
          image.file_dsk->dv && image.file_dsk->dv->bd &&
          BlkDevSameBacking(*image.file_dsk->dv->bd,bd) &&
          (image.file_dsk!=origin || image.type!=BDT_ISO_FILE_WRITE))
        return false;
    }
  return true;
}

static bool BlkDevWriteBytesInternal(CBlkDev &bd,const uint8_t *buf,uint64_t pos,
                                    size_t bytes,const CDrv *fat_origin,
                                    const CFile *image_origin)
{
  CDiskLock lock(bd);
  if (bd.flags&BDF_READ_ONLY && !(bd.flags&BDF_READ_ONLY_OVERRIDE))
    return false;
  if (!ImageBackingWriteAllowed(bd,image_origin)) return false;
  if (image_origin) {
    if (!DrvFATWriteAllowedImage(&bd,pos,bytes,fat_origin)) return false;
  } else if (!DrvFATWriteAllowed(&bd,pos,bytes,fat_origin)) return false;
  // A read-only image is immutable even when the caller overrides its
  // device's read-only flag.
  if (bd.type==BDT_ISO_FILE_READ)
    return false;
  if (bd.type==BDT_ISO_FILE_WRITE) {
    bool res=FImageWriteBytes(bd.file_dsk,buf,pos,bytes);
    DskCacheWriteThrough(&bd,buf,pos,bytes,res);
    DrvFATWriteThrough(&bd,buf,pos,bytes,res,fat_origin);
    return res;
  }
  if (bd.type==BDT_ATA) {
    bool res=ATAWriteBytes(bd,buf,pos,bytes);
    DskCacheWriteThrough(&bd,buf,pos,bytes,res);
    DrvFATWriteThrough(&bd,buf,pos,bytes,res,fat_origin);
    return res;
  }
  if (bd.type!=BDT_RAM || !bd.RAM_dsk || bd.max_blk<0 ||
      uint64_t(bd.max_blk)>=SIZE_MAX/BLK_SIZE)
    return false;
  size_t size=(size_t(bd.max_blk)+1)*BLK_SIZE;
  if (pos>size || bytes>size-pos)
    return false;
  MemCpy(bd.RAM_dsk+pos,buf,bytes);
  DskCacheWriteThrough(&bd,buf,pos,bytes,true);
  DrvFATWriteThrough(&bd,buf,pos,bytes,true,fat_origin);
  return true;
}

bool BlkDevWriteBytes(CBlkDev &bd,const uint8_t *buf,uint64_t pos,size_t bytes,
                     const CDrv *fat_origin)
{
  return BlkDevWriteBytesInternal(bd,buf,pos,bytes,fat_origin,nullptr);
}

bool FImageWriteBytes(CFile *f,const uint8_t *buf,uint64_t pos,size_t bytes)
{
  // The WRITE mount path must publish a preallocated, verified contiguous
  // backing stream. Do not allocate or grow it in a device I/O operation.
  uint64_t physical;
  if (!f || !(f->flags&FF_WRITE) || (f->flags&(FF_NEW_FILE|FF_BUF_DIRTY)) ||
      (bytes && !buf) || !FImageByteRange(*f,pos,bytes,physical))
    return false;
  if (!bytes) return true;
  if (!BlkDevWriteBytesInternal(*f->dv->bd,buf,physical,bytes,nullptr,f))
    return false;
  f->fblk_num=(pos+bytes+BLK_SIZE-1)/BLK_SIZE;
  return true;
}

static bool BlkRange(const CDrv *dv, int64_t blk, int64_t count)
{
  return dv && BlkDevChkCpp(dv->bd,false) && dv->dv_signature==DRV_SIGNATURE_VAL &&
         dv->drv_offset>=0 && blk>=dv->drv_offset &&
         uint64_t(blk)<=UINT64_MAX/BLK_SIZE && count<=dv->size &&
         uint64_t(count)<=SIZE_MAX/BLK_SIZE &&
         blk-dv->drv_offset<=dv->size-count;
}

extern "C" uint64_t BlkReadCpp(CDrv *dv, uint8_t *buf, int64_t blk,
                               int64_t count)
{
  if (count <= 0)
    return true;
  if (!buf || !BlkRange(dv, blk, count))
    return false;
  CDiskLock lock(*dv);
  auto *bd=dv->bd;
  if (!BlkDevEnsureInitialized(*bd) || !BlkRange(dv,blk,count)) return false;
  if (bd->flags&BDF_READ_CACHE) RCacheCpp(dv,&buf,&blk,&count);
  if (!count) return true;
  bool res=BlkDevReadBytes(*bd,buf,uint64_t(blk)*BLK_SIZE,size_t(count)*BLK_SIZE);
  uint64_t time=tSCpp();
  MemCpy(&bd->last_time,&time,sizeof(time));
  if (res && (bd->flags&BDF_READ_CACHE)) DskCacheAddCpp(dv,buf,blk,count);
  return res;
}

extern "C" uint64_t BlkWriteCpp(CDrv *dv, const uint8_t *buf, int64_t blk,
                                int64_t count)
{
  if (count <= 0)
    return true;
  if (!buf || !BlkRange(dv, blk, count))
    return false;
  CDiskLock lock(*dv);
  auto *bd=dv->bd;
  if (!BlkDevEnsureInitialized(*bd) || !BlkRange(dv,blk,count)) return false;
  bool res=BlkDevWriteBytes(*bd,buf,uint64_t(blk)*BLK_SIZE,size_t(count)*BLK_SIZE);
  uint64_t time=tSCpp();
  MemCpy(&bd->last_time,&time,sizeof(time));
  if (res && (bd->flags&BDF_READ_CACHE)) DskCacheAddCpp(dv,buf,blk,count);
  return res;
}

bool DrvReadBytes(const CDrv &drive,uint8_t *buf,uint64_t pos,size_t bytes)
{
  if (!bytes) return true;
  if (!drive.bd || !buf || bytes-1>UINT64_MAX-pos) return false;
  // Mount-time reads use unpublished drive records and must not cache them.
  if (drive.dv_signature!=DRV_SIGNATURE_VAL)
    return BlkDevReadBytes(*drive.bd,buf,pos,bytes);
  auto *dv=const_cast<CDrv *>(&drive);
  CDiskLock lock(*dv);
  if (!BlkDevEnsureInitialized(*dv->bd)) return false;
  int64_t blk=pos/BLK_SIZE;
  int64_t last=(pos+bytes-1)/BLK_SIZE;
  if (!BlkRange(dv,blk,last-blk+1)) return false;
  if (!(drive.bd->flags&BDF_READ_CACHE) || !blkdev.cache_base)
    return BlkDevReadBytes(*drive.bd,buf,pos,bytes);
  size_t offset=pos%BLK_SIZE;
  uint8_t sector[BLK_SIZE];
  if (offset) {
    if (!BlkReadCpp(dv,sector,blk++,1)) return false;
    size_t n=BLK_SIZE-offset;
    if (n>bytes) n=bytes;
    MemCpy(buf,sector+offset,n);
    bytes-=n;
    buf+=n;
  }
  size_t count=bytes/BLK_SIZE;
  if (count) {
    if (!BlkReadCpp(dv,buf,blk,count)) return false;
    blk+=count;
    buf+=count*BLK_SIZE;
    bytes-=count*BLK_SIZE;
  }
  if (bytes) {
    if (!BlkReadCpp(dv,sector,blk,1)) return false;
    MemCpy(buf,sector,bytes);
  }
  return true;
}
