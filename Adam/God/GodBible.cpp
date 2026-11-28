#include "GodBible.hpp"
#include "../DolDoc/DolDoc.hpp"

extern "C" int64_t PopUpCpp(const char *src, CTask *parent, CTask **popup);
extern "C" void CtrlAltBHC();

bool BibleInit()
{
  if (!DefineLstLoadCpp("ST_BIBLE_BOOKS", ST_BIBLE_BOOKS, nullptr) ||
      !DefineLstLoadCpp("ST_BIBLE_BOOK_LINES", ST_BIBLE_BOOK_LINES, nullptr))
    return false;
  const auto last = DefineCntCpp("ST_BIBLE_BOOK_LINES") - 1;
  const uint64_t lines =
      Str2I64Cpp(DefineSubCpp(last, "ST_BIBLE_BOOK_LINES"), 10, nullptr) - 1;
  char text[32];
  if (!DefineLoadCpp("ST_BIBLE_LINES", StrPrintJoin(text, "%d", 1, &lines),
                     nullptr))
    return false;
  CtrlAltCBSetCpp('B', reinterpret_cast<uintptr_t>(CtrlAltBHC), "Cmd /Bible",
                  nullptr, false);
  return true;
}

static bool BibleDigit(char ch)
{
  return '0' <= ch && ch <= '9';
}

extern "C" char *BibleLine2VerseCpp(int64_t line, int64_t separate_ch)
{
  const auto cnt = DefineCntCpp("ST_BIBLE_BOOKS");
  int64_t i = 0, start = 0, last_start = 0;
  auto *ptr = DefineSubCpp(0, "ST_BIBLE_BOOK_LINES");
  while (i <= cnt && ptr && *ptr) {
    last_start = start;
    start = Str2I64Cpp(ptr, 10, nullptr);
    if (line < start)
      break;
    i++;
    ptr += StrLen(ptr) + 1;
  }
  if (--i < 0 || i >= cnt)
    return nullptr;

  auto *doc = DocReadCpp(BIBLE_FILENAME, DOCF_PLAIN_TEXT | DOCF_NO_CURSOR);
  if (!doc)
    return nullptr;
  DocGoToLineCpp(doc, line);
  auto *entry = doc->cur_entry;
  while (entry != &doc->head &&
         (entry->Type() != DOCT_TEXT || !entry->tag || !*entry->tag))
    entry = entry->next;

  char *first = nullptr, *end = nullptr;
  bool book_only = false;
  if (entry != &doc->head && BibleDigit(*entry->tag)) {
    first = end = entry->tag;
    while (BibleDigit(*end) || *end == ':')
      end++;
  } else {
    while ((entry = entry->last) != &doc->head) {
      if (entry->y + 1 < last_start) {
        book_only = true;
        break;
      }
      if (entry->Type() != DOCT_TEXT || !entry->tag)
        continue;
      auto *tag = entry->tag;
      const auto len = StrLen(tag);
      for (size_t j = len; j > 2; j--) {
        auto *colon = tag + j - 2;
        if (*colon == ':' && BibleDigit(colon[-1]) && BibleDigit(colon[1])) {
          first = colon - 1;
          while (first > tag && BibleDigit(first[-1]))
            first--;
          end = colon + 2;
          while (BibleDigit(*end))
            end++;
          break;
        }
      }
      if (first)
        break;
    }
  }

  char *res = nullptr;
  if (first) {
    *end = 0;
    const uint64_t args[]{
        uint64_t(i), reinterpret_cast<uintptr_t>("ST_BIBLE_BOOKS"),
        uint64_t(separate_ch), reinterpret_cast<uintptr_t>(first)};
    if (separate_ch == '%') {
      const uint64_t escaped[]{args[0], args[1], args[3]};
      res = MStrPrintCpp("%Z%%20%s", 3, escaped);
    } else
      res = MStrPrintCpp("%Z%c%s", 4, args);
  } else if (book_only) {
    const uint64_t args[]{uint64_t(i),
                          reinterpret_cast<uintptr_t>("ST_BIBLE_BOOKS")};
    res = MStrPrintCpp("%Z", 2, args);
  }
  DocDelCpp(doc);
  return res;
}

static void BiblePutLines(CDoc *doc_out, CDoc *doc_in, int64_t lines)
{
  for (auto *entry = doc_in->cur_entry; lines > 0 && entry != &doc_in->head;
       entry = entry->next) {
    if (entry->Type() == DOCT_TEXT) {
      const uint64_t arg = reinterpret_cast<uintptr_t>(entry->tag);
      DocPrintCpp(doc_out, "%s", 1, &arg);
    } else if (entry->Type() == DOCT_NEW_LINE) {
      DocPutKeyCpp(doc_out, '\n', 0);
      lines--;
    }
  }
}

extern "C" void BookLinesCpp(CDoc *doc_out, int64_t start, int64_t lines,
                             const char *book_filename)
{
  if (!doc_out)
    doc_out = DocPut();
  if (doc_out && FileFindCpp(book_filename, nullptr, 0))
    if (auto *doc_in =
            DocReadCpp(book_filename, DOCF_PLAIN_TEXT | DOCF_NO_CURSOR)) {
      DocGoToLineCpp(doc_in, start);
      BiblePutLines(doc_out, doc_in, lines);
      DocDelCpp(doc_in);
    }
}

// EdFindNext's forward, case-sensitive scan for a fresh plain-text document.
static void BibleFindNext(CDoc &doc, const char *verse)
{
  auto *start = doc.cur_entry;
  auto *entry = start;
  do {
    if (entry != &doc.head && entry->Type() == DOCT_TEXT && entry->tag) {
      auto col = entry == start ? doc.cur_col + 1 : entry->min_col;
      if (col < entry->max_col)
        if (auto *found = StrFindCpp(verse, entry->tag + col, 0)) {
          doc.cur_entry = entry;
          auto at = found - entry->tag;
          doc.cur_col = at < entry->max_col ? at : entry->max_col - 1;
          DocCenterCpp(&doc, RECALCt_NORMAL);
          return;
        }
    }
    entry = entry->next;
  } while (entry != start);
}

extern "C" void BibleVerseCpp(CDoc *doc_out, const char *verse, int64_t lines)
{
  if (!doc_out)
    doc_out = DocPut();
  if (!doc_out || !verse || !FileFindCpp(BIBLE_FILENAME, nullptr, 0))
    return;
  auto *book = StrNew(verse, CurrentTask()->data_heap);
  if (!book)
    return;
  const char *needle = "";
  if (auto *comma =
          StrLastOccCpp(book, reinterpret_cast<const uint8_t *>(","))) {
    *comma = 0;
    needle = comma + 1;
  }
  const auto i = DefineMatchCpp(reinterpret_cast<const uint8_t *>(book),
                                "ST_BIBLE_BOOKS", 1); // LMF_IGNORE_CASE
  if (i >= 0)
    if (auto *doc_in =
            DocReadCpp(BIBLE_FILENAME, DOCF_PLAIN_TEXT | DOCF_NO_CURSOR)) {
      const auto start =
          Str2I64Cpp(DefineSubCpp(i, "ST_BIBLE_BOOK_LINES"), 10, nullptr);
      DocGoToLineCpp(doc_in, start);
      if (*needle)
        BibleFindNext(*doc_in, needle);
      BiblePutLines(doc_out, doc_in, lines);
      DocDelCpp(doc_in);
    }
  Free(book);
}

extern "C" CDoc *BibleDocCpp()
{
  auto *res = DocReadCpp(BIBLE_FILENAME, 0);
  if (!res)
    return nullptr;
  auto *entry = res->head.next;
  auto *ptr = DefineCpp("ST_BIBLE_BOOK_LINES");
  const auto book_cnt = DefineCntCpp("ST_BIBLE_BOOK_LINES") - 1;
  auto line = Str2I64Cpp(ptr, 10, &ptr);
  ptr++;
  for (int64_t book = 0; book < book_cnt && entry != &res->head; book++) {
    while (entry != &res->head && entry->y + 1 < line) {
      auto *next = entry->next;
      DocEntryDel(*res, *entry);
      entry = next;
    }
    if (entry == &res->head)
      break;
    res->cur_entry = entry;
    res->cur_col = 0;
    const uint64_t args[]{uint64_t(book),
                          reinterpret_cast<uintptr_t>("ST_BIBLE_BOOKS")};
    DocPrintCpp(res, "$TR,\"%Z\"$\n$ID,2$", 2, args);
    line = Str2I64Cpp(ptr, 10, &ptr);
    ptr++;
    uint64_t chapter = 1;
    while (entry != &res->head && entry->y + 1 < line) {
      auto *next = entry->next;
      if (entry->Type() == DOCT_TEXT && entry->tag &&
          StrMatchCpp(":1 ", entry->tag)) {
        res->cur_entry = entry;
        res->cur_col = 0;
        if (chapter > 1)
          DocPrintCpp(res, "$ID,-2$", 0, nullptr);
        DocPrintCpp(res, "$TR,\"%d\"$\n$ID,2$", 1, &chapter);
        chapter++;
      } else if (chapter == 1)
        DocEntryDel(*res, *entry);
      entry = next;
    }
    res->cur_entry = entry;
    res->cur_col = 0;
    if (chapter > 1)
      DocPrintCpp(res, "$ID,-2$", 0, nullptr);
    DocPrintCpp(res, "$ID,-2$", 0, nullptr);
  }
  while (entry != &res->head) {
    auto *next = entry->next;
    DocEntryDel(*res, *entry);
    entry = next;
  }
  return res;
}

extern "C" void BibleViewCpp()
{
  if (auto *doc = BibleDocCpp()) {
    char buf[32];
    const uint64_t arg = reinterpret_cast<uintptr_t>(doc);
    EdCpp(StrPrintJoin(buf, "AL:%d,1", 1, &arg), 0);
    DocDelCpp(doc);
  }
}

extern "C" void CtrlAltBCpp(int64_t)
{
  PopUpCpp("BibleView;", nullptr, nullptr);
}
