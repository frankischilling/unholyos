#include "../Adam/DolDoc/DolDoc.hpp"

extern "C" {
CKeyDevGlbls keydev;
}

extern "C" void KDInputFilterPutKeyHC(), KDInputFilterPutSHC(),
                KDRawPutKeyHC(), KDRawPutSHC();
extern "C" void CtrlAltCHC(), CtrlAltFHC();

extern "C" void KeyDevRemCpp(CKeyDevEntry *entry)
{
  auto &head = keydev.put_key_head;
  for (auto *next = head.next; next && next != &head; next = next->next)
    if (next == entry) {
      QueRem(*entry);
      Free(entry);
      break;
    }
}

extern "C" CKeyDevEntry *KeyDevAddCpp(uintptr_t put_key, uintptr_t put_s,
                                     int64_t priority, uint8_t key_descs)
{
  auto &head = keydev.put_key_head;
  auto *entry = reinterpret_cast<CKeyDevEntry *>(
      CAlloc(sizeof(CKeyDevEntry), adam_task->data_heap));
  if (!entry)
    return nullptr;
  entry->put_key = put_key;
  entry->put_s = put_s;
  entry->priority = priority;
  entry->flags = key_descs ? KDF_HAS_DESCS : 0;
  auto *previous = head.last;
  while (previous != &head && previous->priority > priority)
    previous = previous->last;
  QueIns(*entry, *previous);
  if (previous != &head && previous->priority == priority) {
    QueRem(*previous);
    Free(previous);
  }
  return entry;
}

void SetKeyDescription(const char *desc, uintptr_t hndlr)
{
  size_t length = StrLen(desc);
  if (length >= sizeof(keydev.desc))
    length = sizeof(keydev.desc) - 1;
  MemCpy(keydev.desc, desc, length);
  keydev.desc[length] = 0;
  keydev.hndlr = hndlr;
}

extern "C" void KeyDescSetCpp(const char *fmt, int64_t argc,
                              const uint64_t *argv, const uintptr_t *frame)
{
  auto *desc = StrPrintJoin(nullptr, fmt, argc, argv);
  if (desc) {
    SetKeyDescription(desc, reinterpret_cast<uintptr_t>(Caller(frame, 0, *CurrentTask())));
    Free(desc);
  }
}

extern "C" void CtrlAltCBSetCpp(uint8_t ch, uintptr_t hndlr, const char *no_shift,
                                const char *shift, uint8_t in_irq)
{
  ch = (ch >= 'a' && ch <= 'z' ? ch - ('a' - 'A') : ch) - 'A';
  if (ch >= 26)
    return;
  auto *heap = adam_task->data_heap;
  auto *normal = no_shift ? StrNew(no_shift, heap) : nullptr;
  auto *shifted = shift ? StrNew(shift, heap) : nullptr;
  if ((no_shift && !normal) || (shift && !shifted)) {
    Free(normal);
    Free(shifted);
    return;
  }
  auto *old_normal = keydev.ctrl_alt_no_shift_descs[ch];
  auto *old_shift = keydev.ctrl_alt_shift_descs[ch];
  keydev.fp_ctrl_alt_cbs[ch] = hndlr;
  keydev.ctrl_alt_no_shift_descs[ch] = normal;
  keydev.ctrl_alt_shift_descs[ch] = shifted;
  if (in_irq)
    keydev.ctrl_alt_in_irq_flags |= uint64_t{1} << ch;
  else
    keydev.ctrl_alt_in_irq_flags &= ~(uint64_t{1} << ch);
  Free(old_normal);
  Free(old_shift);
}

bool KeyDevInit()
{
  auto *heap = adam_task->data_heap;
  keydev.fp_ctrl_alt_cbs = reinterpret_cast<uintptr_t *>(CAlloc(26 * sizeof(uintptr_t), heap));
  keydev.ctrl_alt_no_shift_descs = reinterpret_cast<char **>(CAlloc(26 * sizeof(char *), heap));
  keydev.ctrl_alt_shift_descs = reinterpret_cast<char **>(CAlloc(26 * sizeof(char *), heap));
  auto &head = keydev.put_key_head;
  QueInit(head.next, head.last);
  if (!keydev.fp_ctrl_alt_cbs || !keydev.ctrl_alt_no_shift_descs ||
      !keydev.ctrl_alt_shift_descs)
    return false;
  CtrlAltCBSetCpp('C', reinterpret_cast<uintptr_t>(CtrlAltCHC),
                  "Cmd /Break Execution", nullptr, true);
  CtrlAltCBSetCpp('F', reinterpret_cast<uintptr_t>(CtrlAltFHC),
                  "Cmd /Toggle Aux Font", nullptr, false);
  return KeyDevAddCpp(reinterpret_cast<uintptr_t>(KDInputFilterPutKeyHC),
                     reinterpret_cast<uintptr_t>(KDInputFilterPutSHC), 0x40000000, false) &&
         KeyDevAddCpp(reinterpret_cast<uintptr_t>(KDRawPutKeyHC),
                     reinterpret_cast<uintptr_t>(KDRawPutSHC), 0x60000000, false);
}

// A callback can yield or replace an entry at its own priority.
static const CKeyDevEntry *KeyDevNext(bool first, int64_t priority)
{
  auto &head = keydev.put_key_head;
  for (auto *entry = head.next; entry && entry != &head; entry = entry->next)
    if (first || entry->priority > priority)
      return entry;
  return nullptr;
}

void PutTaskS(CTask &task, const char *text)
{
  if (!text || (task.display_flags & DISPLAYF_SILENT))
    return;
  if (task.display_flags & DISPLAYF_NOT_RAW) {
    auto *target = task.task_flags & TASKF_INPUT_FILTER_TASK ? task.parent_task
                                                            : &task;
    auto *doc = target ? target->put_doc : nullptr;
    if (doc && doc->doc_signature == DOC_SIGNATURE_VAL) {
      DocPutS(*doc, text, StrLen(text));
      return;
    }
  }
  SerialPutS(text);
}

extern "C" void PutS(const char *text)
{
  auto &task = *CurrentTask();
  if (!text || (task.display_flags & DISPLAYF_SILENT))
    return;
  if (!keydev.put_key_head.next) {
    PutTaskS(task, text);
    return;
  }
  while ((kbd.scan_code & SCF_SCROLL) && sys_focus_task == &task)
    _YIELD();
  for (auto *entry = KeyDevNext(true, 0); entry;) {
    auto priority = entry->priority;
    if (auto put_s = entry->put_s) {
      if (uint8_t(CallHolyC1(put_s, reinterpret_cast<uintptr_t>(text))))
        break;
    } else if (auto put_key = entry->put_key) {
      bool handled = false;
      for (const auto *p = reinterpret_cast<const uint8_t *>(text); *p; p++)
        handled |= uint8_t(CallHolyC2(put_key, *p, 0)) != 0;
      if (handled)
        break;
    }
    entry = KeyDevNext(false, priority);
  }
}

extern "C" void PutKeyCpp(int64_t ch, int64_t sc)
{
  auto &task = *CurrentTask();
  if ((!ch && !sc) || (task.display_flags & DISPLAYF_SILENT))
    return;
  while (!(sc & SCF_KEY_DESC) && (kbd.scan_code & SCF_SCROLL) &&
         sys_focus_task == &task)
    _YIELD();
  for (auto *entry = KeyDevNext(true, 0); entry;) {
    auto priority = entry->priority;
    if (entry->put_key &&
        (!(sc & SCF_KEY_DESC) || (entry->flags & KDF_HAS_DESCS)) &&
        uint8_t(CallHolyC2(entry->put_key, ch, sc)))
      break;
    entry = KeyDevNext(false, priority);
  }
}

extern "C" bool KDInputFilterPutKeyCpp(int64_t ch, int64_t sc)
{
  if (!(CurrentTask()->task_flags & TASKF_INPUT_FILTER_TASK))
    return false;
  MsgCpp(MSG_KEY_DOWN, ch, sc, 0);
  return true;
}

extern "C" bool KDInputFilterPutSCpp(const uint8_t *text)
{
  if (!(CurrentTask()->task_flags & TASKF_INPUT_FILTER_TASK))
    return false;
  while (*text)
    MsgCpp(MSG_KEY_DOWN, *text++, 0, 0);
  return true;
}

extern "C" bool KDRawPutSCpp(const char *text)
{
  if (!IsRawCpp() && DocPut())
    return false;
  SerialPutS(text);
  return true;
}

extern "C" bool KDRawPutKeyCpp(int64_t ch, int64_t)
{
  const char text[]{static_cast<char>(ch), 0};
  return KDRawPutSCpp(text);
}

extern "C" void PutCharsCpp(uint64_t chars)
{
  while (chars) {
    PutKeyCpp(chars & 255, 0);
    chars >>= 8;
  }
}

void CtrlAltF(int64_t)
{
  auto *font=text.font;
  text.font=text.aux_font;
  text.aux_font=font;
}

extern "C" {
uint32_t sys_ctrl_alt_flags;
}

void CtrlAltC(int64_t)
{
  __atomic_fetch_or(&sys_ctrl_alt_flags, 1U << 1, __ATOMIC_RELAXED);
}

extern "C" void CtrlAltCCpp(int64_t sc)
{
  CtrlAltC(sc);
}

extern "C" void CtrlAltFCpp(int64_t sc)
{
  CtrlAltF(sc);
}

void KbdCtrlAltKey(int64_t sc)
{
  auto ch = ScanCode2CharCpp(uint8_t(sc));
  if (ch < 'a' || ch > 'z' || !keydev.fp_ctrl_alt_cbs)
    return;
  auto i = ch - 'a';
  auto fn = keydev.fp_ctrl_alt_cbs[i];
  const auto *desc = sc & SCF_SHIFT ? keydev.ctrl_alt_shift_descs[i]
                                    : keydev.ctrl_alt_no_shift_descs[i];
  if (fn && desc)
    CallHolyC1(fn, sc);
}
