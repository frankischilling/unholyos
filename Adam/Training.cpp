#include "DolDoc/DolDoc.hpp"

extern "C" int64_t Char2ScanCodeCpp(int64_t ch, int64_t sc_flags);
extern "C" void PostMsgCpp(CTask *task, int64_t code, int64_t arg1,
                           int64_t arg2, int64_t flags);

static char *KeyMapKeyMStrPrint(int64_t sc, uintptr_t hndlr,
                               const char *desc, CTask *task)
{
  int64_t width = 9;
  if (sc & SCF_CTRL) width += 5;
  if (sc & SCF_ALT) width += 4;
  if (sc & (SCF_SHIFT | SCF_NO_SHIFT)) width += 6;
  auto *name = ScanCode2KeyNameCpp(sc);
  if (!name)
    return nullptr;
  auto *caller = CurrentTask();
  auto *old_hash = caller->hash_table;
  if (TaskValidateCpp(task))
    caller->hash_table = task->hash_table;
  auto *link = hndlr ? SrcEdLink(hndlr, 256) : nullptr;
  caller->hash_table = old_hash;
  int64_t color = BLACK;
  if (!StrNCmpCpp(desc, "Edit", 4))
    color = BLUE;
  else if (!StrNCmpCpp(desc, "Dol ", 4))
    color = GREEN;
  else if (!StrNCmpCpp(desc, "Cmd ", 4))
    color = RED;
  const uint64_t args[]{uint64_t(width), reinterpret_cast<uintptr_t>(name),
                         uint64_t(color), reinterpret_cast<uintptr_t>(desc),
                         reinterpret_cast<uintptr_t>(link)};
  auto *res = link
      ? MStrPrintCpp("%-*s $FG,%d$$TX+UL+L+PU,\"%$Q\",A=\"%s\"$$FG$\n", 5, args)
      : MStrPrintCpp("%-*s $FG,%d$$TX,\"%$Q\"$$FG$\n", 4, args);
  Free(name);
  Free(link);
  if (res)
    for (auto *p = res; *p; p++)
      if (*p == ' ')
        *p = CH_SHIFT_SPACE;
  return res;
}

void KeyMapCtrlAltFamily(bool no_shift, bool shift)
{
  const auto flags = SCF_CTRL | SCF_ALT |
                     (no_shift && shift ? SCF_NO_SHIFT : 0);
  for (int64_t i = 0; i < 26; i++) {
    auto callback = keydev.fp_ctrl_alt_cbs[i];
    if (!callback)
      continue;
    const auto sc = Char2ScanCodeCpp(i + 'a', 0);
    if (no_shift && keydev.ctrl_alt_no_shift_descs[i]) {
      auto *text = KeyMapKeyMStrPrint(sc | flags, callback,
                                     keydev.ctrl_alt_no_shift_descs[i], nullptr);
      PutS(text);
      Free(text);
    }
    if (shift && keydev.ctrl_alt_shift_descs[i]) {
      auto *text = KeyMapKeyMStrPrint(sc | SCF_CTRL | SCF_ALT | SCF_SHIFT,
                                     callback, keydev.ctrl_alt_shift_descs[i], nullptr);
      PutS(text);
      Free(text);
    }
  }
}

static void KMComparePrepare(char (&buf)[33], const char *src)
{
  size_t n = src ? StrLen(src) : 0;
  if (n > 32)
    n = 32;
  if (n)
    MemCpy(buf, src, n);
  buf[n] = 0;
  if (auto *shift = StrMatchCpp("SHIFT", buf)) {
    MemSet(const_cast<char *>(shift), CH_SHIFT_SPACE, 5);
    if (auto *dollar = StrMatchCpp("$", buf))
      *const_cast<char *>(dollar) = char(255);
  }
}

static int64_t KMCompare(int64_t left, int64_t right)
{
  char a[33], b[33];
  KMComparePrepare(a, reinterpret_cast<const char *>(left));
  KMComparePrepare(b, reinterpret_cast<const char *>(right));
  return StrCmpCpp(reinterpret_cast<const uint8_t *>(a),
                   reinterpret_cast<const uint8_t *>(b));
}

static void KeyMapFamily2(int64_t *entries, CTask *task, int64_t scf)
{
  for (int64_t i = 0; i < 256; i++) {
    const auto sc = scf | i | SCF_KEY_DESC;
    const auto ch = ScanCode2CharCpp(sc);
    SetKeyDescription("");
    if (TaskValidateCpp(task) && !(task->win_inhibit & WIF_SELF_KEY_DESC)) {
      if (task == CurrentTask())
        PutKeyCpp(ch, sc);
      else
        PostMsgCpp(task, MSG_KEY_DOWN, ch, sc, 0);
      RefreshCpp(0, true);
      SleepCpp(1);
    }
    if (*keydev.desc && StrNCmpCpp(keydev.desc, "Char  /", 7))
      entries[i] = reinterpret_cast<int64_t>(
          KeyMapKeyMStrPrint(sc, keydev.hndlr, keydev.desc, task));
  }
}

void KeyMapFamily(CTask *task, int64_t scf, bool no_shift, bool shift)
{
  int64_t count = 0;
  auto *entries = reinterpret_cast<int64_t *>(
      CAlloc(512 * sizeof(int64_t), CurrentTask()->data_heap));
  if (!entries)
    return;
  if (no_shift) {
    KeyMapFamily2(entries, task, scf | (shift ? SCF_NO_SHIFT : 0));
    count += 256;
  }
  if (shift) {
    KeyMapFamily2(entries + count, task, scf | SCF_SHIFT);
    count += 256;
  }
  QSortI64Native(entries, count, KMCompare);
  for (int64_t i = 0; i < count; i++)
    if (auto *text = reinterpret_cast<char *>(entries[i])) {
      PutS(text);
      Free(text);
    }
  Free(entries);
}

extern "C" int64_t DocMaxCpp(int64_t entries);

extern "C" void KeyMapCpp(CTask *task)
{
  if (!task)
    task = CurrentTask();
  if (!TaskValidateCpp(task))
    return;
  const auto old = __atomic_fetch_and(&task->win_inhibit, ~WIF_SELF_KEY_DESC,
                                      __ATOMIC_RELAXED);
  DocMaxCpp(INT64_MAX);
  KeyMapFamily(task, 0, true, true);
  KeyMapFamily(task, SCF_CTRL, true, true);
  KeyMapFamily(task, SCF_ALT, true, true);
  KeyMapCtrlAltFamily(true, true);
  if (TaskValidateCpp(task) && (old & WIF_SELF_KEY_DESC))
    __atomic_fetch_or(&task->win_inhibit, WIF_SELF_KEY_DESC, __ATOMIC_RELAXED);
  PutS("\nKeyMap Completed.\n");
}

static bool TipStart(const CDocEntryBase &entry)
{
  return entry.Type() == DOCT_TEXT && entry.tag && *entry.tag == '*';
}

extern "C" void TipOfDayCpp(const char *tip_file)
{
  const auto random = RandU16Cpp();
  auto *doc = DocReadCpp(tip_file, 0);
  PutS("$WW,1$\n");
  if (!doc)
    return;

  auto *head = &doc->head;
  unsigned count = 0;
  for (auto *entry = head->next; entry != head; entry = entry->next)
    count += TipStart(*entry);

  auto *out = DocPut();
  auto *tip = count && out ? DocNewCpp(nullptr, nullptr) : nullptr;
  if (tip) {
    auto *entry = head->next;
    auto index = random % count;
    while (!TipStart(*entry) || index--)
      entry = entry->next;
    bool copied = true;
    do {
      if (entry->Type() != DOCT_ERROR) {
        auto *copy = DocEntryCopy(*tip, *entry);
        if (!copy) {
          copied = false;
          break;
        }
        DocInsEntry(*tip, *copy);
      }
      entry = entry->next;
    } while (entry != head && !TipStart(*entry));
    if (copied)
      DocInsDoc(*out, *tip);
    DocDelCpp(tip);
  }
  DocDelCpp(doc);
}
