#include "DolDoc.hpp"

extern "C" bool DocGoToLineCpp(CDoc *doc, int64_t line_num)
{
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL)
    return false;
  CDocLock lock(*doc);
  doc->x = 0;
  doc->y = uint64_t(line_num) - 1;
  DocRecalc(*doc, RECALCt_FIND_CURSOR);
  DocCenterCpp(doc, RECALCt_NORMAL);
  return uint64_t(doc->cur_entry->y) == uint64_t(line_num) - 1;
}

extern "C" bool DocFindCpp(CDoc *doc, int64_t start_line_num, const char *needle,
                           int64_t match)
{
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL)
    return false;
  CDocLock lock(*doc);
  auto *entry = doc->head.next;
  bool res = false;
  if (start_line_num != INT64_MIN) {
    if (DocGoToLineCpp(doc, start_line_num))
      entry = doc->cur_entry;
    else
      entry = &doc->head;
  }
  if (needle && match > 0)
    for (; entry != &doc->head; entry = entry->next)
      if ((entry->de_flags & DOCEF_TAG) && entry->tag) {
        auto *ptr = StrFindCpp(needle, entry->tag, SFF_IGNORE_CASE);
        if (ptr && !--match) {
          doc->cur_entry = entry;
          auto col = ptr - entry->tag;
          if (col < entry->min_col)
            col = entry->min_col;
          if (col > entry->max_col)
            col = entry->max_col;
          doc->cur_col = col;
          res = true;
          break;
        }
      }
  if (!res)
    DocBottomCpp(doc);
  DocCenterCpp(doc, RECALCt_NORMAL);
  return res;
}

extern "C" bool DocAnchorFindCpp(CDoc *doc, const char *needle)
{
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL)
    return false;
  CDocLock lock(*doc);
  bool res = false;
  if (needle)
    for (auto *entry = doc->head.next; entry != &doc->head; entry = entry->next)
      if (entry->Type() == DOCT_ANCHOR && (entry->de_flags & DOCEF_AUX_STR)) {
        auto *tag = reinterpret_cast<CDocEntry *>(entry)->aux_str;
        if (tag && !StrCmpCpp(reinterpret_cast<const uint8_t *>(tag),
                              reinterpret_cast<const uint8_t *>(needle))) {
          doc->cur_entry = entry;
          doc->cur_col = entry->min_col;
          res = true;
          break;
        }
      }
  if (!res)
    DocBottomCpp(doc);
  DocCenterCpp(doc, RECALCt_NORMAL);
  return res;
}
