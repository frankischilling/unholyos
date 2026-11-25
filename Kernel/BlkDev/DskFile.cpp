#include "../Compress.hpp"
#include "../KernelB.hpp"

bool RedSeaDirClus(const CDrv &dv, const char *path, size_t count,
                   uint64_t &dir)
{
  if (!count || *path != '/' || dv.fs_type != FSt_REDSEA)
    return false;
  dir = dv.root_clus;
  for (size_t pos = 1; pos < count;) {
    if (path[pos] == '/') {
      pos++;
      continue;
    }
    size_t start = pos;
    while (pos < count && path[pos] != '/')
      pos++;
    char name[CDIR_FILENAME_LEN];
    size_t len = pos - start;
    if (len >= sizeof(name))
      return false;
    MemCpy(name, path + start, len);
    name[len] = 0;
    CRedSeaDirEntry entry;
    if (!RedSeaFileFind(dv, dir, name, true, entry))
      return false;
    dir = entry.clus;
  }
  return true;
}

bool RedSeaIsDir(const CDrv &dv, const char *path)
{
  uint64_t dir;
  return RedSeaDirClus(dv, path, StrLen(path), dir);
}

static size_t PathFileOffset(const char *path)
{
  size_t prefix = 0;
  for (size_t i = 0; path[i]; i++)
    if (path[i] == '/')
      prefix = i + 1;
  return prefix;
}

static bool FindInParents(const CDrv &dv, const char *path, const char *name,
                          size_t &prefix, CRedSeaDirEntry &entry)
{
  size_t parent = prefix;
  while (parent > 1) {
    do {
      parent--;
    } while (parent > 1 && path[parent - 1] != '/');
    uint64_t dir;
    if (RedSeaDirClus(dv, path, parent, dir) &&
        RedSeaFileFind(dv, dir, name, false, entry)) {
      prefix = parent;
      return true;
    }
  }
  return false;
}

bool RedSeaFileLookup(const CDrv &dv, const char *path, bool alternate,
                      bool scan_parents, CRedSeaDirEntry &entry, size_t &prefix)
{
  prefix = PathFileOffset(path);
  auto *name = path + prefix;
  size_t len = StrLen(name);
  uint64_t dir;
  if (!len || !RedSeaDirClus(dv, path, prefix, dir))
    return false;
  bool exact = len < CDIR_FILENAME_LEN;
  if (exact && RedSeaFileFind(dv, dir, name, false, entry))
    return true;

  char alt[CDIR_FILENAME_LEN]{};
  if (alternate) {
    if (IsDotExt(path, 'Z')) {
      if (len - 2 < sizeof(alt))
        MemCpy(alt, name, len - 2);
    } else if (len + 2 < sizeof(alt)) {
      MemCpy(alt, name, len);
      alt[len] = '.';
      alt[len + 1] = 'Z';
    }
    if (*alt && RedSeaFileFind(dv, dir, alt, false, entry))
      return true;
  }
  return scan_parents &&
         ((exact && FindInParents(dv, path, name, prefix, entry)) ||
          (*alt && FindInParents(dv, path, alt, prefix, entry)));
}

static uint8_t *ExpandBuf(const uint8_t *data, size_t size, size_t &expanded,
                          CHeapCtrl *hc)
{
  CArcCompress arc;
  if (!ArcReadHeader(data, size, arc) || arc.expanded_size > MEM_MAPPED_SPACE)
    return nullptr;
  auto *res = MAlloc(arc.expanded_size + 1, hc);
  if (!res)
    return nullptr;
  CArcCtrl *ctrl = nullptr;
  if (arc.compression_type != CT_NONE && arc.expanded_size)
    ctrl = reinterpret_cast<CArcCtrl *>(MAlloc(sizeof(CArcCtrl), hc));
  bool ok = ArcExpandBuf(data, size, res, arc.expanded_size, ctrl);
  Free(ctrl);
  if (!ok) {
    Free(res);
    return nullptr;
  }
  expanded = arc.expanded_size;
  res[expanded] = 0;
  return res;
}

static uint64_t FileAttr(const char *name, uint64_t attr)
{
  attr &= ~uint64_t{RS_ATTR_COMPRESSED};
  if (IsDotExt(name, 'Z'))
    attr |= RS_ATTR_COMPRESSED;
  if (IsDotExt(name, 'C'))
    attr |= RS_ATTR_CONTIGUOUS;
  return attr;
}

static uint8_t *RedSeaReadFound(const CDrv &dv, const CRedSeaDirEntry &entry,
                                CHeapCtrl *hc, bool block_storage)
{
  if (entry.size > SIZE_MAX - BLK_SIZE ||
      entry.size > uint64_t(dv.size) * BLK_SIZE)
    return nullptr;
  size_t storage = entry.size;
  if (block_storage)
    storage = (storage + BLK_SIZE - 1) & ~(BLK_SIZE - 1);
  auto *data = MAlloc(storage + 1, hc);
  if (!data)
    return nullptr;
  if (!RedSeaFileRead(dv, entry, 0, data, entry.size)) {
    Free(data);
    return nullptr;
  }
  data[entry.size] = 0;
  return data;
}

static uint8_t *FileExpand(uint8_t *data, size_t &size, uint64_t attr,
                           CHeapCtrl *hc)
{
  if (!(attr & RS_ATTR_COMPRESSED))
    return data;
  size_t expanded = 0;
  auto *res = ExpandBuf(data, size, expanded, hc);
  Free(data);
  size = expanded;
  return res;
}

uint8_t *FileRead(const CDrv &dv, const char *path, size_t &size, CHeapCtrl *hc)
{
  size = 0;
  CRedSeaDirEntry entry;
  size_t prefix;
  if (!RedSeaFileLookup(dv, path, true, false, entry, prefix))
    return nullptr;
  auto *data = RedSeaReadFound(dv, entry, hc, false);
  if (!data)
    return nullptr;
  size = entry.size;
  return FileExpand(data, size, FileAttr(entry.name, entry.attr), hc);
}

static const CHashGeneric *ResidentFile(const char *name, const char *alt)
{
  auto *table = adam_task->hash_table;
  auto *entry = HashFind(name, table, HTT_FILE);
  if (!entry && alt)
    entry = HashFind(alt, table, HTT_FILE);
  return reinterpret_cast<CHashGeneric *>(entry);
}

static void CacheResidentFile(const char *name, const uint8_t *data,
                              size_t size, size_t storage)
{
  auto *hc = adam_task->data_heap;
  size_t len = StrLen(name) + 1;
  auto *entry =
      reinterpret_cast<CHashGeneric *>(CAlloc(sizeof(CHashGeneric), hc));
  auto *copy = MAlloc(storage, hc), *key = MAlloc(len, hc);
  if (!entry || !copy || !key) {
    Free(entry);
    Free(copy);
    Free(key);
    return;
  }
  MemCpy(copy, data, size);
  copy[size] = 0;
  MemCpy(key, name, len);
  entry->hash.str = reinterpret_cast<char *>(key);
  entry->hash.type = HTT_FILE;
  entry->user_data0 = reinterpret_cast<uintptr_t>(copy);
  entry->user_data1 = size;
  HashAdd(&entry->hash, adam_task->hash_table);
}

static void RemoveResidentFile(CHash *hash, CHashTable *table)
{
  HashRemove(hash, table);
  HashDel(hash);
}

void ResidentDriveClear(char letter)
{
  auto *table = adam_task->hash_table;
  for (int64_t i = 0; i <= table->mask; i++) {
    for (auto *entry = table->body[i]; entry;) {
      auto *next = entry->next;
      if ((entry->type & HTT_FILE) && *entry->str && entry->str[1] == ':' &&
          (*entry->str == letter || *entry->str == letter + ('a' - 'A')))
        RemoveResidentFile(entry, table);
      entry = next;
    }
  }
}

extern "C" uint8_t *FileReadCpp(const char *filename, size_t *out_size,
                                uint64_t *out_attr)
{
  auto &task = *CurrentTask();
  auto *hc = task.data_heap;
  auto *path = FileNameAbs(filename, task);
  auto *alt = path ? ToggleZorNotZ(path, hc) : nullptr;
  uint8_t *data = nullptr;
  size_t size = 0;
  uint64_t attr = 0;
  if (path) {
    if (auto *cached = ResidentFile(path, alt)) {
      size = cached->user_data1;
      if (size < SIZE_MAX)
        data = MAlloc(size + 1, hc);
      if (data) {
        MemCpy(data, reinterpret_cast<void *>(cached->user_data0), size);
        data[size] = 0;
        attr = FileAttr(cached->hash.str, 0);
      } else {
        size = 0;
      }
    } else if (*path && path[1] == ':') {
      const auto *dv = PathDrive(task, path[0]);
      CRedSeaDirEntry entry;
      size_t prefix;
      if (dv && RedSeaFileLookup(*dv, path + 2, true, true, entry, prefix) &&
          (data = RedSeaReadFound(*dv, entry, hc, true))) {
        size = entry.size;
        attr = FileAttr(entry.name, entry.attr);
        if (alt && (attr & RS_ATTR_RESIDENT)) {
          const char *leaf = path + PathFileOffset(path);
          size_t len = StrLen(leaf);
          bool exact = len == StrLen(entry.name);
          for (size_t i = 0; exact && i < len; i++)
            exact = leaf[i] == entry.name[i];
          CacheResidentFile(exact ? path : alt, data, size, MSize(data));
        }
      }
    }
  }
  if (data)
    data = FileExpand(data, size, attr, hc);
  Free(alt);
  Free(path);
  if (out_size)
    *out_size = size;
  if (out_attr)
    *out_attr = attr;
  return data;
}

extern "C" uint64_t FileWriteCpp(const char *filename, const uint8_t *data,
                                 uint64_t count, uint64_t date, uint64_t attr)
{
  auto &task = *CurrentTask();
  auto *path = FileNameAbs(filename, task);
  if (!path)
    return 0;
  auto *dv = *path && path[1] == ':' ? PathDrive(task, path[0]) : nullptr;
  if (!dv || dv->bd->type != BDT_RAM || (dv->bd->flags & BDF_READ_ONLY)) {
    Free(path);
    return 0;
  }

  size_t size = static_cast<int64_t>(count) >= 0 ? count : 0;
  attr = FileAttr(path + PathFileOffset(path), attr);
  uint8_t *encoded = nullptr;
  if (attr & RS_ATTR_COMPRESSED) {
    auto *hc = task.data_heap;
    if (size <= SIZE_MAX / 8 - ARC_WRITE_OVERHEAD)
      encoded = MAlloc(size + ARC_WRITE_OVERHEAD, hc);
    auto *ctrl =
        reinterpret_cast<CArcEncodeCtrl *>(MAlloc(sizeof(CArcEncodeCtrl), hc));
    size = encoded && ctrl ? ArcCompressBuf(data, size, encoded,
                                            size + ARC_WRITE_OVERHEAD, *ctrl)
                           : 0;
    Free(ctrl);
    if (!size) {
      Free(encoded);
      Free(path);
      return 0;
    }
    data = encoded;
  }

  auto cluster =
      RedSeaFileWrite(*dv, path + 2, data, size, date ? date : Now(), attr);
  if (cluster) {
    auto *table = adam_task->hash_table;
    auto *hash = HashSingleTableFind(path, table, HTT_FILE);
    if (attr & RS_ATTR_RESIDENT)
      CacheResidentFile(path, data, size, size + 1);
    if (hash)
      RemoveResidentFile(hash, table);
  }
  Free(encoded);
  Free(path);
  return cluster;
}
