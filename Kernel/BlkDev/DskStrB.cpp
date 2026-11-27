#include "../KernelB.hpp"

extern "C" void PrintErrCpp(const char *fmt, int64_t argc, const uint64_t *argv,
                            const uintptr_t *frame);

extern "C" uint64_t CFileNameToCpp(char *dst, const char *src)
{
  MemSet(dst, 0, CDIR_FILENAME_LEN);
  if (!FileNameChk(src))
    return false;
  MemCpy(dst, src, StrLen(src) + 1);
  return true;
}

static bool Name2DirClus2(const CDrv &dv, const char *path, size_t count,
                          uint64_t &dir, uint64_t &parent)
{
  if (!ReadableFS(dv))
    return false;
  dir = parent = dv.root_clus;
  for (size_t pos = 0; pos < count;) {
    parent = dir;
    size_t start = pos;
    while (pos < count && path[pos] != '/')
      pos++;
    size_t len = pos - start;
    if (pos < count)
      pos++;
    if (!len)
      continue;
    char name[CDIR_FILENAME_LEN];
    if (len >= sizeof(name))
      return false;
    MemCpy(name, path + start, len);
    name[len] = 0;
    CRedSeaDirEntry entry;
    if (!FsFileFind(dv, dir, name, true, entry))
      return false;
    dir = entry.clus;
  }
  return true;
}

bool Name2DirClus(const CDrv &dv, const char *path, size_t count,
                   uint64_t &dir)
{
  uint64_t parent;
  return count && *path == '/' && Name2DirClus2(dv, path, count, dir, parent);
}

bool Name2ParentDirClus(const CDrv &dv, const char *path, size_t count,
                         uint64_t &parent)
{
  uint64_t dir;
  return count && *path == '/' && Name2DirClus2(dv, path, count, dir, parent);
}

static uint64_t Name2DirClusPrint(CDrv *dv, const char *dirname,
                                  bool want_parent)
{
  DrvChkCpp(dv, true);
  uint64_t dir, parent;
  if (dirname &&
      Name2DirClus2(*dv, dirname, StrLen(dirname), dir, parent))
    return want_parent ? parent : dir;
  uint64_t arg = reinterpret_cast<uintptr_t>(dirname);
  PrintErrCpp("File not found: \"%s\".\n", 1, &arg, nullptr);
  return 0;
}

extern "C" uint64_t Name2DirClusCpp(CDrv *dv, const char *dirname)
{
  return Name2DirClusPrint(dv, dirname, false);
}

extern "C" uint64_t Name2ParentDirClusCpp(CDrv *dv, const char *dirname)
{
  return Name2DirClusPrint(dv, dirname, true);
}
