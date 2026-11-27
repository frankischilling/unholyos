#include "../KernelB.hpp"

static uint64_t slot_flags;

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
