#include "../../Kernel/KernelB.hpp"
#include "AutoComplete.hpp"

extern "C" void ACInitCpp(const char *)
{
  SYS_RUN_LEVEL &= ~RLF_AUTO_COMPLETE;
  AutoCompleteCpp(false);
  while (__atomic_fetch_or(&ac.flags, ACF_INIT_IN_PROGRESS, __ATOMIC_ACQUIRE) &
         ACF_INIT_IN_PROGRESS)
    _YIELD();
  HashTableDelCpp(ac.hash_table);
  ac.hash_table = HashTableNew(2048, adam_task->data_heap);
  ac.num_words = 0;
  Free(ac.cur_word);
  ac.cur_word = nullptr;
  if (ac.hash_table)
    SYS_RUN_LEVEL |= RLF_AUTO_COMPLETE;
  __atomic_fetch_and(&ac.flags, ~ACF_INIT_IN_PROGRESS, __ATOMIC_RELEASE);
  AutoCompleteCpp(true);
}
