#include "../KernelB.hpp"

extern "C" void PrintCpp(const char *fmt,int64_t argc,const uint64_t *argv);
extern "C" void PrintErrCpp(const char *fmt,int64_t argc,const uint64_t *argv,
                           const uintptr_t *frame);

// Which physical FAT copy the clean buffer represents. The guest-visible
// record retains the reference layout and its single dirty bit.
static uint8_t fat_copy[DRVS_NUM];

static uint8_t *DrvFATCopy(const CDrv &dv)
{
  if (blkdev.drvs)
    for (size_t i=0;i<DRVS_NUM;i++)
      if (&dv==&blkdev.drvs[i]) return &fat_copy[i];
  return nullptr;
}

static bool DrvFATSector(const CDrv &dv,int64_t index,uint8_t copy,uint64_t &blk)
{
  if (!dv.bd || dv.bd->max_blk<0 || dv.drv_offset<0 || dv.size<=0 ||
      index<0 || dv.fat1<=dv.drv_offset || dv.fat2<dv.fat1)
    return false;
  uint64_t end=uint64_t(dv.drv_offset)+dv.size;
  if (end>uint64_t(dv.bd->max_blk)+1 || end>UINT64_MAX/BLK_SIZE)
    return false;
  uint64_t limit;
  switch (dv.fs_type&FSG_TYPE_MASK) {
    case FSt_REDSEA:
      if (dv.fat1!=dv.fat2 || dv.data_area<dv.fat1 ||
          uint64_t(dv.data_area)>=end)
        return false;
      limit=uint64_t(dv.data_area)-dv.fat1+1;
      break;
    case FSt_FAT32: {
      if (dv.spc<=0 || dv.spc>128 || dv.data_area<dv.drv_offset)
        return false;
      uint64_t data=uint64_t(dv.data_area)+2*dv.spc;
      if (data>=end || uint64_t(dv.fat2)>=data)
        return false;
      limit=data-dv.fat2;
      if (dv.fat1!=dv.fat2 && uint64_t(dv.fat2-dv.fat1)<limit)
        limit=dv.fat2-dv.fat1;
      break;
    }
    default:
      return false;
  }
  if (uint64_t(index)>=limit)
    return false;
  blk=uint64_t(copy?dv.fat2:dv.fat1)+index;
  return blk<end;
}

static bool DrvFATSelector(int64_t fat_sel)
{
  return fat_sel==0 || fat_sel==1 || fat_sel==3;
}

void DrvFATCacheReset(CDrv &dv)
{
  dv.cur_fat_blk_num=-1;
  dv.fat_blk_dirty=0;
  if (auto *copy=DrvFATCopy(dv)) *copy=0;
}

void DrvFATCacheDrop(CDrv &dv)
{
  Free(dv.cur_fat_blk);
  dv.cur_fat_blk=nullptr;
  DrvFATCacheReset(dv);
}

void DrvFATCacheResetDevice(CBlkDev *bd,bool controller)
{
  if (!bd || !blkdev.drvs) return;
  for (size_t i=0;i<DRVS_NUM;i++) {
    auto &dv=blkdev.drvs[i];
    if (dv.dv_signature==DRV_SIGNATURE_VAL &&
        (dv.bd==bd || (controller && BlkDevLockTarget(dv.bd)==bd)))
      DrvFATCacheReset(dv);
  }
}

bool DrvFATCacheInit(CDrv &dv)
{
  uint64_t blk;
  if (!adam_task || !DrvFATSector(dv,0,0,blk)) return false;
  uint32_t sector[BLK_SIZE/4];
  if (!BlkDevReadBytes(*dv.bd,reinterpret_cast<uint8_t *>(sector),blk*BLK_SIZE,BLK_SIZE))
    return false;
  if (!dv.cur_fat_blk) {
    dv.cur_fat_blk=reinterpret_cast<uint32_t *>(MAlloc(BLK_SIZE,adam_task->data_heap));
    if (!dv.cur_fat_blk) return false;
  }
  MemCpy(dv.cur_fat_blk,sector,BLK_SIZE);
  dv.cur_fat_blk_num=0;
  dv.fat_blk_dirty=0;
  if (auto *copy=DrvFATCopy(dv)) *copy=0;
  return true;
}

static bool DrvFATOverlap(uint64_t blk,uint64_t pos,size_t bytes)
{
  uint64_t start=blk*BLK_SIZE;
  return bytes && (start>=pos?start-pos<bytes:pos-start<BLK_SIZE);
}

bool DrvFATWriteAllowedImage(CBlkDev *bd,uint64_t pos,size_t bytes,
                             const CDrv *origin)
{
  if (!bytes || !blkdev.drvs) return true;
  for (size_t i=0;i<DRVS_NUM;i++) {
    auto &dv=blkdev.drvs[i];
    if (&dv==origin || dv.dv_signature!=DRV_SIGNATURE_VAL || dv.bd!=bd ||
        !dv.cur_fat_blk || !(dv.fat_blk_dirty&1))
      continue;
    uint64_t blk1,blk2;
    if (!DrvFATSector(dv,dv.cur_fat_blk_num,0,blk1) ||
        !DrvFATSector(dv,dv.cur_fat_blk_num,1,blk2) ||
        DrvFATOverlap(blk1,pos,bytes) || DrvFATOverlap(blk2,pos,bytes))
      return false;
  }
  return true;
}

bool DrvFATWriteAllowed(CBlkDev *bd,uint64_t pos,size_t bytes,const CDrv *origin)
{
  return (!bytes || !BlkDevImageBacking(bd)) &&
         DrvFATWriteAllowedImage(bd,pos,bytes,origin);
}

void DrvFATWriteThrough(CBlkDev *bd,const uint8_t *buf,uint64_t pos,
                        size_t bytes,bool okay,const CDrv *origin)
{
  if (!blkdev.drvs) return;
  for (size_t i=0;i<DRVS_NUM;i++) {
    auto &dv=blkdev.drvs[i];
    if (&dv==origin || dv.dv_signature!=DRV_SIGNATURE_VAL || dv.bd!=bd ||
        !dv.cur_fat_blk || dv.cur_fat_blk_num<0 || (dv.fat_blk_dirty&1))
      continue;
    uint64_t blk;
    if (!DrvFATSector(dv,dv.cur_fat_blk_num,fat_copy[i],blk)) {
      DrvFATCacheReset(dv);
      continue;
    }
    if (!DrvFATOverlap(blk,pos,bytes)) continue;
    if (!okay) {
      DrvFATCacheReset(dv);
      continue;
    }
    uint64_t start=blk*BLK_SIZE;
    size_t src=start>pos?start-pos:0,dst=pos>start?pos-start:0;
    size_t cnt=bytes-src;
    if (cnt>BLK_SIZE-dst) cnt=BLK_SIZE-dst;
    MemCpy(reinterpret_cast<uint8_t *>(dv.cur_fat_blk)+dst,buf+src,cnt);
  }
}

static bool DrvFATClean(CDrv &dv,int64_t fat_sel)
{
  if (!(dv.fat_blk_dirty&1)) return true;
  uint64_t blk1,blk2;
  if (!dv.cur_fat_blk || !DrvFATSector(dv,dv.cur_fat_blk_num,0,blk1) ||
      !DrvFATSector(dv,dv.cur_fat_blk_num,1,blk2))
    return false;
  auto *buf=reinterpret_cast<const uint8_t *>(dv.cur_fat_blk);
  if (blk1==blk2 || fat_sel==0 || fat_sel==3)
    if (!BlkDevWriteBytes(*dv.bd,buf,blk1*BLK_SIZE,BLK_SIZE,&dv)) return false;
  if (blk1!=blk2 && (fat_sel==1 || fat_sel==3))
    if (!BlkDevWriteBytes(*dv.bd,buf,blk2*BLK_SIZE,BLK_SIZE,&dv)) return false;
  if (blk1==blk2 || fat_sel!=0) {
    dv.fat_blk_dirty&=~int64_t(1);
    if (auto *copy=DrvFATCopy(dv)) *copy=blk1!=blk2 && fat_sel==1;
  }
  return true;
}

static bool DrvFATLoad(CDrv &dv,int64_t index,uint8_t copy)
{
  uint64_t blk;
  if (!DrvFATSector(dv,index,copy,blk) || !dv.cur_fat_blk) return false;
  uint32_t sector[BLK_SIZE/4];
  if (!BlkDevReadBytes(*dv.bd,reinterpret_cast<uint8_t *>(sector),blk*BLK_SIZE,BLK_SIZE))
    return false;
  MemCpy(dv.cur_fat_blk,sector,BLK_SIZE);
  dv.cur_fat_blk_num=index;
  if (auto *sel=DrvFATCopy(dv)) *sel=copy;
  return true;
}

bool DrvFATRead(const CDrv &drive,uint64_t index,void *buf,size_t offset,size_t bytes)
{
  if (!buf || offset>BLK_SIZE || bytes>BLK_SIZE-offset || index>INT64_MAX)
    return false;
  auto &dv=const_cast<CDrv &>(drive);
  uint64_t blk;
  if (!DrvFATSector(dv,index,0,blk)) return false;
  auto *copy=DrvFATCopy(dv);
  if (!copy || !dv.cur_fat_blk)
    return BlkDevReadBytes(*dv.bd,reinterpret_cast<uint8_t *>(buf),blk*BLK_SIZE+offset,bytes);
  CDiskLock lock(dv);
  if (!BlkDevEnsureInitialized(*dv.bd) || !DrvFATSector(dv,index,0,blk))
    return false;
  if (dv.fat_blk_dirty&1) {
    // A guest may be preparing a sector for a selected mirror. Do not evict
    // that pending write to satisfy a native read of a different sector.
    if (dv.cur_fat_blk_num!=int64_t(index))
      return BlkDevReadBytes(*dv.bd,reinterpret_cast<uint8_t *>(buf),blk*BLK_SIZE+offset,bytes);
  } else if (dv.cur_fat_blk_num!=int64_t(index) || *copy) {
    if (!DrvFATLoad(dv,index,0)) return false;
  }
  MemCpy(buf,reinterpret_cast<uint8_t *>(dv.cur_fat_blk)+offset,bytes);
  return true;
}

bool DrvLockOwner(CDrv *dv)
{
  auto *task=CurrentTask();
  for (;;) {
    {
      CMemLock lock(disk_lock_flags);
      if (!(dv->locked_flags&1)) {
        dv->locked_flags|=1;
        dv->owning_task=task;
        return true;
      }
      if (dv->owning_task==task) return false;
    }
    _YIELD();
  }
}

bool DrvUnlockOwner(CDrv *dv)
{
  CMemLock lock(disk_lock_flags);
  if (!(dv->locked_flags&1) || dv->owning_task!=CurrentTask())
    return false;
  dv->owning_task=nullptr;
  dv->locked_flags&=~uint64_t(1);
  return true;
}

extern "C" uint64_t DrvLockCpp(CDrv *dv)
{
  DrvChkCpp(dv,true);
  BlkDevLockCpp(dv->bd);
  return DrvLockOwner(dv);
}

extern "C" uint64_t DrvUnlockCpp(CDrv *dv,uint8_t rst)
{
  DrvChkCpp(dv,true);
  if (!DrvUnlockOwner(dv)) return false;
  if (auto *bd=BlkDevLockTarget(dv->bd)) BlkDevUnlockOwner(bd,rst);
  _YIELD();
  return true;
}

extern "C" void DrvsReleaseCpp()
{
  if (!blkdev.drvs) return;
  for (size_t i=0;i<DRVS_NUM;i++) {
    auto *dv=&blkdev.drvs[i];
    if (dv->dv_signature==DRV_SIGNATURE_VAL && DrvUnlockOwner(dv)) {
      if (auto *bd=BlkDevLockTarget(dv->bd)) BlkDevUnlockOwner(bd,true);
    }
  }
}

static bool DrvSlot(const CDrv *dv)
{
  if (blkdev.drvs)
    for (size_t i=0;i<DRVS_NUM;i++)
      if (dv==&blkdev.drvs[i]) return true;
  return false;
}

static CFreeLst *DrvFreeListHead(CDrv &dv)
{
  return reinterpret_cast<CFreeLst *>(&dv.next_free);
}

static bool DrvFreeListValid(CDrv &dv)
{
  if (!dv.next_free) return true;
  auto *head=DrvFreeListHead(dv),*last=head;
  uint64_t remaining=dv.size>0?uint64_t(dv.size):0;
  for (auto *p=dv.next_free;p!=head;p=p->next) {
    if (!remaining-- || !p || p->last!=last) return false;
    last=p;
  }
  return last==dv.last_free;
}

static void DrvFreeListClear(CDrv &dv)
{
  auto *head=DrvFreeListHead(dv);
  for (auto *p=dv.next_free;p && p!=head;) {
    auto *next=p->next;
    Free(p);
    p=next;
  }
  dv.next_free=dv.last_free=nullptr;
}

static void DrvResidentClear(CDrv &dv)
{
  if (adam_task && adam_task->hash_table)
    for (size_t i=0;i<DRVS_NUM;i++)
      if (blkdev.let_to_drv[i]==&dv)
        ResidentDriveClear('A'+i);
}

bool DrvDeleteReady(const CDrv *dv)
{
  if (!DrvSlot(dv) || (dv->locked_flags&1) || dv->owning_task ||
      (dv->fat_blk_dirty&1))
    return false;
  if (dv->dv_signature==DRV_SIGNATURE_VAL) {
    auto *bd=BlkDevLockTarget(dv->bd);
    if (!bd || (bd->locked_flags&1) || bd->owning_task ||
        (dv->bd->locked_flags&1) || dv->bd->owning_task ||
        (dv->bd->flags&BDF_INIT_IN_PROGRESS))
      return false;
  }
  if (blkdev.blkdevs)
    for (size_t i=0;i<BLKDEVS_NUM;i++) {
      const auto &bd=blkdev.blkdevs[i];
      if (bd.bd_signature==BD_SIGNATURE_VAL && bd.file_dsk &&
          bd.file_dsk->dv==dv)
        return false;
    }
  return DrvFreeListValid(*const_cast<CDrv *>(dv));
}

void DrvDelete(CDrv &dv)
{
  DskCacheInvalidate2Cpp(&dv);
  DrvResidentClear(dv);
  for (size_t i=0;i<DRVS_NUM;i++)
    if (blkdev.let_to_drv[i]==&dv)
      blkdev.let_to_drv[i]=&blkdev.drvs[i];
  DrvFreeListClear(dv);
  DrvFATCacheDrop(dv);
  Free(dv.fis);
  MemSet(&dv,0,sizeof(dv));
}

extern "C" void DrvDelCpp(CDrv *dv)
{
  bool okay;
  {
    CMemLock mount(mount_flags);
    CMemLock lock(disk_lock_flags);
    okay=DrvDeleteReady(dv);
    if (okay) DrvDelete(*dv);
  }
  if (!okay) RaiseHolyC(0x767244); // Drv
}

extern "C" void DrvBlkDevDelCpp(CBlkDev *bd)
{
  bool okay=false;
  {
    CMemLock mount(mount_flags);
    CMemLock lock(disk_lock_flags);
    if (blkdev.blkdevs && blkdev.drvs)
      for (size_t i=0;i<BLKDEVS_NUM;i++)
        if (bd==&blkdev.blkdevs[i]) {
          okay=true;
          break;
        }
    if (okay)
      for (size_t i=0;i<DRVS_NUM;i++)
        if (blkdev.drvs[i].bd==bd && !DrvDeleteReady(&blkdev.drvs[i])) {
          okay=false;
          break;
        }
    if (okay)
      for (size_t i=0;i<DRVS_NUM;i++)
        if (blkdev.drvs[i].bd==bd) DrvDelete(blkdev.drvs[i]);
  }
  if (!okay) RaiseHolyC(0x767244); // Drv
}

extern "C" CDrv *DrvMakeFreeSlotCpp(uint8_t drv_let)
{
  int64_t i=Let2LetCpp(drv_let)-'A';
  if (!(0<=i && i<static_cast<int64_t>(DRVS_NUM)))
    RaiseHolyC(0x767244); // Drv
  auto *res=&blkdev.drvs[i];
  bool okay;
  {
    CMemLock mount(mount_flags);
    CMemLock lock(disk_lock_flags);
    okay=DrvDeleteReady(res);
    if (okay) {
      DrvDelete(*res);
      if (adam_task && adam_task->hash_table)
        ResidentDriveClear('A'+i);
      blkdev.let_to_drv[i]=res;
      res->drv_let='A'+i;
    }
  }
  if (!okay) RaiseHolyC(0x767244); // Drv
  return res;
}

extern "C" uint64_t DrvNextFreeLetCpp(uint8_t first_drv_let)
{
  int64_t i=Let2LetCpp(first_drv_let)-'A',
          type=Let2BlkDevTypeCpp(first_drv_let);
  if (0<=i && i<static_cast<int64_t>(DRVS_NUM))
    do {
      if (blkdev.drvs[i].dv_signature!=DRV_SIGNATURE_VAL) {
        if (Let2BlkDevTypeCpp(i+'A')!=type)
          break;
        return i+'A';
      }
    } while (++i<static_cast<int64_t>(DRVS_NUM));
  RaiseHolyC(0x767244); // Drv
}

extern "C" void DrvFATBlkAllocCpp(CDrv *dv)
{
  DrvChkCpp(dv,true);
  BlkDevChkCpp(dv->bd,true);
  bool okay;
  {
    CDiskLock lock(*dv);
    okay=BlkDevEnsureInitialized(*dv->bd) && DrvFATCacheInit(*dv);
  }
  if (!okay) RaiseHolyC(0x767244); // Drv
}

extern "C" void DrvFATBlkCleanCpp(CDrv *dv,int64_t fat_sel)
{
  DrvChkCpp(dv,true);
  BlkDevChkCpp(dv->bd,true);
  if (!DrvFATSelector(fat_sel)) RaiseHolyC(0x767244);
  bool okay;
  {
    CDiskLock lock(*dv);
    okay=(!(dv->fat_blk_dirty&1) || (dv->bd->flags&BDF_INITIALIZED)) &&
         DrvFATClean(*dv,fat_sel);
  }
  if (!okay) RaiseHolyC(0x767244); // Drv
}

extern "C" void DrvFATBlkSetCpp(CDrv *dv,int64_t c,int64_t fat_sel)
{
  DrvChkCpp(dv,true);
  BlkDevChkCpp(dv->bd,true);
  if (!DrvFATSelector(fat_sel) || c<0) RaiseHolyC(0x767244);
  bool okay=false;
  {
    CDiskLock lock(*dv);
    if (BlkDevEnsureInitialized(*dv->bd)) {
      int64_t index=-1;
      if (dv->fs_type==FSt_FAT32) index=c/(BLK_SIZE/4);
      else if (dv->fs_type==FSt_REDSEA && c>=dv->data_area && dv->data_area>=0)
        index=(c-dv->data_area)/(BLK_SIZE*8);
      uint64_t blk;
      uint8_t copy=fat_sel==1;
      if (DrvFATSector(*dv,index,copy,blk) &&
          (dv->cur_fat_blk || DrvFATCacheInit(*dv))) {
        // The reference does not switch copies when the sector index matches.
        okay=dv->cur_fat_blk_num==index ||
             (DrvFATClean(*dv,fat_sel) && DrvFATLoad(*dv,index,copy));
      }
    }
  }
  if (!okay) RaiseHolyC(0x767244); // Drv
}

extern "C" CDrv *DrvChkCpp(CDrv *dv,uint8_t except)
{
  if (!dv || dv->dv_signature!=DRV_SIGNATURE_VAL) {
    if (except)
      RaiseHolyC(0x767244); // Drv
    return nullptr;
  }
  return dv;
}

extern "C" uint64_t Drv2LetCpp(CDrv *dv)
{
  if (!dv)
    dv=CurrentTask()->cur_dv;
  return DrvChkCpp(dv,true)->drv_let;
}

extern "C" uint64_t Let2LetCpp(uint8_t drv_let)
{
  if (!drv_let)
    drv_let=Drv2LetCpp(nullptr);
  else if (drv_let==':')
    drv_let=blkdev.boot_drv_let;
  else if (drv_let=='~')
    drv_let=*blkdev.home_dir;
  if ('a'<=drv_let && drv_let<='z')
    drv_let-='a'-'A';
  return drv_let;
}

extern "C" int64_t Let2BlkDevTypeCpp(uint8_t drv_let)
{
  drv_let=Let2LetCpp(drv_let);
  if ('A'<=drv_let && drv_let<='B')
    return BDT_RAM;
  if ('C'<=drv_let && drv_let<='L')
    return BDT_ATA;
  if ('M'<=drv_let && drv_let<='P')
    return BDT_ISO_FILE_READ;
  if ('Q'<=drv_let && drv_let<='S')
    return BDT_ISO_FILE_WRITE;
  if ('T'<=drv_let && drv_let<='Z')
    return BDT_ATAPI;
  return BDT_NULL;
}

CDrv *MountedDrive(char letter)
{
  auto *dv='A'<=letter && letter<='Z' ? blkdev.let_to_drv[letter-'A'] :
                                      nullptr;
  return DrvChkCpp(dv,false);
}

extern "C" CDrv *Let2DrvCpp(uint8_t drv_let,uint8_t except)
{
  auto *dv=drv_let ? MountedDrive(Let2LetCpp(drv_let)) :
                     CurrentTask()->cur_dv;
  return DrvChkCpp(dv,except);
}

extern "C" CBlkDev *DrvIsWritableCpp(uint8_t drv_let,uint8_t except)
{
  auto *bd=Let2BlkDevCpp(drv_let,except);
  if (!bd || bd->flags&BDF_READ_ONLY || BlkDevImageBacking(bd)) {
    if (except)
      RaiseHolyC(0x767244); // Drv
    return nullptr;
  }
  return bd;
}

extern "C" uint64_t DrvMapCpp(uint8_t drv_let,CDrv *dv)
{
  drv_let=Let2LetCpp(drv_let);
  if ('A'<=drv_let && drv_let<='Z' && DrvSlot(dv)) {
    CMemLock mount(mount_flags);
    CMemLock lock(disk_lock_flags);
    if (dv->dv_signature!=DRV_SIGNATURE_VAL) return false;
    if (adam_task && adam_task->hash_table &&
        blkdev.let_to_drv[drv_let-'A']!=dv)
      ResidentDriveClear(drv_let);
    blkdev.let_to_drv[drv_let-'A']=dv;
    dv->drv_let=drv_let;
    return true;
  }
  return false;
}

// Caller owns drive and device. No cache is discarded if a guest has a
// pending allocation-sector write.
static bool DrvRefresh(CDrv &dv,bool invalidate,bool probe)
{
  auto &bd=*dv.bd;
  if ((dv.fat_blk_dirty&1) || !DrvFreeListValid(dv))
    return false;
  bool initialized=bd.flags&BDF_INITIALIZED;
  if (!BlkDevEnsureInitialized(bd)) return false;
  if (invalidate) {
    DskCacheInvalidate2Cpp(&dv);
    DrvResidentClear(dv);
    if (bd.type==BDT_ATAPI && !(bd.flags&BDF_READ_ONLY_OVERRIDE)) {
      uint16_t disabled=dv.fs_type&~FSG_TYPE_MASK;
      if (initialized) {
        bool okay=(!probe || (ATAProbe(bd)==BDT_ATAPI &&
                              ATAPIStartStop(bd,true))) &&
                  ATAPIReadCapacity(bd) && ISOInit(dv) &&
                  DrvFATCacheInit(dv);
        dv.fs_type|=disabled;
        if (!okay) {
          bd.flags&=~BDF_INITIALIZED;
          DrvFATCacheReset(dv);
          return false;
        }
      }
      DrvFreeListClear(dv);
    }
  }
  return true;
}

extern "C" void DskCacheInvalidateCpp(CDrv *dv)
{
  DrvChkCpp(dv,true);
  if (!BlkDevLockTarget(dv->bd)) RaiseHolyC(0x767244);
  bool okay;
  {
    CDiskLock lock(*dv);
    okay=DrvRefresh(*dv,true,false);
  }
  if (!okay) RaiseHolyC(0x767244); // Drv
}

static bool DrvChangeMedia(CDrv &dv)
{
  if (!BlkDevLockTarget(dv.bd)) return false;
  CDiskLock lock(dv);
  bool invalidate=(dv.bd->flags&(BDF_INITIALIZED|BDF_REMOVABLE))==
                  (BDF_INITIALIZED|BDF_REMOVABLE);
  if (!DrvRefresh(dv,invalidate,invalidate)) return false;
  DrvFreeListClear(dv);
  return true;
}

extern "C" void DskChgCpp(uint8_t drv_let)
{
  auto *dv=Let2DrvCpp(drv_let,true);
  if (!DrvChangeMedia(*dv)) RaiseHolyC(0x767244); // Drv
  DrvCpp(drv_let);
}

extern "C" uint64_t DrvCpp(uint8_t drv_let)
{
  auto *dv=Let2DrvCpp(drv_let,true);
  BlkDevChkCpp(dv->bd,true);
  auto *task=CurrentTask();
  if (dv!=task->cur_dv && !(dv->bd->flags&BDF_INITIALIZED) &&
      (dv->bd->type==BDT_RAM || dv->bd->type==BDT_ISO_FILE_READ ||
       dv->bd->type==BDT_ISO_FILE_WRITE ||
       (dv->bd->flags&BDF_REMOVABLE)))
    if (!DrvChangeMedia(*dv)) return false;
  auto *dir=StrNew("/",task->data_heap);
  if (!dir)
    return false;
  task->cur_dv=dv;
  Free(task->cur_dir);
  task->cur_dir=dir;
  if (ReadableFS(*dv))
    return true;
  PrintErrCpp("File System Not Supported\n",0,nullptr,nullptr);
  return false;
}

static char *DrvIDString(uint8_t drv_let,size_t word,size_t cnt)
{
  auto *bd=Let2BlkDevCpp(drv_let,true);
  if (!bd->dev_id_record)
    return nullptr;
  auto *st=CAlloc(cnt*2+1,CurrentTask()->data_heap);
  if (!st)
    return nullptr;
  for (size_t i=0;i<cnt;i++) {
    auto val=bd->dev_id_record[word+i];
    st[i*2]=val>>8;
    st[i*2+1]=val;
  }
  auto *res=MStrUtilCpp(reinterpret_cast<char *>(st),
                       SUF_REM_LEADING|SUF_REM_TRAILING,0);
  Free(st);
  return res;
}

extern "C" char *DrvSerialNumCpp(uint8_t drv_let)
{
  return DrvIDString(drv_let,10,10);
}

extern "C" char *DrvModelNumCpp(uint8_t drv_let)
{
  return DrvIDString(drv_let,27,20);
}

extern "C" {
uint8_t blkdev_text_attr[6]={0,11,15,10,12,9};
uint8_t drv_text_attr[3]={0,1,4};
}

extern "C" uint64_t DrvTextAttrGetCpp(uint8_t drv_let)
{
  drv_let=Let2LetCpp(drv_let);
  if ('A'<=drv_let && drv_let<='Z')
    return blkdev_text_attr[Let2BlkDevTypeCpp(drv_let)]<<4 |
           drv_text_attr[drv_let%sizeof(drv_text_attr)];
  return 15;
}

extern "C" void DrvRepCpp()
{
  PutS("\nDefined Drives:\n");
  for (size_t i=0;i<DRVS_NUM;i++) {
    auto *dv=&blkdev.drvs[i];
    if (dv->dv_signature!=DRV_SIGNATURE_VAL)
      continue;
    auto *bd=dv->bd;
    auto drv_let=Drv2LetCpp(dv);
    auto attr=DrvTextAttrGetCpp(drv_let);
    const uint64_t args[]{
        attr&15,attr>>4,drv_let,
        uint64_t(dv->fs_type&FSG_TYPE_MASK),reinterpret_cast<uintptr_t>("ST_DRV_TYPES"),
        bd->type,reinterpret_cast<uintptr_t>("ST_BLKDEV_TYPES"),
        bd->base0,bd->base1,bd->unit};
    PrintCpp("$FG,%d$$BG,%d$%C %-8Z %-10Z %04X %04X %02X\n",10,args);
    if (auto *st=DrvModelNumCpp(drv_let)) {
      const uint64_t arg=reinterpret_cast<uintptr_t>(st);
      PrintCpp("   Model# :%s\n",1,&arg);
      Free(st);
    }
    if (auto *st=DrvSerialNumCpp(drv_let)) {
      const uint64_t arg=reinterpret_cast<uintptr_t>(st);
      PrintCpp("   Serial#:%s\n",1,&arg);
      Free(st);
    }
    if (bd->type==BDT_ISO_FILE_READ || bd->type==BDT_ISO_FILE_WRITE) {
      const uint64_t arg=reinterpret_cast<uintptr_t>(bd->file_dsk_name);
      PrintCpp("   File=\"%s\"\n",1,&arg);
    }
    const uint64_t range[]{uint64_t(dv->drv_offset),
                           uint64_t(dv->drv_offset)+uint64_t(dv->size)-1};
    PrintCpp("   %016X-%016X\n$FG$$BG$",2,range);
  }
  const uint64_t home=reinterpret_cast<uintptr_t>(blkdev.home_dir);
  PrintCpp("Home Dir:\"%s\"\n",1,&home);
}
