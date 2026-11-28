#include "../DolDoc/DolDoc.hpp"
#include "../Training.hpp"
#include "AutoComplete.hpp"

extern "C" void ACTaskHC(), ACTaskEndCBHC();
extern "C" int64_t ScanMsgCpp(int64_t *arg1, int64_t *arg2, uint64_t mask,
                               CTask *task);
extern "C" void DocClearCpp(CDoc *doc, uint8_t clear_holds);
extern "C" bool DocCursorCpp(bool show, CDoc *doc);
extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);

static void ACDDictWordsAdd(const char *word)
{
  auto *record = ACDWordPtAtCpp(word);
  if (!record)
    return;
  int64_t i = 0;
  for (; i < ACD_FILLINS_NUM && *record == ACD_WORD_CHAR;
       i++, record = ACDWordNext(record)) {
    if (i)
      PutS("\n");
    acd.fillins[i] = record;
    const uint64_t args[]{uint64_t(i), reinterpret_cast<uintptr_t>(record + 1)};
    PrintCpp("$GREEN$'%d'$FG$ %-23ts", 2, args);
  }
  acd.num_fillins = i;
}

static void ACDocRst(int64_t left, int64_t top)
{
  auto *task = CurrentTask();
  auto *doc = DocPut();
  DocClearCpp(doc, true);
  doc->flags |= DOCF_SIZE_MIN;
  task->border_src = 0; // BDS_CONST
  task->border_attr = (LTGRAY << 4) | (DrvTextAttrGetCpp(':') & 15);
  task->text_attr = (LTGRAY << 4) | BLUE;
  task->display_flags &= ~DISPLAYF_SHOW;
  WinHorzCpp(left, TEXT_COLS - 2, task);
  WinVertCpp(top, TEXT_ROWS - 2, task);
  DocCursorCpp(true, doc);
}

static void ACDocShow()
{
  auto *task = CurrentTask();
  auto *doc = DocPut();
  if (task->display_flags & DISPLAYF_SHOW)
    return;
  DocRecalc(*doc);
  auto right = task->win_left + doc->max_x - doc->min_x;
  auto bottom = task->win_top + doc->max_y - doc->min_y;
  WinHorzCpp(task->win_left, right < TEXT_COLS - 2 ? right : TEXT_COLS - 2, task);
  WinVertCpp(task->win_top, bottom < TEXT_ROWS - 2 ? bottom : TEXT_ROWS - 2, task);
  task->display_flags |= DISPLAYF_SHOW;
  WinZBufUpdateCpp();
}

static void ACPriorWordInStr(const char *src, char *dst, int64_t len,
                             size_t buf_size)
{
  int64_t i = len;
  while (i > 0 && ACWordChar(src[i - 1]))
    i--;
  size_t count = 0;
  while (i < len && count + 1 < buf_size)
    dst[count++] = src[i++];
  dst[count] = 0;
}

static void ACFillInAdd(CHashAC *word)
{
  if (ac.num_fillins >= AC_FILLINS_NUM &&
      word->hits <= ac.fillin_hits[ac.num_fillins - 1])
    return;
  auto i = ac.num_fillins;
  while (i && word->hits > ac.fillin_hits[i - 1]) {
    ac.fillin_matches[i] = ac.fillin_matches[i - 1];
    ac.fillin_hits[i] = ac.fillin_hits[i - 1];
    i--;
  }
  ac.fillin_matches[i] = word;
  ac.fillin_hits[i] = word->hits;
  if (ac.num_fillins < AC_FILLINS_NUM)
    ac.num_fillins++;
}

static void ACPutChoices(CDoc &focus_doc, CDocEntryBase *entry,
                         CTask *focus_task, bool force_refresh)
{
  const auto deadline = GetTSC() + (uint64_t(cnts.time_stamp_freq) >> 1);
  int64_t data_col;
  auto *src = DocScanLineCpp(&focus_doc, entry, &data_col, nullptr);
  DocUnlock(focus_doc);
  if (!src)
    return;
  const auto len = int64_t(StrLen(src));
  char word[256];
  ACPriorWordInStr(src, word, data_col > len ? len : data_col, sizeof(word));
  Free(src);
  ac.partial_len = StrLen(word);
  if (!force_refresh && ac.cur_word &&
      !StrCmpCpp(ac.cur_word, reinterpret_cast<const uint8_t *>(word)))
    return;
  auto *copy = StrNew(word, adam_task->data_heap);
  if (!copy)
    return;
  Free(ac.cur_word);
  ac.cur_word = reinterpret_cast<uint8_t *>(copy);
  ac.num_fillins = 0;
  if (*word)
    for (int64_t i = 0; i <= ac.hash_table->mask && GetTSC() < deadline; i++)
      for (auto *item = ac.hash_table->body[i]; item; item = item->next)
        if ((item->type & HTT_WORD) &&
            !StrNCmpCpp(word, item->str, ac.partial_len))
          ACFillInAdd(reinterpret_cast<CHashAC *>(item));
  ACDocRst(51, 13);
  if (!*word) {
    if (FileFindCpp(AC_STANDBY, nullptr, FUF_JUST_FILES))
      TypeCpp(AC_STANDBY, 0);
    return;
  }
  const uint64_t prefix[]{reinterpret_cast<uintptr_t>(word)};
  PrintCpp("$PURPLE$Word:%s$FG$\n", 1, prefix);
  for (int64_t i = 0; i < ac.num_fillins; i++) {
    const auto *match = ac.fillin_matches[i]->hash.str;
    const uint64_t label[]{uint64_t(i + 1)};
    PrintCpp("$GREEN$F%02d$FG$ ", 1, label);
    const auto *sym = TaskValidateCpp(focus_task)
                          ? reinterpret_cast<CHashSrcSym *>(HashFind(
                                match, focus_task->hash_table, HTG_SRC_SYM))
                          : nullptr;
    if (sym && sym->src_link) {
      if (sym->hash.type & HTF_PUBLIC)
        PutS("$RED$");
      const uint64_t args[]{reinterpret_cast<uintptr_t>(match),
                            reinterpret_cast<uintptr_t>(sym->src_link)};
      PrintCpp("$TX+UL+L+PU,\"%$Q\",A=\"%s\"$$FG$\n", 2, args);
    } else {
      const uint64_t args[]{reinterpret_cast<uintptr_t>(match)};
      PrintCpp("%s\n", 1, args);
    }
  }
  if (acd.has_words)
    ACDDictWordsAdd(word);
}

static void ACTaskNormal(int64_t sc, int64_t last_sc, CTask *focus_task,
                         CTask *original_focus_task)
{
  auto *doc = DocPutCpp(focus_task);
  if (doc && focus_task != CurrentTask() &&
      (focus_task->display_flags & DISPLAYF_SHOW)) {
    DocLock(*doc);
    if (TaskValidateCpp(focus_task) && original_focus_task == sys_focus_task &&
        doc == DocPutCpp(focus_task) && doc->cur_entry) {
      auto *head = &doc->head;
      auto *entry = doc->cur_entry == head ? head->last : doc->cur_entry;
      while (entry->last != head && (entry->Type() == DOCT_NEW_LINE ||
                                     entry->Type() == DOCT_SOFT_NEW_LINE))
        entry = entry->last;
      while (entry->last != head && entry->last->Type() != DOCT_NEW_LINE)
        entry = entry->last;
      ACPutChoices(*doc, entry, focus_task, sc != last_sc);
    } else {
      DocUnlock(*doc);
    }
  }
  ACDocShow();
}

static void ACTaskCtrl(int64_t sc, int64_t last_sc, CTask *focus_task,
                      CTask *original_focus_task)
{
  if (TaskValidateCpp(focus_task) && (focus_task->scroll_x || focus_task->scroll_y)) {
    auto *task = CurrentTask();
    if (__atomic_fetch_and(&task->display_flags, ~DISPLAYF_SHOW, __ATOMIC_RELAXED) &
        DISPLAYF_SHOW)
      WinZBufUpdateCpp();
    return;
  }
  if (sc != last_sc) {
    bool shift = sc & SCF_SHIFT;
    bool key_desc = TaskValidateCpp(original_focus_task) &&
                    !(original_focus_task->win_inhibit & WIF_SELF_KEY_DESC);
    if (sc & SCF_ALT) {
      ACDocRst(27, 3);
      if (key_desc)
        KeyMapFamily(original_focus_task, 0, !shift, shift);
      KeyMapCtrlAltFamily(!shift, shift);
    } else if (key_desc) {
      ACDocRst(27, 3);
      KeyMapFamily(original_focus_task, SCF_CTRL, !shift, shift);
    }
  }
  ACDocShow();
}

static void ACTaskAlt(int64_t sc, int64_t last_sc, CTask *original_focus_task)
{
  if (sc != last_sc && TaskValidateCpp(original_focus_task) &&
      !(original_focus_task->win_inhibit & WIF_SELF_KEY_DESC)) {
    bool shift = sc & SCF_SHIFT;
    ACDocRst(27, 3);
    KeyMapFamily(original_focus_task, SCF_ALT, !shift, shift);
  }
  ACDocShow();
}

extern "C" [[noreturn]] void ACTaskEndCBCpp()
{
  ac.task = nullptr;
  Exit();
}

extern "C" void ACTaskCpp()
{
  auto *task = CurrentTask();
  task->task_end_cb = ACTaskEndCBHC;
  task->win_inhibit = WIG_NO_FOCUS_TASK_DFT;
  DocTermNewCpp();
  if (!DocPut())
    return;
  task->display_flags |= DISPLAYF_WIN_ON_TOP;
  Free(ac.cur_word);
  ac.cur_word = nullptr;

  int64_t scan_code = 0, last_scan_code = 0;
  for (;;) {
    auto last_event = ms_hard.timestamp > kbd.timestamp ? ms_hard.timestamp
                                                       : kbd.new_key_timestamp;
    if ((scan_code & (SCF_CTRL | SCF_ALT)) ||
        GetTSC() > uint64_t(last_event) + (uint64_t(cnts.time_stamp_freq) >> 1)) {
      last_scan_code = scan_code;
      scan_code = kbd.scan_code;
    }
    auto *original_focus_task = sys_focus_task;
    auto *focus_task = original_focus_task;
    while (TaskValidateCpp(focus_task) && (focus_task->task_flags & TASKF_INPUT_FILTER_TASK))
      focus_task = focus_task->parent_task;
    if (scan_code & SCF_CTRL)
      ACTaskCtrl(scan_code, last_scan_code, focus_task, original_focus_task);
    else if (TaskValidateCpp(focus_task)) {
      if (scan_code & SCF_ALT)
        ACTaskAlt(scan_code, last_scan_code, original_focus_task);
      else
        ACTaskNormal(scan_code, last_scan_code, focus_task, original_focus_task);
    }
    SleepCpp(333);
    int64_t ch;
    if (ScanMsgCpp(&ch, nullptr, 1 << MSG_KEY_DOWN, task) &&
        (ch == CH_ESC || ch == CH_SHIFT_ESC))
      break;
  }
}

extern "C" uint64_t AutoCompleteCpp(uint8_t val)
{
  while (__atomic_load_n(&ac.flags, __ATOMIC_RELAXED) & ACF_INIT_IN_PROGRESS)
    SleepCpp(10);
  auto *task = ac.task;
  bool running = TaskValidateCpp(task);
  bool enabled = SYS_RUN_LEVEL & RLF_AUTO_COMPLETE;
  bool old_val = running && enabled;
  if (val) {
    if (enabled) {
      if (!running) {
        ac.task = SpawnCpp(reinterpret_cast<uintptr_t>(ACTaskHC), nullptr,
                           "AutoComplete", -1, nullptr, 0, JOBF_ADD_TO_QUE);
        if (ac.task)
          TaskWaitCpp(ac.task, false);
      }
      if (ac.task)
        WinToTopCpp(ac.task, true);
    }
  } else if (running) {
    KillTask(task, false);
    while (ac.task == task && TaskValidateCpp(task))
      _YIELD();
  }
  return old_val;
}
