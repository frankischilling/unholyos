#include "../KernelB.hpp"

static CDirEntry *FilesFind2(CDrv &dv, const char *dir, const char *mask,
                             uint64_t flags)
{
  uint64_t clus, parent;
  size_t size = StrLen(dir);
  if (!Name2DirClus(dv, dir + 2, size - 2, clus))
    return nullptr;
  while (size > 3 && dir[size - 1] != '/')
    size--;
  if (!Name2DirClus(dv, dir + 2, size - 2, parent))
    return nullptr;
  CDirEntry *res = nullptr;
  uint64_t dir_size;
  bool okay=dv.fs_type==FSt_FAT32?
      FAT32FilesFind(dv,clus,dir,mask,flags,nullptr,res,dir_size):
      RedSeaFilesFind(dv,clus,parent,dir,mask,flags,nullptr,res);
  if (!okay ||
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

  auto *dirc = DirContextNewCpp(mask, true, false, false);
  if (!dirc)
    return nullptr;
  auto *directory = DirCurCpp(nullptr, nullptr);
  auto *res = directory ? FilesFind2(*task.cur_dv, directory, dirc->mask, flags)
                        : nullptr;
  Free(directory);
  DirContextDelCpp(dirc, true);
  return res;
}

static bool FileFind2(const char *path, CDirEntry *res, uint64_t flags,
                      bool parents)
{
  if (!path)
    return false;
  auto *dirc = DirContextNewCpp(path, false, false, false);
  if (!dirc)
    return false;
  auto &task = *CurrentTask();
  bool found = false;
  while (*dirc->mask && task.cur_dir) {
    if (parents && (!task.cur_dir[1] || !CdCpp("..", false)))
      break;
    uint64_t clus;
    CRedSeaDirEntry de;
    if (!Name2DirClus(*dirc->dv, task.cur_dir, StrLen(task.cur_dir), clus))
      break;
    if ((!(flags & FUF_JUST_FILES) &&
         FsFileFind(*dirc->dv, clus, dirc->mask, true, de)) ||
        (!(flags & FUF_JUST_DIRS) &&
         FsFileFind(*dirc->dv, clus, dirc->mask, false, de))) {
      found = true;
      if (res) {
        auto *cwd = DirCurCpp(nullptr, nullptr);
        auto *full_name = cwd ? DirEntryFullName(cwd, de.name) : nullptr;
        Free(cwd);
        found = full_name != nullptr;
        if (found) {
          MemCpy(&res->attr, &de, sizeof(de));
          res->full_name = full_name;
        }
      }
      break;
    }
    if (!parents)
      break;
  }
  DirContextDelCpp(dirc, true);
  return found;
}

extern "C" uint64_t FileFindCpp(const char *name, CDirEntry *entry,
                                uint64_t flags)
{
  constexpr uint64_t fuf_error = 'F' | uint64_t('U') << 8 | uint64_t('F') << 16;
  if (flags & ~FUG_FILE_FIND)
    RaiseHolyC(fuf_error);
  if (!name || (*name && name[1] == ':' && !Let2DrvCpp(*name, false)))
    return false;
  auto &task = *CurrentTask();
  auto *filename = StrNew(name, task.data_heap);
  if (!filename)
    return false;
  auto *alt = flags & FUF_Z_OR_NOT_Z ? ToggleZorNotZ(filename, task.data_heap)
                                     : nullptr;
  CDirEntry de{};
  auto *res = entry ? &de : nullptr;
  bool found = FileFind2(filename, res, flags, false) ||
               FileFind2(alt, res, flags, false);
  if (!found && (flags & FUF_SCAN_PARENTS))
    found = FileFind2(filename, res, flags, true) ||
            FileFind2(alt, res, flags, true);
  if (entry)
    *entry = de;
  Free(filename);
  Free(alt);
  return found;
}
