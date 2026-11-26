#include "DolDoc.hpp"

extern "C" int64_t DocGetKeyCpp(int64_t *sc)
{
  int64_t ch, scan_code;
  do {
    ch = GetKeyCpp(&scan_code, true, false);
  } while (scan_code & SCF_KEY_DESC);
  if (auto *doc = DocPut(); doc && DocRecalc(*doc)) {
    auto *task = doc->win_task;
    if (doc->y < doc->top_line_num)
      doc->top_line_num = doc->y;
    else if (task->win_height && doc->y >= doc->top_line_num + task->win_height)
      doc->top_line_num = doc->y - task->win_height + 1;
    if (doc->x < doc->line_start_col)
      doc->line_start_col = doc->x;
    else if (task->win_width && doc->x >= doc->line_start_col + task->win_width)
      doc->line_start_col = doc->x - task->win_width + 1;
  }
  if (sc)
    *sc = scan_code;
  return ch;
}

extern "C" char *DocScanLineCpp(CDoc *doc, CDocEntryBase *entry,
                                int64_t *cur_col, CDocEntryBase **end)
{
  if (cur_col)
    *cur_col = -1;
  if (end)
    *end = nullptr;
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL || !entry)
    return nullptr;
  CDocLock lock(*doc);
  CDocOut count;
  for (auto *e = entry; e != &doc->head && e->Type() != DOCT_NEW_LINE;
       e = e->next) {
    if ((e->de_flags & DOCEF_TAG) && e->tag)
      count.Put(e->tag);
    else if (e->Type() == DOCT_TAB)
      count.Put('\t');
    else if (e->Type() == DOCT_SHIFTED_X || e->Type() == DOCT_SHIFTED_Y)
      count.len += 6 + (reinterpret_cast<CDocEntry *>(e)->attr < 0);
  }
  auto *res = MAlloc(count.len + 1, CurrentTask()->data_heap);
  if (!res)
    return nullptr;
  CDocOut out(res);
  for (; entry != &doc->head && entry->Type() != DOCT_NEW_LINE;
       entry = entry->next) {
    if (cur_col && entry == doc->cur_entry)
      *cur_col = out.len + doc->cur_col;
    if ((entry->de_flags & DOCEF_TAG) && entry->tag)
      out.Put(entry->tag);
    else if (entry->Type() == DOCT_TAB)
      out.Put('\t');
    else if (entry->Type() == DOCT_SHIFTED_X ||
             entry->Type() == DOCT_SHIFTED_Y) {
      auto attr = reinterpret_cast<CDocEntry *>(entry)->attr;
      out.Put(entry->Type() == DOCT_SHIFTED_X ? "$SX," : "$SY,");
      if (attr < 0)
        out.Put('-');
      uint64_t magnitude = attr < 0 ? 0 - uint64_t(attr) : uint64_t(attr);
      out.Put(static_cast<char>('0' + magnitude));
      out.Put('$');
    }
  }
  if (end)
    *end = entry;
  if (cur_col && entry == doc->cur_entry && !doc->cur_col)
    *cur_col = out.len;
  out.Put('\0');
  return reinterpret_cast<char *>(res);
}

char *DocGetStr2(int64_t flags)
{
  auto *doc = DocPut();
  if (!doc)
    return nullptr;
  DocPutS(*doc, "$PT$", 4);
  int64_t ch, sc;
  do {
    ch = DocGetKeyCpp(&sc);
    if (ch == CH_SHIFT_ESC) {
      if (flags & GSF_SHIFT_ESC_EXIT)
        Exit();
      DocPutS(*doc, "$PT$$FG$$BG$", 12);
      return StrNew("", CurrentTask()->data_heap);
    }
    if (ch == CH_ESC && (flags & GSF_WITH_NEW_LINE))
      break;
  } while (ch != '\n' || (flags & GSF_WITH_NEW_LINE) ||
           (sc & (SCF_CTRL | SCF_SHIFT)) == (SCF_CTRL | SCF_SHIFT));

  CDocLock lock(*doc);
  auto *entry = doc->cur_entry->last;
  while (entry != &doc->head && entry->Type() != DOCT_PMT &&
         ((flags & GSF_WITH_NEW_LINE) || entry->Type() != DOCT_NEW_LINE))
    entry = entry->last;
  if (!(flags & GSF_WITH_NEW_LINE) && entry->Type() == DOCT_NEW_LINE) {
    entry = entry->last;
    while (entry != &doc->head && entry->Type() != DOCT_PMT &&
           entry->Type() != DOCT_NEW_LINE)
      entry = entry->last;
  }
  CDocEntryBase *end;
  auto *res = DocScanLineCpp(doc, entry->next, nullptr, &end);
  while (res && (flags & GSF_WITH_NEW_LINE) && end != &doc->head &&
         end->Type() == DOCT_NEW_LINE) {
    auto *next = DocScanLineCpp(doc, end->next, nullptr, &end);
    uint64_t args[]{reinterpret_cast<uintptr_t>(res),
                    reinterpret_cast<uintptr_t>(next)};
    auto *joined = next ? MStrPrintCpp("%s\n%s", 2, args) : nullptr;
    Free(next);
    Free(res);
    res = joined;
  }
  DocPutS(*doc, "$PT$$FG$$BG$", 12);
  return res;
}
