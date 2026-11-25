#include "KernelB.hpp"

extern "C" int64_t LstMatchCpp(const uint8_t *needle, const uint8_t *list,
                               uint64_t flags);

static const CHashDefineStr *DefineFind(const char *name, bool &defined)
{
  auto *entry = name ? HashFind(name, CurrentTask()->hash_table, HTT_DEFINE_STR)
                     : nullptr;
  defined = entry || !name;
  return reinterpret_cast<const CHashDefineStr *>(entry);
}

const char *DefineText(const char *name, bool &defined)
{
  auto *entry = DefineFind(name, defined);
  return entry ? entry->data : nullptr;
}

const char *DefineSub(int64_t index, const char *name, bool &defined)
{
  auto *entry = DefineFind(name, defined);
  return entry && 0 <= index && index < entry->cnt ? entry->sub_idx[index]
                                                   : nullptr;
}

[[noreturn]] void UndefinedDefine(const char *name)
{
  PutS("ERROR: Undefined Define: '");
  PutS(name);
  PutS("'.\n");
  RaiseHolyC(UNDEF_DEFINE_ERROR);
}

extern "C" const char *DefineSubCpp(int64_t index, const char *name)
{
  bool defined;
  auto *res = DefineSub(index, name, defined);
  if (!defined)
    UndefinedDefine(name);
  return res;
}

extern "C" int64_t DefineMatchCpp(const uint8_t *needle, const char *name,
                                  uint64_t flags)
{
  bool defined;
  auto *list = DefineText(name, defined);
  if (!defined)
    UndefinedDefine(name);
  return LstMatchCpp(needle, reinterpret_cast<const uint8_t *>(list), flags);
}

extern "C" CHashDefineStr *DefineLstLoadCpp(const char *name, const char *list,
                                            const uintptr_t *frame)
{
  auto &task = *CurrentTask();
  auto *hc = task.data_heap;
  auto *entry =
      reinterpret_cast<CHashDefineStr *>(CAlloc(sizeof(CHashDefineStr), hc));
  if (!entry)
    return nullptr;
  const char *end = list;
  size_t count = 0;
  while (*end) {
    count += *end != '@';
    end += StrLen(end) + 1;
  }
  entry->sym.hash.str = StrNew(name, hc);
  entry->data = reinterpret_cast<char *>(MAlloc(end - list + 1, hc));
  entry->sub_idx =
      reinterpret_cast<const char **>(MAlloc(count * sizeof(char *), hc));
  uint64_t pc = reinterpret_cast<uintptr_t>(Caller(frame, 0, task));
  char link[5 + 16 + 1];
  entry->sym.src_link = StrNew(StrPrintJoin(link, "AD:0x%X", 1, &pc), hc);
  if (!entry->sym.hash.str || !entry->data || !entry->sub_idx ||
      !entry->sym.src_link) {
    Free(const_cast<char *>(entry->sym.hash.str));
    Free(const_cast<char *>(entry->sym.src_link));
    Free(entry->data);
    Free(entry->sub_idx);
    Free(entry);
    return nullptr;
  }
  MemCpy(entry->data, list, end - list + 1);
  entry->cnt = count;
  auto **index = entry->sub_idx;
  for (auto *p = list; *p; p += StrLen(p) + 1)
    if (*p != '@')
      *index++ = p;
  entry->sym.hash.type = HTT_DEFINE_STR;
  HashAdd(&entry->sym.hash, task.hash_table);
  return entry;
}
