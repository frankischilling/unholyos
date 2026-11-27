#include "../KernelB.hpp"
#include "../../Adam/Opt/Boot/DskISORedSea.hpp"

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
  char home_let=KCfgHomeDrive(kcfg_home_dir,kcfg_boot_drv_let);
  if (!home_let)
    return false;
  auto *hc = adam_task->data_heap;
  auto *devices =
      reinterpret_cast<CBlkDev *>(CAlloc(sizeof(CBlkDev) * BLKDEVS_NUM, hc));
  auto *drives = reinterpret_cast<CDrv *>(CAlloc(sizeof(CDrv) * DRVS_NUM, hc));
  auto *iso = StrNew(DFT_ISO_FILENAME, hc);
  auto *iso_c = StrNew(DFT_ISO_C_FILENAME, hc);
  auto *tmp = StrNew(DFT_TMP_FILENAME, hc);
  auto *home_dir = StrNew(kcfg_home_dir, hc);
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
  home_dir[0]=home_let;
  blkdev.home_dir = home_dir;
  blkdev.boot_drv_let = kcfg_boot_drv_let;
  blkdev.first_hd_drv_let = kcfg_first_hd_drv_let;
  blkdev.first_dvd_drv_let = BOOT_DVD_LET;
  for (size_t i = 0; i < DRVS_NUM; i++)
    blkdev.let_to_drv[i] = drives + i;
  auto *bp=SYS_DATA_BP?SYS_DATA_BP:SYS_CODE_BP;
  DskCacheInitCpp(KCfgScale2Mem(bp->alloced_u8s,kcfg_cache_min,kcfg_cache_max,
                              kcfg_cache_limit));
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

static bool BlkDevImageSlot(const CBlkDev *bd)
{
  if (!bd || !blkdev.blkdevs) return false;
  for (size_t i=0;i<BLKDEVS_NUM;i++)
    if (bd==&blkdev.blkdevs[i])
      return bd->bd_signature!=BD_SIGNATURE_VAL && !bd->file_dsk &&
             (bd->type==BDT_ISO_FILE_READ || bd->type==BDT_ISO_FILE_WRITE);
  return false;
}

static int64_t BlkDevAddImage(CBlkDev *bd,bool make_free)
{
  if (!BlkDevImageSlot(bd)) return 0;
  bool write=bd->type==BDT_ISO_FILE_WRITE;
  char first=write?'Q':'M',last=write?'S':'P';
  if ((!write && !bd->file_dsk_name) || bd->first_drv_let<first ||
      bd->first_drv_let>last || bd->drv_offset<0 ||
      (write && (bd->drv_offset!=ISO_DRV_OFFSET ||
                 bd->max_blk<bd->drv_offset+2 || bd->max_blk>=int64_t(INT64_MAX/BLK_SIZE))))
    return 0;
  if (write) {
    uint64_t blocks=uint64_t(bd->max_blk)+1-bd->drv_offset;
    uint64_t map=(blocks+8*BLK_SIZE-1)/(8*BLK_SIZE);
    uint64_t root=bd->init_root_dir_blks>0?bd->init_root_dir_blks:1;
    if (map+1>=blocks || root>blocks-map-1 ||
        uint64_t(bd->max_blk)+1>uint64_t(UINT32_MAX)*DVD_BLK_SIZE/BLK_SIZE)
      return 0;
  }
  auto *requested_name=bd->file_dsk_name;
  auto requested_letter=bd->first_drv_let;
  auto requested_offset=bd->drv_offset;
  auto requested_max=bd->max_blk;
  auto requested_root=bd->init_root_dir_blks;
  auto requested_type=bd->type;
  char default_name[]="T:/DrvQ.ISO.C";
  default_name[0]=blkdev.boot_drv_let;
  default_name[6]=requested_letter;
  auto *absolute=FileNameAbs(requested_name?reinterpret_cast<const char *>(requested_name):default_name,
                             *CurrentTask());
  if (!absolute) return 0;
  auto *backing=absolute[0] && absolute[1]==':' && absolute[2]=='/'
                ? MountedDrive(absolute[0]) : nullptr;
  int64_t res=0;
  bool rollback_failed=false;
  if (backing && ReadableFS(*backing) && BlkDevLockTarget(backing->bd) &&
      BlkDevImageChain(*backing->bd,write)) {
    auto *source_bd=backing->bd;
    auto *source_lock=BlkDevLockTarget(source_bd);
    // Acquire the potentially contended backing lock before mount_flags.
    // The staged device forwards its lock to this same backing device.
    CDiskLock backing_lock(*backing);
    CMemLock mount(mount_flags);
    if (BlkDevImageSlot(bd) && bd->file_dsk_name==requested_name &&
        bd->first_drv_let==requested_letter && bd->drv_offset==requested_offset &&
        bd->max_blk==requested_max && bd->init_root_dir_blks==requested_root &&
        bd->type==requested_type &&
        MountedDrive(absolute[0])==backing && backing->bd==source_bd &&
        BlkDevLockTarget(source_bd)==source_lock && ReadableFS(*backing) &&
        BlkDevImageChain(*source_bd,write) &&
        (!(source_bd->flags&BDF_REMOVABLE) || source_bd->type==BDT_ATAPI) &&
        BlkDevEnsureInitialized(*source_bd)) {
      bool clean=!write || WritableFS(*backing);
      for (size_t i=0;i<DRVS_NUM;i++)
        if (blkdev.drvs[i].bd &&
            BlkDevSameBacking(*blkdev.drvs[i].bd,*source_bd) &&
            (blkdev.drvs[i].fat_blk_dirty&1))
          clean=false;
      for (size_t i=0;i<BLKDEVS_NUM;i++) {
        const auto &image=blkdev.blkdevs[i];
        if ((write || image.type==BDT_ISO_FILE_WRITE) &&
            BlkDevImageDependsOn(image,*source_bd))
          clean=false;
      }
      char letter=bd->first_drv_let;
      if (!make_free)
        while (letter<=last && (MountedDrive(letter) ||
               blkdev.drvs[letter-'A'].dv_signature==DRV_SIGNATURE_VAL))
          letter++;
      if (clean && letter<=last && !MountedDrive(letter) &&
          blkdev.drvs[letter-'A'].dv_signature!=DRV_SIGNATURE_VAL &&
          (source_bd->type!=BDT_ATAPI || BlkDevMediaPin(*source_bd))) {
        auto &target=blkdev.drvs[letter-'A'];
        CImageMountUndo undo;
        auto *name=StrNew(absolute,adam_task->data_heap);
        auto *f=name?(write?FOpenImageWrite(absolute,backing,requested_max+1,undo):
                            FOpenImageRead(absolute,backing)):nullptr;
        if (f && name && bd->drv_offset<=f->max_blk) {
          CBlkDev device{};
          MemCpy(&device,bd,sizeof(device));
          device.bd_signature=BD_SIGNATURE_VAL;
          device.flags|=BDF_INITIALIZED;
          device.flags&=~(BDF_INIT_IN_PROGRESS|BDF_READ_ONLY_OVERRIDE|BDF_READ_ONLY|
                          BDF_REMOVABLE);
          if (!write) device.flags|=BDF_READ_ONLY;
          device.first_drv_let=letter;
          device.blk_size=BLK_SIZE;
          device.max_blk=f->max_blk;
          device.file_dsk=f;
          device.file_dsk_name=reinterpret_cast<uint8_t *>(name);
          device.lock_fwding=backing->bd;
          CDrv drive{};
          drive.bd=&device;
          drive.drv_offset=device.drv_offset;
          drive.size=device.max_blk+1-drive.drv_offset;
          drive.drv_let=letter;
          size_t root_blocks=bd->init_root_dir_blks>0?bd->init_root_dir_blks:1;
          if (BlkDevImageChain(device,write) &&
              (write?(RedSeaFormatRAM(drive,root_blocks) && RedSeaISO9660Write(drive)):
                     RedSeaInit(drive)) && DrvFATCacheInit(drive)) {
            CMemLock lock(disk_lock_flags);
            if (DrvDeleteReady(&target)) {
              DrvDelete(target);
              if (adam_task && adam_task->hash_table)
                ResidentDriveClear(letter);
              blkdev.let_to_drv[letter-'A']=&target;
              auto *old_name=bd->file_dsk_name;
              MemCpy(bd,&device,sizeof(device));
              drive.dv_signature=DRV_SIGNATURE_VAL;
              drive.bd=bd;
              MemCpy(&target,&drive,sizeof(drive));
              // BlkDevAdd consumes the supplied heap-owned name on success.
              Free(old_name);
              res=1;
            }
          }
          if (!res) DrvFATCacheDrop(drive);
        }
        if (!res) {
          if (write && f && !FDiscardImageWrite(f,undo))
            undo.rollback_failed=true;
          Free(name);
          FCloseImage(f);
          if (source_bd->type==BDT_ATAPI)
            BlkDevMediaUnpin(*source_bd);
        }
        rollback_failed=undo.rollback_failed;
        Free(undo.data);
      }
    }
  }
  Free(absolute);
  if (rollback_failed)
    PutS("Image mount rollback failed\n");
  return res;
}

extern "C" int64_t BlkDevAddCpp(CBlkDev *bd, int64_t prt_num, uint8_t whole_drv,
                                uint8_t make_free)
{
  if (BlkDevImageSlot(bd))
    return BlkDevAddImage(bd,make_free);
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
         (val || (!(dv->fat_blk_dirty&1) && !DrvHasImageDependents(*dv)));
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
  if (kcfg_mount_primary_hd && (dvd->base0!=0x1F0 || dvd->unit!=0)) {
    if (auto *disk=BlkDevNextFreeSlot(blkdev.first_hd_drv_let,BDT_ATA)) {
      disk->base0=0x1F0;
      disk->base1=0x3F4;
      disk->unit=0;
      BlkDevAddCpp(disk,INT64_MIN,false,false);
    }
  }
  return drive;
}
