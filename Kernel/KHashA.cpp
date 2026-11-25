#include "KernelB.hpp"

static uint64_t HashStr(const char *name)
{
  if (!name)
    return 0;
  uint64_t value = 0;
  for (const auto *p = reinterpret_cast<const uint8_t *>(name); *p; p++)
    value = (value << 1) + (value >> 63) + *p;
  return value + (value >> 16) + ((value >> 15) & 1);
}

static CHash **HashFindInBucket(const char *name, CHash **link, uint32_t mask,
                                uint64_t &instance)
{
  for (; *link; link = &(*link)->next) {
    auto *entry = *link;
    if (!(entry->type & mask))
      continue;
    const char *p = name, *q = entry->str;
    while (*p && *p == *q) {
      p++;
      q++;
    }
    if (*p == *q && --instance == 0) {
      entry->use_cnt++;
      return link;
    }
  }
  return nullptr;
}

CHash *HashSingleTableFind(const char *name, CHashTable *table, uint32_t mask,
                           uint64_t instance)
{
  if (!instance)
    return nullptr;
  auto **link = HashFindInBucket(
      name, &table->body[HashStr(name) & table->mask], mask, instance);
  return link ? *link : nullptr;
}

CHash *HashFind(const char *name, CHashTable *table, uint32_t mask,
                uint64_t instance)
{
  if (!instance || !table)
    return nullptr;
  uint64_t hash = HashStr(name);
  for (; table; table = table->next)
    if (auto **link = HashFindInBucket(name, &table->body[hash & table->mask],
                                       mask, instance))
      return *link;
  return nullptr;
}

static void HashAddAt(CHash *entry, CHash *&link)
{
  uint64_t flags;
  asm volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
  entry->next = link;
  link = entry;
  asm volatile("pushq %0; popfq" : : "r"(flags) : "cc", "memory");
}

void HashAdd(CHash *entry, CHashTable *table)
{
  HashAddAt(entry, table->body[HashStr(entry->str) & table->mask]);
}

void HashRemove(CHash *entry, CHashTable *table)
{
  auto **link = &table->body[HashStr(entry->str) & table->mask];
  uint64_t flags;
  asm volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
  while (*link && *link != entry)
    link = &(*link)->next;
  if (*link)
    *link = entry->next;
  asm volatile("pushq %0; popfq" : : "r"(flags) : "cc", "memory");
}

extern "C" uint64_t HashRemDelCpp(CHash *entry, CHashTable *table,
                                  uint64_t instance)
{
  if (!entry || !instance)
    return 0;
  uint64_t flags;
  asm volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
  auto **link = HashFindInBucket(
      entry->str, &table->body[HashStr(entry->str) & table->mask],
      entry->type & ~HTG_FLAGS_MASK, instance);
  bool removed = link && *link == entry;
  if (removed)
    *link = entry->next;
  asm volatile("pushq %0; popfq" : : "r"(flags) : "cc", "memory");
  if (removed)
    HashDel(entry);
  return removed;
}

extern "C" CHash *HashFindCpp(const char *name, CHashTable *table,
                              uint32_t mask, uint64_t instance)
{
  return HashFind(name, table, mask, instance);
}

extern "C" CHash *HashSingleTableFindCpp(const char *name, CHashTable *table,
                                         uint32_t mask, uint64_t instance)
{
  return HashSingleTableFind(name, table, mask, instance);
}

extern "C" void HashAddCpp(CHash *entry, CHashTable *table)
{
  HashAdd(entry, table);
}

extern "C" void HashAddAfterCpp(CHash *entry, CHash *pred)
{
  HashAddAt(entry, pred->next);
}
