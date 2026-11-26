#include "DolDoc.hpp"

bool DocEntryRun(CDoc &doc, CDocEntryBase &entry)
{
  auto flags = entry.de_flags;
  if (flags & (DOCEF_ESC | DOCEF_QUIT)) {
    auto key = flags & DOCEF_ESC ? CH_ESC : CH_SHIFT_ESC;
    MsgCpp(MSG_KEY_DOWN, key, 0, JOBF_DONT_FILTER);
    return true;
  }

  auto &full = reinterpret_cast<const CDocEntry &>(entry);
  int64_t result;
  if (doc.cmd_U8 == ' ' && (flags & DOCEF_LEFT_EXP))
    result = full.attr;
  else if (doc.cmd_U8 == '\n' && (flags & DOCEF_RIGHT_EXP))
    result = full.right_exp;
  else {
    doc.cmd_U8 = ' ';
    return false;
  }
  MsgCpp(MSG_CMD, result, 0, JOBF_DONT_FILTER);
  doc.cmd_U8 = ' ';
  return true;
}
