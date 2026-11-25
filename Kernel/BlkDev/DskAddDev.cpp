#include "../KernelB.hpp"

static bool ram_owned[BLKDEVS_NUM];
static uint64_t mount_flags;

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
  return true;
}

CDrv *MountedDrive(char letter)
{
  auto *dv = 'A' <= letter && letter <= 'Z' ? blkdev.let_to_drv[letter - 'A']
                                            : nullptr;
  return dv && dv->dv_signature == DRV_SIGNATURE_VAL ? dv : nullptr;
}

static uint8_t Let2Let(uint8_t letter)
{
  if (!letter) {
    auto *drive = CurrentTask()->cur_dv;
    letter = drive ? drive->drv_let : 0;
  } else if (letter == ':') {
    letter = blkdev.boot_drv_let;
  } else if (letter == '~') {
    letter = blkdev.home_dir ? *blkdev.home_dir : 0;
  }
  if (letter >= 'a' && letter <= 'z')
    letter -= 'a' - 'A';
  return letter;
}

static uint32_t Let2BlkDevType(uint8_t letter)
{
  if (letter < 'A' || letter > 'Z')
    return BDT_NULL;
  if (letter <= 'B')
    return BDT_RAM;
  if (letter <= 'L')
    return BDT_ATA;
  if (letter <= 'P')
    return BDT_ISO_FILE_READ;
  if (letter <= 'S')
    return BDT_ISO_FILE_WRITE;
  return BDT_ATAPI;
}

static CBlkDev *BlkDevNextFreeSlot(uint8_t letter, uint32_t type)
{
  if (!blkdev.blkdevs)
    return nullptr;
  CMemLock lock(mount_flags);
  for (size_t i = 0; i < BLKDEVS_NUM; i++) {
    auto &bd = blkdev.blkdevs[i];
    if (bd.bd_signature != BD_SIGNATURE_VAL) {
      MemSet(&bd, 0, sizeof(bd));
      bd.first_drv_let = letter;
      bd.type = type;
      bd.blk_size = BLK_SIZE;
      bd.max_blk = 0xEFFFFFFF;
      if (type != BDT_RAM)
        bd.flags = BDF_READ_CACHE;
      if (type == BDT_ISO_FILE_READ)
        bd.flags |= BDF_READ_ONLY;
      if (type == BDT_ATAPI) {
        bd.flags |= BDF_REMOVABLE | BDF_READ_ONLY;
        bd.blk_size = DVD_BLK_SIZE;
      }
      return &bd;
    }
  }
  return nullptr;
}

extern "C" CBlkDev *BlkDevNextFreeSlotCpp(uint8_t letter, int64_t type)
{
  auto resolved = Let2Let(letter);
  if (type != BDT_NULL && Let2BlkDevType(resolved) == type)
    if (auto *bd = BlkDevNextFreeSlot(resolved, type))
      return bd;
  RaiseHolyC(0x7665446B6C42); // BlkDev
}

extern "C" int64_t BlkDevAddCpp(CBlkDev *bd, int64_t, uint8_t,
                                uint8_t make_free)
{
  CMemLock lock(mount_flags);
  unsigned slot = 0;
  while (slot < BLKDEVS_NUM && bd != blkdev.blkdevs + slot)
    slot++;
  if (slot == BLKDEVS_NUM || bd->bd_signature == BD_SIGNATURE_VAL ||
      bd->type != BDT_RAM || bd->first_drv_let < 'A' ||
      bd->first_drv_let > 'B' || bd->drv_offset < 0 ||
      bd->max_blk < bd->drv_offset ||
      uint64_t(bd->max_blk) >= SIZE_MAX / BLK_SIZE)
    return 0;
  char letter = bd->first_drv_let;
  if (!make_free) {
    while (letter <= 'B' && MountedDrive(letter))
      letter++;
    if (letter > 'B')
      return 0;
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
              : RedSeaInit(drive) || RedSeaFormatRAM(drive, root_blocks))) {
    if (owned)
      Free(device.RAM_dsk);
    return 0;
  }

  auto &target = *blkdev.let_to_drv[letter - 'A'];
  if (target.dv_signature == DRV_SIGNATURE_VAL) {
    ResidentDriveClear(letter);
    auto *old = target.bd;
    for (size_t i = 0; i < BLKDEVS_NUM; i++) {
      if (old != &blkdev.blkdevs[i] || old->type != BDT_RAM)
        continue;
      if (old->RAM_dsk == device.RAM_dsk)
        owned |= ram_owned[i];
      else if (ram_owned[i])
        Free(old->RAM_dsk);
      MemSet(old, 0, sizeof(*old));
      ram_owned[i] = false;
      break;
    }
  }
  MemCpy(bd, &device, sizeof(device));
  drive.bd = bd;
  MemCpy(&target, &drive, sizeof(target));
  ram_owned[slot] = owned;
  return 1;
}

extern "C" CDrv *Let2DrvCpp(uint8_t letter, uint8_t except)
{
  auto *drive = letter ? MountedDrive(Let2Let(letter)) : CurrentTask()->cur_dv;
  if (drive && drive->dv_signature == DRV_SIGNATURE_VAL)
    return drive;
  if (except)
    RaiseHolyC(0x767244); // Drv
  return nullptr;
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
  if (!ISOInit(*drive)) {
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
  return drive;
}
