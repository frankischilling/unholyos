#include "../KernelB.hpp"

bool RedSeaInit(CDrv &dv)
{
  CRedSeaBoot boot;
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

bool RedSeaFormatRAM(CDrv &dv, size_t root_blocks)
{
  constexpr size_t clusters_per_bitmap_block = 8 * BLK_SIZE;
  uint64_t blocks = dv.size;
  uint64_t map_blocks =
      (blocks + clusters_per_bitmap_block - 1) / clusters_per_bitmap_block;
  if (!map_blocks || root_blocks >= blocks ||
      map_blocks + 1 > blocks - root_blocks)
    return false;
  auto *base = dv.bd->RAM_dsk + size_t(dv.drv_offset) * BLK_SIZE;
  uint64_t root = dv.drv_offset + map_blocks + 1;
  auto *boot = reinterpret_cast<CRedSeaBoot *>(base);
  MemSet(boot, 0, (map_blocks + 1) * BLK_SIZE);
  boot->signature = MBR_PT_REDSEA;
  boot->signature2 = 0xAA55;
  boot->drv_offset = dv.drv_offset;
  boot->sects = blocks;
  boot->bitmap_sects = map_blocks;
  boot->root_clus = root;
  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  uint64_t date = Now();
  boot->unique_id = ((uint64_t(hi) << 32) | lo) ^ date;
  for (size_t bit = 0; bit <= root_blocks; bit++)
    base[BLK_SIZE + bit / 8] |= uint8_t{1} << (bit % 8);

  auto *dir =
      reinterpret_cast<CRedSeaDirEntry *>(dv.bd->RAM_dsk + root * BLK_SIZE);
  MemSet(dir, 0, root_blocks * BLK_SIZE);
  dir[0].attr = dir[1].attr = RS_ATTR_DIR | RS_ATTR_CONTIGUOUS;
  dir[0].name[0] = dir[1].name[0] = dir[1].name[1] = '.';
  dir[0].clus = dir[1].clus = root;
  dir[0].size = root_blocks * BLK_SIZE;
  dir[0].datetime = dir[1].datetime = date;
  return RedSeaInit(dv);
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

static bool RedSeaNameValid(const char *name)
{
  if (name[0] == '.' && (!name[1] || (name[1] == '.' && !name[2])))
    return true;
  for (size_t i = 0; i < CDIR_FILENAME_LEN; i++) {
    uint8_t ch = name[i];
    if (!ch)
      return i != 0;
    if (!(char_bmp_filename[ch / 32] & (uint32_t{1} << (ch % 32))))
      return false;
  }
  return false;
}

bool RedSeaFileFind(const CDrv &dv, uint64_t dir, const char *name,
                    bool directory, CRedSeaDirEntry &entry)
{
  MemSet(&entry, 0, sizeof(entry));
  CRedSeaDirEntry block[BLK_SIZE / sizeof(CRedSeaDirEntry)];
  auto *buf = reinterpret_cast<uint8_t *>(block);
  if (dv.fs_type != FSt_REDSEA || !RedSeaNameValid(name) ||
      !RedSeaExtent(dv, dir, BLK_SIZE) ||
      !BlkDevReadBytes(*dv.bd, buf, dir * BLK_SIZE, BLK_SIZE))
    return false;
  uint64_t size = block[0].size;
  if (size < BLK_SIZE || size % BLK_SIZE || !RedSeaExtent(dv, dir, size))
    return false;
  block[0].name[0] = '.';
  block[0].name[1] = 0;
  for (uint64_t pos = 0; pos < size; pos += BLK_SIZE) {
    if (pos && !BlkDevReadBytes(*dv.bd, buf, dir * BLK_SIZE + pos, BLK_SIZE))
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
          BlkDevReadBytes(*dv.bd, buf, entry.clus * BLK_SIZE + pos, bytes));
}

static uint8_t *RedSeaBlock(const CDrv &dv, uint64_t cluster)
{
  return dv.bd->RAM_dsk + cluster * BLK_SIZE;
}

static bool RedSeaClusterUsed(const CDrv &dv, uint64_t cluster)
{
  auto bit = cluster - dv.data_area;
  return RedSeaBlock(dv, dv.fat1)[bit / 8] & (uint8_t{1} << (bit % 8));
}

static void RedSeaMark(const CDrv &dv, uint64_t cluster, uint64_t blocks,
                       bool used)
{
  auto *map = RedSeaBlock(dv, dv.fat1);
  for (uint64_t i = 0; i < blocks; i++) {
    auto bit = cluster + i - dv.data_area;
    auto mask = uint8_t{1} << (bit % 8);
    if (used)
      map[bit / 8] |= mask;
    else
      map[bit / 8] &= ~mask;
  }
}

static uint64_t RedSeaAllocClus(const CDrv &dv, uint64_t blocks)
{
  uint64_t end = dv.drv_offset + dv.size;
  uint64_t best = INVALID_CLUS, best_size = UINT64_MAX;
  for (uint64_t clus = dv.data_area; clus < end;) {
    uint64_t start = clus;
    while (clus < end && !RedSeaClusterUsed(dv, clus))
      clus++;
    uint64_t size = clus - start;
    if (size >= blocks && size < best_size) {
      best = start;
      best_size = size;
      if (size == blocks)
        break;
    }
    clus++;
  }
  if (best != INVALID_CLUS)
    RedSeaMark(dv, best, blocks, true);
  return best;
}

static uint64_t RedSeaBlocks(uint64_t bytes)
{
  return bytes / BLK_SIZE + (bytes % BLK_SIZE != 0);
}

struct CRedSeaDir
{
  uint64_t cluster;
  CRedSeaDirEntry *parent_entry;
};

static CRedSeaDirEntry *RedSeaDirHead(const CDrv &dv, CRedSeaDir dir)
{
  if (!RedSeaExtent(dv, dir.cluster, BLK_SIZE))
    return nullptr;
  auto *head =
      reinterpret_cast<CRedSeaDirEntry *>(RedSeaBlock(dv, dir.cluster));
  if (head->size < BLK_SIZE || head->size % BLK_SIZE ||
      !RedSeaExtent(dv, dir.cluster, head->size))
    return nullptr;
  return head;
}

static CRedSeaDirEntry *RedSeaDirGrow(CDrv &dv, CRedSeaDir &dir,
                                      CRedSeaDirEntry *head)
{
  size_t bytes = head->size, blocks = bytes / BLK_SIZE;
  RedSeaMark(dv, dir.cluster, blocks, false);
  uint64_t cluster = RedSeaAllocClus(dv, blocks + 1);
  if (cluster == INVALID_CLUS) {
    RedSeaMark(dv, dir.cluster, blocks, true);
    return nullptr;
  }
  auto *src = reinterpret_cast<uint8_t *>(head),
       *dst = RedSeaBlock(dv, cluster);
  if (cluster > dir.cluster && cluster - dir.cluster < blocks) {
    for (size_t i = bytes; i > 0; i--)
      dst[i - 1] = src[i - 1];
  } else if (cluster != dir.cluster) {
    MemCpy(dst, src, bytes);
  }
  MemSet(dst + bytes, 0, BLK_SIZE);
  head = reinterpret_cast<CRedSeaDirEntry *>(dst);
  head->size = bytes + BLK_SIZE;
  head->clus = cluster;
  if (dir.parent_entry) {
    dir.parent_entry->clus = cluster;
    dir.parent_entry->size = head->size;
  } else {
    auto *boot =
        reinterpret_cast<CRedSeaBoot *>(RedSeaBlock(dv, dv.drv_offset));
    dv.root_clus = boot->root_clus = cluster;
  }
  dir.cluster = cluster;
  return head;
}

static uint64_t RedSeaDirNew(CDrv &dv, CRedSeaDir &dir, CRedSeaDirEntry &entry,
                             const uint8_t *data, bool directory)
{
  auto *head = RedSeaDirHead(dv, dir);
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
  if (i == capacity)
    return 0;

  uint64_t old_size = previous ? head[previous].size : 0;
  uint64_t old_cluster = previous ? head[previous].clus : INVALID_CLUS;
  if (old_size && !RedSeaExtent(dv, old_cluster, old_size))
    return 0;
  uint64_t old_blocks = RedSeaBlocks(old_size),
           blocks = RedSeaBlocks(entry.size);
  RedSeaMark(dv, old_cluster, old_blocks, false);
  uint64_t cluster = blocks ? RedSeaAllocClus(dv, blocks) : INVALID_CLUS;
  if (blocks && cluster == INVALID_CLUS) {
    RedSeaMark(dv, old_cluster, old_blocks, true);
    return 0;
  }
  if (empty == i && i + 1 == capacity) {
    head = RedSeaDirGrow(dv, dir, head);
    if (!head) {
      RedSeaMark(dv, cluster, blocks, false);
      RedSeaMark(dv, old_cluster, old_blocks, true);
      return 0;
    }
  }

  entry.clus = cluster;
  if (directory) {
    auto *child = reinterpret_cast<CRedSeaDirEntry *>(RedSeaBlock(dv, cluster));
    MemSet(child, 0, blocks * BLK_SIZE);
    child[0] = entry;
    child[1] = {entry.attr, "..", dir.cluster, 0, entry.datetime};
  } else if (blocks) {
    auto *buf = RedSeaBlock(dv, cluster);
    MemCpy(buf, data, entry.size);
    MemSet(buf + entry.size, 0, blocks * BLK_SIZE - entry.size);
  }
  if (previous && previous != empty)
    head[previous].attr |= RS_ATTR_DELETED;
  head[empty] = entry;
  return cluster;
}

static CRedSeaDirEntry *RedSeaChild(const CDrv &dv, CRedSeaDir dir,
                                    const char *name)
{
  auto *head = RedSeaDirHead(dv, dir);
  if (!head)
    return nullptr;
  for (size_t i = 2; i < head->size / sizeof(*head) && head[i].name[0]; i++)
    if (!(head[i].attr & RS_ATTR_DELETED) && RedSeaNameEq(head[i].name, name))
      return head + i;
  return nullptr;
}

uint64_t RedSeaFileWrite(CDrv &dv, const char *path, const uint8_t *data,
                         size_t size, uint64_t date, uint16_t attr)
{
  if (*path++ != '/' || dv.bd->type != BDT_RAM ||
      (dv.bd->flags & BDF_READ_ONLY) || dv.fs_type != FSt_REDSEA ||
      size > uint64_t(dv.size) * BLK_SIZE || (attr & RS_ATTR_DIR))
    return 0;
  CMemLock lock(dv.locked_flags);
  CRedSeaDir dir{uint64_t(dv.root_clus), nullptr};
  for (;;) {
    const char *part = path;
    while (*path && *path != '/')
      path++;
    size_t len = path - part;
    CRedSeaDirEntry entry{};
    if (!len || len >= sizeof(entry.name))
      return 0;
    MemCpy(entry.name, part, len);
    if (!RedSeaNameValid(entry.name) ||
        (entry.name[0] == '.' &&
         (len == 1 || (len == 2 && entry.name[1] == '.'))))
      return 0;
    entry.attr = attr | RS_ATTR_CONTIGUOUS;
    entry.datetime = date;
    if (!*path) {
      entry.size = size;
      return RedSeaDirNew(dv, dir, entry, data, false);
    }

    auto *child = RedSeaChild(dv, dir, entry.name);
    if (!child) {
      entry.attr = RS_ATTR_DIR | RS_ATTR_CONTIGUOUS;
      entry.size = BLK_SIZE;
      if (!RedSeaDirNew(dv, dir, entry, nullptr, true))
        return 0;
      child = RedSeaChild(dv, dir, entry.name);
    }
    if (!child || !(child->attr & RS_ATTR_DIR))
      return 0;
    dir = {child->clus, child};
    path++;
  }
}
