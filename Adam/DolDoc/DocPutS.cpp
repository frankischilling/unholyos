#include "DolDoc.hpp"

static bool PutBase(CDoc &doc, uint8_t type, const char *text = nullptr,
                    size_t size = 0, bool final = false, uint32_t columns = 0)
{
  auto *entry =
      DocEntryNewBase(doc, type | (doc.settings_head.dft_text_attr << 8));
  if (!entry)
    return false;
  if (text) {
    auto *tag =
        reinterpret_cast<char *>(MAlloc(size + 1, MemTaskHeap(doc.mem_task)));
    if (!tag) {
      Free(entry);
      return false;
    }
    size_t len = 0;
    for (size_t i = 0; i < size; i++) {
      unsigned char ch = text[i];
      if (!(doc.flags & DOCF_NO_CURSOR) ||
          (ch != CH_CURSOR && (!final || ch >= 0x1F)))
        tag[len++] = ch;
    }
    tag[len] = 0;
    entry->tag = tag;
    entry->max_col = columns ? columns : len;
  }
  if (!DocInsEntry(doc, *entry)) {
    Free(entry->tag);
    Free(entry);
    return false;
  }
  return true;
}

CDocEntryBase *DocPutS(CDoc &doc, const char *src, size_t size)
{
  if (!src)
    return nullptr;
  CDocLock lock(doc);
  CDocEntryBase *last_command = nullptr;
  const char *end = src + size;
  while (src < end && *src) {
    const char *first = src;
    while (src < end && *src && *src != '\r' && *src != '\n' &&
           (*src != CH_CURSOR || (doc.flags & DOCF_NO_CURSOR)) &&
           (*src != '\t' || (doc.flags & DOCF_PLAIN_TEXT_TABS)) &&
           (*src != '$' || (doc.flags & DOCG_PLAIN)))
      src++;
    bool final = src == end || !*src;
    if (src != first && !PutBase(doc, DOCT_TEXT, first, src - first, final))
      break;
    if (final)
      break;
    char ch = *src++;
    if (ch == CH_CURSOR || ch == '\t') {
      if (!PutBase(doc, ch == CH_CURSOR ? DOCT_CURSOR : DOCT_TAB))
        break;
    } else if (ch == '$') {
      const char *body = src;
      while (src < end && *src && *src != '$')
        src++;
      if (src == end || !*src)
        break;
      const char *closing = src++;
      if (closing == body) {
        if (!PutBase(doc, DOCT_TEXT, "$$", doc.flags & DOCF_DBL_DOLLARS ? 2 : 1,
                     false, 1))
          break;
        continue;
      }
      size_t cursors = 0;
      for (auto *p = body; p < closing; p++)
        cursors += *p == CH_CURSOR;
      char *clean = nullptr;
      if (cursors) {
        for (size_t i = 0; i < cursors; i++)
          PutBase(doc, DOCT_CURSOR);
        if (closing - body == static_cast<ptrdiff_t>(cursors))
          continue;
        clean = reinterpret_cast<char *>(
            MAlloc(closing - body - cursors, MemTaskHeap(doc.mem_task)));
        if (!clean)
          break;
        char *write = clean;
        for (auto *p = body; p < closing; p++)
          if (*p != CH_CURSOR)
            *write++ = *p;
        body = clean;
        closing = write;
      }
      auto *entry = PrsDollarCmd(doc, body, closing);
      Free(clean);
      if (!entry)
        break;
      if (!DocInsEntry(doc, entry->base)) {
        entry->base.next = entry->base.last = &entry->base;
        DocEntryDel(doc, entry->base);
        break;
      }
      last_command = &entry->base;
    } else {
      if (!PutBase(doc, DOCT_NEW_LINE))
        break;
      if (ch == '\r') {
        while (src < end && *src == '\r')
          src++;
        if (src < end && *src == '\n')
          src++;
      }
      while (src < end && *src == '\r')
        src++;
    }
  }
  return last_command;
}

extern "C" CDocEntryBase *DocPrintCpp(CDoc *doc, const char *fmt, int64_t argc,
                                      const uint64_t *argv)
{
  if (!doc)
    doc = DocPut();
  auto *text = StrPrintJoin(nullptr, fmt, argc, argv);
  auto *entry = doc && doc->doc_signature == DOC_SIGNATURE_VAL
                    ? DocPutS(*doc, text, StrLen(text))
                    : nullptr;
  Free(text);
  return entry;
}

extern "C" CDocEntryBase *DocPutLineCpp(CDoc *doc, CDocEntryBase *entry)
{
  if (!doc)
    doc = DocPut();
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL || !entry)
    return nullptr;
  CDocLock lock(*doc);
  while (entry != &doc->head && entry->Type() != DOCT_NEW_LINE) {
    if ((entry->de_flags & DOCEF_TAG) && entry->tag) {
      for (const char *tag = entry->tag; *tag; tag++) {
        char bytes[]{*tag, static_cast<char>(*tag == '$' ? '$' : 0), 0};
        PutS(bytes);
      }
    } else if (entry->Type() == DOCT_TAB) {
      PutS("\t");
    }
    entry = entry->next;
  }
  PutS("\n");
  return entry == &doc->head ? entry : entry->next;
}
