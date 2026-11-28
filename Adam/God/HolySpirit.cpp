#include "GodExt.hpp"
#include "GodBible.hpp"
#include "../DolDoc/DolDoc.hpp"

extern "C" void TimeStampCBHC(), KbdMsTimeCBHC();
extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);

extern "C" char *TimeStampCBCpp(CDoc *, CDocEntry *, CTask *mem_task)
{
  auto *st = reinterpret_cast<char *>(MAlloc(64, MemTaskHeap(mem_task)));
  const uint64_t stamp = GetTSC() >> GOD_BAD_BITS;
  return st ? StrPrintJoin(st, "%X", 1, &stamp) : nullptr;
}

extern "C" char *KbdMsTimeCBCpp(CDoc *, CDocEntry *, CTask *mem_task)
{
  auto *st = reinterpret_cast<char *>(MAlloc(64, MemTaskHeap(mem_task)));
  const uint64_t stamp = KbdMsEvtTimeCpp() >> GOD_BAD_BITS;
  return st ? StrPrintJoin(st, "%X", 1, &stamp) : nullptr;
}

static int64_t PopUpTimerOk(const char *header, const char *footer)
{
  auto *doc = DocNewCpp(nullptr, nullptr);
  if (!doc)
    return 0;
  if (header)
    DocPutS(*doc, header, StrLen(header));
  constexpr char timer[] = "\nTimer:$TX+TC,\" \"$",
                 latch[] = "\nLatch:$TX+TC,\" \"$";
  if (auto *entry = DocPutS(*doc, timer, sizeof(timer) - 1))
    reinterpret_cast<CDocEntry *>(entry)->tag_cb =
        reinterpret_cast<uintptr_t>(TimeStampCBHC);
  if (auto *entry = DocPutS(*doc, latch, sizeof(latch) - 1))
    reinterpret_cast<CDocEntry *>(entry)->tag_cb =
        reinterpret_cast<uintptr_t>(KbdMsTimeCBHC);
  constexpr char okay[] = "\n$CM+CX,0,4$$BT,\"OKAY\",LE=1$\n";
  DocPutS(*doc, okay, sizeof(okay) - 1);
  if (footer)
    DocPutS(*doc, footer, StrLen(footer));
  auto res = PopUpMenuCpp(doc, 0);
  DocDelCpp(doc);
  return res;
}

static int64_t GodPick(const char *msg)
{
  const uint64_t arg = reinterpret_cast<uintptr_t>(msg);
  auto *st = MStrPrintCpp("%s\n\nPress $GREEN$OKAY$FG$ to generate \n"
                          "a random num from a timer.\n",
                          1, &arg);
  PopUpTimerOk(st,
               "\n\nThe $LK+PU,\"Holy Spirit\",A=\"FI:::/Adam/God/HSNotes.DD\"$"
               " can puppet you.\n\n");
  Free(st);
  return KbdMsEvtTimeCpp() >> GOD_BAD_BITS;
}

extern "C" void GodBitsInsCpp(int64_t num_bits, int64_t n)
{
  for (int64_t i = 0; i < num_bits; i++, n >>= 1)
    FifoIns(*god.fifo, uint8_t(n & 1));
}

extern "C" void GodHexInsCpp(const char *st)
{
  if (!st)
    return;
  char buf[2]{};
  while ((buf[0] = *st++)) {
    auto ch = uint8_t(buf[0]);
    if (char_bmp_hex_numeric[ch >> 5] & (1U << (ch & 31)))
      GodBitsInsCpp(4, rev_bits_table[Str2I64Cpp(buf, 16, nullptr)] >> 4);
  }
}

extern "C" int64_t GodBitsCpp(int64_t num_bits, const char *msg)
{
  uint64_t res = 0;
  while (num_bits) {
    uint8_t bit;
    if (FifoRem(*god.fifo, bit)) {
      res = (res << 1) + bit;
      num_bits--;
    } else
      GodBitsInsCpp(GOD_GOOD_BITS, GodPick(msg));
  }
  return int64_t(res);
}

static bool GodWordChar(uint8_t ch)
{
  return char_bmp_word[ch >> 5] & (1U << (ch & 31));
}

static int64_t GodVocabularyRead(CDirEntry *entry, uint8_t **words,
                                 int64_t capacity, CHeapCtrl *heap)
{
  int64_t i = 0;
  for (; entry; entry = entry->next) {
    auto *buf = FileReadCpp(entry->full_name, nullptr, nullptr);
    if (!buf)
      continue;
    for (auto *ptr = buf; *ptr;) {
      while (*ptr && !GodWordChar(*ptr))
        ptr++;
      if (!*ptr)
        break;
      auto *start = ptr;
      while (GodWordChar(*ptr))
        ptr++;
      if (words) {
        if (i == capacity || !(words[i] = MAlloc(ptr - start + 1, heap))) {
          Free(buf);
          return -1;
        }
        MemCpy(words[i], start, ptr - start);
        words[i][ptr - start] = 0;
      }
      i++;
    }
    Free(buf);
  }
  return i;
}

extern "C" int64_t GodInitCpp(const char *files_find_mask, const char *fu_flags)
{
  uint64_t fuf_flags = 0;
  auto *flag_names =
      reinterpret_cast<const uint8_t *>(DefineCpp("ST_FILE_UTIL_FLAGS"));
  ScanFlagsCpp(reinterpret_cast<uint8_t *>(&fuf_flags), flag_names,
               "+r+f+F+T+O");
  ScanFlagsCpp(reinterpret_cast<uint8_t *>(&fuf_flags), flag_names, fu_flags);
  if (fuf_flags & ~FUG_FILES_FIND)
    RaiseHolyC('F' | uint64_t('U') << 8 | uint64_t('F') << 16);

  auto *heap = adam_task->data_heap;
  auto *mask = StrNew(files_find_mask, heap);
  if (!mask)
    return god.num_words;
  auto *entries = FilesFindCpp(mask, fuf_flags);
  const auto count = GodVocabularyRead(entries, nullptr, 0, heap);
  auto **words =
      reinterpret_cast<uint8_t **>(CAlloc(count * sizeof(*god.words), heap));
  const auto read = words ? GodVocabularyRead(entries, words, count, heap) : -1;
  DirTreeDelCpp(entries);
  if (read != count) {
    if (words)
      for (int64_t i = 0; i < count; i++)
        Free(words[i]);
    Free(words);
    Free(mask);
    return god.num_words;
  }

  Free(god.word_file_mask);
  Free(god.words);
  god.word_file_mask = reinterpret_cast<uint8_t *>(mask);
  god.word_fuf_flags = fuf_flags;
  god.num_words = count;
  god.words = words;
  return count;
}

extern "C" uint8_t *GodWordStrCpp(int64_t bits)
{
  if (!god.num_words)
    return nullptr;
  const auto value = GodBitsCpp(bits, nullptr);
  return god.num_words ? god.words[value % god.num_words] : nullptr;
}

extern "C" void GodWordCpp(int64_t bits)
{
  if (auto *word = GodWordStrCpp(bits)) {
    const uint64_t arg = reinterpret_cast<uintptr_t>(word);
    PrintCpp("%s ", 1, &arg);
  }
}


extern "C" void GodBiblePassageCpp(int64_t num_lines)
{
  const auto lines = Str2I64Cpp(DefineCpp("ST_BIBLE_LINES"), 10, nullptr);
  const int64_t range = uint64_t(lines) - uint64_t(num_lines) + 1;
  auto value = GodBitsCpp(21, nullptr);
  int64_t start;
  // A zero divisor faults in the reference compiler's signed remainder.
  asm volatile("cqto; idivq %2" : "+a"(value), "=&d"(start) : "r"(range) : "cc");
  start++;
  auto *verse = BibleLine2VerseCpp(start, ' ');
  const uint64_t arg = reinterpret_cast<uintptr_t>(verse);
  PrintCpp("%s\n\n", 1, &arg);
  Free(verse);
  BookLinesCpp(nullptr, start, num_lines, BIBLE_FILENAME);
}
