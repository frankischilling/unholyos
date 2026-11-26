#include "DolDoc.hpp"

extern "C" int64_t DocEntryRunCpp(CDoc *doc, CDocEntryBase *entry,
                                  uint8_t exited, int64_t *has_action)
{
  if (has_action)
    *has_action = false;
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL || !entry ||
      entry == &doc->head)
    return -1;
  DocLock(*doc);
  auto flags = entry->de_flags;
  if (!exited && (flags & (DOCEF_ESC | DOCEF_QUIT))) {
    auto key = flags & DOCEF_ESC ? CH_ESC : CH_SHIFT_ESC;
    MsgCpp(MSG_KEY_DOWN, key, 0, JOBF_DONT_FILTER);
    if (has_action)
      *has_action = true;
    return -1;
  }

  const auto ch = doc->cmd_U8;
  const auto &full = reinterpret_cast<const CDocEntry &>(*entry);
  int64_t result = -1;
  bool action = false;
  if (!exited && (flags & DOCEF_CHECK_COLLAPSABLE)) {
    entry->de_flags ^= DOCEF_CHECKED_COLLAPSED;
    doc->flags |= DOCF_DO_FULL_REFRESH;
    action = true;
  }
  if (ch == ' ' || ch == '\n') {
    const bool left = ch == ' ';
    auto doc_addr = reinterpret_cast<uintptr_t>(doc);
    auto entry_addr = reinterpret_cast<uintptr_t>(entry);
    const auto exp = left ? DOCEF_LEFT_EXP : DOCEF_RIGHT_EXP;
    const auto cb = left ? DOCEF_LEFT_CB : DOCEF_RIGHT_CB;
    const auto value =
        flags & (exp | cb) ? (left ? full.attr : full.right_exp) : 0;
    const auto link = left ? doc->left_click_link : doc->right_click_link;
    if ((flags & DOCEF_LINK) && link) {
      uint64_t returned;
      if (!CallHolyC2Checked(link, doc_addr, entry_addr, &returned)) {
        DocBottomCpp(doc);
        RethrowHolyC();
      }
      result = returned;
      action = true;
    }
    if (flags & exp) {
      result = value;
      action = true;
      MsgCpp(MSG_CMD, result, 0, JOBF_DONT_FILTER);
    }
    if ((flags & cb) && value) {
      DocUnlock(*doc);
      uint64_t returned;
      if (!CallHolyC2Checked(value, doc_addr, entry_addr, &returned)) {
        DocBottomCpp(doc);
        RethrowHolyC();
      }
      result = returned;
      action = true;
      MsgCpp(MSG_CMD, result, 0, JOBF_DONT_FILTER);
    }
  }
  doc->cmd_U8 = ' ';
  if (has_action)
    *has_action = action;
  return result;
}
