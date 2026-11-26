#include "DolDoc.hpp"

void DocBinFree(CDocBin &bin)
{
  bin.last->next = bin.next;
  bin.next->last = bin.last;
  Free(bin.tag);
  Free(bin.data);
  Free(&bin);
}

void DocBinDel(CDoc &, CDocBin *bin)
{
  if (bin && bin->use_cnt && !--bin->use_cnt)
    DocBinFree(*bin);
}

static CDocBin *DocBinFindNum(CDoc &doc, uint32_t num)
{
  for (auto *bin = doc.bin_head.next; bin != &doc.bin_head; bin = bin->next)
    if (bin->num == num)
      return bin;
  return nullptr;
}

static CDocBin *DocBinFindTag(CDoc &doc, const char *tag)
{
  for (auto *bin = doc.bin_head.next; bin != &doc.bin_head; bin = bin->next)
    if (bin->tag && !StrCmpCpp(reinterpret_cast<const uint8_t *>(bin->tag),
                               reinterpret_cast<const uint8_t *>(tag)))
      return bin;
  return nullptr;
}

CDocBin *DocBinCopy(CDoc &doc, const CDocBin &source, const char *tag)
{
  auto *bin =
      reinterpret_cast<CDocBin *>(MAllocIdentCpp(&source, doc.mem_task));
  if (!bin)
    return nullptr;
  bin->data = MAllocIdentCpp(source.data, doc.mem_task);
  bin->tag = tag && *tag ? StrNew(tag, MemTaskHeap(doc.mem_task)) : nullptr;
  if ((source.data && !bin->data) || (tag && *tag && !bin->tag)) {
    Free(bin->tag);
    Free(bin->data);
    Free(bin);
    return nullptr;
  }
  return bin;
}

void DocBinsValidate(CDoc &doc)
{
  CDocLock lock(doc);
  for (auto *bin = doc.bin_head.next; bin != &doc.bin_head; bin = bin->next) {
    bin->use_cnt = bin->tmp_use_cnt = 0;
    bin->renum_num = -1;
    Free(bin->tag);
    bin->tag = nullptr;
  }
  uint32_t renum = 0;
  for (auto *base = doc.head.next; base != &doc.head; base = base->next) {
    if (!(base->de_flags & DOCEF_HAS_BIN))
      continue;
    auto *entry = reinterpret_cast<CDocEntry *>(base);
    auto *bin = DocBinFindNum(doc, entry->bin_num);
    entry->bin_data = bin;
    if (!bin) {
      PutS("Bin Not Found\n");
      DocEntryError(*entry);
      continue;
    }
    if (base->de_flags & DOCEF_BIN_PTR_LINK)
      bin->tmp_use_cnt = INT32_MAX;
    if (!bin->use_cnt++)
      bin->renum_num = ++renum;
    entry->bin_num = bin->renum_num;
    if (!bin->tag && (base->de_flags & DOCEF_TAG) && base->tag && *base->tag)
      bin->tag = StrNew(base->tag, MemTaskHeap(doc.mem_task));
  }
  doc.cur_bin_num = 1;
  for (auto *bin = doc.bin_head.next; bin != &doc.bin_head;) {
    auto *next = bin->next;
    if (!bin->use_cnt)
      DocBinFree(*bin);
    else {
      bin->num = bin->renum_num;
      if (bin->num >= doc.cur_bin_num)
        doc.cur_bin_num = bin->num + 1;
    }
    bin = next;
  }
}

bool DocBinPtrRst(CDoc &doc, CDocEntry &entry, const CDocReadContext *context)
{
  if (!(entry.base.de_flags & DOCEF_HAS_BIN) || !entry.bin_ptr_link)
    return false;
  auto *path = StrNew(entry.bin_ptr_link, CurrentTask()->data_heap);
  if (!path)
    return false;
  char *selector = nullptr;
  for (auto *p = path; *p; p++)
    if (*p == ',')
      selector = p;
  if (!selector || !selector[1]) {
    Free(path);
    return false;
  }
  *selector++ = 0;
  int64_t index = Str2I64Cpp(selector, 10, nullptr);
  auto *source =
      index >= 0 && index <= UINT32_MAX ? DocRead(path, 0, context) : nullptr;
  const CDocBin *data = nullptr;
  if (source)
    data = index ? DocBinFindNum(*source, index)
                 : DocBinFindTag(*source, selector);
  uint32_t num = entry.bin_num > 0 ? uint32_t(entry.bin_num) : doc.cur_bin_num;
  const char *tag = entry.base.de_flags & DOCEF_TAG ? entry.base.tag : nullptr;
  auto *copy = data && num < INT32_MAX ? DocBinCopy(doc, *data, tag) : nullptr;
  DocDelCpp(source);
  Free(path);
  if (!copy)
    return false;
  copy->num = num;
  copy->tmp_use_cnt = INT32_MAX;
  copy->use_cnt = 1;
  if (auto *old = DocBinFindNum(doc, num)) {
    for (auto *base = doc.head.next; base != &doc.head; base = base->next) {
      if (!(base->de_flags & DOCEF_HAS_BIN))
        continue;
      auto *other = reinterpret_cast<CDocEntry *>(base);
      if (other->bin_data == old) {
        other->bin_data = copy;
        copy->use_cnt++;
      }
    }
    DocBinFree(*old);
  }
  if (num >= doc.cur_bin_num)
    doc.cur_bin_num = num + 1;
  QueIns(*copy, *doc.bin_head.last);
  entry.bin_num = num;
  entry.bin_data = copy;
  return true;
}

extern "C" void DocBinsValidateCpp(CDoc *doc)
{
  if (doc && doc->doc_signature == DOC_SIGNATURE_VAL)
    DocBinsValidate(*doc);
}
