#include "../KernelB.hpp"

extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);

extern "C" uint64_t CdCpp(const char *name, uint8_t make_dirs)
{
  if (!name)
    name = "~";
  else if (!*name)
    return true;
  auto &task = *CurrentTask();
  auto *dv = task.cur_dv;
  const char *cwd = task.cur_dir ? task.cur_dir : "/";
  if (name[1] == ':') {
    auto *selected = Let2DrvCpp(*name, false);
    if (!selected || selected->fs_type != FSt_REDSEA)
      return false;
    if (selected != dv)
      cwd = "/";
    dv = selected;
    name += 2;
  }
  if (*name == '/' || !*name) {
    cwd = "/";
    if (*name)
      name++;
  }
  auto *input = PathTrim(name, task.data_heap);
  if (!input)
    return false;
  const char *home = blkdev.home_dir ? blkdev.home_dir + 2 : HOME_DIR;
  size_t len = StrLen(cwd), home_len = StrLen(home);
  auto *dir = reinterpret_cast<char *>(
      MAlloc(len + home_len + StrLen(input) + 2, task.data_heap));
  if (!dir) {
    Free(input);
    return false;
  }
  MemCpy(dir, cwd, len + 1);
  bool res = true;
  for (char *part = input; *part && res;) {
    char *end = part;
    while (*end && *end != '/')
      end++;
    size_t size = end - part;
    if (*end)
      *end++ = 0;
    if (!size) {
      len = 1;
      dir[1] = 0;
    } else if (size == 2 && part[0] == '.' && part[1] == '.') {
      while (len > 1 && dir[len - 1] != '/')
        len--;
      if (len > 1)
        len--;
      dir[len] = 0;
    } else if (size == 1 && *part == '~') {
      auto *selected = Let2DrvCpp('~', false);
      if (selected) {
        dv = selected;
        len = home_len;
        MemCpy(dir, home, len + 1);
      } else {
        res = false;
      }
    } else if (size != 1 || *part != '.') {
      size_t pos = len;
      if (pos > 1)
        dir[pos++] = '/';
      MemCpy(dir + pos, part, size + 1);
      if (dv &&
          (RedSeaIsDir(*dv, dir) || (make_dirs && RedSeaMkDir(*dv, dir)))) {
        len = pos + size;
      } else {
        dir[len] = 0;
        res = false;
      }
    }
    part = end;
  }
  Free(input);
  Free(task.cur_dir);
  task.cur_dir = dir;
  task.cur_dv = dv;
  return res;
}

extern "C" uint64_t IsDirCpp(const char *name)
{
  if (!name)
    return false;
  auto *context = DirContextNewCpp(name, false, false, true);
  bool res = context != nullptr;
  DirContextDelCpp(context, true);
  return res;
}

extern "C" int64_t DirCpp(const char *mask, uint8_t full)
{
  auto *entries = FilesFindCpp(mask, 0);
  auto *cwd = DirCurCpp(nullptr, nullptr);
  if (!cwd) {
    PutS("Invalid Drive\n");
    DirTreeDelCpp(entries);
    return 0;
  }
  Free(cwd);
  if (!entries) {
    PutS("No matching entries\n");
    return 0;
  }

  uint64_t size = 0xFFFF, clus = 0xFFFF;
  for (auto *entry = entries; entry; entry = entry->next) {
    if (entry->size > int64_t(size))
      size = entry->size;
    if (entry->clus > int64_t(clus))
      clus = entry->clus;
  }
  size = (63 - __builtin_clzll(size)) / 4 + 1;
  clus = (63 - __builtin_clzll(clus)) / 4 + 1;
  auto *name = entries->full_name;
  auto *slash = name + 2;
  for (auto *p = slash; *p; p++)
    if (*p == '/')
      slash = p;
  auto *end = slash == name + 2 ? slash + 1 : slash;
  char saved = *end;
  *end = 0;
  const uint64_t dir_args[]{reinterpret_cast<uintptr_t>(name)};
  PrintCpp(IsRawCpp() ? " of %s\n"
                      : "$MA,T=\"Directory\",LM=\"PopUpCd;Dir;\n\"$ of %s\n",
           1, dir_args);
  *end = saved;
  const uint64_t heading[]{size, reinterpret_cast<uintptr_t>("SIZE"), clus,
                           reinterpret_cast<uintptr_t>("BLK")};
  PrintCpp(full ? "__DATE__ __TIME__ %*ts %*ts\n" : "DATE_ TIME_ %*ts\n",
           full ? 4 : 2, heading);
  int64_t count = 0;
  while (entries) {
    auto *entry = entries;
    entries = entry->next;
    if (full) {
      uint64_t datetime;
      MemCpy(&datetime, &entry->datetime, sizeof(datetime));
      const uint64_t args[]{datetime, datetime,
                            size,     uint64_t(entry->size),
                            clus,     uint64_t(entry->clus)};
      PrintCpp("%D %T %0*tX %0*tX ", 6, args);
    } else {
      CDateStruct ds;
      Date2Struct(ds, DateFromBits(DateBits(entry->datetime) +
                                   DateBits(local_time_offset)));
      const uint64_t args[]{ds.mon, ds.day_of_mon, ds.hour,
                            ds.min, size,          uint64_t(entry->size)};
      PrintCpp("%02d/%02d %02d:%02d %0*tX ", 6, args);
    }
    if (entry->attr & RS_ATTR_DIR)
      PutDirLinkCpp(entry->name, entry->full_name);
    else
      PutFileLinkCpp(entry->name, entry->full_name, 0, false);
    PutS("\n");
    DirEntryDelCpp(entry);
    count++;
  }
  return count;
}
