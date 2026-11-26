#include "../KernelB.hpp"

bool BlkDevReadBytes(const CBlkDev &bd, uint8_t *buf, uint64_t pos,
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

static bool BlkRange(const CDrv *dv, int64_t blk, int64_t count)
{
  return dv && dv->dv_signature == DRV_SIGNATURE_VAL && blk >= dv->drv_offset &&
         count <= dv->size && blk - dv->drv_offset <= dv->size - count;
}

extern "C" uint64_t BlkReadCpp(CDrv *dv, uint8_t *buf, int64_t blk,
                               int64_t count)
{
  if (count <= 0)
    return true;
  if (!BlkRange(dv, blk, count))
    return false;
  if (dv->bd->type == BDT_ATAPI)
    return ATAPIReadBytes(*dv->bd, buf, uint64_t(blk) * BLK_SIZE,
                          size_t(count) * BLK_SIZE);
  CMemLock lock(dv->locked_flags);
  return BlkDevReadBytes(*dv->bd, buf, uint64_t(blk) * BLK_SIZE,
                         size_t(count) * BLK_SIZE);
}

extern "C" uint64_t BlkWriteCpp(CDrv *dv, const uint8_t *buf, int64_t blk,
                                int64_t count)
{
  if (count <= 0)
    return true;
  if (!BlkRange(dv, blk, count) || dv->bd->type != BDT_RAM ||
      (dv->bd->flags & BDF_READ_ONLY))
    return false;
  CMemLock lock(dv->locked_flags);
  MemCpy(dv->bd->RAM_dsk + uint64_t(blk) * BLK_SIZE, buf,
         size_t(count) * BLK_SIZE);
  return true;
}
