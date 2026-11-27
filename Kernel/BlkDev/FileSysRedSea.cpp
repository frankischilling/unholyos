#include "../KernelB.hpp"

extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);
extern "C" void PrintErrCpp(const char *fmt, int64_t argc, const uint64_t *argv,
                             const uintptr_t *frame);

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
  DskCacheInvalidateDevice(dv.bd);
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

uint64_t RedSeaAllocClus(const CDrv &dv, uint64_t blocks)
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

static bool RedSeaRAMGeometry(const CDrv &dv)
{
  if (!WritableFS(dv) || dv.fs_type != FSt_REDSEA || !dv.bd->RAM_dsk ||
      dv.bd->bd_signature != BD_SIGNATURE_VAL || dv.bd->max_blk < 0 ||
      uint64_t(dv.bd->max_blk) >= SIZE_MAX / BLK_SIZE || dv.drv_offset < 0 ||
      dv.size <= 0 || dv.spc != 1)
    return false;
  uint64_t first = dv.drv_offset, disk_end = uint64_t(dv.bd->max_blk) + 1;
  if (first >= disk_end || uint64_t(dv.size) > disk_end - first ||
      dv.fat1 != dv.drv_offset + 1 || dv.fat2 != dv.fat1 ||
      dv.data_area <= dv.drv_offset || dv.root_clus <= dv.data_area)
    return false;
  uint64_t end = first + dv.size, data = dv.data_area;
  return uint64_t(dv.root_clus) < end &&
         (end - data - 1) / (BLK_SIZE * 8) < data - first &&
         RedSeaClusterUsed(dv, data);
}

static bool RedSeaClusterRange(const CDrv &dv, uint64_t c, uint64_t cnt)
{
  uint64_t end = uint64_t(dv.drv_offset) + dv.size;
  return c > uint64_t(dv.data_area) && c < end && cnt <= end - c;
}

static bool RedSeaAllocated(const CDrv &dv, uint64_t c, uint64_t cnt)
{
  if (!RedSeaClusterRange(dv, c, cnt))
    return false;
  for (uint64_t i = 0; i < cnt; i++)
    if (!RedSeaClusterUsed(dv, c + i))
      return false;
  return true;
}

static bool RedSeaOverlap(uint64_t a, uint64_t an, uint64_t b, uint64_t bn)
{
  return an && bn && (a <= b ? b - a < an : a - b < bn);
}

static CRedSeaDirEntry *RedSeaAllocatedDir(const CDrv &dv, CRedSeaDir dir)
{
  auto *head = RedSeaDirHead(dv, dir);
  if (!head || !(head->attr & RS_ATTR_DIR) || head->clus != dir.cluster ||
      !RedSeaAllocated(dv, dir.cluster, RedSeaBlocks(head->size)))
    return nullptr;
  return head;
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
  if (!RedSeaRAMGeometry(*dv) || !RedSeaClusterRange(*dv, c, cnt))
    return;
  auto *root = RedSeaAllocatedDir(*dv, {uint64_t(dv->root_clus), nullptr});
  if (root && !RedSeaOverlap(c, cnt, dv->root_clus, RedSeaBlocks(root->size)))
    RedSeaMark(*dv, c, cnt, false);
}

extern "C" int64_t RedSeaAllocClusCpp(CDrv *dv, int64_t cnt)
{
  if (cnt <= 0)
    RaiseHolyC(0x767244); // Drv
  DrvChkCpp(dv, true);
  uint64_t c=INVALID_CLUS;
  {
    CDiskLock lock(*dv);
    DskCacheInvalidateDevice(dv->bd);
    if (RedSeaRAMGeometry(*dv) && cnt<=dv->size &&
        RedSeaAllocatedDir(*dv,{uint64_t(dv->root_clus),nullptr}))
      c=RedSeaAllocClus(*dv,cnt);
  }
  if (c==INVALID_CLUS) RaiseHolyC(0x767244); // Drv
  return int64_t(c);
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
                             const uint8_t *data, bool directory,
                             bool reserve = false)
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

  if (blocks || !reserve)
    entry.clus = cluster;
  if (directory) {
    auto *child = reinterpret_cast<CRedSeaDirEntry *>(RedSeaBlock(dv, cluster));
    MemSet(child, 0, blocks * BLK_SIZE);
    child[0] = entry;
    child[1] = {entry.attr, "..", dir.cluster, 0, entry.datetime};
  } else if (blocks && !reserve) {
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

static bool RedSeaDirPath(CDrv &dv, const char *path, CRedSeaDir *dirs,
                          size_t &depth)
{
  depth = 1;
  dirs[0] = {uint64_t(dv.root_clus), nullptr};
  if (!RedSeaAllocatedDir(dv, dirs[0]))
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
    auto *child = FileNameChk(name) ? RedSeaChild(dv, dirs[depth - 1], name)
                                    : nullptr;
    if (!child || !(child->attr & RS_ATTR_DIR))
      return false;
    CRedSeaDir next{child->clus, child};
    auto *head = RedSeaAllocatedDir(dv, next);
    if (!head || head->size != child->size)
      return false;
    for (size_t i = 0; i < depth; i++) {
      auto *ancestor = RedSeaDirHead(dv, dirs[i]);
      if (RedSeaOverlap(next.cluster, RedSeaBlocks(head->size), dirs[i].cluster,
                         RedSeaBlocks(ancestor->size)))
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
    auto *head = RedSeaDirHead(dv, dirs[i]);
    if (RedSeaOverlap(c, blocks, dirs[i].cluster, RedSeaBlocks(head->size)))
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
  auto *head = RedSeaDirHead(dv, dir);
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
      if (empty) {
        head[empty] = entry;
        head[i].attr |= RS_ATTR_DELETED;
      } else
        head[i] = entry;
      if (free_old_chain)
        RedSeaMark(dv, old_cluster, old_blocks, false);
      return;
    }
  }
  if (i == capacity)
    return;
  if (i + 1 == capacity) {
    head = RedSeaDirGrow(dv, dir, head);
    if (!head)
      return;
  }
  head[empty ? empty : i] = entry;
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
    if (RedSeaRAMGeometry(*dv) && RedSeaDirPath(*dv, cur_dir, dirs, depth))
      RedSeaDirPut(*dv, dirs, depth, record, free_old_chain);
  }
  Free(dirs);
  // The reference returns FALSE even after publishing a directory entry.
  return false;
}

static bool RedSeaStreamDir(CDrv &dv, const char *path, CRedSeaDir &dir,
                             char (&name)[CDIR_FILENAME_LEN])
{
  if (dv.fs_type != FSt_REDSEA || dv.bd->type != BDT_RAM ||
      (dv.bd->flags & BDF_READ_ONLY) || *path++ != '/')
    return false;
  dir = {uint64_t(dv.root_clus), nullptr};
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
    if (!*path)
      return RedSeaDirHead(dv, dir) != nullptr;
    auto *child = RedSeaChild(dv, dir, name);
    if (!child || !(child->attr & RS_ATTR_DIR))
      return false;
    dir = {child->clus, child};
    path++;
  }
}

bool RedSeaStreamPrepare(CDrv &dv, const char *path, CDirEntry &de, int64_t cnt)
{
  CDiskLock lock(dv);
  DskCacheInvalidateDevice(dv.bd);
  CRedSeaDir dir;
  CRedSeaDirEntry entry{};
  if (!RedSeaStreamDir(dv, path, dir, entry.name) || cnt > dv.size)
    return false;
  auto *old = RedSeaChild(dv, dir, entry.name);
  if (old && (old->attr & RS_ATTR_DIR))
    return false;
  if (cnt > 0) {
    entry.size = uint64_t(cnt) * BLK_SIZE;
    entry.attr = RS_ATTR_CONTIGUOUS;
    if (!RedSeaDirNew(dv, dir, entry, nullptr, false, true))
      return false;
  } else if (old) {
    if (old->size && !RedSeaExtent(dv, old->clus, old->size))
      return false;
    RedSeaMark(dv, old->clus, RedSeaBlocks(old->size), false);
    old->attr |= RS_ATTR_DELETED;
  }
  MemCpy(&de.attr, &entry, sizeof(entry));
  return true;
}

bool RedSeaStreamCommit(CDrv &dv, const char *path, CDirEntry &de, bool new_file)
{
  CDiskLock lock(dv);
  DskCacheInvalidateDevice(dv.bd);
  CRedSeaDir dir;
  char name[CDIR_FILENAME_LEN];
  if (!RedSeaStreamDir(dv, path, dir, name) || !RedSeaNameEq(de.name, name) ||
      de.size < 0 || (de.attr & (RS_ATTR_DIR | RS_ATTR_DELETED)))
    return false;
  CRedSeaDirEntry entry;
  de.attr |= RS_ATTR_CONTIGUOUS;
  MemCpy(&entry, &de.attr, sizeof(entry));
  if (new_file)
    return !entry.size && !entry.clus &&
           RedSeaDirNew(dv, dir, entry, nullptr, false, true);
  auto *old = RedSeaChild(dv, dir, name);
  if (!old || (old->attr & RS_ATTR_DIR) || old->clus != entry.clus ||
      RedSeaBlocks(entry.size) > RedSeaBlocks(old->size))
    return false;
  *old = entry;
  return true;
}

static uint64_t RedSeaWrite(CDrv &dv, const char *path, const uint8_t *data,
                            size_t size, uint64_t date, uint16_t attr,
                            bool directory, int64_t entry_cnt = 0)
{
  if (*path++ != '/' || dv.bd->type != BDT_RAM ||
      (dv.bd->flags & BDF_READ_ONLY) || dv.fs_type != FSt_REDSEA ||
      size > uint64_t(dv.size) * BLK_SIZE || (attr & RS_ATTR_DIR))
    return 0;
  CDiskLock lock(dv);
  DskCacheInvalidateDevice(dv.bd);
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

    auto *child = RedSeaChild(dv, dir, entry.name);
    if (!child) {
      entry.attr = RS_ATTR_DIR | RS_ATTR_CONTIGUOUS;
      entry.size = BLK_SIZE;
      if (directory && !*path && entry_cnt > 0) {
        if (uint64_t(entry_cnt) > uint64_t(dv.size) *
                                      (BLK_SIZE / sizeof(entry)) - 3)
          return 0;
        entry.size = RedSeaBlocks((uint64_t(entry_cnt) + 3) * sizeof(entry)) *
                     BLK_SIZE;
      }
      if (!RedSeaDirNew(dv, dir, entry, nullptr, true))
        return 0;
      child = RedSeaChild(dv, dir, entry.name);
    }
    if (!child || !(child->attr & RS_ATTR_DIR))
      return 0;
    dir = {child->clus, child};
    if (!*path)
      return dir.cluster;
    path++;
  }
}

int64_t RedSeaFilesDel(CDrv &dv, const char *cur_dir, const char *mask,
                      uint64_t flags, bool del_dir, bool print_msg)
{
  if (dv.fs_type != FSt_REDSEA || dv.bd->type != BDT_RAM ||
      (dv.bd->flags & BDF_READ_ONLY))
    return 0;
  CDiskLock lock(dv);
  DskCacheInvalidateDevice(dv.bd);
  uint64_t cluster;
  if (!RedSeaDirClus(dv, cur_dir, StrLen(cur_dir), cluster))
    return 0;
  auto *head = RedSeaDirHead(dv, {cluster, nullptr});
  if (!head)
    return 0;
  int64_t res = 0;
  size_t count = head->size / sizeof(*head);
  for (size_t i = 2; i < count && head[i].name[0]; ++i) {
    auto &entry = head[i];
    if ((entry.attr & RS_ATTR_DELETED) || entry.name[0] == '.' ||
        (!del_dir && (entry.attr & RS_ATTR_DIR)) ||
        !FileNameChk(entry.name) || !FilesFindMatch(entry.name, mask, flags))
      continue;
    if (entry.size && (!RedSeaExtent(dv, entry.clus, entry.size) ||
                       entry.clus == cluster || entry.clus == uint64_t(dv.root_clus)))
      continue;
    if (!(entry.attr & RS_ATTR_DIR))
      res++;
    if (print_msg) {
      uint64_t arg = reinterpret_cast<uintptr_t>(entry.name);
      PrintCpp("Del %s\n", 1, &arg);
    }
    entry.attr |= RS_ATTR_DELETED;
    RedSeaMark(dv, entry.clus, RedSeaBlocks(entry.size), false);
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
