#include "../KernelB.hpp"

constexpr uint64_t FUF_JUST_DIRS = 0x400, FUF_Z_OR_NOT_Z = 0x0040000,
                   FUF_SCAN_PARENTS = 0x0100000;

static char *Join(const char *dir, const char *separator, const char *name,
                  const char *extension, CHeapCtrl *hc)
{
  size_t a = StrLen(dir), b = StrLen(separator), c = StrLen(name);
  size_t d = StrLen(extension);
  auto *res = reinterpret_cast<char *>(MAlloc(a + b + c + d + 1, hc));
  if (res) {
    MemCpy(res, dir, a);
    MemCpy(res + a, separator, b);
    MemCpy(res + a + b, name, c);
    MemCpy(res + a + b + c, extension, d + 1);
  }
  return res;
}

static bool PathSpace(uint8_t ch)
{
  return char_bmp_white_space[ch / 32] & (uint32_t{1} << (ch % 32));
}

static char *PathTrim(const char *name, CHeapCtrl *hc)
{
  if (!name)
    name = "";
  while (*name && PathSpace(*name))
    name++;
  auto *res = reinterpret_cast<char *>(MAlloc(StrLen(name) + 1, hc));
  if (!res)
    return nullptr;
  auto *out = res;
  for (; *name; name++) {
    auto ch = static_cast<uint8_t>(*name);
    if (ch >= 0x1F || PathSpace(ch))
      *out++ = *name;
  }
  while (out != res && PathSpace(out[-1]))
    out--;
  *out = 0;
  return res;
}

static char AsciiUpper(char ch)
{
  return ch >= 'a' && ch <= 'z' ? ch - ('a' - 'A') : ch;
}

static char BootLetter()
{
  return blkdev.boot_drv_let ? blkdev.boot_drv_let : BOOT_DVD_LET;
}

CDrv *PathDrive(const CTask &task, char letter)
{
  char upper = AsciiUpper(letter);
  if (task.cur_dv && task.cur_dv->drv_let == upper)
    return task.cur_dv;
  return MountedDrive(upper);
}

static char *LastSlash(char *path)
{
  char *slash = nullptr;
  for (; *path; path++)
    if (*path == '/')
      slash = path;
  return slash;
}

static char *DirNameAbs(const char *name, const CTask &task)
{
  auto *hc = task.data_heap;
  if (!task.cur_dir || !*task.cur_dir)
    return StrNew(name, hc);
  auto *input = PathTrim(name, hc);
  if (!input)
    return nullptr;
  const char *home = blkdev.home_dir;
  if (!home || !home[0] || home[1] != ':')
    home = "T:/Home";
  size_t home_len = StrLen(home + 2);
  auto *res = reinterpret_cast<char *>(
      MAlloc(StrLen(input) + StrLen(task.cur_dir) + home_len + 5, hc));
  if (!res) {
    Free(input);
    return nullptr;
  }

  char drive = task.cur_dv ? task.cur_dv->drv_let : BootLetter();
  auto *p = input;
  const char *cwd = task.cur_dir;
  if (*p && p[1] == ':') {
    drive = *p == ':' ? BootLetter() : *p == '~' ? *home : *p;
    p += 2;
    cwd = "/";
  }
  if (*p == '/') {
    cwd = "/";
    p++;
  }
  auto *path = res + 2;
  MemCpy(path, cwd, StrLen(cwd) + 1);
  size_t len = StrLen(path);
  while (*p) {
    auto *part = p;
    while (*p && *p != '/')
      p++;
    size_t size = p - part;
    if (*p)
      p++;
    if (!size) {
      len = 1;
      path[0] = '/';
    } else if (size == 2 && part[0] == '.' && part[1] == '.') {
      while (len > 1 && path[len - 1] != '/')
        len--;
      if (len > 1)
        len--;
    } else if (size == 1 && *part == '~') {
      drive = *home;
      MemCpy(path, home + 2, home_len);
      len = home_len;
    } else if (size != 1 || *part != '.') {
      if (len > 1)
        path[len++] = '/';
      MemCpy(path + len, part, size);
      len += size;
    }
  }
  path[len] = 0;
  res[0] = drive;
  res[1] = ':';
  Free(input);
  return res;
}

char *FileNameAbs(const char *name, const CTask &task, uint64_t flags)
{
  auto *hc = task.data_heap;
  auto *input = PathTrim(name, hc);
  if (!input)
    return nullptr;
  auto *body = *input && input[1] == ':' ? input + 2 : input;
  auto *slash = LastSlash(body);
  const char *file = slash ? slash + 1 : body;
  char root[4]{};
  if (body != input) {
    root[0] = input[0];
    root[1] = ':';
  }
  if (slash == body)
    root[body - input] = '/';
  if (slash)
    *slash = 0;
  auto *dir = DirNameAbs(slash && slash != body ? input : root, task);
  char *res = nullptr;
  if (dir) {
    size_t len = StrLen(dir);
    res = Join(dir, len && dir[len - 1] == '/' ? "" : "/", file, "", hc);
  }
  Free(dir);
  Free(input);

  if (res && flags && !(flags & FUF_JUST_DIRS) && *res && res[1] == ':') {
    const auto *dv = PathDrive(task, res[0]);
    CRedSeaDirEntry found;
    size_t prefix;
    if (dv && RedSeaFileLookup(*dv, res + 2, flags & FUF_Z_OR_NOT_Z,
                               flags & FUF_SCAN_PARENTS, found, prefix)) {
      res[0] = dv->drv_let;
      res[prefix + 2] = 0;
      auto *canonical = Join(res, "", found.name, "", hc);
      Free(res);
      res = canonical;
    }
  }
  return res;
}

const char *FileExtDot(const char *name)
{
  for (; *name; name++)
    if (*name == '.' && name[1] != '/' && name[1] != '.')
      return name;
  return nullptr;
}

bool IsDotExt(const char *name, char ext)
{
  size_t len = StrLen(name);
  if (len < 3 || name[len - 2] != '.' || name[len - 1] != ext)
    return false;
  for (size_t i = 0; i < len - 2; i++)
    if (name[i] == '.')
      return true;
  return false;
}

char *ToggleZorNotZ(const char *name, CHeapCtrl *hc)
{
  if (!IsDotExt(name, 'Z'))
    return Join(name, "", ".Z", "", hc);
  auto *res = StrNew(name, hc);
  if (res)
    res[StrLen(res) - 2] = 0;
  return res;
}

static char *ExtSet(const char *name, const char *ext, bool replace)
{
  auto *hc = MemTaskHeap(nullptr);
  const auto *dot = FileExtDot(name);
  if (dot && !replace)
    return StrNew(name, hc);
  size_t len = dot ? dot - name : StrLen(name), suffix = StrLen(ext);
  auto *res = reinterpret_cast<char *>(MAlloc(len + suffix + 2, hc));
  if (res) {
    MemCpy(res, name, len);
    res[len] = '.';
    MemCpy(res + len + 1, ext, suffix + 1);
  }
  return res;
}

extern "C" char *ExtDftCpp(const char *name, const char *ext)
{
  return ExtSet(name, ext, false);
}

extern "C" char *ExtChgCpp(const char *name, const char *ext)
{
  return ExtSet(name, ext, true);
}

extern "C" char *DirNameAbsCpp(const char *name)
{
  return DirNameAbs(name, *CurrentTask());
}

extern "C" char *FileNameAbsCpp(const char *name, uint64_t flags)
{
  return FileNameAbs(name, *CurrentTask(), flags);
}

extern "C" char *DirCurCpp(const CTask *task, void *mem_task)
{
  if (!task)
    task = CurrentTask();
  if (!task->cur_dir || !task->cur_dv)
    return nullptr;
  char drive[]{static_cast<char>(task->cur_dv->drv_let), ':', 0};
  return Join(drive, "", task->cur_dir, "", MemTaskHeap(mem_task));
}

extern "C" char *DirFileCpp(const char *dirname, const char *name,
                            const char *extension)
{
  const auto &task = *CurrentTask();
  auto *hc = task.data_heap;
  auto *dir = DirNameAbs(dirname, task);
  if (!dir)
    return nullptr;
  const auto *dv = *dir && dir[1] == ':' ? PathDrive(task, dir[0]) : nullptr;
  const char *ext = extension ? extension : "";
  auto *suffix = Join(*ext && *ext != '.' ? "." : "", "", ext, "", hc);
  char *fallback = nullptr;
  if (dv && suffix) {
    auto *slash = LastSlash(dir);
    while (slash && !RedSeaIsDir(*dv, dir + 2)) {
      *slash = 0;
      slash = LastSlash(dir);
    }
    while (slash) {
      char *candidate =
          name || *suffix ? Join(dir, "/", name ? name : slash + 1, suffix, hc)
                          : StrNew(dir, hc);
      if (!candidate)
        break;
      CRedSeaDirEntry entry;
      size_t prefix;
      if ((!*suffix && (!name || !*name)) ||
          RedSeaFileLookup(*dv, candidate + 2, false, false, entry, prefix) ||
          RedSeaIsDir(*dv, candidate + 2)) {
        Free(fallback);
        fallback = candidate;
        break;
      }
      if (!fallback)
        fallback = candidate;
      else
        Free(candidate);
      *slash = 0;
      slash = LastSlash(dir);
    }
  }
  Free(suffix);
  Free(dir);
  return fallback;
}
