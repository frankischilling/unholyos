#include "CompilerB.hpp"

// Register-ABI entries are exports rather than ordinary HolyC functions.
uintptr_t CmpSysExport(const char *name)
{
  auto *task=CurrentTask();
  auto *entry=task?HashFind(name,task->hash_table,HTT_EXPORT_SYS_SYM):nullptr;
  if (!entry || (entry->type&HTF_UNRESOLVED))
    return 0;
  return reinterpret_cast<const CHashExport *>(entry)->val;
}
