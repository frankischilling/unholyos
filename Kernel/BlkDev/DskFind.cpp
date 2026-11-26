#include "../KernelB.hpp"

static CDirEntry *FilesFind2(CDrv &dv, const char *dir, const char *mask,
                             uint64_t flags)
{
  uint64_t clus, parent;
  size_t size = StrLen(dir);
  if (!RedSeaDirClus(dv, dir + 2, size - 2, clus))
    return nullptr;
  while (size > 3 && dir[size - 1] != '/')
    size--;
  if (!RedSeaDirClus(dv, dir + 2, size - 2, parent))
    return nullptr;
  CDirEntry *res = nullptr;
  if (!RedSeaFilesFind(dv, clus, parent, dir, mask, flags, nullptr, res) ||
      !DirFilesSort(res, false)) {
    DirTreeDelCpp(res);
    return nullptr;
  }
  if (flags & (FUF_FLATTEN_TREE | FUF_JUST_FILES))
    DirFilesFlatten(res, flags);
  if ((flags & FUF_CLUS_ORDER) && !DirFilesSort(res, true)) {
    DirTreeDelCpp(res);
    return nullptr;
  }
  return res;
}

extern "C" CDirEntry *FilesFindCpp(const char *mask, uint64_t flags)
{
  constexpr uint64_t fuf_error = 'F' | uint64_t('U') << 8 | uint64_t('F') << 16;
  if (flags & ~FUG_FILES_FIND)
    RaiseHolyC(fuf_error);
  auto &task = *CurrentTask();
  if (flags & FUF_SINGLE) {
    auto *entry = reinterpret_cast<CDirEntry *>(
        MAlloc(sizeof(CDirEntry), task.data_heap));
    if (entry && FileFindCpp(mask, entry, 0))
      return entry;
    Free(entry);
    return nullptr;
  }

  auto *text = PathTrim(mask, task.data_heap);
  if (!text)
    return nullptr;
  auto *tail = text;
  while (*tail && *tail != ';')
    tail++;
  char separator = *tail;
  *tail = 0;
  auto *directory = !separator ? DirNameAbs(text, task) : nullptr;
  auto *dv = directory && *directory && directory[1] == ':'
                 ? PathDrive(task, directory[0])
                 : nullptr;
  if (dv && RedSeaIsDir(*dv, directory + 2)) {
    directory[0] = dv->drv_let;
    auto *res = FilesFind2(*dv, directory, "*", flags);
    Free(directory);
    Free(text);
    return res;
  }
  Free(directory);

  auto *path = FileNameAbs(text, task);
  *tail = separator;
  CDirEntry *res = nullptr;
  dv = path && *path && path[1] == ':' ? PathDrive(task, path[0]) : nullptr;
  if (dv && path[2] == '/') {
    char *leaf = path + 3;
    for (char *p = leaf; *p; p++)
      if (*p == '/')
        leaf = p + 1;
    size_t suffix = StrLen(leaf);
    size_t tail_len = StrLen(tail);
    auto *pattern =
        reinterpret_cast<char *>(MAlloc(suffix + tail_len + 1, task.data_heap));
    if (pattern) {
      MemCpy(pattern, leaf, suffix);
      MemCpy(pattern + suffix, tail, tail_len + 1);
      if (leaf != path + 3)
        leaf--;
      *leaf = 0;
      path[0] = dv->drv_let;
      res = FilesFind2(*dv, path, pattern, flags);
      Free(pattern);
    }
  }
  Free(path);
  Free(text);
  return res;
}

static bool FileFind2(char *path, CDirEntry *res, uint64_t flags, bool parents)
{
  if (!path || !*path || path[1] != ':' || path[2] != '/')
    return false;
  auto *dv = PathDrive(*CurrentTask(), *path);
  if (!dv)
    return false;
  auto *dir = path + 2;
  size_t prefix = 1;
  for (size_t i = 1; dir[i]; i++)
    if (dir[i] == '/')
      prefix = i + 1;
  auto *name = dir + prefix;
  if (!*name)
    return false;
  uint64_t clus;
  while (RedSeaDirClus(*dv, dir, prefix, clus)) {
    CRedSeaDirEntry de;
    if (!parents && ((!(flags & FUF_JUST_FILES) &&
                      RedSeaFileFind(*dv, clus, name, true, de)) ||
                     (!(flags & FUF_JUST_DIRS) &&
                      RedSeaFileFind(*dv, clus, name, false, de)))) {
      if (res) {
        char save = dir[prefix];
        dir[prefix] = 0;
        path[0] = dv->drv_let;
        auto *full_name = DirEntryFullName(path, de.name);
        dir[prefix] = save;
        if (!full_name)
          return false;
        MemCpy(&res->attr, &de, sizeof(de));
        res->full_name = full_name;
      }
      return true;
    }
    if (prefix <= 1 || !(flags & FUF_SCAN_PARENTS))
      break;
    parents = false;
    do {
      prefix--;
    } while (prefix > 1 && dir[prefix - 1] != '/');
  }
  return false;
}

extern "C" uint64_t FileFindCpp(const char *name, CDirEntry *entry,
                                uint64_t flags)
{
  constexpr uint64_t fuf_error = 'F' | uint64_t('U') << 8 | uint64_t('F') << 16;
  if (flags & ~FUG_FILE_FIND)
    RaiseHolyC(fuf_error);
  if (!name)
    return false;
  auto &task = *CurrentTask();
  auto *alt =
      flags & FUF_Z_OR_NOT_Z ? ToggleZorNotZ(name, task.data_heap) : nullptr;
  char *paths[]{FileNameAbs(name, task),
                alt ? FileNameAbs(alt, task) : nullptr};
  Free(alt);
  CDirEntry de{};
  auto *res = entry ? &de : nullptr;
  bool found = FileFind2(paths[0], res, flags & ~FUF_SCAN_PARENTS, false) ||
               FileFind2(paths[1], res, flags & ~FUF_SCAN_PARENTS, false);
  if (!found && (flags & FUF_SCAN_PARENTS))
    found = FileFind2(paths[0], res, flags, true) ||
            FileFind2(paths[1], res, flags, true);
  if (entry)
    *entry = de;
  Free(paths[0]);
  Free(paths[1]);
  return found;
}
