#include "DolDoc.hpp"
#include "../God/GodBible.hpp"

extern "C" void DocEdHC(), DocEntryRunHC(), DocWriteHC();

extern "C" char *DocEntryLinkCpp(CDoc *doc, CDocEntryBase *entry)
{
  if (!doc || !entry || !(entry->de_flags & DOCEF_LINK))
    return nullptr;
  const char *st = nullptr;
  if (entry->de_flags & DOCEF_AUX_STR)
    st = reinterpret_cast<const CDocEntry *>(entry)->aux_str;
  else if (entry->de_flags & DOCEF_TAG)
    st = entry->tag;
  return st ? StrNew(st, MemTaskHeap(doc->mem_task)) : nullptr;
}

bool DocFileEd(int64_t type, const char *filename, const char *needle,
               int64_t *num, int64_t edf_dof_flags)
{
  const bool in_memory = LK_DOC <= type && type <= LK_DOC_LINE;
  uint64_t flags = 0;
  switch (type) {
  case LK_PLAIN:      flags = DOCF_PLAIN_TEXT | DOCF_NO_CURSOR; [[fallthrough]];
  case LK_DOC:        type = LK_FILE; break;
  case LK_DOC_ANCHOR: type = LK_FILE_ANCHOR; break;
  case LK_PLAIN_FIND: flags = DOCF_PLAIN_TEXT | DOCF_NO_CURSOR; [[fallthrough]];
  case LK_DOC_FIND:   type = LK_FILE_FIND; break;
  case LK_PLAIN_LINE: flags = DOCF_PLAIN_TEXT | DOCF_NO_CURSOR; [[fallthrough]];
  case LK_DOC_LINE:   type = LK_FILE_LINE; break;
  case LK_BIBLE_FIND: flags = DOCF_PLAIN_TEXT | DOCF_NO_CURSOR; break;
  }
  auto *task = CurrentTask();
  const auto old_border_src = task->border_src;
  bool res = true, other_found = false;
  CDoc *doc;
  if (in_memory) {
    doc = reinterpret_cast<CDoc *>(Str2I64Cpp(filename, 10, nullptr));
    if (!ChkPtrRange(doc, sizeof(*doc)))
      return false;
  } else {
    const char *base = filename;
    for (const auto *s = filename; *s; s++)
      if (*s == '/')
        base = s + 1;
    if (!FileNameChk(base))
      return false;
    constexpr auto find_parents = FUF_JUST_FILES | FUF_Z_OR_NOT_Z |
                                  FUF_SCAN_PARENTS;
    if (edf_dof_flags & EDF_BAIL)
      res = FileFindCpp(filename, nullptr, find_parents);
    else if (!(res = FileFindCpp(filename, nullptr, FUF_JUST_FILES)))
      other_found = FileFindCpp(filename, nullptr, find_parents);
    doc = DocReadCpp(filename, flags);
    if (doc) {
      doc->desc = 0x74696445; // Edit
      task->border_src = 2; // BDS_ED_FILENAME_DRV
    }
  }
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL)
    return false;
  if (edf_dof_flags & EDF_COLLAPSE)
    DocCollapseCpp(true, doc);
  else if (edf_dof_flags & EDF_UNCOLLAPSE)
    DocCollapseCpp(false, doc);
  if (res || other_found)
    switch (type) {
    case LK_FILE_LINE:   res = DocGoToLineCpp(doc, *num); break;
    case LK_FILE_ANCHOR: res = DocAnchorFindCpp(doc, needle); break;
    case LK_FILE_FIND:   res = DocFindCpp(doc, INT64_MIN, needle, *num); break;
    case LK_BIBLE_FIND:  res = DocFindCpp(doc, *num, needle, 1); break;
    default:             DocCenterCpp(doc, RECALCt_NORMAL); break;
    }
  *num = doc->cur_entry->y + 1;
  if (edf_dof_flags & EDF_WAS_WRITE)
    res = false;
  if (!(edf_dof_flags & EDF_BAIL)) {
    auto *old_dirc = doc->filename.dirc;
    doc->filename.dirc = *doc->filename.name
        ? DirContextNewCpp(doc->filename.name, false, false, false) : nullptr;
    const auto arg = reinterpret_cast<uintptr_t>(doc);
    uint64_t accepted;
    bool okay = CallHolyC2Checked(reinterpret_cast<uintptr_t>(DocEdHC), arg,
                                  edf_dof_flags | DOF_DONT_HOME, &accepted);
    if (okay && accepted) {
      DocLock(*doc);
      uint64_t action;
      if (doc->cur_entry != &doc->head)
        okay = CallHolyC4Checked(reinterpret_cast<uintptr_t>(DocEntryRunHC),
                                 arg, reinterpret_cast<uintptr_t>(doc->cur_entry),
                                 true, 0, &action);
      DocUnlock(*doc);
      if (okay && !in_memory) {
        okay = CallHolyC2Checked(reinterpret_cast<uintptr_t>(DocWriteHC),
                                 arg, 0, &action);
        if (edf_dof_flags & EDF_WAS_WRITE)
          res = okay;
      }
    }
    if (!okay)
      res = false;
    DirContextDelCpp(doc->filename.dirc, true);
    doc->filename.dirc = old_dirc;
  }
  if (!in_memory)
    DocDelCpp(doc);
  task->border_src = old_border_src;
  return res;
}

extern "C" int64_t EdLinkCvtCpp(const char *link_st, char **_filename,
                               char **_needle_str, int64_t *_num,
                               int64_t edf_dof_flags)
{
  auto *task = CurrentTask();
  char *filename = nullptr, *needle_str = nullptr;
  int64_t num = 1, res = -1;
  if (!link_st || !*link_st)
    link_st = edf_dof_flags & EDF_BAIL ? nullptr : blkdev.tmp_filename;
  auto *st = link_st ? StrNew(link_st, task->data_heap) : nullptr;
  if (st) {
    char *filename2 = st;
    res = LK_FILE;
    if (StrLen(st) > 3 && st[2] == ':') {
      constexpr const char *types[]{
          "FI", "FA", "FF", "FL", "MN", "PI", "PF", "PL", "BF", "DN",
          "HI", "AD", "AI", "AA", "AF", "AL"};
      res = -1;
      for (size_t i = 0; i < sizeof(types) / sizeof(*types); i++)
        if (!StrNICmpCpp(types[i], st, 2)) {
          res = i;
          break;
        }
      filename2 = st + 3;
      if (res == LK_MAN_PAGE) {
        auto *sym = reinterpret_cast<CHashSrcSym *>(
            HashFind(filename2, task->hash_table, HTG_SRC_SYM));
        res = sym && sym->src_link && *sym->src_link
                  ? EdLinkCvtCpp(sym->src_link, &filename, &needle_str, &num,
                                 edf_dof_flags)
                  : -1;
        filename2 = nullptr;
      } else if (res == LK_HELP_INDEX || res == LK_ADDR)
        res = -1;
      switch (res) {
      case LK_BIBLE_FIND: {
        if (auto *ptr = StrLastOccCpp(filename2,
                                      reinterpret_cast<const uint8_t *>(","))) {
          *ptr++ = 0;
          for (auto *end = ptr; *end; end++)
            if (*end == '-') {
              *end = 0;
              break;
            }
          needle_str = StrNew(ptr, task->data_heap);
        }
        const auto book = DefineMatchCpp(reinterpret_cast<const uint8_t *>(filename2),
                                         "ST_BIBLE_BOOKS", 1); // LMF_IGNORE_CASE
        if (book < 0)
          res = -1;
        else {
          num = Str2I64Cpp(DefineSubCpp(book, "ST_BIBLE_BOOK_LINES"), 10, nullptr);
          filename = FileNameAbs(BIBLE_FILENAME, *task, FUF_Z_OR_NOT_Z);
        }
        filename2 = nullptr;
        break;
      }
      case LK_DEF:
        num = -1;
        if (auto *ptr = StrLastOccCpp(filename2,
                                    reinterpret_cast<const uint8_t *>(","))) {
          *ptr++ = 0;
          num = Str2I64Cpp(ptr, 10, nullptr);
        }
        filename = StrNew(filename2, task->data_heap);
        filename2 = nullptr;
        break;
      case LK_FILE_LINE:
      case LK_PLAIN_LINE:
      case LK_DOC_LINE:
      case LK_FILE_ANCHOR:
      case LK_DOC_ANCHOR:
      case LK_FILE_FIND:
      case LK_PLAIN_FIND:
      case LK_DOC_FIND:
        if (filename2) {
          auto *ptr = StrLastOccCpp(filename2,
                                    reinterpret_cast<const uint8_t *>(","));
          if (!ptr)
            break;
          *ptr++ = 0;
          if (res == LK_FILE_LINE || res == LK_PLAIN_LINE || res == LK_DOC_LINE)
            num = Str2I64Cpp(ptr, 10, nullptr);
          else {
            needle_str = StrNew(ptr, task->data_heap);
            if (needle_str && res != LK_FILE_ANCHOR && res != LK_DOC_ANCHOR)
              if (auto *suffix = StrLastOccCpp(
                      needle_str, reinterpret_cast<const uint8_t *>(":"))) {
                *suffix++ = 0;
                num = Str2I64Cpp(suffix, 10, nullptr);
              }
          }
        }
        break;
      }
    }
    if (res >= 0 && filename2)
      filename = LK_DOC <= res && res <= LK_DOC_LINE
                     ? StrNew(filename2, task->data_heap)
                     : FileNameAbs(filename2, *task, FUF_Z_OR_NOT_Z);
    Free(st);
  }
  if (_filename)
    *_filename = filename;
  else
    Free(filename);
  if (_needle_str)
    *_needle_str = needle_str;
  else
    Free(needle_str);
  if (_num)
    *_num = num;
  return res;
}
