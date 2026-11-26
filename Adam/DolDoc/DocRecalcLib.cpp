#include "DolDoc.hpp"

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
    auto x = task->scroll_x, y = task->scroll_y, z = task->scroll_z;
    task->scroll_x = task->scroll_y = task->scroll_z = 0;
    DocRecalc(*doc, RECALCt_TO_SCRN);
    task->scroll_x = x;
    task->scroll_y = y;
    task->scroll_z = z;
  }
}
