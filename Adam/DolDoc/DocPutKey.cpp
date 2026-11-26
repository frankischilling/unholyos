#include "DolDoc.hpp"

extern "C" void DocPutKeyCpp(CDoc *doc, int64_t ch, int64_t sc)
{
  if (!doc)
    doc = DocPut();
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL)
    return;
  if (doc->user_put_key &&
      CallHolyC4(doc->user_put_key, reinterpret_cast<uintptr_t>(doc),
                 reinterpret_cast<uintptr_t>(doc->user_put_data), ch, sc))
    return;
  CDocLock lock(*doc);
  if (sc & (SCF_ALT | SCF_KEY_DESC))
    return;
  if (!ch) {
    switch (sc & 0x7F) {
    case SC_CURSOR_LEFT:  EdCursorLeft(*doc, sc); break;
    case SC_CURSOR_RIGHT: EdCursorRight(*doc, sc); break;
    case SC_HOME:
      doc->cur_entry = doc->head.next;
      doc->cur_col = doc->cur_entry->min_col;
      break;
    case SC_END: DocBottomCpp(doc); break;
    case SC_DELETE: EdCharDel(*doc); break;
    }
    return;
  }
  if (ch == CH_BACKSPACE) {
    if (sc & SCF_SHIFT)
      return;
    auto *entry = doc->cur_entry;
    auto col = doc->cur_col;
    EdCursorLeft(*doc, sc & ~SCF_CTRL);
    if (entry != doc->cur_entry || col != doc->cur_col)
      EdCharDel(*doc);
    return;
  }
  if (ch == CH_ESC || ch == CH_SHIFT_ESC) {
    doc->flags &= ~DOCF_IN_DOLLAR;
    doc->dollar_buf_ptr = 0;
    return;
  }
  if (ch < ' ' && ch != '\n' && ch != '\t')
    return;
  if (EdCharIns(*doc, ch, sc))
    return;
  const char text[]{static_cast<char>(ch), 0};
  DocPutS(*doc, text, StrLen(text));
}
