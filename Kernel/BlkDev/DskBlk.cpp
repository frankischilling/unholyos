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
  if (bd.type == BDT_ATAPI)
    return ATAPIReadBytes(bd, buf, pos, bytes);
  if (bd.type == BDT_ATA)
    return ATAReadBytes(bd, buf, pos, bytes);
  if (bd.type != BDT_RAM || !bd.RAM_dsk || bd.max_blk < 0 ||
      uint64_t(bd.max_blk) >= SIZE_MAX / BLK_SIZE)
    return false;
  size_t size = (size_t(bd.max_blk) + 1) * BLK_SIZE;
  if (pos > size || bytes > size - pos)
    return false;
  MemCpy(buf, bd.RAM_dsk + pos, bytes);
  return true;
}

bool BlkDevWriteBytes(CBlkDev &bd, const uint8_t *buf, uint64_t pos,
                      size_t bytes)
{
  if (bd.flags&BDF_READ_ONLY && !(bd.flags&BDF_READ_ONLY_OVERRIDE))
    return false;
  if (bd.type==BDT_ATA)
    return ATAWriteBytes(bd,buf,pos,bytes);
  if (bd.type!=BDT_RAM || !bd.RAM_dsk || bd.max_blk<0 ||
      uint64_t(bd.max_blk)>=SIZE_MAX/BLK_SIZE)
    return false;
  size_t size=(size_t(bd.max_blk)+1)*BLK_SIZE;
  if (pos>size || bytes>size-pos)
    return false;
  MemCpy(bd.RAM_dsk+pos,buf,bytes);
  return true;
}

static bool BlkRange(const CDrv *dv, int64_t blk, int64_t count)
{
  return dv && dv->bd && dv->dv_signature == DRV_SIGNATURE_VAL && blk >= dv->drv_offset &&
         count <= dv->size && blk - dv->drv_offset <= dv->size - count;
}

extern "C" uint64_t BlkReadCpp(CDrv *dv, uint8_t *buf, int64_t blk,
                               int64_t count)
{
  if (count <= 0)
    return true;
  if (!BlkRange(dv, blk, count))
    return false;
  if (dv->bd->type==BDT_ATAPI || dv->bd->type==BDT_ATA)
    return BlkDevReadBytes(*dv->bd,buf,uint64_t(blk)*BLK_SIZE,
                           size_t(count)*BLK_SIZE);
  CMemLock lock(dv->locked_flags);
  return BlkDevReadBytes(*dv->bd, buf, uint64_t(blk) * BLK_SIZE,
                         size_t(count) * BLK_SIZE);
}

extern "C" uint64_t BlkWriteCpp(CDrv *dv, const uint8_t *buf, int64_t blk,
                                int64_t count)
{
  if (count <= 0)
    return true;
  if (!BlkRange(dv, blk, count))
    return false;
  if (dv->bd->type==BDT_ATA)
    return BlkDevWriteBytes(*dv->bd,buf,uint64_t(blk)*BLK_SIZE,
                            size_t(count)*BLK_SIZE);
  CMemLock lock(dv->locked_flags);
  return BlkDevWriteBytes(*dv->bd,buf,uint64_t(blk)*BLK_SIZE,
                          size_t(count)*BLK_SIZE);
}
