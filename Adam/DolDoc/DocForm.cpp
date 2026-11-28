#include "DolDoc.hpp"
#include "../../Compiler/CompilerA.hpp"

bool DocFormEntry(const CDocEntryBase &entry)
{
  return !(entry.de_flags & (DOCEF_SKIP | DOCEF_FILTER_SKIP | DOCEF_SKIP_IN_FORM)) &&
         ((entry.Type() != DOCT_TEXT && (entry.de_flags & DOCEF_TAG)) ||
          entry.Type() == DOCT_HEX_ED || (entry.de_flags & DOCEF_LINK));
}

void DocFormFwd(CDoc &doc, bool giveup)
{
  if (!(doc.flags & DOCF_FORM))
    return;
  auto *entry = doc.cur_entry;
  while (entry != &doc.head && !DocFormEntry(*entry))
    entry = entry->next;
  if (entry != doc.cur_entry || entry == &doc.head) {
    doc.cur_entry = entry;
    doc.cur_col = entry->min_col;
  }
  if (entry == &doc.head && !giveup) {
    doc.cur_entry = entry->last;
    doc.cur_col = doc.cur_entry->min_col;
    DocFormBwd(doc, true);
  }
}

void DocFormBwd(CDoc &doc, bool giveup)
{
  if (!(doc.flags & DOCF_FORM))
    return;
  auto *entry = doc.cur_entry == &doc.head ? doc.head.last : doc.cur_entry;
  while (entry != &doc.head && !DocFormEntry(*entry))
    entry = entry->last;
  if (entry != doc.cur_entry || entry == &doc.head) {
    doc.cur_entry = entry;
    doc.cur_col = entry->min_col;
  }
  if (entry == &doc.head && !giveup) {
    doc.cur_entry = entry->next;
    doc.cur_col = doc.cur_entry->min_col;
    DocFormFwd(doc, true);
  }
}

extern "C" void DocFormFwdCpp(CDoc *doc, uint64_t giveup)
{
  if (doc && doc->doc_signature == DOC_SIGNATURE_VAL) {
    CDocLock lock(*doc);
    DocFormFwd(*doc, giveup);
  }
}

extern "C" void DocFormBwdCpp(CDoc *doc, uint64_t giveup)
{
  if (doc && doc->doc_signature == DOC_SIGNATURE_VAL) {
    CDocLock lock(*doc);
    DocFormBwd(*doc, giveup);
  }
}

static CHashDefineStr *DataList(CDoc &doc, CDocEntry &entry)
{
  return entry.define_str && doc.win_task
             ? reinterpret_cast<CHashDefineStr *>(
                   HashFind(entry.define_str, doc.win_task->hash_table, HTT_DEFINE_STR))
             : nullptr;
}

static bool DataAddress(CDocEntry &entry, uint8_t *&ptr, size_t &size)
{
  auto type = static_cast<CRawType>(entry.raw_type);
  size = TypeSize(type);
  if (!size)
    return false;
  if ((entry.base.de_flags & DOCEF_DEREF_DATA) &&
      !(entry.base.de_flags & DOCEF_REMALLOC_DATA)) {
    ptr = entry.data;
    return ChkPtrRange(ptr, size);
  }
  ptr = reinterpret_cast<uint8_t *>(&entry.data);
  return true;
}

static uint64_t DataValue(const uint8_t *ptr, uint8_t type, size_t size)
{
  uint64_t value = 0;
  MemCpy(&value, ptr, size);
  if (size < 8 && !(type & 1)) {
    unsigned shift = 64 - 8 * size;
    value = int64_t(value << shift) >> shift;
  }
  return value;
}

static bool DataFormatIs(const CDocEntry &entry, const char *fmt)
{
  return entry.aux_str &&
         !StrCmpCpp(reinterpret_cast<const uint8_t *>(entry.aux_str),
                    reinterpret_cast<const uint8_t *>(fmt));
}

void DocDataFmt(CDoc &doc, CDocEntry &entry, int64_t d)
{
  auto &base = entry.base;
  if (base.Type() != DOCT_CHECK_BOX && !(base.de_flags & DOCEF_LST) &&
      (base.Type() != DOCT_DATA || !(base.de_flags & DOCEF_AUX_STR) ||
       !entry.aux_str))
    return;
  CDocLock lock(doc);
  uint8_t *ptr;
  size_t size;
  if (d == DOCM_CANCEL) {
    if (!DataAddress(entry, ptr, size))
      return;
    d = DataValue(ptr, entry.raw_type, size);
  }
  if (base.de_flags & DOCEF_LST) {
    auto *list = DataList(doc, entry);
    const char *label = list && 0 <= d && d < list->cnt ? list->sub_idx[d] : "";
    uint64_t arg = reinterpret_cast<uintptr_t>(label);
    auto *tag = StrPrintJoin(nullptr, "[%s]", 1, &arg);
    if (tag) {
      Free(base.tag);
      base.tag = tag;
      base.min_col = 1;
    }
    return;
  }
  if (base.Type() == DOCT_CHECK_BOX) {
    if (d)
      base.de_flags |= DOCEF_CHECKED_COLLAPSED;
    else
      base.de_flags &= ~DOCEF_CHECKED_COLLAPSED;
    if (base.tag && StrLen(base.tag) >= 3)
      base.tag[1] = d ? 'X' : ' ';
    return;
  }
  // Class reports show at most 40 bytes of a live string.
  char str[41];
  if (DataFormatIs(entry, "%40ts")) {
    auto *src = reinterpret_cast<const char *>(d);
    size_t n = 0;
    while (n < sizeof(str) - 1 && ChkPtr(src + n) && src[n]) {
      str[n] = src[n];
      n++;
    }
    str[n] = 0;
    d = reinterpret_cast<int64_t>(str);
  }
  uint64_t args[]{uint64_t(d), reinterpret_cast<uintptr_t>(entry.my_fmt_data)};
  auto *text = StrPrintJoin(nullptr, entry.aux_str, 2, args);
  if (!text)
    return;
  size_t len = StrLen(text);
  if (base.de_flags & DOCEF_REMALLOC_DATA) {
    if (!entry.data)
      entry.data = CAlloc(2, MemTaskHeap(doc.mem_task));
    if (entry.len < 0 || size_t(entry.len) + base.min_col < len)
      entry.len = len >= base.min_col ? len - base.min_col + 8 : 8;
  }
  if (base.de_flags & DOCEF_HAS_TERMINATOR)
    len++;
  auto capacity = len + 1;
  if (entry.len >= 0 && size_t(entry.len) + base.min_col + 2 > capacity)
    capacity = size_t(entry.len) + base.min_col + 2;
  if (!base.tag || MSize(base.tag) < capacity) {
    auto *tag = reinterpret_cast<char *>(MAlloc(capacity, MemTaskHeap(doc.mem_task)));
    if (!tag) {
      Free(text);
      return;
    }
    Free(base.tag);
    base.tag = tag;
  }
  MemCpy(base.tag, text, StrLen(text) + 1);
  Free(text);
  if (base.de_flags & DOCEF_HAS_TERMINATOR) {
    base.tag[len - 1] = '_';
    base.tag[len] = 0;
  }
  base.max_col = len;
  if (doc.cur_entry == &base && doc.cur_col > int64_t(len))
    doc.cur_col = len;
}

bool DocDataScan(CDoc &doc, CDocEntry &entry)
{
  auto &base = entry.base;
  if (base.Type() != DOCT_CHECK_BOX && !(base.de_flags & DOCEF_LST) &&
      (base.Type() != DOCT_DATA || !(base.de_flags & DOCEF_AUX_STR) ||
       !entry.aux_str))
    return false;
  uint8_t *ptr;
  size_t size;
  if (!DataAddress(entry, ptr, size))
    return false;
  uint64_t value = 0;
  if (base.Type() == DOCT_DATA) {
    if (!base.tag)
      return false;
    size_t len = StrLen(base.tag);
    bool terminator = base.de_flags & DOCEF_HAS_TERMINATOR;
    if (terminator) {
      if (!len)
        return false;
      base.tag[--len] = 0;
    }
    if (entry.len < 0 || len < base.min_col)
      return false;
    if (len > size_t(entry.len) + base.min_col)
      base.tag[size_t(entry.len) + base.min_col] = 0;
    if (entry.raw_type <= RT_U32) {
      uint64_t args[]{reinterpret_cast<uintptr_t>(&value),
                      reinterpret_cast<uintptr_t>(entry.my_fmt_data)};
      StrScanCpp(base.tag, entry.aux_str, 2, args);
      if (terminator)
        base.tag[len] = '_';
    } else {
      if (base.de_flags & DOCEF_REMALLOC_DATA) {
        auto *data = MAlloc(len - base.min_col + 8, MemTaskHeap(doc.mem_task));
        if (!data)
          RaiseHolyC(0x6D654D74754FULL); // OutMem
        MemCpy(data, base.tag + base.min_col, len - base.min_col + 1);
        Free(entry.data);
        entry.data = data;
        entry.len = MSize(data) - 1;
      } else {
        uint64_t args[]{reinterpret_cast<uintptr_t>(ptr),
                        reinterpret_cast<uintptr_t>(entry.my_fmt_data)};
        StrScanCpp(base.tag, entry.aux_str, 2, args);
      }
      if (terminator)
        base.tag[len] = '_';
      return true;
    }
  } else if (base.de_flags & DOCEF_LST) {
    auto *list = base.tag && (base.de_flags & DOCEF_DEFINE)
                     ? DataList(doc, entry)
                     : nullptr;
    if (list) {
      auto *label = StrNew(base.tag, CurrentTask()->data_heap);
      if (!label)
        RaiseHolyC(0x6D654D74754FULL); // OutMem
      auto *start = label;
      if (*start == '[') {
        start++;
        size_t len = StrLen(start);
        if (len && start[len - 1] == ']')
          start[len - 1] = 0;
      }
      value = LstMatchCpp(reinterpret_cast<const uint8_t *>(start),
                          reinterpret_cast<const uint8_t *>(list->data), 0);
      Free(label);
    }
  } else
    value = bool(base.de_flags & DOCEF_CHECKED_COLLAPSED);
  MemCpy(ptr, &value, size);
  return true;
}

extern "C" void DocDataFmtCpp(CDoc *doc, CDocEntry *entry, int64_t d)
{
  if (doc && entry && doc->doc_signature == DOC_SIGNATURE_VAL)
    DocDataFmt(*doc, *entry, d);
}

extern "C" void DocDataScanCpp(CDoc *doc, CDocEntry *entry)
{
  if (doc && entry && doc->doc_signature == DOC_SIGNATURE_VAL)
    DocDataScan(*doc, *entry);
}

static bool FormDataKey(CDoc &doc, CDocEntry &entry, int64_t ch, int64_t sc)
{
  auto &base = entry.base;
  if (!base.tag || entry.len < 0)
    return true;
  size_t len = StrLen(base.tag);
  auto col = size_t(doc.cur_col);
  if (col < base.min_col || col > len)
    return true;
  bool term = base.de_flags & DOCEF_HAS_TERMINATOR;
  if (ch == CH_BACKSPACE || (!ch && (sc & 0x7F) == SC_DELETE)) {
    if (ch == CH_BACKSPACE) {
      if (col == base.min_col)
        return true;
      doc.cur_col = --col;
    }
    if (col < len - (term && len != 0)) {
      for (size_t i = col; i < len; i++)
        base.tag[i] = base.tag[i + 1];
      base.max_col = len - 1;
    }
  } else if (' ' <= ch && ch <= 255 && !(sc & (SCF_ALT | SCF_CTRL))) {
    size_t limit = size_t(entry.len) + base.min_col;
    bool insert = !(doc.flags & DOCF_OVERSTRIKE);
    if (col >= limit && (base.de_flags & DOCEF_REMALLOC_DATA))
      insert = true;
    if (insert || (term && col == len - 1)) {
      if (!term && !insert)
        return true;
      if (len >= limit + term) {
        if (!(base.de_flags & DOCEF_REMALLOC_DATA))
          return true;
        auto *tag = reinterpret_cast<char *>(MAlloc(len + 10, MemTaskHeap(doc.mem_task)));
        if (!tag)
          return true;
        MemCpy(tag, base.tag, len + 1);
        Free(base.tag);
        base.tag = tag;
        entry.len = MSize(tag) - base.min_col - 2;
      }
      if (len + 2 > MSize(base.tag))
        return true;
      for (size_t i = len + 1; i > col; i--)
        base.tag[i] = base.tag[i - 1];
      base.max_col = ++len;
    } else if (col >= len || col >= limit)
      return true;
    base.tag[col] = ch;
    doc.cur_col++;
  } else
    return false;
  if (base.de_flags & DOCEF_UPDATE_DATA)
    DocDataScan(doc, entry);
  return true;
}

bool DocFormKey(CDoc &doc, int64_t ch, int64_t sc)
{
  auto *entry = doc.cur_entry;
  if (!DocFormEntry(*entry) || entry == &doc.head) {
    DocFormFwd(doc);
    entry = doc.cur_entry;
  }
  auto move = [&](bool fwd) {
    doc.cur_entry = fwd ? entry->next : entry->last;
    if (doc.cur_entry == &doc.head)
      doc.cur_entry = entry;
    doc.cur_col = doc.cur_entry->min_col;
    if (fwd)
      DocFormFwd(doc);
    else
      DocFormBwd(doc);
  };
  if (entry == &doc.head)
    return true;
  if (ch == CH_ESC || ch == CH_SHIFT_ESC)
    return true;
  if (ch == '\t' || (ch == '\n' && entry->Type() == DOCT_DATA)) {
    move(!(sc & SCF_SHIFT));
    return true;
  }
  if (!ch) {
    switch (sc & 0x7F) {
    case SC_CURSOR_UP: move(false); return true;
    case SC_CURSOR_DOWN: move(true); return true;
    case SC_HOME:
      doc.cur_entry = doc.head.next;
      doc.cur_col = doc.cur_entry->min_col;
      DocFormFwd(doc);
      return true;
    case SC_END:
      doc.cur_entry = doc.head.last;
      doc.cur_col = doc.cur_entry->min_col;
      DocFormBwd(doc);
      return true;
    case SC_CURSOR_LEFT:
      if (sc & SCF_CTRL)
        doc.cur_col = entry->min_col;
      else if (doc.cur_col > int64_t(entry->min_col))
        doc.cur_col--;
      else
        move(false);
      return true;
    case SC_CURSOR_RIGHT:
      if (sc & SCF_CTRL)
        doc.cur_col = entry->max_col > entry->min_col ? entry->max_col - 1 : entry->min_col;
      else if (doc.cur_col + 1 < int64_t(entry->max_col))
        doc.cur_col++;
      else
        move(true);
      return true;
    case SC_INS: doc.flags ^= DOCF_OVERSTRIKE; return true;
    }
  }
  auto &full = *reinterpret_cast<CDocEntry *>(entry);
  if (entry->Type() == DOCT_DATA)
    return FormDataKey(doc, full, ch, sc), true;
  if (ch == ' ' || ch == '\n') {
    if (entry->Type() == DOCT_CHECK_BOX) {
      entry->de_flags ^= DOCEF_CHECKED_COLLAPSED;
      if (entry->de_flags & DOCEF_UPDATE_DATA)
        DocDataScan(doc, full);
      return true;
    }
    if (entry->de_flags & DOCEF_LST) {
      auto *list = DataList(doc, full);
      if (!list || list->cnt <= 0)
        return true;
      auto *menu = DocNewCpp(nullptr, doc.mem_task);
      if (!menu)
        return true;
      menu->flags |= DOCF_FORM;
      for (int64_t i = 0; i < list->cnt; i++) {
        auto *item = reinterpret_cast<CDocEntry *>(
            CAlloc(sizeof(CDocEntry), MemTaskHeap(menu->mem_task)));
        if (!item)
          break;
        item->base.type = DOCT_MENU_VAL;
        item->base.de_flags = DOCEF_TAG | DOCEF_ESC | DOCEF_LEFT_EXP;
        item->base.tag = StrNew(list->sub_idx[i], MemTaskHeap(menu->mem_task));
        item->attr = i;
        DocInsEntry(*menu, item->base);
        DocPutS(*menu, "\n", 1);
      }
      DocUnlock(doc);
      bool accepted = DocEdCpp(menu, DOF_SIZE_MIN);
      int64_t selected = accepted && menu->cur_entry != &menu->head
                             ? reinterpret_cast<CDocEntry *>(menu->cur_entry)->attr
                             : DOCM_CANCEL;
      DocDelCpp(menu);
      DocLock(doc);
      if (selected != DOCM_CANCEL) {
        DocDataFmt(doc, full, selected);
        DocDataScan(doc, full);
      }
      return true;
    }
    doc.cmd_U8 = ch;
    DocEntryRunCpp(&doc, entry, false, nullptr);
    DocLock(doc);
  }
  return true;
}

extern "C" uint64_t DocFormCpp(uint8_t *d, const char *class_name,
                                int64_t dof_flags, const char *header,
                                const char *footer)
{
  auto *task = CurrentTask();
  auto *cls = class_name ? reinterpret_cast<CHashClass *>(
      HashFind(class_name, task->hash_table, HTT_CLASS)) : nullptr;
  if (!cls || !d || !ChkPtrRange(d, cls->size))
    return false;
  auto *doc = DocNewCpp(nullptr, nullptr);
  if (!doc)
    return false;
  doc->desc = 0x6D726F46; // Form
  if (header)
    DocPutS(*doc, header, StrLen(header));
  doc->flags |= DOCF_FORM | DOCF_OVERSTRIKE;
  if (dof_flags & DOF_SIZE_MIN)
    doc->flags |= DOCF_SIZE_MIN;
  for (auto *ml = cls->member_lst_and_root; ml; ml = ml->next) {
    const auto *format = reinterpret_cast<const char *>(MemberMetaDataCpp("format", ml));
    if (!format)
      continue;
    auto *base = DocPutS(*doc, format, StrLen(format));
    if (!base || (base->Type() != DOCT_DATA && base->Type() != DOCT_LST &&
                  base->Type() != DOCT_CHECK_BOX))
      continue;
    auto &entry = *reinterpret_cast<CDocEntry *>(base);
    auto *type = ml->member_class;
    if (type && !type->ptr_stars_cnt) {
      while (type->fwd_class)
        type = type->fwd_class;
      type -= type->ptr_stars_cnt;
      if (type->sym.hash.type & HTT_INTERNAL_TYPE) {
        if (ml->dim.next) {
          if (type->raw_type == RT_U8 && (base->de_flags & DOCEF_DFT_LEN)) {
            base->de_flags &= ~DOCEF_DFT_LEN;
            entry.len = ml->dim.total_cnt - bool(base->de_flags & DOCEF_HAS_TERMINATOR);
          }
        } else if (base->de_flags & DOCEF_DFT_RAW_TYPE) {
          base->de_flags &= ~DOCEF_DFT_RAW_TYPE;
          entry.raw_type = type->raw_type;
        }
      }
    }
    if (base->de_flags & DOCEF_REMALLOC_DATA) {
      base->user_data = reinterpret_cast<uintptr_t>(d + ml->offset);
      MemCpy(&entry.data, d + ml->offset, sizeof(entry.data));
    } else
      entry.data = d + ml->offset;
    entry.my_fmt_data = reinterpret_cast<char *>(MemberMetaDataCpp("data", ml));
    DocDataFmt(*doc, entry);
  }
  if (footer)
    DocPutS(*doc, footer, StrLen(footer));
  bool res = false;
  auto old_border_src = task->border_src;
  if (doc->head.next != &doc->head) {
    task->border_src = 0; // BDS_CONST
    DocRecalc(*doc);
    res = DocEdCpp(doc, dof_flags);
    if (res && doc->cur_entry != &doc->head) {
      int64_t action;
      if (DocEntryRunCpp(doc, doc->cur_entry, true, &action) == DOCM_CANCEL && action)
        res = false;
      DocUnlock(*doc);
    }
  }
  for (auto *base = doc->head.next; base != &doc->head; base = base->next)
    if ((base->de_flags & DOCEF_REMALLOC_DATA) && base->user_data) {
      auto &entry = *reinterpret_cast<CDocEntry *>(base);
      MemCpy(reinterpret_cast<void *>(base->user_data), &entry.data, sizeof(entry.data));
      entry.data = nullptr;
    }
  DocDelCpp(doc);
  task->border_src = old_border_src;
  return res;
}

extern "C" void DocMenuEndTaskCBHC();
extern "C" void DocMenuTryHC();

extern "C" [[noreturn]] void DocMenuEndTaskCBCpp()
{
  WinToTopCpp(nullptr, true);
  CurrentTask()->except_ch = 0;
  RethrowHolyC();
}

extern "C" int64_t DocMenuTryCpp(CDoc *doc, int64_t dof_flags)
{
  while (DocEdCpp(doc, dof_flags) && doc->cur_entry != &doc->head) {
    int64_t action;
    auto res = DocEntryRunCpp(doc, doc->cur_entry, true, &action);
    DocUnlock(*doc);
    if (action)
      return res;
    dof_flags |= DOF_DONT_HOME;
  }
  DocUnlock(*doc);
  return DOCM_CANCEL;
}

extern "C" int64_t DocMenuCpp(CDoc *doc, int64_t dof_flags)
{
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL)
    return DOCM_CANCEL;
  auto *task = CurrentTask();
  auto *old_end = task->task_end_cb;
  const auto old_border = task->border_src;
  const auto old_break = __atomic_fetch_or(
      &task->task_flags, TASKF_BREAK_TO_SHIFT_ESC, __ATOMIC_RELAXED);
  task->task_end_cb = DocMenuEndTaskCBHC;
  task->border_src = 0; // BDS_CONST
  doc->desc = 0x756E654D; // Menu
  uint64_t res;
  const bool okay = CallHolyC2Checked(
      reinterpret_cast<uintptr_t>(DocMenuTryHC),
      reinterpret_cast<uintptr_t>(doc), dof_flags, &res);
  if (!okay && !task->except_ch && !(dof_flags & DOF_INTERCEPT_TASK_END))
    Exit();
  if (old_break & TASKF_BREAK_TO_SHIFT_ESC)
    __atomic_fetch_or(&task->task_flags, TASKF_BREAK_TO_SHIFT_ESC,
                      __ATOMIC_RELAXED);
  else
    __atomic_fetch_and(&task->task_flags, ~TASKF_BREAK_TO_SHIFT_ESC,
                       __ATOMIC_RELAXED);
  task->border_src = old_border;
  task->task_end_cb = old_end;
  if (!okay && task->except_ch)
    RethrowHolyC();
  return okay ? int64_t(res) : DOCM_CANCEL;
}

extern "C" int64_t PopUpMenuCpp(CDoc *doc, int64_t dof_flags)
{
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL)
    return DOCM_CANCEL;
  auto *old_win = doc->win_task;
  auto *old_parent = doc->parent_doc;
  doc->flags |= DOCF_SIZE_MIN | DOCF_FORM;
  const uint64_t args[]{reinterpret_cast<uintptr_t>(doc), uint64_t(dof_flags)};
  auto res = PopUpPrintCpp("DocMenu(0x%X,0x%X);", 2, args);
  doc->win_task = old_win;
  doc->parent_doc = old_parent;
  return res;
}

extern "C" uint64_t PopUpFormCpp(uint8_t *d, const char *class_name,
                                  int64_t dof_flags, const char *header,
                                  const char *footer)
{
  const uint64_t args[]{reinterpret_cast<uintptr_t>(d),
                       reinterpret_cast<uintptr_t>(class_name),
                       uint64_t(dof_flags), reinterpret_cast<uintptr_t>(header),
                       reinterpret_cast<uintptr_t>(footer)};
  return PopUpPrintCpp("DocForm(0x%X,0x%X,0x%X,0x%X,0x%X);", 5, args);
}
