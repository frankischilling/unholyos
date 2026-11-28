#include "../../Kernel/KernelB.hpp"
#include "../DolDoc/DolDoc.hpp"
#include "AutoComplete.hpp"

// Dictionary generator by Terry A. Davis. See Doc/Credits.DD for the input.

extern "C" int64_t ToUpperCpp(uint8_t ch);
extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);

static uint8_t ACDHexDigit(uint8_t ch)
{
  ch = ToUpperCpp(ch);
  if ('0' <= ch && ch <= '9')
    return ch - '0';
  if ('A' <= ch && ch <= 'F')
    return ch - 'A' + 10;
  return 0;
}

static bool ACDPreprocess(const char *in_name, const char *out_name)
{
  if (!FileFindCpp(in_name, nullptr, FUF_JUST_FILES))
    return false;
  auto *doc = DocReadCpp(in_name, DOCF_PLAIN_TEXT_TABS | DOCF_DBL_DOLLARS);
  if (!doc)
    return false;
  for (auto *doc_e = doc->head.next; doc_e != &doc->head; doc_e = doc_e->next) {
    if (doc_e->Type() != DOCT_TEXT || !doc_e->tag)
      continue;
    auto *src = doc_e->tag, *dst = src;
    while (*src) {
      auto ch = *src++;
      if (ch == '\\' && *src == '\'' && src[1] && src[2]) {
        ch = (ACDHexDigit(src[1]) << 4) | ACDHexDigit(src[2]);
        src += 3;
      }
      *dst++ = ch;
    }
    *dst = 0;
  }
  size_t size = 0;
  auto *data = DocSave(*doc, &size);
  bool written = data && FileWriteCpp(out_name, data, size, 0, doc->file_attr);
  Free(data);
  DocDelCpp(doc);
  return written;
}

static int64_t ACDNextCmd(uint8_t *&ptr)
{
  static constexpr char tags[] =
      "h1\0/h1\0def\0/def\0hw\0/hw\0tt\0/tt\0"
      "ety\0@fld\0@cd\0@blockquote\0@wordforms\0@note\0@altname\0@chform\0"
      "@cref\0@syn\0/ety\0@/fld\0@/cd\0@/blockquote\0@/wordforms\0@/note\0"
      "@/altname\0@/chform\0@/cref\0@/syn\0";
  while (*ptr) {
    while (*ptr && *ptr != '<')
      ptr++;
    if (!*ptr)
      return -1;
    auto *start = ++ptr;
    while (*ptr && *ptr != '>')
      ptr++;
    if (!*ptr)
      return -1;
    *ptr = 0;
    auto res = LstMatchCpp(start, reinterpret_cast<const uint8_t *>(tags), 0);
    *ptr++ = '>';
    if (res >= 0)
      return res;
  }
  return -1;
}

static bool ACDNextEntry(uint8_t *&ptr, char *buf)
{
  static constexpr char ignored[] =
      "b>\0i>\0ppp>\0/b>\0/i>\0/p>\0"
      "ets>\0col>\0spn>\0/ets>\0/col>\0/spn>\0er>\0as>\0cs>\0cd>\0ex>\0"
      "/er>\0/as>\0/cs>\0/cd>\0/ex>\0"
      "note>\0/note>\0blockquote>\0/blockquote>\0";
  auto *out_ptr = buf;
  while (*ptr) {
    if (*ptr != '<') {
      auto ch = *ptr++;
      if (buf + ACD_BLK_SIZE - out_ptr <= (ch == '$' ? 2 : 1))
        return false;
      *out_ptr++ = ch;
      if (ch == '$')
        *out_ptr++ = ch;
      continue;
    }
    auto *ignore = ignored;
    while (*ignore) {
      auto len = StrLen(ignore);
      if (!StrNCmpCpp(reinterpret_cast<const char *>(ptr + 1), ignore, len)) {
        ptr += len + 1;
        break;
      }
      ignore += len + 1;
    }
    if (!*ignore)
      break;
  }
  *out_ptr = 0;
  return true;
}

static int64_t ACDCompareWords(int64_t e1, int64_t e2)
{
  return StrICmpCpp(reinterpret_cast<const char *>(e1),
                    reinterpret_cast<const char *>(e2));
}

static bool ACDSortWords(const uint8_t *start, uint8_t *out, size_t word_cnt)
{
  if (word_cnt > SIZE_MAX / sizeof(int64_t))
    return false;
  auto *order = reinterpret_cast<int64_t *>(MAlloc(
      (word_cnt ? word_cnt : 1) * sizeof(int64_t), MemTaskHeap(nullptr)));
  if (!order)
    return false;
  auto *ptr = start;
  for (size_t i = 0; i < word_cnt; i++) {
    order[i] = reinterpret_cast<uintptr_t>(ptr);
    ptr += StrLen(reinterpret_cast<const char *>(ptr)) + 3;
  }
  PutS("Sorting...\n");
  SleepCpp(100);
  QSortI64Native(order, word_cnt, ACDCompareWords);
  PutS("Done...\n");
  SleepCpp(100);
  for (size_t i = 0; i < word_cnt; i++) {
    auto *word = reinterpret_cast<const uint8_t *>(order[i]);
    auto size = StrLen(reinterpret_cast<const char *>(word)) + 3;
    MemCpy(out, word, size);
    out += size;
  }
  *out = ACD_END_CHAR;
  Free(order);
  return true;
}

static bool ACDGen(const char *in_file)
{
  size_t size = 0;
  auto *in_start = FileReadCpp(in_file, &size, nullptr);
  if (!in_start)
    return false;
  if (size > (SIZE_MAX - 1) / 2) {
    Free(in_start);
    return false;
  }
  auto *heap = MemTaskHeap(nullptr);
  // The tags pay for the record headers. Dollar escaping can add one byte
  // per input byte, and even an empty input needs an end marker.
  auto *out_start = MAlloc(size * 2 + 1, heap);
  auto *word_start = MAlloc(size * 2 + 1, heap);
  auto *entry = reinterpret_cast<char *>(MAlloc(ACD_BLK_SIZE, heap));
  auto generate = [&] {
    if (!out_start || !word_start || !entry)
      return false;
    auto *in_ptr = in_start, *out_ptr = out_start, *word_ptr = word_start;
    auto *def_word_start = out_start;
    const char *last_word = "";
    size_t word_cnt = 0, largest_entry = 0;
    auto cmd = ACDNextCmd(in_ptr);
    while (cmd >= 0) {
      if (cmd != ACD_H1) {
        cmd = ACDNextCmd(in_ptr);
        continue;
      }
      auto length = size_t(out_ptr - def_word_start);
      if (length > largest_entry)
        largest_entry = length;
      def_word_start = out_ptr;
      if (!ACDNextEntry(in_ptr, entry))
        return false;
      if (!*entry) {
        cmd = ACDNextCmd(in_ptr);
        continue;
      }
      if (StrICmpCpp(entry, last_word)) {
        word_cnt++;
        length = StrLen(entry) + 1;
        *word_ptr++ = ACD_WORD_CHAR;
        last_word = reinterpret_cast<char *>(word_ptr);
        MemCpy(word_ptr, entry, length);
        word_ptr += length;
        uint16_t block = (out_ptr - out_start) / ACD_BLK_SIZE;
        MemCpy(word_ptr, &block, sizeof(block));
        word_ptr += sizeof(block);
        *out_ptr++ = ACD_WORD_CHAR;
        MemCpy(out_ptr, entry, length);
        out_ptr += length;
      }
      while ((cmd = ACDNextCmd(in_ptr)) >= 0 && cmd != ACD_H1) {
        uint8_t tag;
        switch (cmd) {
        case ACD_DEF:
          tag = ACD_DEF_CHAR;
          break;
        case ACD_PRONUNCIATION:
          tag = ACD_PRONUNCIATION_CHAR;
          break;
        case ACD_POS:
          tag = ACD_POS_CHAR;
          break;
        case ACD_EXTRA:
          tag = ACD_EXTRA_CHAR;
          break;
        default:
          continue;
        }
        if (!ACDNextEntry(in_ptr, entry))
          return false;
        if (*entry) {
          auto bytes = StrLen(entry) + 1;
          *out_ptr++ = tag;
          MemCpy(out_ptr, entry, bytes);
          out_ptr += bytes;
        }
      }
    }
    *out_ptr++ = ACD_END_CHAR;
    *word_ptr++ = ACD_END_CHAR;
    const uint64_t def_size = out_ptr - out_start,
                   word_size = word_ptr - word_start;
    const uint64_t args[]{ACD_BLK_SIZE,
                          (def_size + ACD_BLK_SIZE - 1) / ACD_BLK_SIZE,
                          largest_entry, word_cnt};
    PrintCpp("Blk Size      :%d\nBlk Cnt       :%04X\n"
             "Largest Entry :%d\nWord Count    :%d\n",
             4, args);
    auto *sorted = MAlloc(word_size, heap);
    if (!sorted)
      return false;
    bool written = FileWriteCpp(ACD_DEF_FILENAME, out_start, def_size, 0, 0);
    if (written) {
      PrintCpp("Def File Size :%d\n", 1, &def_size);
      written = ACDSortWords(word_start, sorted, word_cnt) &&
                FileWriteCpp(ACD_WORD_FILENAME, sorted, word_size, 0, 0);
      if (written)
        PrintCpp("Word File Size:%d\n", 1, &word_size);
    }
    Free(sorted);
    return written;
  };
  bool written = generate();
  Free(entry);
  Free(word_start);
  Free(out_start);
  Free(in_start);
  return written;
}

extern "C" void ACDictGenCpp()
{
  if (!ACDPreprocess("DICTIONARY.DD", "DICTIONARY2.DD")) {
    PutS("Unable to preprocess DICTIONARY.DD\n");
    return;
  }
  if (!ACDGen("DICTIONARY2.DD"))
    PutS("Unable to write the dictionary files\n");
}
