#include "../KernelB.hpp"

static uint64_t slot_flags;
uint64_t disk_lock_flags;

CBlkDev *BlkDevLockTarget(CBlkDev *bd)
{
  // Bound forwarding traversal even if a guest supplies a cyclic record.
  for (size_t i=0;i<BLKDEVS_NUM && BlkDevChkCpp(bd,false);i++) {
    if (!bd->lock_fwding) return bd;
    bd=bd->lock_fwding;
  }
  return nullptr;
}

bool BlkDevLockOwner(CBlkDev *bd)
{
  auto *task=CurrentTask();
  for (;;) {
    {
      CMemLock lock(disk_lock_flags);
      if (!(bd->locked_flags&1)) {
        bd->locked_flags|=1;
        bd->owning_task=task;
        return true;
      }
      if (bd->owning_task==task) return false;
    }
    _YIELD();
  }
}

bool BlkDevUnlockOwner(CBlkDev *bd,bool rst)
{
  CMemLock lock(disk_lock_flags);
  if (!(bd->locked_flags&1) || bd->owning_task!=CurrentTask())
    return false;
  if (rst) {
    bd->flags&=~(BDF_INITIALIZED|BDF_INIT_IN_PROGRESS);
    DskCacheInvalidateDevice(bd);
  }
  bd->owning_task=nullptr;
  bd->locked_flags&=~uint64_t(1);
  return true;
}

extern "C" uint64_t BlkDevLockCpp(CBlkDev *bd)
{
  if (!(bd=BlkDevLockTarget(bd))) RaiseHolyC(0x7665446B6C42);
  return BlkDevLockOwner(bd);
}

extern "C" uint64_t BlkDevUnlockCpp(CBlkDev *bd,uint8_t rst)
{
  if (!(bd=BlkDevLockTarget(bd))) RaiseHolyC(0x7665446B6C42);
  bool res=BlkDevUnlockOwner(bd,rst);
  if (res) _YIELD();
  return res;
}

bool BlkDevEnsureInitialized(CBlkDev &bd)
{
  if (bd.flags&BDF_INITIALIZED) return true;
  if (bd.flags&BDF_INIT_IN_PROGRESS) return false;
  bd.flags|=BDF_INIT_IN_PROGRESS;
  DskCacheInvalidateDevice(&bd);
  bool res=false;
  switch (bd.type) {
    case BDT_RAM:
      for (size_t i=0;i<DRVS_NUM;i++) {
        auto &dv=blkdev.drvs[i];
        if (dv.dv_signature==DRV_SIGNATURE_VAL && dv.bd==&bd) {
          res=bd.RAM_dsk && RedSeaInit(dv);
          if (!res) break;
        }
      }
      break;
    case BDT_ATA:
      res=ATAProbe(bd)==BDT_ATA && ATAReadCapacity(bd);
      break;
    case BDT_ATAPI:
      res=ATAPIReadCapacity(bd);
      if (res)
        for (size_t i=0;i<DRVS_NUM;i++) {
          auto &dv=blkdev.drvs[i];
          if (dv.dv_signature==DRV_SIGNATURE_VAL && dv.bd==&bd && !ISOInit(dv)) {
            res=false;
            break;
          }
        }
      break;
  }
  bd.flags&=~BDF_INIT_IN_PROGRESS;
  if (res) bd.flags|=BDF_INITIALIZED;
  return res;
}

extern "C" uint64_t BlkDevInitCpp(CBlkDev *bd)
{
  BlkDevChkCpp(bd,true);
  CDiskLock lock(*bd);
  return BlkDevEnsureInitialized(*bd);
}

extern "C" void BlkDevsReleaseCpp()
{
  if (!blkdev.blkdevs) return;
  for (size_t i=0;i<BLKDEVS_NUM;i++) {
    auto *bd=&blkdev.blkdevs[i];
    if (bd->bd_signature==BD_SIGNATURE_VAL && bd->owning_task==CurrentTask())
      BlkDevUnlockOwner(bd,true);
  }
}

CBlkDev *BlkDevNextFreeSlot(uint8_t first_drv_let,uint32_t type)
{
  if (!blkdev.blkdevs)
    return nullptr;
  CMemLock lock(slot_flags);
  for (size_t i=0;i<BLKDEVS_NUM;i++) {
    auto *res=&blkdev.blkdevs[i];
    if (res->bd_signature!=BD_SIGNATURE_VAL) {
      MemSet(res,0,sizeof(CBlkDev));
      res->first_drv_let=first_drv_let;
      res->type=type;
      res->flags=BDF_READ_CACHE;
      res->blk_size=BLK_SIZE;
      res->max_blk=0xEFFFFFFF;
      switch (type) {
        case BDT_RAM:
          res->flags&=~BDF_READ_CACHE;
          break;
        case BDT_ISO_FILE_READ:
          res->flags|=BDF_READ_ONLY;
          break;
        case BDT_ATAPI:
          res->flags|=BDF_REMOVABLE|BDF_READ_ONLY;
          res->blk_size=DVD_BLK_SIZE;
          break;
      }
      return res;
    }
  }
  return nullptr;
}

extern "C" CBlkDev *BlkDevNextFreeSlotCpp(uint8_t first_drv_let,int64_t type)
{
  if (type!=BDT_NULL && Let2BlkDevTypeCpp(first_drv_let)==type)
    if (auto *res=BlkDevNextFreeSlot(Let2LetCpp(first_drv_let),type))
      return res;
  RaiseHolyC(0x7665446B6C42); // BlkDev
}

extern "C" CBlkDev *BlkDevChkCpp(CBlkDev *bd,uint8_t except)
{
  if (!bd || bd->bd_signature!=BD_SIGNATURE_VAL ||
      !(BDT_NULL<bd->type && bd->type<=BDT_ATAPI)) {
    if (except)
      RaiseHolyC(0x7665446B6C42); // BlkDev
    return nullptr;
  }
  return bd;
}

extern "C" CBlkDev *Let2BlkDevCpp(uint8_t drv_let,uint8_t except)
{
  if (auto *dv=Let2DrvCpp(drv_let,except))
    return BlkDevChkCpp(dv->bd,except);
  return nullptr;
}
