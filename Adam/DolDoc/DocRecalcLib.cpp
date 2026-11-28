#include "DolDoc.hpp"

extern "C" int64_t DocMaxCpp(int64_t entries)
{
  if (auto *doc = DocPut()) {
    CDocLock lock(*doc);
    auto old = doc->max_entries;
    doc->max_entries = entries;
    return old;
  }
  return 0;
}

bool DocDelToNum(CDoc &doc, int64_t count)
{
  bool removed = false;
  for (auto *entry = doc.head.next; entry != &doc.head && count-- > 0;) {
    auto *next = entry->next;
    if (!(entry->de_flags & (DOCEF_HOLD | DOCEF_FILTER_SKIP))) {
      if (entry == doc.cur_entry) {
        doc.cur_entry = next;
        doc.cur_col = next->min_col;
      }
      DocEntryDel(doc, *entry);
      removed = true;
    }
    entry = next;
  }
  return removed;
}

void DocDelToEntry(CDoc &doc, CDocEntryBase *clear_entry, bool clear_holds)
{
  for (auto *entry = doc.head.next; entry != &doc.head;) {
    auto *next = entry->next;
    bool done = entry == clear_entry;
    if (clear_holds || !(entry->de_flags & (DOCEF_HOLD | DOCEF_FILTER_SKIP))) {
      if (doc.cur_entry == entry) {
        doc.cur_entry = next;
        doc.cur_col = next->min_col;
      }
      DocEntryDel(doc, *entry);
    }
    if (done)
      break;
    entry = next;
  }
}

extern "C" void DocCenterCpp(CDoc *doc, int64_t recalc_flags)
{
  if (!doc)
    doc = DocPut();
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL)
    return;
  CDocLock lock(*doc);
  DocRecalc(*doc, recalc_flags);
  if (!(doc->flags & DOCF_BORDER_DOC))
    doc->top_line_num = doc->y - ((doc->win_task->win_height + 1) >> 1);
}

extern "C" void DocBottomCpp(CDoc *doc)
{
  if (!doc)
    doc = DocPut();
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL)
    return;
  CDocLock lock(*doc);
  doc->cur_entry = &doc->head;
  doc->cur_col = 0;
  DocRecalc(*doc);
}

extern "C" void DocCollapseCpp(uint8_t collapse, CDoc *doc)
{
  if (!doc)
    doc = DocPut();
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL)
    return;
  CDocLock lock(*doc);
  for (auto *entry = doc->head.next; entry != &doc->head; entry = entry->next)
    if (entry->de_flags & DOCEF_TREE) {
      if (collapse)
        entry->de_flags |= DOCEF_CHECKED_COLLAPSED;
      else
        entry->de_flags &= ~DOCEF_CHECKED_COLLAPSED;
    }
  DocRecalc(*doc);
}

static bool DocShowFlag(bool show, CDoc *doc, uint64_t mask)
{
  if (!doc)
    doc = DocPut();
  if (!doc)
    return false;
  uint64_t old = show ? __atomic_fetch_and(&doc->flags, ~mask, __ATOMIC_RELAXED)
                      : __atomic_fetch_or(&doc->flags, mask, __ATOMIC_RELAXED);
  return !(old & mask);
}

extern "C" bool DocCursorCpp(bool show, CDoc *doc)
{
  return DocShowFlag(show, doc, DOCF_HIDE_CURSOR);
}

extern "C" bool DocHighlightCursorCpp(bool show, CDoc *doc)
{
  return DocShowFlag(show, doc, DOCF_DONT_HIGHLIGHT_CURSOR);
}

extern "C" bool DocScrollCpp(bool val, CDoc *doc)
{
  return DocShowFlag(val, doc, DOCF_NO_SCROLL_BARS);
}

extern "C" void DocClearCpp(CDoc *doc, uint8_t clear_holds)
{
  if (!doc)
    doc = DocPut();
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL)
    return;
  CDocLock lock(*doc);
  DocBottomCpp(doc);
  const char *clear = clear_holds ? "$CL+H$" : "$CL$";
  DocPutS(*doc, clear, StrLen(clear));
  DocRecalc(*doc);
}

void DocUpdateTaskDocs(CTask *task)
{
  if (task->title_src == TTS_TASK_NAME) {
    MemSet(task->task_title, 0, sizeof(task->task_title));
    MemCpy(task->task_title, task->task_name, sizeof(task->task_name));
  }
  if (auto *doc = DocDisplayCpp(task); doc && !(doc->flags & DOCF_DONT_SHOW))
    DocRecalc(*doc, RECALCt_TO_SCRN | RECALCF_HAS_CURSOR);
  if (auto *doc = DocBorderCpp(task); doc && !(doc->flags & DOCF_DONT_SHOW)) {
    CD3I64 saved_scroll;
    WinScrollNullCpp(task, &saved_scroll);
    DocRecalc(*doc, RECALCt_TO_SCRN);
    WinScrollRestoreCpp(task, &saved_scroll);
  }
}
