#include "DolDoc.hpp"

bool EdCharIns(CDoc &doc, int64_t ch, int64_t sc)
{
  if ((ch != ' ' && ch != '\n') ||
      (sc & (SCF_ALT | SCF_CTRL | SCF_SHIFT | SCF_KEY_DESC)))
    return false;
  auto *entry = doc.cur_entry;
  constexpr auto actions = DOCEF_LEFT_EXP | DOCEF_RIGHT_EXP;
  if (entry == &doc.head || !(entry->de_flags & actions))
    return false;
  doc.cmd_U8 = ch;
  return DocEntryRun(doc, *entry);
}
