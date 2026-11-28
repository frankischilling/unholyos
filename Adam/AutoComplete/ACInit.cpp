#include "../../Kernel/KernelB.hpp"
#include "AutoComplete.hpp"

extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);
extern "C" int64_t ToUpperCpp(uint8_t ch);

uint8_t *ACDWordNext(uint8_t *record)
{
  auto *end = acd.word_lst + acd.word_lst_size;
  const auto step = StrLen(reinterpret_cast<char *>(record + 1)) + 4;
  return step < size_t(end - record) ? record + step : end - 1;
}

static int64_t ACDWordMarker(int64_t i)
{
  const auto *list = acd.word_lst;
  const auto size = acd.word_lst_size;
  if (i >= size)
    i = size - 1;
  while (i > 0) {
    while (i && list[i] != ACD_WORD_CHAR)
      i--;
    // A block index can contain the same byte as the word marker.
    if (i <= 3 || i + 2 >= size ||
        (list[i + 1] != ACD_WORD_CHAR && list[i + 2] != ACD_WORD_CHAR))
      break;
    i--;
  }
  return i;
}

extern "C" uint8_t *ACDWordPtAtCpp(const char *word)
{
  if (!word || !*word || !acd.word_lst_size)
    return acd.word_lst;
  auto *list = acd.word_lst;
  int64_t start = 0, end = acd.word_lst_size;
  while (start + 3 < end) {
    auto i = ACDWordMarker((start + end) >> 1);
    auto *record = list + i;
    if (*record != ACD_WORD_CHAR)
      break;
    auto cmp = StrICmpCpp(word, reinterpret_cast<char *>(record + 1));
    if (!cmp)
      return record;
    if (cmp < 0) {
      end = i - 1;
    } else {
      auto next = ACDWordNext(record) - list;
      if (next <= start)
        break;
      start = next;
    }
  }
  auto *record = list + ACDWordMarker(start + end > 0 ? (start + end) >> 1 : 0);
  if (*record == ACD_WORD_CHAR &&
      StrICmpCpp(word, reinterpret_cast<char *>(record + 1)) > 0)
    record = ACDWordNext(record);
  return record < list + acd.word_lst_size && *record == ACD_WORD_CHAR ? record
                                                                       : list;
}

static void ACDWordsClear()
{
  auto *words = acd.word_lst;
  acd = {};
  Free(words);
}

extern "C" void ACDWordsLoadCpp()
{
  auto *table = ac.hash_table;
  if (!table)
    return;
  size_t size;
  auto *data = FileReadCpp(ACD_WORD_FILENAME, &size, nullptr);
  if (!data)
    return;
  auto *heap = adam_task->data_heap;
  auto *words = size && !data[size - 1] ? MAlloc(size, heap) : nullptr;
  if (words)
    MemCpy(words, data, size);
  Free(data);
  CHash *pending = nullptr;
  auto **tail = &pending;
  int64_t count = 0;
  auto *in = words;
  auto *end = words ? words + size : nullptr;
  bool valid = words;
  while (in != end) {
    if (*in == ACD_WORD_CHAR)
      in++;
    if (!*in) {
      if (end - in <= 3)
        break;
      in += 3;
      continue;
    }
    const auto length = StrLen(reinterpret_cast<char *>(in));
    const auto *block = in + length + 1;
    if (end - block <= 2) {
      valid = false;
      break;
    }
    auto *entry = reinterpret_cast<CHashGeneric *>(
        CAlloc(sizeof(CHashGeneric) + length + 1, heap));
    if (!entry) {
      valid = false;
      break;
    }
    auto *name = reinterpret_cast<char *>(entry + 1);
    for (size_t j = 0; j < length; j++)
      name[j] = ToUpperCpp(in[j]);
    entry->user_data0 = uint64_t(block[0]) | uint64_t(block[1]) << 8;
    entry->hash.str = name;
    entry->hash.type = HTT_DICT_WORD;
    entry->hash.use_cnt = 1;
    *tail = &entry->hash;
    tail = &entry->hash.next;
    in += length + 3;
    count++;
  }
  if (!valid || ac.hash_table != table) {
    while (pending) {
      auto *entry = pending;
      pending = entry->next;
      HashDel(entry);
    }
    Free(words);
    return;
  }

  for (int64_t bucket = 0; bucket <= table->mask; bucket++) {
    auto **slot = &table->body[bucket];
    while (auto *entry = *slot) {
      if (entry->type & HTT_DICT_WORD) {
        *slot = entry->next;
        HashDel(entry);
      } else {
        slot = &entry->next;
      }
    }
  }
  while (pending) {
    auto *entry = pending;
    pending = entry->next;
    HashAdd(entry, table);
  }
  ACDWordsClear();
  acd.word_lst = words;
  acd.word_lst_size = size;
  acd.num_words = count;
  acd.has_words = true;
  acd.has_defs = FileFindCpp(ACD_DEF_FILENAME, nullptr, FUF_JUST_FILES) ||
                 FileFindCpp(ACD_DEF_FILENAME_Z, nullptr, FUF_JUST_FILES);
}

static CHashAC *ACHashAdd(const char *word)
{
  auto *entry =
      reinterpret_cast<CHashAC *>(HashFind(word, ac.hash_table, HTT_WORD));
  if (entry) {
    entry->hits++;
    return entry;
  }
  auto *heap = adam_task->data_heap;
  entry = reinterpret_cast<CHashAC *>(CAlloc(sizeof(CHashAC), heap));
  if (!entry)
    return nullptr;
  entry->hash.str = StrNew(word, heap);
  if (!entry->hash.str) {
    Free(entry);
    return nullptr;
  }
  entry->hash.type = HTT_WORD;
  entry->hash.use_cnt = 1;
  entry->hits = 1;
  HashAdd(&entry->hash, ac.hash_table);
  ac.num_words++;
  return entry;
}

bool ACWordChar(uint8_t ch)
{
  return char_bmp_alpha_numeric[ch / 32] & (1u << (ch % 32));
}

static void ACSingleFileAdd(uint8_t *ptr)
{
  while (*ptr) {
    while (*ptr && !ACWordChar(*ptr))
      ptr++;
    if (!*ptr)
      return;
    auto *word = ptr++;
    auto *end = ptr;
    while (*ptr && (ACWordChar(*ptr) || *ptr == CH_CURSOR)) {
      if (*ptr != CH_CURSOR)
        *end++ = *ptr;
      ptr++;
    }
    if (*ptr)
      ptr++;
    *end = 0;
    ACHashAdd(reinterpret_cast<char *>(word));
  }
}

static void ACMainFileLstTraverse(const char *mask)
{
  auto *files = FilesFindCpp(mask, FUF_RECURSE | FUF_JUST_TXT | FUF_JUST_FILES |
                                       FUF_CLUS_ORDER);
  for (auto *entry = files; entry; entry = entry->next) {
    const uint64_t args[]{reinterpret_cast<uintptr_t>(entry->full_name)};
    PrintCpp("%s\n", 1, args);
    if (auto *buf = FileReadCpp(entry->full_name, nullptr, nullptr)) {
      ACSingleFileAdd(buf);
      Free(buf);
    }
  }
  DirTreeDelCpp(files);
}

extern "C" void ACInitCpp(const char *mask)
{
  SYS_RUN_LEVEL &= ~RLF_AUTO_COMPLETE;
  AutoCompleteCpp(false);
  while (__atomic_fetch_or(&ac.flags, ACF_INIT_IN_PROGRESS, __ATOMIC_ACQUIRE) &
         ACF_INIT_IN_PROGRESS)
    _YIELD();
  if (DrvIsWritableCpp(0, false) &&
      FileFindCpp(ACD_DEF_FILENAME_Z, nullptr, FUF_JUST_FILES) &&
      !FileFindCpp(ACD_DEF_FILENAME, nullptr, FUF_JUST_FILES))
    MoveCpp(ACD_DEF_FILENAME_Z, ACD_DEF_FILENAME);
  HashTableDelCpp(ac.hash_table);
  ACDWordsClear();
  ac.hash_table = HashTableNew(2048, adam_task->data_heap);
  ac.num_words = 0;
  ac.partial_len = ac.num_fillins = 0;
  Free(ac.cur_word);
  ac.cur_word = nullptr;
  if (ac.hash_table) {
    if (mask)
      ACMainFileLstTraverse(mask);
    ACDWordsLoadCpp();
    SYS_RUN_LEVEL |= RLF_AUTO_COMPLETE;
  }
  __atomic_fetch_and(&ac.flags, ~ACF_INIT_IN_PROGRESS, __ATOMIC_RELEASE);
  AutoCompleteCpp(true);
}
