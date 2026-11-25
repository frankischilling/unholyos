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

extern "C" void DocBinsValidateCpp(CDoc *doc)
{
  if (doc && doc->doc_signature == DOC_SIGNATURE_VAL)
    DocBinsValidate(*doc);
}
