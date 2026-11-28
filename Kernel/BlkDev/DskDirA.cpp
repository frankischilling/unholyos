#include "../KernelB.hpp"

extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);
extern "C" void PrintErrCpp(const char *fmt,int64_t argc,const uint64_t *argv,
                            const uintptr_t *rbp);

extern "C" uint64_t DirNewCpp(CDrv *dv,const char *cur_dir,CDirEntry *entry,
                               uint8_t free_old_chain)
{
  DrvChkCpp(dv,true);
  switch (dv->fs_type) {
    case FSt_REDSEA:
      return RedSeaDirNewCpp(dv,cur_dir,entry,free_old_chain);
    case FSt_FAT32:
      return FAT32DirNewCpp(dv,cur_dir,entry,free_old_chain);
    default:
      PrintErrCpp("File System Not Supported\n",0,nullptr,nullptr);
      return false;
  }
}

extern "C" void DirEntryDelCpp(CDirEntry *entry)
{
  if (entry) {
    Free(entry->full_name);
    Free(entry);
  }
}

extern "C" void DirEntryDel2Cpp(CDirEntry *entry)
{
  if (entry) {
    Free(entry->full_name);
    Free(reinterpret_cast<void *>(entry->user_data));
    Free(entry);
  }
}

static void DirTreeDel(CDirEntry *entry, bool free_data)
{
  while (entry) {
    auto *next = entry->next;
    DirTreeDel(entry->sub, free_data);
    if (free_data)
      DirEntryDel2Cpp(entry);
    else
      DirEntryDelCpp(entry);
    entry = next;
  }
}

extern "C" void DirTreeDelCpp(CDirEntry *entry)
{
  DirTreeDel(entry, false);
}

extern "C" void DirTreeDel2Cpp(CDirEntry *entry)
{
  DirTreeDel(entry, true);
}

char *DirEntryFullName(const char *directory, const char *name)
{
  size_t len = StrLen(directory);
  const uint64_t args[]{
      reinterpret_cast<uintptr_t>(directory),
      reinterpret_cast<uintptr_t>(len && directory[len - 1] == '/' ? "" : "/"),
      reinterpret_cast<uintptr_t>(name)};
  return MStrPrintCpp("%s%s%s", 3, args);
}

static int64_t DirEntryCompareName(const char *a, bool da, const char *b,
                                   bool db)
{
  if (da != db)
    return db - da;
  const char *ext_a = FileExtDot(a), *ext_b = FileExtDot(b);
  auto ext_cmp = StrCmpCpp(
      reinterpret_cast<const uint8_t *>(ext_a ? ext_a + 1 : ""),
      reinterpret_cast<const uint8_t *>(ext_b ? ext_b + 1 : ""));
  if (ext_cmp)
    return ext_cmp;
  size_t len_a = ext_a ? size_t(ext_a - a) : StrLen(a);
  size_t len_b = ext_b ? size_t(ext_b - b) : StrLen(b);
  for (size_t i = 0; i < len_a && i < len_b; i++)
    if (a[i] != b[i])
      return uint8_t(a[i]) > uint8_t(b[i]) ? 1 : -1;
  return (len_a > len_b) - (len_a < len_b);
}

int64_t DirEntryCompareName(const CRedSeaDirEntry &a, const CRedSeaDirEntry &b)
{
  return DirEntryCompareName(a.name, a.attr & RS_ATTR_DIR, b.name,
                             b.attr & RS_ATTR_DIR);
}

extern "C" int64_t DirEntryCompareNameCpp(const CDirEntry *a, const CDirEntry *b)
{
  return DirEntryCompareName(a->name, a->attr & RS_ATTR_DIR, b->name,
                             b->attr & RS_ATTR_DIR);
}

extern "C" int64_t DirEntryCompareClusCpp(const CDirEntry *a, const CDirEntry *b)
{
  return int64_t(uint64_t(a->clus) - uint64_t(b->clus));
}

static int64_t DirEntryCompareNameI64(int64_t a, int64_t b)
{
  return DirEntryCompareNameCpp(reinterpret_cast<const CDirEntry *>(a),
                                reinterpret_cast<const CDirEntry *>(b));
}

static int64_t DirEntryCompareClusI64(int64_t a, int64_t b)
{
  return DirEntryCompareClusCpp(reinterpret_cast<const CDirEntry *>(a),
                                reinterpret_cast<const CDirEntry *>(b));
}

static bool DirFilesSortKey(CDirEntry *&first, int64_t key)
{
  int64_t count = 0;
  for (auto *entry = first; entry; entry = entry->next)
    count++;
  if (count > 1) {
    auto *buf = reinterpret_cast<int64_t *>(
        MAlloc(count * sizeof(int64_t), CurrentTask()->data_heap));
    if (!buf)
      return false;
    int64_t i = 0;
    for (auto *entry = first; entry; entry = entry->next)
      buf[i++] = reinterpret_cast<int64_t>(entry);
    switch (key) {
      case 0:
        QSortI64Native(buf, count, DirEntryCompareNameI64);
        break;
      case 1:
        QSortI64Native(buf, count, DirEntryCompareClusI64);
        break;
    }
    auto **link = &first;
    for (i = 0; i < count; i++) {
      *link = reinterpret_cast<CDirEntry *>(buf[i]);
      link = &(*link)->next;
    }
    *link = nullptr;
    Free(buf);
  }
  for (auto *entry = first; entry; entry = entry->next)
    if (!DirFilesSortKey(entry->sub, key))
      return false;
  return true;
}

bool DirFilesSort(CDirEntry *&first, bool by_cluster)
{
  return DirFilesSortKey(first, by_cluster ? 1 : 0);
}

extern "C" void DirFilesSortCpp(CDirEntry **first, int64_t key)
{
  DirFilesSortKey(*first, key);
}

static CDirEntry **DirFilesFlatten(CDirEntry **tail, CDirEntry *entry,
                                   uint64_t flags)
{
  while (entry) {
    auto *next = entry->next;
    if ((flags & FUF_JUST_FILES) && (entry->attr & RS_ATTR_DIR)) {
      tail = DirFilesFlatten(tail, entry->sub, flags);
      DirEntryDelCpp(entry);
    } else {
      *tail = entry;
      tail = &entry->next;
      tail = DirFilesFlatten(tail, entry->sub, flags);
      entry->sub = nullptr;
    }
    entry = next;
  }
  return tail;
}

extern "C" CDirEntry *DirFilesFlattenCpp(CDirEntry *entry, CDirEntry **res,
                                         uint64_t flags)
{
  auto **tail = DirFilesFlatten(res, entry, flags);
  *tail = nullptr;
  // CDirEntry.next is the first member; HolyC returns the final link slot.
  return reinterpret_cast<CDirEntry *>(tail);
}

void DirFilesFlatten(CDirEntry *&first, uint64_t flags)
{
  DirFilesFlattenCpp(first, &first, flags);
}

extern "C" void PutFileLinkCpp(const char *filename, const char *full_name,
                               int64_t line, uint8_t plain_text)
{
  if (!filename)
    return;
  uint64_t args[5];
  args[0] = reinterpret_cast<uintptr_t>(filename);
  if (IsRawCpp()) {
    args[1] = line;
    PrintCpp(line ? "%s,%04d" : "%s", line ? 2 : 1, args);
    return;
  }
  bool address = filename[0] == 'A' && filename[1] && filename[2] == ':';
  char *abs = nullptr;
  if (address) {
    filename += 3;
    full_name = filename;
  } else if (!full_name) {
    abs = FileNameAbs(filename, *CurrentTask());
    full_name = abs;
  }
  const char *kind = address      ? (line ? "AL" : "AI")
                     : plain_text ? (line ? "PL" : "PI")
                                  : (line ? "FL" : "FI");
  args[0] = reinterpret_cast<uintptr_t>(filename);
  size_t n = 1;
  if (line)
    args[n++] = line;
  args[n++] = reinterpret_cast<uintptr_t>(kind);
  args[n++] = reinterpret_cast<uintptr_t>(full_name);
  if (line)
    args[n++] = line;
  PrintCpp(line ? "$LK,\"%s,%04d\",A=\"%s:%s,%d\"$" : "$LK,\"%s\",A=\"%s:%s\"$",
           n, args);
  Free(abs);
}

extern "C" void PutDirLinkCpp(const char *name, const char *full_name)
{
  if (!name)
    return;
  if (IsRawCpp()) {
    PutS(name);
    return;
  }
  char *abs = full_name ? nullptr : DirNameAbs(name, *CurrentTask());
  const uint64_t args[]{
      reinterpret_cast<uintptr_t>(name),
      reinterpret_cast<uintptr_t>(full_name ? full_name : abs)};
  PrintCpp("$MA,T=\"%s\",LM=\"Cd(\\\"%s\\\");Dir;\n\"$", 2, args);
  Free(abs);
}
