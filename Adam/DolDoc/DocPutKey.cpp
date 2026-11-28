#include "DolDoc.hpp"
#include "../AutoComplete/AutoComplete.hpp"
#include "../God/GodSong.hpp"

static bool DocACKey(int64_t ch, int64_t sc)
{
  auto key = uint8_t(sc);
  return !ch && (sc & (SCF_CTRL | SCF_ALT)) == SCF_CTRL && SC_F1 <= key &&
         key <= SC_F10;
}

static bool DocACDigit(int64_t ch, int64_t sc)
{
  return '0' <= ch && ch <= '9' && (sc & (SCF_CTRL | SCF_ALT)) == SCF_CTRL;
}

static void DocKeyDescription(int64_t ch, int64_t sc)
{
  const char *desc = nullptr;
  const bool ctrl = sc & SCF_CTRL, shift = sc & SCF_SHIFT;
  if (sc & SCF_ALT)
    return;
  if (!ctrl && ch >= 0 && ch <= UINT8_MAX &&
      (char_bmp_printable[ch / 32] & (1U << (ch % 32)))) {
    auto *name = Char2KeyNameCpp(ch, false);
    const uint64_t arg = reinterpret_cast<uintptr_t>(name);
    auto *text = name ? MStrPrintCpp("Char  /'%s'", 1, &arg) : nullptr;
    if (text)
      SetKeyDescription(text);
    Free(text);
    Free(name);
    return;
  }
  if (ch == CH_BACKSPACE && !shift)
    desc = "Char  /Back Space";
  else if (DocACKey(ch, sc))
    desc = shift ? "Cmd /Src Code of Sym" : "Edit/Autocomplete Sym";
  else if (DocACDigit(ch, sc))
    desc = shift ? "Cmd /Word Definition" : "Edit/Autocomplete Word";
  else if (!ch)
    switch (uint8_t(sc)) {
    case SC_F1:
      if (!ctrl)
        desc = shift ? "Cmd /About" : "Cmd /Help";
      break;
    case SC_F6:
      if (!ctrl && !shift)
        desc = "Cmd /God Song";
      break;
    case SC_CURSOR_LEFT:
      if (!shift)
        desc = ctrl ? "Edit/GoTo Start of Line" : "Edit/Cursor Left";
      break;
    case SC_CURSOR_RIGHT:
      if (!shift)
        desc = ctrl ? "Edit/GoTo End of Line" : "Edit/Cursor Right";
      break;
    case SC_HOME: desc = "Edit/GoTo Top of Doc"; break;
    case SC_END: desc = "Edit/GoTo Doc End"; break;
    case SC_DELETE: desc = "Char  /Delete"; break;
    }
  if (desc)
    SetKeyDescription(desc);
}

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
  if (sc & SCF_KEY_DESC) {
    DocKeyDescription(ch, sc);
    return;
  }
  if (DocACKey(ch, sc)) {
    if (AutoCompleteCpp(true)) {
      auto choice = uint8_t(sc) - SC_F1 + 1;
      if (sc & SCF_SHIFT)
        ACMan(choice, CurrentTask());
      else
        ACFillIn(choice);
    }
    return;
  }
  if (DocACDigit(ch, sc)) {
    if (AutoCompleteCpp(true)) {
      if (sc & SCF_SHIFT)
        ACDDef(ch - '0', CurrentTask());
      else
        ACDFillin(ch - '0');
    }
    return;
  }
  if (!ch && uint8_t(sc) == SC_F1 &&
      !(sc & (SCF_CTRL | SCF_ALT | SCF_KEY_DESC))) {
    EdCpp(sc & SCF_SHIFT ? "::/Doc/AboutTempleOS.DD.Z" : "::/Doc/HelpIndex.DD.Z", 0);
    return;
  }
  if (!ch && uint8_t(sc) == SC_F6 &&
      !(sc & (SCF_CTRL | SCF_ALT | SCF_SHIFT | SCF_KEY_UP))) {
    GodSongCpp();
    return;
  }
  CDocLock lock(*doc);
  if (sc & (SCF_ALT | SCF_KEY_DESC))
    return;
  if ((doc->flags & DOCF_FORM) && DocFormKey(*doc, ch, sc))
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

extern "C" bool KDDocPutKeyCpp(int64_t ch, int64_t sc)
{
  if (auto *doc = DocPut())
    DocPutKeyCpp(doc, ch, sc);
  return false;
}
