#include "DolDoc.hpp"

// Copy and cut include both endpoints. The document head is the end sentinel.
extern "C" void DocCutCpp(CDoc *doc, CDocEntryBase *start, CDocEntryBase *end)
{
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL || !start || !end)
    return;
  CDocLock lock(*doc);
  auto *head = &doc->head;
  if (start != head && (start->last != end || end == head)) {
    do {
      auto *next = start->next;
      if (doc->cur_entry == start) {
        doc->cur_entry = next;
        doc->cur_col = next->min_col;
      }
      DocEntryDel(*doc, *start);
      if (start == end)
        break;
      start = next;
    } while (start != head);
  }
  for (auto *entry = head->next; entry != head;) {
    auto *next = entry->next;
    if (entry->Type() == DOCT_SOFT_NEW_LINE)
      DocEntryDel(*doc, *entry);
    entry = next;
  }
}

extern "C" CDoc *DocCopyCpp(CDoc *doc, CDocEntryBase *start, CDocEntryBase *end)
{
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL || !start || !end)
    return nullptr;
  auto *copy = DocNewCpp(nullptr, nullptr);
  if (!copy)
    return nullptr;
  CDocLock lock(*doc);
  auto *head = &doc->head;
  if (start != head && (start->last != end || end == head)) {
    do {
      const auto type = start->Type();
      if (type != DOCT_DATA && type != DOCT_HEX_ED && type != DOCT_CHECK_BOX) {
        auto *entry = DocEntryCopy(*copy, *start);
        if (!entry) {
          DocDelCpp(copy);
          return nullptr;
        }
        DocEntryInsert(*entry, copy->head);
      }
      if (start == end)
        break;
      start = start->next;
    } while (start != head);
  }
  return copy;
}
