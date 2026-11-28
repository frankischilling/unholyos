#include "../../Kernel/KernelB.hpp"
#include "AutoComplete.hpp"

extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);

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
  HashTableDelCpp(ac.hash_table);
  ac.hash_table = HashTableNew(2048, adam_task->data_heap);
  ac.num_words = 0;
  ac.partial_len = ac.num_fillins = 0;
  Free(ac.cur_word);
  ac.cur_word = nullptr;
  if (ac.hash_table) {
    if (mask)
      ACMainFileLstTraverse(mask);
    SYS_RUN_LEVEL |= RLF_AUTO_COMPLETE;
  }
  __atomic_fetch_and(&ac.flags, ~ACF_INIT_IN_PROGRESS, __ATOMIC_RELEASE);
  AutoCompleteCpp(true);
}
