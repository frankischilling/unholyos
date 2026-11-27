#include "../KernelB.hpp"

extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);
extern "C" void PrintErrCpp(const char *fmt, int64_t argc, const uint64_t *argv,
                             const uintptr_t *frame);

bool RedSeaInit(CDrv &dv)
{
  CRedSeaBoot boot;
  if (!dv.bd || dv.drv_offset < 0 || dv.bd->max_blk < 0 ||
      uint64_t(dv.bd->max_blk) >= SIZE_MAX / BLK_SIZE)
    return false;
  uint64_t first = dv.drv_offset, end = uint64_t(dv.bd->max_blk) + 1;
  if (first >= end ||
      !BlkDevReadBytes(*dv.bd, reinterpret_cast<uint8_t *>(&boot),
                       first * BLK_SIZE, sizeof(boot)) ||
      boot.signature != MBR_PT_REDSEA || boot.signature2 != 0xAA55 ||
      boot.sects > end - first || !boot.bitmap_sects ||
      boot.bitmap_sects >= boot.sects ||
      (boot.sects - 1) / (8 * BLK_SIZE) >= boot.bitmap_sects ||
      boot.root_clus < first + boot.bitmap_sects ||
      boot.root_clus >= first + boot.sects)
    return false;
  dv.fs_type = FSt_REDSEA;
  dv.spc = 1;
  dv.size = boot.sects;
  dv.data_area = first + boot.bitmap_sects;
  dv.root_clus = boot.root_clus;
  dv.fat1 = dv.fat2 = first + 1;
  return true;
}

static bool RedSeaWriteDevice(const CDrv &dv)
{
  return dv.bd && !(dv.bd->flags & BDF_READ_ONLY) &&
         ((dv.bd->type == BDT_RAM && dv.bd->RAM_dsk) ||
          dv.bd->type == BDT_ISO_FILE_WRITE) &&
         !BlkDevImageBacking(dv.bd) &&
         dv.bd->max_blk >= 0 &&
         uint64_t(dv.bd->max_blk) < SIZE_MAX / BLK_SIZE &&
         dv.drv_offset >= 0 && dv.size > 0 &&
         uint64_t(dv.drv_offset) <= uint64_t(dv.bd->max_blk) &&
         uint64_t(dv.size) <= uint64_t(dv.bd->max_blk) + 1 - dv.drv_offset;
}

bool RedSeaFormatRAM(CDrv &dv, size_t root_blocks)
{
  constexpr size_t clusters_per_bitmap_block = 8 * BLK_SIZE;
  if (!RedSeaWriteDevice(dv) || !root_blocks || (dv.fat_blk_dirty & 1))
    return false;
  CDiskLock lock(dv);
  if (!RedSeaWriteDevice(dv) || (dv.fat_blk_dirty & 1))
    return false;
  uint64_t blocks = dv.size;
  uint64_t map_blocks =
      (blocks + clusters_per_bitmap_block - 1) / clusters_per_bitmap_block;
  if (!map_blocks || root_blocks >= blocks ||
      map_blocks + 1 > blocks - root_blocks)
    return false;
  if (!DrvFATWriteAllowed(dv.bd, uint64_t(dv.drv_offset) * BLK_SIZE,
                          blocks * BLK_SIZE))
    return false;
  DrvFATCacheReset(dv);
  DskCacheInvalidateDevice(dv.bd);
  uint64_t root = dv.drv_offset + map_blocks + 1;
  CRedSeaBoot boot{};
  boot.signature = MBR_PT_REDSEA;
  boot.signature2 = 0xAA55;
  boot.drv_offset = dv.drv_offset;
  boot.sects = blocks;
  boot.bitmap_sects = map_blocks;
  boot.root_clus = root;
  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  uint64_t date = Now();
  boot.unique_id = ((uint64_t(hi) << 32) | lo) ^ date;
  uint64_t used = root_blocks + 1;
  uint8_t map[BLK_SIZE];
  for (uint64_t i = 0; i < map_blocks; i++) {
    MemSet(map, 0, sizeof(map));
    uint64_t n = used < clusters_per_bitmap_block ? used : clusters_per_bitmap_block;
    for (uint64_t bit = 0; bit < n; bit++)
      map[bit / 8] |= uint8_t{1} << (bit % 8);
    if (!BlkDevWriteBytes(*dv.bd, map, (dv.drv_offset + 1 + i) * BLK_SIZE,
                          sizeof(map)))
      return false;
    used -= n;
  }
  CRedSeaDirEntry dir[BLK_SIZE / sizeof(CRedSeaDirEntry)]{};
  dir[0].attr = dir[1].attr = RS_ATTR_DIR | RS_ATTR_CONTIGUOUS;
  dir[0].name[0] = dir[1].name[0] = dir[1].name[1] = '.';
  dir[0].clus = dir[1].clus = root;
  dir[0].size = root_blocks * BLK_SIZE;
  dir[0].datetime = dir[1].datetime = date;
  return BlkWriteZero(dv, root + 1, root_blocks - 1) &&
         BlkDevWriteBytes(*dv.bd, reinterpret_cast<const uint8_t *>(dir),
                          root * BLK_SIZE, sizeof(dir)) &&
         BlkDevWriteBytes(*dv.bd, reinterpret_cast<const uint8_t *>(&boot),
                          uint64_t(dv.drv_offset) * BLK_SIZE, sizeof(boot)) &&
         RedSeaInit(dv);
}

static bool RedSeaExtent(const CDrv &dv, uint64_t clus, uint64_t bytes)
{
  uint64_t end = uint64_t(dv.drv_offset) + dv.size;
  return clus >= uint64_t(dv.data_area) && clus < end &&
         bytes <= (end - clus) * BLK_SIZE;
}

static bool RedSeaNameEq(const char (&disk_name)[CDIR_FILENAME_LEN],
                         const char *name)
{
  for (size_t i = 0; i < sizeof(disk_name); i++) {
    if (disk_name[i] != name[i])
      return false;
    if (!disk_name[i])
      return true;
  }
  return false;
}

bool RedSeaFileFind(const CDrv &dv, uint64_t dir, const char *name,
                    bool directory, CRedSeaDirEntry &entry)
{
  MemSet(&entry, 0, sizeof(entry));
  CRedSeaDirEntry block[BLK_SIZE / sizeof(CRedSeaDirEntry)];
  auto *buf = reinterpret_cast<uint8_t *>(block);
  if (dv.fs_type != FSt_REDSEA || !FileNameChk(name) ||
      !RedSeaExtent(dv, dir, BLK_SIZE) ||
      !DrvReadBytes(dv, buf, dir * BLK_SIZE, BLK_SIZE))
    return false;
  uint64_t size = block[0].size;
  if (size < BLK_SIZE || size % BLK_SIZE || !RedSeaExtent(dv, dir, size))
    return false;
  block[0].name[0] = '.';
  block[0].name[1] = 0;
  for (uint64_t pos = 0; pos < size; pos += BLK_SIZE) {
    if (pos && !DrvReadBytes(dv, buf, dir * BLK_SIZE + pos, BLK_SIZE))
      return false;
    for (const auto &candidate : block) {
      if (!candidate.name[0])
        return false;
      if (!(candidate.attr & RS_ATTR_DELETED) &&
          directory == bool(candidate.attr & RS_ATTR_DIR) &&
          RedSeaNameEq(candidate.name, name)) {
        entry = candidate;
        return true;
      }
    }
  }
  return false;
}

bool RedSeaFileRead(const CDrv &dv, const CRedSeaDirEntry &entry, uint64_t pos,
                    uint8_t *buf, size_t bytes)
{
  if (dv.fs_type != FSt_REDSEA ||
      (entry.attr & (RS_ATTR_DIR | RS_ATTR_DELETED)) || pos > entry.size ||
      bytes > entry.size - pos)
    return false;
  return !bytes ||
         (RedSeaExtent(dv, entry.clus, entry.size) &&
          DrvReadBytes(dv, buf, entry.clus * BLK_SIZE + pos, bytes));
}

static uint64_t RedSeaBlocks(uint64_t bytes)
{
  return bytes / BLK_SIZE + (bytes % BLK_SIZE != 0);
}

struct CRedSeaBitmap
{
  uint64_t index = UINT64_MAX;
  uint8_t sector[BLK_SIZE];
};

static bool RedSeaClusterUsed(const CDrv &dv, CRedSeaBitmap &map,
                               uint64_t cluster, bool &used)
{
  uint64_t bit = cluster - dv.data_area;
  uint64_t index = bit / (BLK_SIZE * 8);
  if (map.index != index) {
    if (!DrvFATRead(dv, index, map.sector, 0, sizeof(map.sector)))
      return false;
    map.index = index;
  }
  used = map.sector[(bit / 8) % BLK_SIZE] & (uint8_t{1} << (bit % 8));
  return true;
}

static bool RedSeaGeometry(const CDrv &dv)
{
  if (!RedSeaWriteDevice(dv) || dv.fs_type != FSt_REDSEA || dv.spc != 1 ||
      (dv.dv_signature == DRV_SIGNATURE_VAL &&
       dv.bd->bd_signature != BD_SIGNATURE_VAL) ||
      dv.fat1 != dv.drv_offset + 1 || dv.fat2 != dv.fat1 ||
      dv.data_area <= dv.drv_offset || dv.root_clus <= dv.data_area)
    return false;
  uint64_t end = uint64_t(dv.drv_offset) + dv.size, data = dv.data_area;
  CRedSeaBitmap map;
  bool used;
  return data < end && uint64_t(dv.root_clus) < end &&
         (end - data - 1) / (BLK_SIZE * 8) < data - dv.drv_offset &&
         RedSeaClusterUsed(dv, map, data, used) && used;
}

static bool RedSeaNativeWriteReady(CDrv &dv)
{
  if (!RedSeaWriteDevice(dv) || (dv.fat_blk_dirty & 1))
    return false;
  // Staged RAM and image drives are not yet registered.
  if (dv.dv_signature == DRV_SIGNATURE_VAL &&
      !BlkDevEnsureInitialized(*dv.bd))
    return false;
  return RedSeaGeometry(dv) &&
         DrvFATWriteAllowed(dv.bd, uint64_t(dv.drv_offset) * BLK_SIZE,
                            uint64_t(dv.size) * BLK_SIZE);
}

static bool RedSeaClusterRange(const CDrv &dv, uint64_t c, uint64_t cnt)
{
  uint64_t end = uint64_t(dv.drv_offset) + dv.size;
  return c > uint64_t(dv.data_area) && c < end && cnt <= end - c;
}

static bool RedSeaMark(const CDrv &dv, uint64_t cluster, uint64_t blocks,
                        bool used)
{
  if (!blocks)
    return true;
  if (!RedSeaClusterRange(dv, cluster, blocks))
    return false;
  constexpr uint64_t bits = BLK_SIZE * 8;
  uint64_t first = cluster - dv.data_area, last = first + blocks - 1;
  uint64_t index = first / bits, count = last / bits - index + 1;
  uint8_t saved[BLK_SIZE], sector[BLK_SIZE];
  auto *old = count == 1 ? saved :
      MAlloc(count * BLK_SIZE, CurrentTask()->data_heap);
  if (!old)
    return false;
  bool okay = true;
  for (uint64_t i = 0; i < count; i++)
    if (!DrvFATRead(dv, index + i, old + i * BLK_SIZE, 0, BLK_SIZE)) {
      okay = false;
      break;
    }
  if (okay)
    for (uint64_t i = 0; i < count; i++) {
      MemCpy(sector, old + i * BLK_SIZE, sizeof(sector));
      uint64_t base = (index + i) * bits;
      uint64_t begin = first > base ? first - base : 0;
      uint64_t end = last - base < bits ? last - base + 1 : bits;
      for (uint64_t bit = begin; bit < end; bit++) {
        auto mask = uint8_t{1} << (bit % 8);
        if (used)
          sector[bit / 8] |= mask;
        else
          sector[bit / 8] &= ~mask;
      }
      if (!BlkDevWriteBytes(*dv.bd, sector,
                            (uint64_t(dv.fat1) + index + i) * BLK_SIZE,
                            sizeof(sector))) {
        // Restore the original bitmap if a later sector write fails.
        for (uint64_t j = 0; j <= i; j++)
          BlkDevWriteBytes(*dv.bd, old + j * BLK_SIZE,
                           (uint64_t(dv.fat1) + index + j) * BLK_SIZE,
                           BLK_SIZE);
        okay = false;
        break;
      }
    }
  if (old != saved)
    Free(old);
  return okay;
}

uint64_t RedSeaAllocClus(const CDrv &drive, uint64_t blocks)
{
  auto &dv = const_cast<CDrv &>(drive);
  CDiskLock lock(dv);
  if (!blocks || !RedSeaNativeWriteReady(dv) || blocks > uint64_t(dv.size))
    return INVALID_CLUS;
  uint64_t end = uint64_t(dv.drv_offset) + dv.size;
  uint64_t best = INVALID_CLUS, best_size = UINT64_MAX;
  CRedSeaBitmap map;
  for (uint64_t clus = dv.data_area + 1; clus < end;) {
    uint64_t start = clus;
    while (clus < end) {
      bool used;
      if (!RedSeaClusterUsed(dv, map, clus, used))
        return INVALID_CLUS;
      if (used)
        break;
      clus++;
    }
    uint64_t size = clus - start;
    if (size >= blocks && size < best_size) {
      best = start;
      best_size = size;
      if (size == blocks)
        break;
    }
    clus++;
  }
  if (best != INVALID_CLUS && !RedSeaMark(dv, best, blocks, true))
    return INVALID_CLUS;
  return best;
}

struct CRedSeaDir
{
  uint64_t cluster, parent_pos;
};

static bool RedSeaDirHead(const CDrv &dv, CRedSeaDir dir, CRedSeaDirEntry &head)
{
  return RedSeaExtent(dv, dir.cluster, BLK_SIZE) &&
         DrvReadBytes(dv, reinterpret_cast<uint8_t *>(&head),
                      dir.cluster * BLK_SIZE, sizeof(head)) &&
         head.size >= BLK_SIZE && !(head.size % BLK_SIZE) &&
         RedSeaExtent(dv, dir.cluster, head.size);
}

static bool RedSeaAllocated(const CDrv &dv, uint64_t c, uint64_t cnt)
{
  if (!RedSeaClusterRange(dv, c, cnt))
    return false;
  CRedSeaBitmap map;
  for (uint64_t i = 0; i < cnt; i++) {
    bool used;
    if (!RedSeaClusterUsed(dv, map, c + i, used) || !used)
      return false;
  }
  return true;
}

static bool RedSeaOverlap(uint64_t a, uint64_t an, uint64_t b, uint64_t bn)
{
  return an && bn && (a <= b ? b - a < an : a - b < bn);
}

static bool RedSeaAllocatedDir(const CDrv &dv, CRedSeaDir dir,
                                CRedSeaDirEntry &head)
{
  return RedSeaDirHead(dv, dir, head) && (head.attr & RS_ATTR_DIR) &&
         head.clus == dir.cluster &&
         RedSeaAllocated(dv, dir.cluster, RedSeaBlocks(head.size));
}

static CRedSeaDirEntry *RedSeaReadDir(CDrv &dv, uint64_t clus);

struct CRedSeaDirBuf
{
  CRedSeaDirEntry *head;

  CRedSeaDirBuf(CDrv &dv, CRedSeaDir dir) : head(RedSeaReadDir(dv, dir.cluster))
  {
    if (head && (!(head->attr & RS_ATTR_DIR) || head->clus != dir.cluster ||
                 !RedSeaAllocated(dv, dir.cluster, RedSeaBlocks(head->size)))) {
      Free(head);
      head = nullptr;
    }
  }

  ~CRedSeaDirBuf()
  {
    Free(head);
  }

  CRedSeaDirBuf(const CRedSeaDirBuf &) = delete;
  CRedSeaDirBuf &operator=(const CRedSeaDirBuf &) = delete;
};

static bool RedSeaEntryWrite(const CDrv &dv, uint64_t pos,
                              const CRedSeaDirEntry &entry)
{
  return !(pos % sizeof(entry)) &&
         RedSeaExtent(dv, pos / BLK_SIZE, BLK_SIZE) &&
         BlkDevWriteBytes(*dv.bd, reinterpret_cast<const uint8_t *>(&entry),
                          pos, sizeof(entry));
}

static uint64_t RedSeaEntryPos(CRedSeaDir dir, uint64_t index)
{
  return dir.cluster * BLK_SIZE + index * sizeof(CRedSeaDirEntry);
}

extern "C" void RedSeaFreeClusCpp(CDrv *dv, int64_t c, int64_t cnt)
{
  DrvChkCpp(dv, true);
  if (!c || cnt <= 0)
    return;
  if (dv->fs_type != FSt_REDSEA) {
    PrintErrCpp("Not RedSea Drv\n", 0, nullptr, nullptr);
    return;
  }
  CDiskLock lock(*dv);
  DskCacheInvalidateDevice(dv->bd);
  if (!RedSeaNativeWriteReady(*dv) || !RedSeaClusterRange(*dv, c, cnt))
    return;
  CRedSeaDirEntry root;
  if (RedSeaAllocatedDir(*dv, {uint64_t(dv->root_clus), 0}, root) &&
      !RedSeaOverlap(c, cnt, dv->root_clus, RedSeaBlocks(root.size)))
    RedSeaMark(*dv, c, cnt, false);
}

extern "C" int64_t RedSeaAllocClusCpp(CDrv *dv, int64_t cnt)
{
  if (cnt <= 0)
    RaiseHolyC(0x767244); // Drv
  DrvChkCpp(dv, true);
  uint64_t c = INVALID_CLUS;
  {
    CDiskLock lock(*dv);
    DskCacheInvalidateDevice(dv->bd);
    CRedSeaDirEntry root;
    if (RedSeaNativeWriteReady(*dv) && cnt <= dv->size &&
        RedSeaAllocatedDir(*dv, {uint64_t(dv->root_clus), 0}, root))
      c = RedSeaAllocClus(*dv, cnt);
  }
  if (c == INVALID_CLUS) RaiseHolyC(0x767244); // Drv
  return int64_t(c);
}

static bool RedSeaDirGrow(CDrv &dv, CRedSeaDir &dir, CRedSeaDirEntry *head)
{
  CRedSeaDirEntry parent, old_head = head[0];
  CRedSeaBoot boot;
  size_t bytes = head->size, blocks = bytes / BLK_SIZE;
  if (dir.parent_pos) {
    if (!DrvReadBytes(dv, reinterpret_cast<uint8_t *>(&parent),
                       dir.parent_pos, sizeof(parent)) ||
        !(parent.attr & RS_ATTR_DIR) || (parent.attr & RS_ATTR_DELETED) ||
        parent.clus != dir.cluster || parent.size != bytes)
      return false;
  } else if (dir.cluster != uint64_t(dv.root_clus) ||
             !DrvReadBytes(dv, reinterpret_cast<uint8_t *>(&boot),
                            uint64_t(dv.drv_offset) * BLK_SIZE, sizeof(boot)) ||
             boot.root_clus != dir.cluster)
    return false;

  // The in-memory directory survives even if its new extent overlaps the old.
  if (!RedSeaMark(dv, dir.cluster, blocks, false))
    return false;
  uint64_t cluster = RedSeaAllocClus(dv, blocks + 1);
  if (cluster == INVALID_CLUS) {
    RedSeaMark(dv, dir.cluster, blocks, true);
    return false;
  }
  head->size = bytes + BLK_SIZE;
  head->clus = cluster;
  bool okay = BlkDevWriteBytes(*dv.bd, reinterpret_cast<const uint8_t *>(head),
                               cluster * BLK_SIZE, bytes) &&
              BlkWriteZero(dv, cluster + blocks, 1);
  if (okay) {
    if (dir.parent_pos) {
      parent.clus = cluster;
      parent.size = head->size;
      okay = RedSeaEntryWrite(dv, dir.parent_pos, parent);
      if (!okay) {
        parent.clus = dir.cluster;
        parent.size = bytes;
        RedSeaEntryWrite(dv, dir.parent_pos, parent);
      }
    } else {
      boot.root_clus = cluster;
      okay = BlkDevWriteBytes(*dv.bd, reinterpret_cast<const uint8_t *>(&boot),
                               uint64_t(dv.drv_offset) * BLK_SIZE, sizeof(boot));
      if (okay)
        dv.root_clus = cluster;
      else {
        boot.root_clus = dir.cluster;
        BlkDevWriteBytes(*dv.bd, reinterpret_cast<const uint8_t *>(&boot),
                         uint64_t(dv.drv_offset) * BLK_SIZE, sizeof(boot));
      }
    }
  }
  if (!okay) {
    head[0] = old_head;
    BlkDevWriteBytes(*dv.bd, reinterpret_cast<const uint8_t *>(head),
                     dir.cluster * BLK_SIZE, bytes);
    RedSeaMark(dv, cluster, blocks + 1, false);
    RedSeaMark(dv, dir.cluster, blocks, true);
    return false;
  }
  dir.cluster = cluster;
  return true;
}

static bool RedSeaWriteData(CDrv &dv, uint64_t cluster, const uint8_t *data,
                             uint64_t size)
{
  if (!size)
    return true;
  if (!data || !RedSeaExtent(dv, cluster, size))
    return false;
  uint64_t bytes = size / BLK_SIZE * BLK_SIZE;
  if (bytes && !BlkDevWriteBytes(*dv.bd, data, cluster * BLK_SIZE, bytes))
    return false;
  if (bytes != size) {
    uint8_t sector[BLK_SIZE]{};
    MemCpy(sector, data + bytes, size - bytes);
    return BlkDevWriteBytes(*dv.bd, sector, cluster * BLK_SIZE + bytes,
                            sizeof(sector));
  }
  return true;
}

static uint64_t RedSeaDirNew(CDrv &dv, CRedSeaDir &dir, CRedSeaDirEntry &entry,
                             const uint8_t *data, bool directory,
                             bool reserve = false)
{
  CRedSeaDirBuf buf(dv, dir);
  auto *head = buf.head;
  if (!head)
    return 0;
  size_t capacity = head->size / sizeof(*head), empty = 0, previous = 0, i = 1;
  for (; i < capacity && head[i].name[0]; i++) {
    if (head[i].attr & RS_ATTR_DELETED) {
      if (!empty)
        empty = i;
    } else if (RedSeaNameEq(head[i].name, entry.name)) {
      if (directory || (head[i].attr & RS_ATTR_DIR))
        return 0;
      previous = i;
      if (!empty)
        empty = i;
    }
  }
  if (!empty)
    empty = i;
  if (i == capacity || (directory && (!entry.size || entry.size % BLK_SIZE)))
    return 0;

  uint64_t old_size = previous ? head[previous].size : 0;
  uint64_t old_cluster = previous ? head[previous].clus : INVALID_CLUS;
  uint64_t old_blocks = RedSeaBlocks(old_size), blocks = RedSeaBlocks(entry.size);
  if (old_blocks &&
      (!RedSeaAllocated(dv, old_cluster, old_blocks) ||
       RedSeaOverlap(old_cluster, old_blocks, dir.cluster,
                      RedSeaBlocks(head->size))))
    return 0;
  if (!RedSeaMark(dv, old_cluster, old_blocks, false))
    return 0;
  uint64_t cluster = blocks ? RedSeaAllocClus(dv, blocks) : INVALID_CLUS;
  if (blocks && cluster == INVALID_CLUS) {
    RedSeaMark(dv, old_cluster, old_blocks, true);
    return 0;
  }
  if (empty == i && i + 1 == capacity && !RedSeaDirGrow(dv, dir, head)) {
    RedSeaMark(dv, cluster, blocks, false);
    RedSeaMark(dv, old_cluster, old_blocks, true);
    return 0;
  }

  // A failed payload write must not leave an old live entry pointing into
  // the newly allocated extent, which can reuse part of its previous chain.
  if (previous) {
    CRedSeaDirEntry deleted = head[previous];
    deleted.attr |= RS_ATTR_DELETED;
    if (!RedSeaEntryWrite(dv, RedSeaEntryPos(dir, previous), deleted)) {
      RedSeaMark(dv, cluster, blocks, false);
      RedSeaMark(dv, old_cluster, old_blocks, true);
      RedSeaEntryWrite(dv, RedSeaEntryPos(dir, previous), head[previous]);
      return 0;
    }
    head[previous] = deleted;
  }

  if (blocks || !reserve)
    entry.clus = cluster;
  bool okay = true;
  if (directory) {
    CRedSeaDirEntry child[BLK_SIZE / sizeof(CRedSeaDirEntry)]{};
    child[0] = entry;
    child[1] = {entry.attr, "..", dir.cluster, 0, entry.datetime};
    okay = BlkWriteZero(dv, cluster + 1, blocks - 1) &&
           BlkDevWriteBytes(*dv.bd, reinterpret_cast<const uint8_t *>(child),
                            cluster * BLK_SIZE, sizeof(child));
  } else if (blocks && !reserve)
    okay = RedSeaWriteData(dv, cluster, data, entry.size);
  if (!okay) {
    RedSeaMark(dv, cluster, blocks, false);
    return 0;
  }
  uint64_t pos = RedSeaEntryPos(dir, empty);
  if (!RedSeaEntryWrite(dv, pos, entry)) {
    // Do not release an extent if its directory entry could have been written.
    if (RedSeaEntryWrite(dv, pos, head[empty]))
      RedSeaMark(dv, cluster, blocks, false);
    return 0;
  }
  return cluster;
}

static bool RedSeaChild(CDrv &dv, CRedSeaDir dir, const char *name,
                         CRedSeaDirEntry &child, uint64_t &pos)
{
  pos = 0;
  CRedSeaDirBuf buf(dv, dir);
  auto *head = buf.head;
  if (!head)
    return false;
  for (size_t i = 2; i < head->size / sizeof(*head) && head[i].name[0]; i++)
    if (!(head[i].attr & RS_ATTR_DELETED) && RedSeaNameEq(head[i].name, name)) {
      child = head[i];
      pos = RedSeaEntryPos(dir, i);
      break;
    }
  return true;
}

static bool RedSeaDirPath(CDrv &dv, const char *path, CRedSeaDir *dirs,
                          size_t &depth)
{
  depth = 1;
  dirs[0] = {uint64_t(dv.root_clus), 0};
  CRedSeaDirEntry root;
  if (!RedSeaAllocatedDir(dv, dirs[0], root))
    return false;
  while (*path) {
    const char *part = path;
    while (*path && *path != '/')
      path++;
    size_t len = path - part;
    if (*path)
      path++;
    if (!len || (len == 1 && *part == '.'))
      continue;
    if (len == 2 && part[0] == '.' && part[1] == '.') {
      if (depth > 1)
        depth--;
      continue;
    }
    char name[CDIR_FILENAME_LEN];
    if (len >= sizeof(name))
      return false;
    MemCpy(name, part, len);
    name[len] = 0;
    CRedSeaDirEntry child, head;
    uint64_t pos;
    if (!FileNameChk(name) || !RedSeaChild(dv, dirs[depth - 1], name, child, pos) ||
        !pos || !(child.attr & RS_ATTR_DIR))
      return false;
    CRedSeaDir next{child.clus, pos};
    if (!RedSeaAllocatedDir(dv, next, head) || head.size != child.size)
      return false;
    for (size_t i = 0; i < depth; i++) {
      CRedSeaDirEntry ancestor;
      if (!RedSeaDirHead(dv, dirs[i], ancestor) ||
          RedSeaOverlap(next.cluster, RedSeaBlocks(head.size), dirs[i].cluster,
                         RedSeaBlocks(ancestor.size)))
        return false;
    }
    dirs[depth++] = next;
  }
  return true;
}

static bool RedSeaDirExtentSafe(const CDrv &dv, const CRedSeaDir *dirs,
                                size_t depth, uint64_t c, uint64_t blocks)
{
  if (!blocks)
    return true;
  if (!RedSeaAllocated(dv, c, blocks))
    return false;
  for (size_t i = 0; i < depth; i++) {
    CRedSeaDirEntry head;
    if (!RedSeaDirHead(dv, dirs[i], head) ||
        RedSeaOverlap(c, blocks, dirs[i].cluster, RedSeaBlocks(head.size)))
      return false;
  }
  return true;
}

static void RedSeaDirPut(CDrv &dv, CRedSeaDir *dirs, size_t depth,
                          const CRedSeaDirEntry &entry, bool free_old_chain)
{
  uint64_t blocks = RedSeaBlocks(entry.size);
  uint64_t extent_blocks = blocks;
  if (!extent_blocks && entry.clus && entry.clus != INVALID_CLUS)
    extent_blocks = 1;
  if ((entry.attr & RS_ATTR_DIR) && (!entry.size || entry.size % BLK_SIZE))
    return;
  if (!RedSeaDirExtentSafe(dv, dirs, depth, entry.clus, extent_blocks))
    return;

  auto &dir = dirs[depth - 1];
  CRedSeaDirBuf buf(dv, dir);
  auto *head = buf.head;
  if (!head)
    return;
  size_t capacity = head->size / sizeof(*head), empty = 0, i = 2;
  for (; i < capacity && head[i].name[0]; i++) {
    if (head[i].attr & RS_ATTR_DELETED) {
      if (!empty)
        empty = i;
    } else if (RedSeaNameEq(head[i].name, entry.name)) {
      uint64_t old_blocks = RedSeaBlocks(head[i].size),
               old_cluster = head[i].clus;
      if (free_old_chain && old_blocks &&
          (!RedSeaDirExtentSafe(dv, dirs, depth, old_cluster, old_blocks) ||
           RedSeaOverlap(old_cluster, old_blocks, entry.clus, extent_blocks)))
        return;
      uint64_t pos = RedSeaEntryPos(dir, empty ? empty : i);
      if (!RedSeaEntryWrite(dv, pos, entry))
        return;
      if (empty) {
        CRedSeaDirEntry deleted = head[i];
        deleted.attr |= RS_ATTR_DELETED;
        if (!RedSeaEntryWrite(dv, RedSeaEntryPos(dir, i), deleted)) {
          RedSeaEntryWrite(dv, pos, head[empty]);
          return;
        }
      }
      if (free_old_chain)
        RedSeaMark(dv, old_cluster, old_blocks, false);
      return;
    }
  }
  if (i == capacity)
    return;
  if (i + 1 == capacity && !RedSeaDirGrow(dv, dir, head))
    return;
  RedSeaEntryWrite(dv, RedSeaEntryPos(dir, empty ? empty : i), entry);
}

extern "C" uint64_t RedSeaDirNewCpp(CDrv *dv, const char *cur_dir,
                                     CDirEntry *entry, uint8_t free_old_chain)
{
  static_assert(sizeof(CRedSeaDirEntry) == 64 &&
                sizeof(CRedSeaDirEntry) == sizeof(CDirEntry) -
                                               offsetof(CDirEntry, attr));
  if (!entry)
    return false;
  entry->attr |= RS_ATTR_CONTIGUOUS;
  if (!DrvChkCpp(dv, false) || !cur_dir || entry->size < 0 ||
      !FileNameChk(entry->name) ||
      (entry->name[0] == '.' && (!entry->name[1] ||
                               (entry->name[1] == '.' && !entry->name[2]))))
    return false;
  CRedSeaDirEntry record;
  MemCpy(&record, &entry->attr, sizeof(record));
  size_t capacity = StrLen(cur_dir) / 2 + 2;
  if (capacity > SIZE_MAX / sizeof(CRedSeaDir))
    return false;
  auto *dirs = reinterpret_cast<CRedSeaDir *>(
      MAlloc(capacity * sizeof(CRedSeaDir), CurrentTask()->data_heap));
  if (!dirs)
    return false;
  {
    CDiskLock lock(*dv);
    DskCacheInvalidateDevice(dv->bd);
    size_t depth;
    if (RedSeaNativeWriteReady(*dv) &&
        RedSeaDirPath(*dv, cur_dir, dirs, depth))
      RedSeaDirPut(*dv, dirs, depth, record, free_old_chain);
  }
  Free(dirs);
  // The reference returns FALSE even after publishing a directory entry.
  return false;
}

static bool RedSeaStreamDir(CDrv &dv, const char *path, CRedSeaDir &dir,
                             char (&name)[CDIR_FILENAME_LEN])
{
  if (!path || *path++ != '/')
    return false;
  dir = {uint64_t(dv.root_clus), 0};
  for (;;) {
    const char *part = path;
    while (*path && *path != '/')
      path++;
    size_t len = path - part;
    if (!len || len >= sizeof(name))
      return false;
    MemCpy(name, part, len);
    name[len] = 0;
    if (!FileNameChk(name) || (name[0] == '.' &&
        (len == 1 || (len == 2 && name[1] == '.'))))
      return false;
    CRedSeaDirEntry child;
    if (!*path)
      return RedSeaAllocatedDir(dv, dir, child);
    uint64_t pos;
    if (!RedSeaChild(dv, dir, name, child, pos) || !pos ||
        !(child.attr & RS_ATTR_DIR) || child.clus == dir.cluster ||
        child.clus == uint64_t(dv.root_clus))
      return false;
    dir = {child.clus, pos};
    path++;
  }
}

bool RedSeaStreamPrepare(CDrv &dv, const char *path, CDirEntry &de, int64_t cnt)
{
  CDiskLock lock(dv);
  DskCacheInvalidateDevice(dv.bd);
  if (!RedSeaNativeWriteReady(dv))
    return false;
  CRedSeaDir dir;
  CRedSeaDirEntry entry{}, old;
  uint64_t pos;
  if (!RedSeaStreamDir(dv, path, dir, entry.name) || cnt > dv.size ||
      !RedSeaChild(dv, dir, entry.name, old, pos) ||
      (pos && (old.attr & RS_ATTR_DIR)))
    return false;
  if (cnt > 0) {
    entry.size = uint64_t(cnt) * BLK_SIZE;
    entry.attr = RS_ATTR_CONTIGUOUS;
    if (!RedSeaDirNew(dv, dir, entry, nullptr, false, true))
      return false;
  } else if (pos) {
    uint64_t blocks = RedSeaBlocks(old.size);
    CRedSeaDirEntry head;
    if (blocks &&
        (!RedSeaAllocated(dv, old.clus, blocks) ||
         !RedSeaDirHead(dv, dir, head) ||
         RedSeaOverlap(old.clus, blocks, dir.cluster, RedSeaBlocks(head.size))))
      return false;
    old.attr |= RS_ATTR_DELETED;
    if (!RedSeaEntryWrite(dv, pos, old) ||
        !RedSeaMark(dv, old.clus, blocks, false))
      return false;
  }
  MemCpy(&de.attr, &entry, sizeof(entry));
  return true;
}

bool RedSeaStreamCommit(CDrv &dv, const char *path, CDirEntry &de, bool new_file)
{
  CDiskLock lock(dv);
  DskCacheInvalidateDevice(dv.bd);
  if (!RedSeaNativeWriteReady(dv))
    return false;
  CRedSeaDir dir;
  char name[CDIR_FILENAME_LEN];
  if (!RedSeaStreamDir(dv, path, dir, name) || !RedSeaNameEq(de.name, name) ||
      de.size < 0 || (de.attr & (RS_ATTR_DIR | RS_ATTR_DELETED)))
    return false;
  CRedSeaDirEntry entry, old;
  de.attr |= RS_ATTR_CONTIGUOUS;
  MemCpy(&entry, &de.attr, sizeof(entry));
  if (new_file)
    return !entry.size && !entry.clus &&
           RedSeaDirNew(dv, dir, entry, nullptr, false, true);
  uint64_t pos;
  if (!RedSeaChild(dv, dir, name, old, pos) || !pos ||
      (old.attr & RS_ATTR_DIR) || old.clus != entry.clus ||
      RedSeaBlocks(entry.size) > RedSeaBlocks(old.size) ||
      (old.size && !RedSeaAllocated(dv, old.clus, RedSeaBlocks(old.size))))
    return false;
  return RedSeaEntryWrite(dv, pos, entry);
}

static uint64_t RedSeaWrite(CDrv &dv, const char *path, const uint8_t *data,
                            size_t size, uint64_t date, uint16_t attr,
                            bool directory, int64_t entry_cnt = 0)
{
  if (!path || *path++ != '/' || (attr & RS_ATTR_DIR))
    return 0;
  CDiskLock lock(dv);
  DskCacheInvalidateDevice(dv.bd);
  if (!RedSeaNativeWriteReady(dv) || size > uint64_t(dv.size) * BLK_SIZE)
    return 0;
  CRedSeaDir dir{uint64_t(dv.root_clus), 0};
  for (;;) {
    const char *part = path;
    while (*path && *path != '/')
      path++;
    size_t len = path - part;
    CRedSeaDirEntry entry{};
    if (!len || len >= sizeof(entry.name))
      return 0;
    MemCpy(entry.name, part, len);
    if (!FileNameChk(entry.name) ||
        (entry.name[0] == '.' &&
         (len == 1 || (len == 2 && entry.name[1] == '.'))))
      return 0;
    entry.attr = attr | RS_ATTR_CONTIGUOUS;
    entry.datetime = date;
    if (!*path && !directory) {
      entry.size = size;
      return RedSeaDirNew(dv, dir, entry, data, false);
    }

    CRedSeaDirEntry child;
    uint64_t pos;
    if (!RedSeaChild(dv, dir, entry.name, child, pos))
      return 0;
    if (!pos) {
      entry.attr = RS_ATTR_DIR | RS_ATTR_CONTIGUOUS;
      entry.size = BLK_SIZE;
      if (directory && !*path && entry_cnt > 0) {
        if (uint64_t(entry_cnt) > uint64_t(dv.size) *
                                      (BLK_SIZE / sizeof(entry)) - 3)
          return 0;
        entry.size = RedSeaBlocks((uint64_t(entry_cnt) + 3) * sizeof(entry)) *
                     BLK_SIZE;
      }
      if (!RedSeaDirNew(dv, dir, entry, nullptr, true) ||
          !RedSeaChild(dv, dir, entry.name, child, pos))
        return 0;
    }
    if (!pos || !(child.attr & RS_ATTR_DIR) || child.clus == dir.cluster ||
        child.clus == uint64_t(dv.root_clus))
      return 0;
    dir = {child.clus, pos};
    if (!*path)
      return dir.cluster;
    path++;
  }
}

int64_t RedSeaFilesDel(CDrv &dv, const char *cur_dir, const char *mask,
                      uint64_t flags, bool del_dir, bool print_msg)
{
  CDiskLock lock(dv);
  DskCacheInvalidateDevice(dv.bd);
  if (!RedSeaNativeWriteReady(dv))
    return 0;
  uint64_t cluster;
  if (!RedSeaDirClus(dv, cur_dir, StrLen(cur_dir), cluster))
    return 0;
  CRedSeaDir dir{cluster, 0};
  CRedSeaDirBuf buf(dv, dir);
  auto *head = buf.head;
  if (!head)
    return 0;
  CRedSeaDirEntry root;
  if (!RedSeaAllocatedDir(dv, {uint64_t(dv.root_clus), 0}, root))
    return 0;
  int64_t res = 0;
  size_t count = head->size / sizeof(*head);
  for (size_t i = 2; i < count && head[i].name[0]; ++i) {
    auto &entry = head[i];
    if ((entry.attr & RS_ATTR_DELETED) || entry.name[0] == '.' ||
        (!del_dir && (entry.attr & RS_ATTR_DIR)) ||
        !FileNameChk(entry.name) || !FilesFindMatch(entry.name, mask, flags))
      continue;
    uint64_t blocks = RedSeaBlocks(entry.size);
    if (blocks &&
        (!RedSeaAllocated(dv, entry.clus, blocks) ||
         RedSeaOverlap(entry.clus, blocks, cluster, RedSeaBlocks(head->size)) ||
         RedSeaOverlap(entry.clus, blocks, dv.root_clus, RedSeaBlocks(root.size))))
      continue;
    entry.attr |= RS_ATTR_DELETED;
    if (!RedSeaEntryWrite(dv, RedSeaEntryPos(dir, i), entry))
      break;
    if (!(entry.attr & RS_ATTR_DIR))
      res++;
    if (print_msg) {
      uint64_t arg = reinterpret_cast<uintptr_t>(entry.name);
      PrintCpp("Del %s\n", 1, &arg);
    }
    if (!RedSeaMark(dv, entry.clus, blocks, false))
      break;
  }
  return res;
}

uint64_t RedSeaFileWrite(CDrv &dv, const char *path, const uint8_t *data,
                         size_t size, uint64_t date, uint16_t attr)
{
  return RedSeaWrite(dv, path, data, size, date, attr, false);
}

static CRedSeaDirEntry *RedSeaReadDir(CDrv &dv, uint64_t clus)
{
  CDiskLock lock(dv);
  CRedSeaDirEntry block[BLK_SIZE / sizeof(CRedSeaDirEntry)];
  if (!RedSeaExtent(dv, clus, BLK_SIZE) ||
      !DrvReadBytes(dv, reinterpret_cast<uint8_t *>(block),
                       clus * BLK_SIZE, BLK_SIZE))
    return nullptr;
  auto size = block[0].size;
  if (size < BLK_SIZE || size % BLK_SIZE || !RedSeaExtent(dv, clus, size))
    return nullptr;
  auto *data = MAlloc(size, CurrentTask()->data_heap);
  if (!data)
    return nullptr;
  MemCpy(data, block, BLK_SIZE);
  if (size > BLK_SIZE &&
      !DrvReadBytes(dv, data + BLK_SIZE, (clus + 1) * BLK_SIZE,
                       size - BLK_SIZE)) {
    Free(data);
    return nullptr;
  }
  return reinterpret_cast<CRedSeaDirEntry *>(data);
}

bool RedSeaFilesFind(CDrv &dv, uint64_t clus, uint64_t parent_clus,
                     const char *directory, const char *mask, uint64_t flags,
                     CDirEntry *parent, CDirEntry *&res)
{
  auto *disk = RedSeaReadDir(dv, clus);
  if (!disk)
    return false;
  disk[0].name[0] = '.';
  disk[0].name[1] = 0;
  disk[1].clus = parent_clus;
  size_t count = disk[0].size / sizeof(*disk);
  bool valid = true;
  for (size_t i = 0; valid && i < count && disk[i].name[0]; i++) {
    const auto &de = disk[i];
    if ((de.attr & RS_ATTR_DELETED) || !FileNameChk(de.name))
      continue;
    bool dir = de.attr & RS_ATTR_DIR;
    bool recurse = dir && (flags & FUF_RECURSE);
    if ((recurse && de.name[0] == '.') || (!dir && (flags & FUF_JUST_DIRS)))
      continue;
    auto *name = DirEntryFullName(directory, de.name);
    if (!name) {
      valid = false;
      break;
    }
    if (!recurse && !FilesFindMatch(name, mask, flags)) {
      Free(name);
      continue;
    }
    auto *entry = reinterpret_cast<CDirEntry *>(
        CAlloc(sizeof(CDirEntry), CurrentTask()->data_heap));
    if (!entry) {
      Free(name);
      valid = false;
      break;
    }
    MemCpy(&entry->attr, &de, sizeof(de));
    entry->full_name = name;
    entry->parent = parent;
    entry->next = res;
    res = entry;
    if (recurse) {
      valid = de.clus != clus && de.clus != uint64_t(dv.root_clus);
      for (auto *p = parent; valid && p; p = p->parent)
        valid = uint64_t(p->clus) != de.clus;
      if (valid)
        valid = RedSeaFilesFind(dv, de.clus, clus, name, mask, flags, entry,
                                entry->sub);
    }
  }
  Free(disk);
  return valid;
}

bool RedSeaMkDir(CDrv &dv, const char *path, int64_t entry_cnt)
{
  return RedSeaWrite(dv, path, nullptr, 0, Now(), 0, true, entry_cnt);
}
