#include "DolDoc.hpp"

constexpr size_t DOC_BIN_HEADER_BYTES =
    offsetof(CDocBin, data) - offsetof(CDocBin, num);

static void LoadBins(CDoc &doc, const uint8_t *src, size_t size)
{
  while (size >= DOC_BIN_HEADER_BYTES) {
    uint32_t bytes;
    MemCpy(&bytes, src + 8, sizeof(bytes));
    size_t available = size - DOC_BIN_HEADER_BYTES;
    if (bytes > available && bytes - available != 1)
      return;
    size_t stored = bytes < available ? bytes : available;
    auto *hc = MemTaskHeap(doc.mem_task);
    auto *bin = reinterpret_cast<CDocBin *>(CAlloc(sizeof(CDocBin), hc));
    if (!bin)
      return;
    bin->data = MAlloc(bytes, hc);
    if (!bin->data) {
      Free(bin);
      return;
    }
    MemCpy(&bin->num, src, DOC_BIN_HEADER_BYTES);
    src += DOC_BIN_HEADER_BYTES;
    MemCpy(bin->data, src, stored);
    if (stored < bytes)
      bin->data[stored] = 0; // FileRead supplies this byte after the file.
    src += stored;
    size -= DOC_BIN_HEADER_BYTES + stored;
    auto &head = doc.bin_head;
    bin->last = head.last;
    bin->next = &head;
    head.last->next = bin;
    head.last = bin;
    if (bin->num >= doc.cur_bin_num)
      doc.cur_bin_num = bin->num + 1;
  }
}

extern "C" void DocLoadCpp(CDoc *doc, const uint8_t *src, int64_t size)
{
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL || size < 0)
    return;
  CDocLock lock(*doc);
  doc->find_replace->filter_lines = 0;
  if (src) {
    size_t text = 0, total = size;
    while (text < total && src[text])
      text++;
    DocPutS(*doc, reinterpret_cast<const char *>(src), text);
    if (text < total)
      LoadBins(*doc, src + text + 1, total - text - 1);
  }
  if (!(doc->flags & DOCG_PLAIN))
    DocBinsValidate(*doc);
  doc->cur_entry = doc->head.next;
  doc->cur_col = doc->cur_entry->min_col;
  doc->line_start_col = doc->top_line_num = 0;
  DocRecalc(*doc);
}

extern "C" CDoc *DocReadCpp(const char *name, int64_t flags)
{
  auto *path = FileNameAbs(name ? name : blkdev.tmp_filename, *CurrentTask());
  auto *doc = DocNewCpp(path, nullptr);
  if (doc) {
    doc->flags |= flags;
    size_t size = 0;
    auto *src = path ? FileReadCpp(path, &size, &doc->file_attr) : nullptr;
    if (src && size <= INT64_MAX)
      DocLoadCpp(doc, src, size);
    Free(src);
  }
  Free(path);
  return doc;
}

static bool PlainEntry(const CDocEntryBase &entry)
{
  return entry.Type() == DOCT_TEXT && !(entry.de_flags & ~DOCG_PLAIN_ENTRY) &&
         !(entry.type & DOCG_BL_IV_UL);
}

static void SaveDoc(CDocOut &out, const CDoc &doc)
{
  const auto &head = doc.head;
  bool cursor = !(doc.flags & DOCF_NO_CURSOR);
  bool escape_dollars =
      !(doc.flags & DOCG_PLAIN) || (doc.flags & DOCF_DBL_DOLLARS);
  for (auto *entry = head.next; entry != &head; entry = entry->next) {
    bool here = cursor && entry == doc.cur_entry;
    if (here && (!PlainEntry(*entry) || doc.cur_col <= 0) && entry != head.next)
      out.Put(CH_CURSOR);
    switch (entry->Type()) {
    case DOCT_TAB:
      out.Put('\t');
      break;
    case DOCT_NEW_LINE:
      if (doc.flags & DOCF_CARRIAGE_RETURN)
        out.Put('\r');
      out.Put('\n');
      break;
    case DOCT_SOFT_NEW_LINE:
    case DOCT_DATA:
    case DOCT_CHECK_BOX:
    case DOCT_HEX_ED:
      break;
    case DOCT_TEXT:
      if (PlainEntry(*entry)) {
        int32_t col = 0;
        for (auto *src = entry->tag; src && *src; src++, col++) {
          if (here && col && col == doc.cur_col)
            out.Put(CH_CURSOR);
          out.Put(*src);
          if (escape_dollars && *src == '$')
            out.Put('$');
        }
        if (here && col && doc.cur_col >= col)
          out.Put(CH_CURSOR);
        break;
      }
      [[fallthrough]];
    default:
      out.Put('$');
      Doc2PlainText(out, doc, *entry);
      out.Put('$');
      break;
    }
  }
  if (cursor && doc.cur_entry == &head && head.next != &head)
    out.Put(CH_CURSOR);
  out.Put(char{0});
  for (auto *bin = doc.bin_head.next; bin != &doc.bin_head; bin = bin->next) {
    if (bin->use_cnt > uint32_t(bin->tmp_use_cnt)) {
      out.Put(&bin->num, DOC_BIN_HEADER_BYTES);
      out.Put(bin->data, bin->size);
    }
  }
}

uint8_t *DocSave(CDoc &doc, size_t *size)
{
  CDocLock lock(doc);
  if (!(doc.flags & DOCG_PLAIN))
    DocBinsValidate(doc);
  DocRecalc(doc);
  CDocOut measure;
  SaveDoc(measure, doc);
  auto *data = MAlloc(measure.len, CurrentTask()->data_heap);
  if (!data)
    return nullptr;
  CDocOut out(data);
  SaveDoc(out, doc);
  if (size)
    *size = out.len - (doc.bin_head.next == &doc.bin_head);
  return data;
}

extern "C" uint8_t *DocSaveCpp(CDoc *doc, size_t *size)
{
  return doc && doc->doc_signature == DOC_SIGNATURE_VAL ? DocSave(*doc, size)
                                                        : nullptr;
}

extern "C" uint8_t *DocSaveNoCursorCpp(CDoc *doc)
{
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL)
    return nullptr;
  CDocLock lock(*doc);
  auto flags = doc->flags;
  doc->flags |= DOCF_NO_CURSOR;
  auto *data = DocSave(*doc, nullptr);
  doc->flags = flags;
  return data;
}

extern "C" uint64_t FileWriteCpp(const char *filename, const uint8_t *data,
                                 uint64_t size, uint64_t datetime,
                                 uint64_t attr);

extern "C" uint64_t DocWriteCpp(CDoc *doc, uint8_t prompt)
{
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL || prompt ||
      (doc->filename.name[0] == 'A' && doc->filename.name[2] == ':'))
    return 0;
  size_t size = 0;
  auto *data = DocSave(*doc, &size);
  if (!data)
    return 0;
  FileWriteCpp(doc->filename.name, data, size, 0, doc->file_attr);
  Free(data);
  return 1;
}

static bool DocInsDoc(CDoc &doc, CDoc &source)
{
  CDocLock lock_source(source);
  CDocLock lock(doc);
  const auto bin_num = doc.cur_bin_num;
  CDocEntryBase staged{};
  staged.next = staged.last = &staged;
  auto discard = [&] {
    while (staged.next != &staged)
      DocEntryDel(doc, *staged.next);
    doc.cur_bin_num = bin_num;
    return false;
  };
  for (auto *entry = source.head.next; entry != &source.head;
       entry = entry->next) {
    if (entry->Type() == DOCT_SOFT_NEW_LINE)
      continue;
    auto *copy = DocEntryCopy(doc, *entry);
    if (!copy)
      return discard();
    DocEntryInsert(*copy, staged);
  }
  if (staged.next != &staged) {
    auto *first = staged.next;
    auto *next = first->next;
    if (!DocInsEntry(doc, *first))
      return discard();
    while (next != &staged) {
      auto *entry = next;
      next = entry->next;
      DocEntryInsert(*entry, *doc.cur_entry);
    }
  }
  DocRecalc(doc);
  return true;
}

extern "C" uint64_t DocTypeCpp(CDoc *doc, const char *name,
                               int64_t trailing_new_lines)
{
  if (!doc)
    doc = DocPut();
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL ||
      !FilesFindMatch(name, FILEMASK_TXT))
    return 0;
  auto *source = DocReadCpp(name, 0);
  if (!source)
    return 0;
  CDocLock lock(*doc);
  bool inserted = DocInsDoc(*doc, *source);
  if (inserted && IsRawCpp())
    DocDump(*source, 100000);
  DocDelCpp(source);
  if (inserted) {
    for (int64_t i = 0; i < trailing_new_lines; i++)
      DocPutS(*doc, "\n", 1);
  }
  return inserted;
}

extern "C" uint64_t TypeCpp(const char *name, int64_t trailing_new_lines)
{
  return DocTypeCpp(nullptr, name, trailing_new_lines);
}
