#include "DolDoc.hpp"

extern "C" void EdLeftClickLinkHC();

bool DocCanLock(const CDoc &doc)
{
  return !(__atomic_load_n(&doc.locked_flags, __ATOMIC_ACQUIRE) & 1) ||
         __atomic_load_n(&doc.owning_task, __ATOMIC_ACQUIRE) == CurrentTask();
}

bool DocLock(CDoc &doc)
{
  auto *task = CurrentTask();
  if (__atomic_load_n(&doc.owning_task, __ATOMIC_ACQUIRE) == task)
    return false;
  while (__atomic_fetch_or(&doc.locked_flags, 1, __ATOMIC_ACQUIRE) & 1)
    _YIELD();
  constexpr uint32_t break_locked = 1 << 10;
  bool was_locked =
      __atomic_fetch_or(&task->task_flags, break_locked, __ATOMIC_RELAXED) &
      break_locked;
  if (was_locked)
    doc.flags &= ~DOCF_BREAK_UNLOCKED;
  else
    doc.flags |= DOCF_BREAK_UNLOCKED;
  __atomic_store_n(&doc.owning_task, task, __ATOMIC_RELEASE);
  return true;
}

bool DocUnlock(CDoc &doc)
{
  auto *task = CurrentTask();
  if (__atomic_load_n(&doc.owning_task, __ATOMIC_ACQUIRE) != task)
    return false;
  bool unlock_break = doc.flags & DOCF_BREAK_UNLOCKED;
  __atomic_store_n(&doc.owning_task, nullptr, __ATOMIC_RELAXED);
  __atomic_fetch_and(&doc.locked_flags, ~uint64_t{1}, __ATOMIC_RELEASE);
  if (unlock_break)
    __atomic_fetch_and(&task->task_flags, ~(1u << 10), __ATOMIC_RELAXED);
  return true;
}

extern "C" uint64_t DocLockCpp(CDoc *doc)
{
  return doc && DocLock(*doc);
}

extern "C" uint64_t DocUnlockCpp(CDoc *doc)
{
  return doc && DocUnlock(*doc);
}

CDocLock::CDocLock(CDoc &doc) : locked(DocLock(doc) ? &doc : nullptr)
{
}

CDocLock::~CDocLock()
{
  if (locked)
    DocUnlock(*locked);
}

CDocEntryBase *DocEntryNewBase(CDoc &doc, uint32_t type, uint64_t flags)
{
  auto *entry = reinterpret_cast<CDocEntryBase *>(
      CAlloc(sizeof(CDocEntryBase), MemTaskHeap(doc.mem_task)));
  if (entry) {
    entry->type = type;
    entry->de_flags = flags | (entry->Type() == DOCT_TEXT ? DOCEF_TAG : 0);
  }
  return entry;
}

static size_t DocEntryPayload(CDocEntryBase &base, bool discard)
{
  const auto *entry = reinterpret_cast<CDocEntry *>(&base);
  size_t bytes = 0;
  auto payload = [&](void *ptr) {
    if (discard)
      Free(ptr);
    else
      bytes += MSize2(ptr);
  };
  if (base.de_flags & DOCEF_TAG)
    payload(base.tag);
  if (base.de_flags & DOCEF_AUX_STR)
    payload(entry->aux_str);
  if (base.de_flags & DOCEF_DEFINE)
    payload(entry->define_str);
  if (base.de_flags & DOCEF_HTML_LINK)
    payload(entry->html_link);
  if (base.de_flags & DOCEF_LEFT_MACRO)
    payload(entry->left_macro);
  if (base.de_flags & DOCEF_RIGHT_MACRO)
    payload(entry->right_macro);
  if (base.de_flags & DOCEF_BIN_PTR_LINK)
    payload(entry->bin_ptr_link);
  if (base.de_flags & DOCEF_REMALLOC_DATA)
    payload(entry->data);
  return bytes;
}

CDocEntryBase *DocEntryCopy(CDoc &doc, const CDocEntryBase &entry)
{
  auto *copy =
      reinterpret_cast<CDocEntry *>(MAllocIdentCpp(&entry, doc.mem_task));
  if (!copy)
    return nullptr;
  copy->base.next = copy->base.last = &copy->base;
  copy->base.de_flags = 0;
  auto dup = [&]<class T>(uint64_t flag, T *&field) {
    if (!(entry.de_flags & flag))
      return true;
    if (field) {
      field = reinterpret_cast<T *>(MAllocIdentCpp(field, doc.mem_task));
      if (!field)
        return false;
    }
    copy->base.de_flags |= flag;
    return true;
  };
  bool okay = dup(DOCEF_TAG, copy->base.tag);
  if (okay && MSize(&entry) >= sizeof(CDocEntry)) {
    okay = dup(DOCEF_AUX_STR, copy->aux_str) &&
           dup(DOCEF_DEFINE, copy->define_str) &&
           dup(DOCEF_HTML_LINK, copy->html_link) &&
           dup(DOCEF_LEFT_MACRO, copy->left_macro) &&
           dup(DOCEF_RIGHT_MACRO, copy->right_macro) &&
           dup(DOCEF_BIN_PTR_LINK, copy->bin_ptr_link) &&
           dup(DOCEF_REMALLOC_DATA, copy->data);
    if (okay && (entry.de_flags & DOCEF_HAS_BIN)) {
      auto *old = copy->bin_data;
      const auto *title =
          (entry.de_flags & DOCEF_TAG) ? copy->base.tag : nullptr;
      auto *bin = old && doc.cur_bin_num < INT32_MAX
                      ? DocBinCopy(doc, *old, title)
                      : nullptr;
      okay = bin != nullptr;
      if (bin) {
        bin->num = doc.cur_bin_num++;
        bin->use_cnt = 1;
        QueIns(*bin, *doc.bin_head.last);
        copy->bin_data = bin;
        copy->bin_num = bin->num;
      }
    }
  }
  if (!okay) {
    DocEntryPayload(copy->base, true);
    Free(copy);
    return nullptr;
  }
  copy->base.de_flags = entry.de_flags;
  return &copy->base;
}

void DocEntryError(CDocEntry &entry)
{
  DocEntryPayload(entry.base, true);
  entry.base.tag = nullptr;
  entry.base.type = DOCT_ERROR;
  entry.base.de_flags = 0;
}

void DocEntryDel(CDoc &doc, CDocEntryBase &entry)
{
  if (&entry == &doc.head)
    return;
  if (doc.cur_entry == &entry)
    doc.cur_entry = entry.next;
  entry.last->next = entry.next;
  entry.next->last = entry.last;
  DocEntryPayload(entry, true);
  if (entry.de_flags & DOCEF_HAS_BIN)
    DocBinDel(doc, reinterpret_cast<CDocEntry *>(&entry)->bin_data);
  Free(&entry);
}

void DocEntryInsert(CDocEntryBase &entry, CDocEntryBase &before)
{
  entry.last = before.last;
  entry.next = &before;
  before.last->next = &entry;
  before.last = &entry;
}

bool DocInsEntry(CDoc &doc, CDocEntryBase &entry)
{
  auto *position = doc.cur_entry;
  entry.x = position->x;
  entry.y = position->y;
  entry.page_line_num = position->page_line_num;
  MemCpy(&entry.settings, &position->settings, sizeof(entry.settings));
  if (position->Type() == DOCT_TEXT && doc.cur_col > 0 && position->tag &&
      !(position->de_flags & (DOCEF_DEFINE | DOCEF_AUX_STR | DOCEF_HTML_LINK |
                              DOCEF_BIN_PTR_LINK | (1ull << 32))) &&
      uint32_t(doc.cur_col) < position->max_col) {
    auto *suffix = DocEntryNewBase(doc, position->type, position->de_flags);
    if (!suffix)
      return false;
    suffix->tag =
        StrNew(position->tag + doc.cur_col, MemTaskHeap(doc.mem_task));
    if (!suffix->tag) {
      Free(suffix);
      return false;
    }
    suffix->max_col = position->max_col - doc.cur_col;
    suffix->x = position->x;
    suffix->y = position->y;
    suffix->page_line_num = position->page_line_num;
    MemCpy(&suffix->settings, &position->settings, sizeof(suffix->settings));
    position->tag[doc.cur_col] = 0;
    position->max_col = doc.cur_col;
    DocEntryInsert(*suffix, *position->next);
    position = suffix;
  } else if (position->Type() == DOCT_TEXT && doc.cur_col >= 0 &&
             uint32_t(doc.cur_col) >= position->max_col) {
    position = position->next;
  }
  DocEntryInsert(entry, *position);
  doc.cur_entry = position;
  doc.cur_col = position->min_col;
  return true;
}

extern "C" CDoc *DocNewCpp(const char *filename, CTask *task)
{
  if (!task)
    task = CurrentTask();
  auto *hc = MemTaskHeap(task);
  auto *doc = reinterpret_cast<CDoc *>(CAlloc(sizeof(CDoc), hc));
  if (!doc)
    return nullptr;
  doc->find_replace =
      reinterpret_cast<CEdFindText *>(CAlloc(sizeof(CEdFindText), hc));
  doc->dollar_buf_size = 84;
  doc->dollar_buf = reinterpret_cast<char *>(MAlloc(doc->dollar_buf_size, hc));
  if (!doc->find_replace || !doc->dollar_buf) {
    Free(doc->find_replace);
    Free(doc->dollar_buf);
    Free(doc);
    return nullptr;
  }
  if (!filename)
    filename = blkdev.tmp_filename ? blkdev.tmp_filename : DFT_TMP_FILENAME;
  size_t len = StrLen(filename);
  if (len >= sizeof(doc->filename.name))
    len = sizeof(doc->filename.name) - 1;
  MemCpy(doc->filename.name, filename, len);
  doc->find_replace->scan_fwd = doc->find_replace->match_case =
      doc->find_replace->pmt = true;
  doc->mem_task = doc->win_task = task;
  doc->left_click_link = reinterpret_cast<uintptr_t>(EdLeftClickLinkHC);
  doc->max_entries = doc->best_d = INT64_MAX;
  doc->head.type = DOCT_ERROR;
  doc->cur_entry = &doc->head;
  doc->head.next = doc->head.last = &doc->head;
  QueInit(doc->bin_head.next, doc->bin_head.last);
  QueInit(doc->undo_head.next, doc->undo_head.last);
  doc->cur_bin_num = 1;
  doc->cmd_U8 = ' ';
  doc->settings_head.page_len = 66;
  doc->settings_head.cur_text_attr = doc->settings_head.dft_text_attr = 0xF0;
  MemCpy(&doc->head.settings, &doc->settings_head, sizeof(CDocSettings));
  CDocLock lock(*doc);
  doc->doc_signature = DOC_SIGNATURE_VAL;
  return doc;
}

extern "C" void DocDelCpp(CDoc *doc)
{
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL)
    return;
  DocLock(*doc);
  doc->doc_signature = 0;
  while (doc->head.next != &doc->head)
    DocEntryDel(*doc, *doc->head.next);
  for (auto *undo = doc->undo_head.next; undo != &doc->undo_head;) {
    auto *next = undo->next;
    Free(undo->body);
    Free(undo);
    undo = next;
  }
  while (doc->bin_head.next != &doc->bin_head)
    DocBinFree(*doc->bin_head.next);
  Free(doc->find_replace);
  Free(doc->dollar_buf);
  DocUnlock(*doc);
  Free(doc);
}

extern "C" size_t DocSizeCpp(CDoc *doc)
{
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL)
    return 0;
  CDocLock lock(*doc);
  size_t size =
      MSize2(doc) + MSize2(doc->find_replace) + MSize2(doc->dollar_buf);
  for (auto *entry = doc->head.next; entry != &doc->head; entry = entry->next)
    size += MSize2(entry) + DocEntryPayload(*entry, false);
  for (auto *undo = doc->undo_head.next; undo != &doc->undo_head;
       undo = undo->next)
    size += MSize2(undo) + MSize2(undo->body);
  for (auto *bin = doc->bin_head.next; bin != &doc->bin_head; bin = bin->next)
    size += MSize2(bin) + MSize2(bin->data);
  return size;
}
