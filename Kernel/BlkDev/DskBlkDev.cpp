#include "../KernelB.hpp"

uint64_t disk_lock_flags;

bool BlkDevRegistered(const CBlkDev *bd)
{
  if (blkdev.blkdevs)
    for (size_t i=0;i<BLKDEVS_NUM;i++)
      if (bd==&blkdev.blkdevs[i])
        return bd->bd_signature==BD_SIGNATURE_VAL;
  return false;
}

CBlkDev *BlkDevLockTarget(CBlkDev *bd)
{
  // Bound forwarding traversal even if a guest supplies a cyclic record.
  for (size_t i=0;i<BLKDEVS_NUM && BlkDevChkCpp(bd,false);i++) {
    if ((bd->type==BDT_ISO_FILE_READ || bd->type==BDT_ISO_FILE_WRITE) &&
        !BlkDevImageChain(*bd))
      return nullptr;
    if (!bd->lock_fwding) return bd;
    if (!BlkDevRegistered(bd->lock_fwding)) return nullptr;
    if (bd->type!=BDT_ISO_FILE_READ && bd->type!=BDT_ISO_FILE_WRITE &&
        ((bd->type!=BDT_ATA && bd->type!=BDT_ATAPI) ||
         (bd->lock_fwding->type!=BDT_ATA && bd->lock_fwding->type!=BDT_ATAPI) ||
         bd->base0!=bd->lock_fwding->base0 ||
         bd->base1!=bd->lock_fwding->base1))
      return nullptr;
    bd=bd->lock_fwding;
  }
  return nullptr;
}

// A direct optical dependency holds a media-removal pin until the last
// image on that physical drive is detached. Nested images share this pin.
static bool media_pinned[BLKDEVS_NUM];

bool BlkDevMediaPinned(const CBlkDev &bd)
{
  if (blkdev.blkdevs)
    for (size_t i=0;i<BLKDEVS_NUM;i++)
      if (media_pinned[i] && BlkDevSameBacking(blkdev.blkdevs[i],bd))
        return true;
  return false;
}

bool BlkDevMediaPin(CBlkDev &bd)
{
  if (bd.type!=BDT_ATAPI || !BlkDevRegistered(&bd))
    return false;
  if (BlkDevMediaPinned(bd))
    return true;
  if (!ATAPIPreventRemoval(bd,true))
    return false;
  for (size_t i=0;i<BLKDEVS_NUM;i++)
    if (&bd==&blkdev.blkdevs[i]) {
      media_pinned[i]=true;
      return true;
    }
  return false;
}

void BlkDevMediaUnpin(CBlkDev &bd)
{
  if (bd.type!=BDT_ATAPI || BlkDevImageBacking(&bd) ||
      !BlkDevMediaPinned(bd))
    return;
  ATAPIPreventRemoval(bd,false);
  for (size_t i=0;i<BLKDEVS_NUM;i++)
    if (media_pinned[i] && BlkDevSameBacking(blkdev.blkdevs[i],bd))
      media_pinned[i]=false;
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
    if (blkdev.blkdevs)
      for (size_t i=0;i<BLKDEVS_NUM;i++) {
        auto &image=blkdev.blkdevs[i];
        if (image.bd_signature==BD_SIGNATURE_VAL &&
            (image.type==BDT_ISO_FILE_READ || image.type==BDT_ISO_FILE_WRITE) &&
            BlkDevLockTarget(&image)==bd) {
          image.flags&=~(BDF_INITIALIZED|BDF_INIT_IN_PROGRESS);
          DskCacheInvalidateDevice(&image);
        }
      }
    DrvFATCacheResetDevice(bd,true);
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
  if (bd.type==BDT_ISO_FILE_READ || bd.type==BDT_ISO_FILE_WRITE) {
    if (!BlkDevImageChain(bd) ||
        !BlkDevEnsureInitialized(*bd.file_dsk->dv->bd) ||
        !BlkDevImageChain(bd))
      return false;
  }
  if (bd.flags&BDF_INITIALIZED) return true;
  if (bd.flags&BDF_INIT_IN_PROGRESS) return false;
  if (blkdev.drvs)
    for (size_t i=0;i<DRVS_NUM;i++)
      if (blkdev.drvs[i].bd==&bd && (blkdev.drvs[i].fat_blk_dirty&1))
        return false;
  bd.flags|=BDF_INIT_IN_PROGRESS;
  DskCacheInvalidateDevice(&bd);
  DrvFATCacheResetDevice(&bd);
  bool res=false;
  switch (bd.type) {
    case BDT_RAM:
      for (size_t i=0;i<DRVS_NUM;i++) {
        auto &dv=blkdev.drvs[i];
        if (dv.dv_signature==DRV_SIGNATURE_VAL && dv.bd==&bd) {
          uint16_t disabled=dv.fs_type&~FSG_TYPE_MASK;
          res=bd.RAM_dsk && RedSeaInit(dv) && DrvFATCacheInit(dv);
          dv.fs_type|=disabled;
          if (!res) break;
        }
      }
      break;
    case BDT_ISO_FILE_READ:
    case BDT_ISO_FILE_WRITE: {
      uint64_t physical;
      res=bd.file_dsk &&
          !(bd.file_dsk->flags&(FF_BUF_DIRTY|FF_NEEDS_WRITE|FF_NEW_FILE)) &&
          bool(bd.file_dsk->flags&FF_WRITE)==(bd.type==BDT_ISO_FILE_WRITE) &&
          FImageByteRange(*bd.file_dsk,0,0,physical) &&
          bd.max_blk==bd.file_dsk->max_blk;
      if (res)
        for (size_t i=0;i<DRVS_NUM;i++) {
          auto &dv=blkdev.drvs[i];
          if (dv.dv_signature==DRV_SIGNATURE_VAL && dv.bd==&bd) {
            uint16_t disabled=dv.fs_type&~FSG_TYPE_MASK;
            res=RedSeaInit(dv) && DrvFATCacheInit(dv);
            dv.fs_type|=disabled;
            if (!res) break;
          }
        }
      break;
    }
    case BDT_ATA:
      res=ATAProbe(bd)==BDT_ATA && ATAReadCapacity(bd);
      if (res)
        for (size_t i=0;i<DRVS_NUM;i++) {
          auto &dv=blkdev.drvs[i];
          if (dv.dv_signature==DRV_SIGNATURE_VAL && dv.bd==&bd &&
              (dv.fs_type&FSG_TYPE_MASK)==FSt_FAT32) {
            uint16_t disabled=dv.fs_type&~FSG_TYPE_MASK;
            res=FAT32Init(dv) && DrvFATCacheInit(dv);
            dv.fs_type|=disabled;
            if (!res) break;
          }
        }
      break;
    case BDT_ATAPI:
      res=ATAPIReadCapacity(bd);
      if (res)
        for (size_t i=0;i<DRVS_NUM;i++) {
          auto &dv=blkdev.drvs[i];
          if (dv.dv_signature==DRV_SIGNATURE_VAL && dv.bd==&bd) {
            uint16_t disabled=dv.fs_type&~FSG_TYPE_MASK;
            res=ISOInit(dv) && DrvFATCacheInit(dv);
            dv.fs_type|=disabled;
            if (!res) break;
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
  CMemLock lock(mount_flags);
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

bool BlkDevDeleteReady(const CBlkDev *bd)
{
  if (!bd || !blkdev.blkdevs || !blkdev.drvs) return false;
  bool found=false;
  for (size_t i=0;i<BLKDEVS_NUM;i++)
    if (bd==&blkdev.blkdevs[i]) {
      found=true;
      break;
    }
  if (!found || (bd->locked_flags&1) || bd->owning_task ||
      (bd->flags&BDF_INIT_IN_PROGRESS))
    return false;
  if (bd->bd_signature==BD_SIGNATURE_VAL && BlkDevImageBacking(bd))
    return false;
  if (bd->file_dsk &&
      ((bd->type!=BDT_ISO_FILE_READ && bd->type!=BDT_ISO_FILE_WRITE) ||
       (bd->file_dsk->flags&(FF_BUF_DIRTY|FF_NEEDS_WRITE|FF_NEW_FILE))))
    return false;
  auto *lock_bd=BlkDevLockTarget(const_cast<CBlkDev *>(bd));
  if (bd->bd_signature==BD_SIGNATURE_VAL &&
      (!lock_bd || (lock_bd->locked_flags&1) || lock_bd->owning_task))
    return false;
  auto *owned_ram=BlkDevOwnedRAM(bd);
  for (size_t i=0;i<BLKDEVS_NUM;i++) {
    const auto &other=blkdev.blkdevs[i];
    if (&other==bd || other.bd_signature!=BD_SIGNATURE_VAL) continue;
    if (other.lock_fwding==bd ||
        (other.file_dsk && other.file_dsk->dv && other.file_dsk->dv->bd==bd) ||
        (owned_ram && other.RAM_dsk==owned_ram))
      return false;
  }
  for (size_t i=0;i<DRVS_NUM;i++)
    if (blkdev.drvs[i].bd==bd && !DrvDeleteReady(&blkdev.drvs[i]))
      return false;
  return true;
}

void BlkDevDelete(CBlkDev &bd,uint8_t *keep_ram)
{
  auto *parent=bd.file_dsk && bd.file_dsk->dv?bd.file_dsk->dv->bd:nullptr;
  if (bd.type==BDT_ATAPI)
    BlkDevMediaUnpin(bd);
  DskCacheInvalidateDevice(&bd);
  for (size_t i=0;i<DRVS_NUM;i++)
    if (blkdev.drvs[i].bd==&bd) DrvDelete(blkdev.drvs[i]);
  FCloseImage(bd.file_dsk);
  BlkDevReleaseRAM(bd,keep_ram);
  Free(bd.file_dsk_name);
  Free(bd.dev_id_record);
  MemSet(&bd,0,sizeof(bd));
  if (parent && parent->type==BDT_ATAPI)
    BlkDevMediaUnpin(*parent);
}

extern "C" void BlkDevDelCpp(CBlkDev *bd)
{
  bool okay;
  {
    CMemLock mount(mount_flags);
    CMemLock lock(disk_lock_flags);
    okay=BlkDevDeleteReady(bd);
    if (okay) BlkDevDelete(*bd);
  }
  if (!okay) RaiseHolyC(0x7665446B6C42); // BlkDev
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
