#include "AHash.hpp"
#include "DolDoc/DolDoc.hpp"

extern "C" int64_t HashTypeNumCpp(const CHash *entry);
extern "C" uint64_t HashValCpp(const CHash *entry);
extern "C" int64_t PopUpCpp(const char *src, CTask *parent, CTask **popup);

extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);

static int64_t StrCmp(const char *a, const char *b)
{
  return StrCmpCpp(reinterpret_cast<const uint8_t *>(a),
                   reinterpret_cast<const uint8_t *>(b));
}

struct CWho
{
  const CHash *h;
  char *idx;
};

static_assert(sizeof(CWho) == 16 && offsetof(CWho, idx) == 8);

static int64_t HashEntriesCompare(const void *a, const void *b)
{
  auto &h1 = *static_cast<const CWho *>(a), &h2 = *static_cast<const CWho *>(b);
  if (auto i = StrCmp(h1.h->str, h2.h->str))
    return i;
  return HashTypeNumCpp(h1.h) - HashTypeNumCpp(h2.h);
}

static int64_t HashEntriesCompare2(const void *a, const void *b)
{
  auto &h1 = *static_cast<const CWho *>(a), &h2 = *static_cast<const CWho *>(b);
  const auto i1 = HashValCpp(h1.h), i2 = HashValCpp(h2.h);
  if (i1 == i2) {
    auto type = HashTypeNumCpp(h1.h) - HashTypeNumCpp(h2.h);
    return type ? type : StrCmp(h1.h->str, h2.h->str);
  }
  return int64_t(i1 - i2);
}

static int64_t HelpIndexCnt(const char *ptr, const char *idx)
{
  int64_t cnt = 0;
  const auto len = StrLen(idx);
  while (*ptr) {
    if (!StrNCmpCpp(ptr, idx, len))
      cnt++;
    while (*ptr && *ptr++ != ';') {
    }
  }
  return cnt;
}

static char *HelpIndexStr(const char **_ptr, const char *idx)
{
  const auto len = StrLen(idx);
  auto *ptr = *_ptr;
  while (*ptr) {
    auto *first = ptr;
    while (*ptr && *ptr != ';')
      ptr++;
    *_ptr = *ptr ? ptr + 1 : ptr;
    if (!StrNCmpCpp(first, idx, len)) {
      const auto size = ptr - first;
      auto *res =
          reinterpret_cast<char *>(MAlloc(size + 1, CurrentTask()->data_heap));
      if (res) {
        MemCpy(res, first, size);
        res[size] = 0;
      }
      return res;
    }
    ptr = *_ptr;
  }
  return nullptr;
}

// Read the source at the same cursor position used by the hidden Plain editor.
static char *HelpComment(const CHash &tmph, const char *src_link)
{
  auto *link = StrNew(src_link, CurrentTask()->data_heap);
  if (!link)
    return nullptr;
  if (StrLen(link) > 2 && *link == 'F' && link[2] == ':')
    *link = 'P';
  char *filename, *needle;
  int64_t num;
  const auto type = EdLinkCvtCpp(link, &filename, &needle, &num, 0);
  Free(link);
  CDoc *doc = nullptr;
  if (filename &&
      (type == LK_PLAIN || type == LK_PLAIN_LINE || type == LK_PLAIN_FIND)) {
    doc = DocReadCpp(filename, DOCF_PLAIN_TEXT | DOCF_NO_CURSOR);
    if (doc && type == LK_PLAIN_LINE)
      DocGoToLineCpp(doc, num);
    else if (doc && type == LK_PLAIN_FIND)
      DocFindCpp(doc, INT64_MIN, needle, num);
  }
  Free(filename);
  Free(needle);
  if (!doc)
    return nullptr;

  auto *entry = doc->cur_entry;
  if (tmph.type & HTT_FUN) {
    auto *fun = reinterpret_cast<const CHashFun *>(&tmph);
    const char delim = fun->cls.flags & (FF__EXTERN | FF_INTERNAL) ? ';' : '{';
    while (entry != &doc->head &&
           (!(entry->de_flags & DOCEF_TAG) || !entry->tag ||
            !StrOccCpp(reinterpret_cast<const uint8_t *>(entry->tag), delim)))
      entry = entry->next;
  }
  char *res = nullptr;
  if (entry != &doc->head) {
    size_t skip = 0;
    if ((entry->de_flags & DOCEF_TAG) && entry->tag) {
      const auto *first = entry->tag;
      auto *start = StrMatchCpp("//", first);
      if (!start)
        start = StrMatchCpp("/*", first);
      if (start)
        skip = start - first + 2;
      else if (!StrNCmpCpp(first, "public", 6))
        skip = 6;
    }
    res = DocScanLineCpp(doc, entry, nullptr, nullptr);
    if (res) {
      if (skip)
        StrCpyCpp(res, res + skip);
      StrUtilCpp(res, SUF_REM_LEADING | SUF_REM_TRAILING | SUF_SINGLE_SPACE);
    }
  }
  DocDelCpp(doc);
  return res;
}

static int64_t HashEntriesCompare3(const void *a, const void *b)
{
  auto &h1 = *static_cast<const CWho *>(a), &h2 = *static_cast<const CWho *>(b);
  if (auto i = StrCmp(h1.idx, h2.idx))
    return i;
  const int64_t i1 = bool(h1.h->type & HTT_HELP_FILE);
  const int64_t i2 = bool(h2.h->type & HTT_HELP_FILE);
  return i1 != i2 ? i2 - i1 : StrCmp(h1.h->str, h2.h->str);
}

static int64_t HashEntrySize(const CHash &entry)
{
  const auto type = entry.type & HTG_TYPE_MASK;
  switch (type & (0 - type)) {
  case HTT_DEFINE_STR:
    return MSize(reinterpret_cast<const CHashDefineStr *>(&entry)->data);
  case HTT_GLBL_VAR:
    return reinterpret_cast<const CHashGlblVar *>(&entry)->size;
  case HTT_CLASS:
  case HTT_INTERNAL_TYPE:
    return reinterpret_cast<const CHashClass *>(&entry)->size;
  case HTT_FUN: {
    auto *dbg = reinterpret_cast<const CDbgInfo *>(
        reinterpret_cast<const CHashSrcSym *>(&entry)->dbg_info);
    if (dbg && dbg->max_line >= dbg->min_line)
      return int64_t(dbg->body[dbg->max_line + 1 - dbg->min_line]) -
             dbg->body[0];
    return -1;
  }
  case HTT_MODULE:
    if (!StrCmp(entry.str, "/Kernel/Kernel")) {
      auto *header = reinterpret_cast<const CBinFile *>(uintptr_t(SYS_KERNEL) -
                                                        sizeof(CBinFile));
      return header->file_size;
    }
    [[fallthrough]];
  case HTT_FILE:
    return MSize(reinterpret_cast<const void *>(
        reinterpret_cast<const CHashGeneric *>(&entry)->user_data0));
  case HTT_WORD:
    return StrLen(entry.str);
  default:
    return -1;
  }
}

static bool WhoMatch(const CHash &entry, bool publics)
{
  return !(entry.type & (HTF_IMPORT | HTF_PRIVATE)) &&
         (!publics || (entry.type & HTF_PUBLIC));
}

static void WhoPrint(CDoc *doc, const CHash &ptr, bool indexed, bool publics)
{
  auto *sym = ptr.type & HTG_SRC_SYM
                  ? reinterpret_cast<const CHashSrcSym *>(&ptr)
                  : nullptr;
  const uint64_t name = reinterpret_cast<uintptr_t>(ptr.str);
  char *comment = nullptr;
  if (sym && sym->src_link) {
    const uint64_t args[]{name, reinterpret_cast<uintptr_t>(sym->src_link)};
    DocPrintCpp(doc, "$LK,\"%-20s\",A=\"%s\"$", 2, args);
    if (indexed)
      comment = HelpComment(ptr, sym->src_link);
  } else
    DocPrintCpp(doc, "%-20s", 1, &name);

  char buf[512]{};
  if (!indexed) {
    if (ptr.type & HTT_DEFINE_STR) {
      auto *value = reinterpret_cast<const CHashDefineStr *>(&ptr);
      const uint64_t args[]{reinterpret_cast<uintptr_t>(value->data),
                            uint64_t(value->cnt)};
      StrPrintJoin(buf, value->cnt == -1 ? "%-10t$Q   " : "%-10t$Q %02X",
                   value->cnt == -1 ? 1 : 2, args);
    } else {
      const uint64_t value =
          ptr.type & HTT_GLBL_VAR
              ? reinterpret_cast<uintptr_t>(
                    reinterpret_cast<const CHashGlblVar *>(&ptr)->data_addr)
              : HashValCpp(&ptr);
      StrPrintJoin(buf, "%010X   ", 1, &value);
    }
    const auto size = HashEntrySize(ptr);
    const uint64_t args[]{ptr.use_cnt, uint64_t(size)};
    CatPrintCpp(buf, size == -1 ? " %04X            " : " %04X %010X ",
                size == -1 ? 1 : 2, args);
  }

  uint32_t type = publics ? ptr.type & ~HTF_PUBLIC : ptr.type;
  if (!(type & HTG_TYPE_MASK))
    CatPrintCpp(buf, "NULL ", 0, nullptr);
  const uint64_t types = reinterpret_cast<uintptr_t>("ST_HTT_TYPES");
  while (type) {
    const uint64_t args[]{uint64_t(__builtin_ctz(type)), types};
    CatPrintCpp(buf, "%Z ", 2, args);
    type &= type - 1;
  }
  const uint64_t text = reinterpret_cast<uintptr_t>(buf);
  DocPrintCpp(doc, "%s", 1, &text);
  if (comment) {
    const uint64_t arg = reinterpret_cast<uintptr_t>(comment);
    DocPrintCpp(doc, "$GREEN$%s$FG$", 1, &arg);
    Free(comment);
  }
  DocPrintCpp(doc, "\n", 0, nullptr);
}

extern "C" void WhoCpp(const char *fu_flags, CHashTable *h, const char *idx,
                       CDoc *doc)
{
  uint64_t f = FUF_RECURSE;
  auto *flags =
      reinterpret_cast<const uint8_t *>(DefineCpp("ST_FILE_UTIL_FLAGS"));
  ScanFlagsCpp(reinterpret_cast<uint8_t *>(&f), flags, fu_flags);
  if (f & ~(FUF_RECURSE | FUF_PUBLIC | FUF_MAP))
    RaiseHolyC(0x465546); // 'FUF'
  const bool recurse = f & FUF_RECURSE, publics = f & FUF_PUBLIC,
             map = f & FUF_MAP;
  if (!h)
    h = CurrentTask()->hash_table;
  int64_t cnt = 0;
  for (auto *table = h; table; table = recurse ? table->next : nullptr)
    for (int64_t i = 0; i <= table->mask; i++)
      for (auto *ptr = table->body[i]; ptr; ptr = ptr->next)
        if (WhoMatch(*ptr, publics)) {
          if (!idx)
            cnt++;
          else if (ptr->type & HTG_SRC_SYM)
            if (auto *cur_idx = reinterpret_cast<CHashSrcSym *>(ptr)->idx)
              cnt += HelpIndexCnt(cur_idx, idx);
        }

  auto *lst = cnt && uint64_t(cnt) <= SIZE_MAX / sizeof(CWho)
                  ? reinterpret_cast<CWho *>(
                        CAlloc(cnt * sizeof(CWho), CurrentTask()->data_heap))
                  : nullptr;
  if (lst) {
    int64_t j = 0;
    for (auto *table = h; table; table = recurse ? table->next : nullptr)
      for (int64_t i = 0; i <= table->mask && j < cnt; i++)
        for (auto *ptr = table->body[i]; ptr && j < cnt; ptr = ptr->next)
          if (WhoMatch(*ptr, publics)) {
            if (!idx)
              lst[j++].h = ptr;
            else if (ptr->type & HTG_SRC_SYM)
              if (auto *cur_idx = reinterpret_cast<CHashSrcSym *>(ptr)->idx)
                while (j < cnt) {
                  auto *group = HelpIndexStr(&cur_idx, idx);
                  if (!group)
                    break;
                  lst[j++] = {ptr, group};
                }
          }
    cnt = j;
    QSortNative(lst, cnt, sizeof(CWho),
                map   ? HashEntriesCompare2
                : idx ? HashEntriesCompare3
                      : HashEntriesCompare);

    auto &progress = sys_progresses[0];
    if (idx) {
      progress.max = cnt;
      progress.val = 0;
    }
    const char *last_idx = "";
    for (int64_t i = 0; i < cnt; i++) {
      auto *cur_idx = lst[i].idx;
      auto &ptr = *lst[i].h;
      if (cur_idx && StrCmp(cur_idx, last_idx)) {
        last_idx = cur_idx;
        if (i)
          DocPrintCpp(doc, "\n\n", 0, nullptr);
        const uint64_t arg = reinterpret_cast<uintptr_t>(cur_idx);
        DocPrintCpp(doc, "$WW,0$$PURPLE$$TX+CX,\"%$Q\"$$FG$\n", 1, &arg);
      }
      if (idx && (ptr.type & HTT_HELP_FILE)) {
        DocPrintCpp(doc, "$WW,1$", 0, nullptr);
        DocTypeCpp(doc, ptr.str, 1);
        DocPrintCpp(doc, "$WW,0$", 0, nullptr);
      } else
        WhoPrint(doc, ptr, idx, publics);
      if (idx)
        progress.val++;
    }
    for (int64_t i = 0; i < cnt; i++)
      Free(lst[i].idx);
    Free(lst);
    if (idx)
      progress.val = progress.max = 0;
  }
  if (doc) {
    if (doc->head.next == &doc->head)
      DocPrintCpp(doc, "No Match", 0, nullptr);
    else
      DocRecalc(*doc);
  }
}

extern "C" int64_t HashDepthRepCpp(CHashTable *table)
{
  if (!table)
    table = CurrentTask()->hash_table;
  constexpr int64_t HDR_NUM = 16;
  int64_t longest = 0, cnt = 0, a[HDR_NUM]{};
  for (int64_t i = 0; i <= table->mask; i++)
    if (auto *ptr = table->body[i]) {
      const auto j = LinkedLstCntCpp(ptr);
      if (j < HDR_NUM)
        a[j]++;
      if (j > longest)
        longest = j;
      cnt += j;
    }
  PrintCpp("Histogram\n", 0, nullptr);
  for (int64_t i = 0; i < HDR_NUM; i++)
    if (a[i]) {
      const uint64_t args[]{uint64_t(i), uint64_t(a[i])};
      PrintCpp("%02d:%d\n", 2, args);
    }
  const uint64_t args[]{uint64_t(table->mask) + 1, uint64_t(cnt),
                        uint64_t(longest)};
  PrintCpp("Size:%d Count:%d Longest:%d\n", 3, args);
  return longest;
}

extern "C" void DocHelpIdxCpp(CDoc *doc, const char *idx)
{
  WhoCpp("+p", nullptr, idx, doc);
}

extern "C" void PopUpHelpIndexCpp(const char *idx, CTask *parent)
{
  const uint64_t arg = reinterpret_cast<uintptr_t>(idx);
  auto *buf = MStrPrintCpp("DocHelpIdx(DocPut,\"%s\");View;", 1, &arg);
  if (buf) {
    PopUpCpp(buf, parent, nullptr);
    Free(buf);
  }
}

extern "C" void MapFileLoadCpp(const char *filename)
{
  auto *name = filename ? ExtDftCpp(filename, "MAP.Z") : nullptr;
  if (!name)
    return;
  auto *task = CurrentTask();
  auto *absname = FileNameAbs(name, *task);
  auto *doc = DocReadCpp(name, 0);
  Free(name);
  uint64_t base = 0;
  if (absname) {
    FileExtRemCpp(absname, nullptr);
    if (StrLen(absname) > 2 && absname[1] == ':')
      if (auto *module =
              HashSingleTableFind(absname + 2, task->hash_table, HTT_MODULE))
        base = reinterpret_cast<CHashGeneric *>(module)->user_data0 +
               sizeof(CBinFile);
    Free(absname);
  }
  if (!doc)
    return;
  for (auto *entry = doc->head.next; entry != &doc->head; entry = entry->next) {
    if (entry->Type() != DOCT_LINK)
      continue;
    auto *doc_e = reinterpret_cast<CDocEntry *>(entry);
    const bool tagged = entry->tag && *entry->tag;
    auto *link = entry->de_flags & DOCEF_AUX_STR ? doc_e->aux_str : nullptr;
    auto *st = tagged || link ? MStrUtilCpp(tagged ? entry->tag : link,
                                            SUF_REM_TRAILING, 0)
                              : nullptr;
    if (!st)
      continue;
    auto *sym = reinterpret_cast<CHashSrcSym *>(
        HashSingleTableFind(st, task->hash_table, HTG_SRC_SYM));
    Free(st);
    if (!sym)
      continue;
    if (tagged) {
      Free(const_cast<char *>(sym->src_link));
      sym->src_link = link;
      doc_e->aux_str = nullptr;
      if (link && StrLen(link) >= 4 && link[2] == ':') {
        if (link[3] == ':')
          link[3] = blkdev.boot_drv_let;
        else if (link[3] == '~')
          link[3] = *blkdev.home_dir;
      }
    }
    auto *bin = entry->de_flags & DOCEF_HAS_BIN ? doc_e->bin_data : nullptr;
    if (!(sym->hash.type & (HTT_FUN | HTT_EXPORT_SYS_SYM)) || sym->dbg_info ||
        !bin || !bin->data)
      continue;
    auto *dbg_info = reinterpret_cast<CDbgInfo *>(bin->data);
    const auto size =
        bin->size >= offsetof(CDbgInfo, body) && bin->size <= MSize(dbg_info)
            ? dbg_info->Size()
            : 0;
    if (!size || size > bin->size) {
      PrintCpp("Corrupt Map Entry\n", 0, nullptr);
      continue;
    }
    bin->data = nullptr;
    sym->dbg_info = dbg_info;
    auto *body = reinterpret_cast<uint32_t *>(
        reinterpret_cast<uint8_t *>(dbg_info) + offsetof(CDbgInfo, body));
    for (size_t i = 0; i < (size - offsetof(CDbgInfo, body)) / sizeof(*body);
         i++)
      if (body[i])
        body[i] += uint32_t(base);
  }
  DocDelCpp(doc);
}
