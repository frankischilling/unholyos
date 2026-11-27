#include "../KernelB.hpp"

static uint8_t *ram_owned[BLKDEVS_NUM];
uint64_t mount_flags;

uint8_t *BlkDevOwnedRAM(const CBlkDev *bd)
{
  if (!blkdev.blkdevs) return nullptr;
  for (size_t i=0;i<BLKDEVS_NUM;i++)
    if (bd==&blkdev.blkdevs[i]) return ram_owned[i];
  return nullptr;
}

void BlkDevReleaseRAM(CBlkDev &bd,uint8_t *keep_ram)
{
  if (!blkdev.blkdevs) return;
  for (size_t i=0;i<BLKDEVS_NUM;i++)
    if (&bd==&blkdev.blkdevs[i]) {
      if (ram_owned[i]!=keep_ram) Free(ram_owned[i]);
      ram_owned[i]=nullptr;
      return;
    }
}

static void BlkDevShareController(CBlkDev &bd)
{
  for (size_t i=0;i<BLKDEVS_NUM;i++) {
    auto *other=&blkdev.blkdevs[i];
    if (other->bd_signature==BD_SIGNATURE_VAL &&
        (other->type==BDT_ATA || other->type==BDT_ATAPI) &&
        other->base0==bd.base0 && other->base1==bd.base1) {
      bd.lock_fwding=BlkDevLockTarget(other);
      break;
    }
  }
}

bool BlkDevGlblsInit()
{
  auto *hc = adam_task->data_heap;
  auto *devices =
      reinterpret_cast<CBlkDev *>(CAlloc(sizeof(CBlkDev) * BLKDEVS_NUM, hc));
  auto *drives = reinterpret_cast<CDrv *>(CAlloc(sizeof(CDrv) * DRVS_NUM, hc));
  auto *iso = StrNew(DFT_ISO_FILENAME, hc);
  auto *iso_c = StrNew(DFT_ISO_C_FILENAME, hc);
  auto *tmp = StrNew(DFT_TMP_FILENAME, hc);
  char home[]{"T:/Home"};
  home[0] = BOOT_DVD_LET;
  auto *home_dir = StrNew(home, hc);
  if (!devices || !drives || !iso || !iso_c || !tmp || !home_dir) {
    Free(devices);
    Free(drives);
    Free(iso);
    Free(iso_c);
    Free(tmp);
    Free(home_dir);
    return false;
  }
  blkdev.blkdevs = devices;
  blkdev.drvs = drives;
  blkdev.dft_iso_filename = iso;
  blkdev.dft_iso_c_filename = iso_c;
  blkdev.tmp_filename = tmp;
  blkdev.home_dir = home_dir;
  blkdev.boot_drv_let = BOOT_DVD_LET;
  blkdev.first_hd_drv_let = 'C';
  blkdev.first_dvd_drv_let = BOOT_DVD_LET;
  for (size_t i = 0; i < DRVS_NUM; i++)
    blkdev.let_to_drv[i] = drives + i;
  // STD_DISTRO_DVD_CFG in KernelA.HH selects this Scale2Mem range.
  auto *bp=SYS_DATA_BP?SYS_DATA_BP:SYS_CODE_BP;
  constexpr int64_t limit=2LL*1024*1024*1024;
  int64_t mem=bp->alloced_u8s;
  DskCacheInitCpp(mem>=limit?0x40000:2048+(0x40000-2048)*mem/limit);
  return true;
}

static int64_t BlkDevAddATA(CBlkDev *bd,int64_t prt_num,bool whole_drv)
{
  if (whole_drv || bd->first_drv_let<'C' || bd->first_drv_let>'L' ||
      !bd->base0 || bd->unit>1)
    return 0;
  CBlkDev device;
  MemCpy(&device,bd,sizeof(device));
  if (ATAProbe(device)!=BDT_ATA || !ATAReadCapacity(device))
    return 0;
  uint8_t mbr[BLK_SIZE];
  if (!ATAReadBytes(device,mbr,0,sizeof(mbr)) || mbr[510]!=0x55 || mbr[511]!=0xAA)
    return 0;
  int64_t res=0,num=0;
  char letter=device.first_drv_let;
  for (unsigned i=0;i<4;i++) {
    auto *p=mbr+446+16*i;
    if (!p[4])
      continue;
    auto current=num++;
    if ((prt_num>=0 && current!=prt_num) ||
        (p[4]!=0x0B && p[4]!=0x0C && p[4]!=0x1B && p[4]!=0x1C))
      continue;
    uint32_t offset,size;
    MemCpy(&offset,p+8,sizeof(offset));
    MemCpy(&size,p+12,sizeof(size));
    if (!offset || !size || uint64_t(offset)+size>uint64_t(device.max_blk)+1)
      continue;
    while (letter<='L' && (MountedDrive(letter) ||
           blkdev.drvs[letter-'A'].dv_signature==DRV_SIGNATURE_VAL))
      letter++;
    if (letter>'L')
      break;
    CDrv drive{};
    drive.bd=&device;
    drive.drv_offset=offset;
    drive.size=size;
    drive.prt_num=current;
    drive.drv_let=letter;
    if (!FAT32Init(drive) || !DrvFATCacheInit(drive))
      continue;
    auto &target=blkdev.drvs[letter-'A'];
    {
      CMemLock lock(disk_lock_flags);
      if (!DrvDeleteReady(&target)) {
        DrvFATCacheDrop(drive);
        continue;
      }
      DrvDelete(target);
      blkdev.let_to_drv[letter-'A']=&target;
      drive.dv_signature=DRV_SIGNATURE_VAL;
      drive.bd=bd;
      MemCpy(&target,&drive,sizeof(drive));
    }
    res++;
    letter++;
  }
  if (res) {
    device.bd_signature=BD_SIGNATURE_VAL;
    device.flags|=BDF_INITIALIZED | BDF_READ_ONLY;
    BlkDevShareController(device);
    MemCpy(bd,&device,sizeof(device));
    ATAIdentifyRecord(*bd);
  }
  return res;
}

extern "C" int64_t BlkDevAddCpp(CBlkDev *bd, int64_t prt_num, uint8_t whole_drv,
                                uint8_t make_free)
{
  CMemLock lock(mount_flags);
  unsigned slot = 0;
  while (slot < BLKDEVS_NUM && bd != blkdev.blkdevs + slot)
    slot++;
  if (slot!=BLKDEVS_NUM && bd->bd_signature!=BD_SIGNATURE_VAL && bd->type==BDT_ATA)
    return BlkDevAddATA(bd,prt_num,whole_drv);
  if (slot == BLKDEVS_NUM || bd->bd_signature == BD_SIGNATURE_VAL ||
      bd->type != BDT_RAM || bd->first_drv_let < 'A' ||
      bd->first_drv_let > 'B' || bd->drv_offset < 0 ||
      bd->max_blk < bd->drv_offset ||
      uint64_t(bd->max_blk) >= SIZE_MAX / BLK_SIZE)
    return 0;
  char letter = bd->first_drv_let;
  if (!make_free) {
    while (letter <= 'B' && (MountedDrive(letter) ||
           blkdev.drvs[letter-'A'].dv_signature==DRV_SIGNATURE_VAL))
      letter++;
    if (letter > 'B')
      return 0;
  }
  auto &target = blkdev.drvs[letter - 'A'];
  CBlkDev *old=nullptr;
  {
    CMemLock lock(disk_lock_flags);
    if (!DrvDeleteReady(&target))
      return 0;
    if (target.dv_signature==DRV_SIGNATURE_VAL) {
      old=target.bd;
      if (old->type!=BDT_RAM || !BlkDevDeleteReady(old))
        return 0;
      for (size_t i=0;i<DRVS_NUM;i++)
        if (&target!=&blkdev.drvs[i] && blkdev.drvs[i].bd==old)
          return 0;
    }
    if (bd->RAM_dsk)
      for (size_t i=0;i<BLKDEVS_NUM;i++) {
        auto *other=&blkdev.blkdevs[i];
        if (other!=old && other!=bd &&
            other->bd_signature==BD_SIGNATURE_VAL &&
            other->RAM_dsk==bd->RAM_dsk)
          return 0;
      }
  }

  CBlkDev device;
  MemCpy(&device, bd, sizeof(device));
  bool owned = !device.RAM_dsk;
  if (owned) {
    device.RAM_dsk =
        MAlloc((size_t(device.max_blk) + 1) * BLK_SIZE, adam_task->data_heap);
    if (!device.RAM_dsk)
      return 0;
    device.max_blk = MSize(device.RAM_dsk) / BLK_SIZE - 1;
  }
  device.bd_signature = BD_SIGNATURE_VAL;
  device.flags |= BDF_INITIALIZED;
  device.first_drv_let = letter;

  CDrv drive;
  MemSet(&drive, 0, sizeof(drive));
  drive.dv_signature = DRV_SIGNATURE_VAL;
  drive.drv_let = letter;
  drive.bd = &device;
  drive.drv_offset = device.drv_offset;
  drive.size = device.max_blk + 1 - device.drv_offset;
  size_t root_blocks =
      device.init_root_dir_blks > 0 ? size_t(device.init_root_dir_blks) : 1;
  if (!(owned ? RedSeaFormatRAM(drive, root_blocks)
              : RedSeaInit(drive) || RedSeaFormatRAM(drive, root_blocks)) ||
      !DrvFATCacheInit(drive)) {
    DrvFATCacheDrop(drive);
    if (owned)
      Free(device.RAM_dsk);
    return 0;
  }

  {
    CMemLock lock(disk_lock_flags);
    if (!DrvDeleteReady(&target) || (old && !BlkDevDeleteReady(old))) {
      DrvFATCacheDrop(drive);
      if (owned) Free(device.RAM_dsk);
      return 0;
    }
    if (old) {
      owned |= BlkDevOwnedRAM(old)==device.RAM_dsk;
      BlkDevDelete(*old,device.RAM_dsk);
    } else
      DrvDelete(target);
    if (adam_task && adam_task->hash_table)
      ResidentDriveClear(letter);
    blkdev.let_to_drv[letter-'A']=&target;
    MemCpy(bd, &device, sizeof(device));
    drive.bd = bd;
    MemCpy(&target, &drive, sizeof(target));
    ram_owned[slot] = owned?device.RAM_dsk:nullptr;
  }
  return 1;
}

extern "C" uint64_t DrvEnableCpp(uint8_t drv_let,uint8_t val)
{
  auto *dv=Let2DrvCpp(drv_let,false);
  if (!dv) return false;
  bool old,okay;
  {
    CMemLock mount(mount_flags);
    CMemLock lock(disk_lock_flags);
    old=!(dv->fs_type&(1u<<FStf_DISABLE));
    auto *bd=BlkDevLockTarget(dv->bd);
    okay=dv->dv_signature==DRV_SIGNATURE_VAL && bd &&
         (!(dv->locked_flags&1) || dv->owning_task==CurrentTask()) &&
         (!(bd->locked_flags&1) || bd->owning_task==CurrentTask()) &&
         (val || !(dv->fat_blk_dirty&1));
    if (okay) {
      if (val)
        dv->fs_type&=FSG_TYPE_MASK;
      else {
        dv->fs_type|=1u<<FStf_DISABLE;
        if (adam_task && adam_task->hash_table)
          for (size_t i=0;i<DRVS_NUM;i++)
            if (blkdev.let_to_drv[i]==dv) ResidentDriveClear('A'+i);
      }
    }
  }
  if (!okay) RaiseHolyC(0x767244); // Drv
  return old;
}

CDrv *BlkDevsInitAll()
{
  if ((SYS_BOOT_SRC & 0xFFFF) != BOOT_SRC_DVD)
    return nullptr;
  auto *dvd = BlkDevNextFreeSlot(blkdev.boot_drv_let, BDT_ATAPI);
  if (!dvd || !BootDVDProbeAll(*dvd) || !ATAPIReadCapacity(*dvd))
    return nullptr;

  auto *drive = blkdev.let_to_drv[blkdev.boot_drv_let - 'A'];
  drive->bd = dvd;
  if (!ISOInit(*drive) || !DrvFATCacheInit(*drive)) {
    DrvFATCacheDrop(*drive);
    MemSet(drive, 0, sizeof(*drive));
    return nullptr;
  }
  dvd->bd_signature = BD_SIGNATURE_VAL;
  dvd->flags |= BDF_INITIALIZED;
  dvd->max_reads = 128;
  dvd->max_writes = 0xFFFF * 4;
  drive->drv_let = dvd->first_drv_let;
  drive->dv_signature = DRV_SIGNATURE_VAL;
  blkdev.dvd_boot_is_good = true;
  // Discover the primary legacy IDE disk without touching its contents.
  if (dvd->base0!=0x1F0 || dvd->unit!=0) {
    if (auto *disk=BlkDevNextFreeSlot('C',BDT_ATA)) {
      disk->base0=0x1F0;
      disk->base1=0x3F4;
      disk->unit=0;
      BlkDevAddCpp(disk,INT64_MIN,false,false);
    }
  }
  return drive;
}
