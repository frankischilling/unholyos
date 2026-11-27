#include "../Kernel/KernelB.hpp"
#include "../Compiler/CompilerA.hpp"

// These tables are private until all startup declarations succeed. Release
// generated functions before their metadata; borrowed native entry points
// and internal opcodes are not allocations of the source namespace.
static void AdamSourceDel(CHashTable *table, CHashTable *kernel)
{
  while (table != kernel) {
    auto *next = table->next;
    for (int64_t i = 0; i <= table->mask; ++i)
      for (auto *entry = table->body[i]; entry; entry = entry->next)
        if (entry->type & HTT_FUN) {
          auto *fun = reinterpret_cast<CHashFun *>(entry);
          if (!(fun->cls.flags & (CF_EXTERN | FF__EXTERN | FF_INTERNAL)))
            Free(fun->exe_addr);
          for (auto *member = fun->cls.member_lst_and_root; member;
               member = member->next)
            if (member->flags & MLF_STATIC)
              Free(member->static_data);
        }
    HashTableDelCpp(table);
    table = next;
  }
}

// The original MakeAdam.HC also loads Training, graphics, DolDoc and the
// remaining Adam subsystems. Compile the implemented original sources in
// their reference order before starting the native desktop providers.
bool MakeAdam()
{
  auto *task = CurrentTask();
  auto *old_fun = task->last_fun;
  auto *kernel = task->hash_table;
  auto *code_heap = task->code_heap;
  auto *data_heap = task->data_heap;
  const auto code_used = code_heap->used_u8s;
  const auto data_used = data_heap->used_u8s;
  auto *table = HashTableNew(1024, data_heap);
  if (!table)
    return false;
  table->next = kernel;
  task->hash_table = table;

  const char *files[]{"/Adam/AMath.HC", "/Adam/AMem.HC", "/Adam/TaskRep.HC"};
  for (const char *file : files) {
    SerialPutS("UnholyOS: loading Adam source ");
    SerialPutS(file);
    SerialPutS("\r\n");
    auto result = ExeFile(file);
    if (!result.okay || result.exception) {
      // No child may inherit partially initialized compiler symbols.
      task->hash_table = kernel;
      task->last_fun = old_fun;
      AdamSourceDel(table, kernel);
      SerialPutS("UnholyOS: Adam source failed ");
      SerialPutS(file);
      SerialPutS("\r\n");
      SerialPutS("UnholyOS: Adam source rollback, code bytes ");
      SerialPutHex(code_heap->used_u8s - code_used);
      SerialPutS(", data bytes ");
      SerialPutHex(data_heap == code_heap ? 0 : data_heap->used_u8s - data_used);
      SerialPutS("\r\n");
      return false;
    }
  }

  // The idle task was created before the compiler was loaded.
  CurrentCPU()->idle_task->hash_table->next = table;
  SerialPutS("UnholyOS: Adam source ready, table ");
  SerialPutHex(reinterpret_cast<uintptr_t>(table));
  SerialPutS(", code bytes ");
  SerialPutHex(code_heap->used_u8s - code_used);
  SerialPutS(", data bytes ");
  SerialPutHex(data_heap == code_heap ? 0 : data_heap->used_u8s - data_used);
  SerialPutS("\r\n");
  return true;
}
