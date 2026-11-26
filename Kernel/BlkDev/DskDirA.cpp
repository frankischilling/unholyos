#include "../KernelB.hpp"

extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);

extern "C" void DirEntryDelCpp(CDirEntry *entry)
{
  if (entry) {
    Free(entry->full_name);
    Free(entry);
  }
}

extern "C" void DirEntryDel2Cpp(CDirEntry *entry)
{
  if (entry)
    Free(reinterpret_cast<void *>(entry->user_data));
  DirEntryDelCpp(entry);
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
  auto ext_cmp = StrCmpCpp(reinterpret_cast<const uint8_t *>(ext_a ? ext_a+1 : ""),
                           reinterpret_cast<const uint8_t *>(ext_b ? ext_b+1 : ""));
  if (ext_cmp)
    return ext_cmp;
  size_t len_a = ext_a ? size_t(ext_a - a) : StrLen(a);
  size_t len_b = ext_b ? size_t(ext_b - b) : StrLen(b);
  for (size_t i = 0; i < len_a && i < len_b; i++)
    if (a[i] != b[i])
      return uint8_t(a[i]) - uint8_t(b[i]);
  return (len_a > len_b) - (len_a < len_b);
}

int64_t DirEntryCompareName(const CRedSeaDirEntry &a, const CRedSeaDirEntry &b)
{
  return DirEntryCompareName(a.name, a.attr & RS_ATTR_DIR, b.name,
                             b.attr & RS_ATTR_DIR);
}

static int64_t DirEntryCompareNameI64(int64_t a, int64_t b)
{
  auto *first = reinterpret_cast<const CDirEntry *>(a);
  auto *second = reinterpret_cast<const CDirEntry *>(b);
  return DirEntryCompareName(first->name, first->attr & RS_ATTR_DIR,
                             second->name, second->attr & RS_ATTR_DIR);
}

static int64_t DirEntryCompareClus(int64_t a, int64_t b)
{
  auto x = reinterpret_cast<const CDirEntry *>(a)->clus;
  auto y = reinterpret_cast<const CDirEntry *>(b)->clus;
  return (x > y) - (x < y);
}

bool DirFilesSort(CDirEntry *&first, bool by_cluster)
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
    QSortI64Native(buf, count,
                   by_cluster ? DirEntryCompareClus : DirEntryCompareNameI64);
    auto **link = &first;
    for (i = 0; i < count; i++) {
      *link = reinterpret_cast<CDirEntry *>(buf[i]);
      link = &(*link)->next;
    }
    *link = nullptr;
    Free(buf);
  }
  for (auto *entry = first; entry; entry = entry->next)
    if (!DirFilesSort(entry->sub, by_cluster))
      return false;
  return true;
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
      if (flags & FUF_JUST_FILES)
        entry->parent = nullptr;
      tail = DirFilesFlatten(tail, entry->sub, flags);
      entry->sub = nullptr;
    }
    entry = next;
  }
  return tail;
}

void DirFilesFlatten(CDirEntry *&first, uint64_t flags)
{
  *DirFilesFlatten(&first, first, flags) = nullptr;
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
