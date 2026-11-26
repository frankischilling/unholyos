#include "DolDoc.hpp"

void EdCursorLeft(CDoc &doc, int64_t sc)
{
  auto *entry = doc.cur_entry;
  if (doc.cur_col > int64_t(entry->min_col) && !(sc & SCF_CTRL)) {
    --doc.cur_col;
    return;
  }
  do {
    auto *prev = entry->last;
    if (prev == &doc.head || prev->Type() == DOCT_PMT ||
        ((sc & SCF_CTRL) && prev->Type() == DOCT_NEW_LINE))
      return;
    entry = prev;
    if (entry->Type() == DOCT_TEXT || entry->Type() == DOCT_NEW_LINE ||
        entry->Type() == DOCT_TAB || (entry->de_flags & DOCEF_TAG)) {
      doc.cur_entry = entry;
      doc.cur_col = entry->min_col;
      if (!(sc & SCF_CTRL)) {
        if (entry->max_col > entry->min_col)
          doc.cur_col = entry->max_col - 1;
        return;
      }
    }
  } while (entry != &doc.head);
}

void EdCursorRight(CDoc &doc, int64_t sc)
{
  auto *entry = doc.cur_entry;
  if (!(sc & SCF_CTRL) && doc.cur_col + 1 < int64_t(entry->max_col)) {
    ++doc.cur_col;
    return;
  }
  while (entry != &doc.head) {
    entry = entry->next;
    if (entry == &doc.head || entry->Type() == DOCT_NEW_LINE ||
        (!(sc & SCF_CTRL) && ((entry->de_flags & DOCEF_TAG) ||
                              entry->Type() == DOCT_TAB))) {
      doc.cur_entry = entry;
      doc.cur_col = entry->min_col;
      return;
    }
  }
}

void EdCharDel(CDoc &doc)
{
  auto *entry = doc.cur_entry;
  if (entry == &doc.head || entry->Type() == DOCT_PMT ||
      (entry->de_flags & DOCEF_FILTER_SKIP))
    return;
  if (entry->Type() == DOCT_TEXT && entry->tag &&
      !(entry->de_flags & ~DOCG_PLAIN_ENTRY)) {
    size_t len = StrLen(entry->tag);
    if (doc.cur_col >= 0 && size_t(doc.cur_col) < len) {
      for (size_t i = doc.cur_col; i < len; ++i)
        entry->tag[i] = entry->tag[i + 1];
      entry->max_col = --len;
      if (len) {
        if (size_t(doc.cur_col) >= len) {
          doc.cur_entry = entry->next;
          doc.cur_col = doc.cur_entry->min_col;
        }
        return;
      }
    } else if (len)
      return;
  }
  doc.cur_entry = entry->next;
  doc.cur_col = doc.cur_entry->min_col;
  DocEntryDel(doc, *entry);
}

static bool ChkDollarBufSize(CDoc &doc)
{
  if (doc.dollar_buf_ptr < doc.dollar_buf_size - 2)
    return true;
  if (doc.dollar_buf_size > INT32_MAX / 2)
    return false;
  auto *buf = reinterpret_cast<char *>(
      MAlloc(doc.dollar_buf_size * 2, MemTaskHeap(doc.mem_task)));
  if (!buf)
    return false;
  MemCpy(buf, doc.dollar_buf, doc.dollar_buf_ptr);
  Free(doc.dollar_buf);
  doc.dollar_buf = buf;
  doc.dollar_buf_size *= 2;
  return true;
}

bool EdCharIns(CDoc &doc, int64_t ch, int64_t sc)
{
  if ((doc.flags & DOCF_IN_DOLLAR) ||
      (ch == '$' && !(doc.flags & DOCG_PLAIN))) {
    if (ch < ' ' || !ChkDollarBufSize(doc))
      return true;
    doc.flags |= DOCF_IN_DOLLAR;
    doc.dollar_buf[doc.dollar_buf_ptr++] = ch;
    if (ch == '$' && doc.dollar_buf_ptr > 1) {
      doc.flags &= ~DOCF_IN_DOLLAR;
      DocPutS(doc, doc.dollar_buf, doc.dollar_buf_ptr);
      doc.dollar_buf_ptr = 0;
    }
    return true;
  }
  if ((ch != ' ' && ch != '\n') ||
      (sc & (SCF_ALT | SCF_CTRL | SCF_SHIFT | SCF_KEY_DESC)))
    return false;
  auto *entry = doc.cur_entry;
  constexpr auto actions = DOCEF_LEFT_EXP | DOCEF_RIGHT_EXP | DOCEF_LEFT_CB |
                           DOCEF_RIGHT_CB | DOCEF_LINK;
  if (entry == &doc.head || !(entry->de_flags & actions))
    return false;
  doc.cmd_U8 = ch;
  DocEntryRunCpp(&doc, entry, false, nullptr);
  DocLock(doc);
  return true;
}
