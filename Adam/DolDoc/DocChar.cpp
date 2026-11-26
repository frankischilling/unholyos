#include "DolDoc.hpp"

bool EdCharIns(CDoc &doc, int64_t ch, int64_t sc)
{
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
