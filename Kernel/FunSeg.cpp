#include "KernelB.hpp"

static bool HasLower(const char *name)
{
  for (; *name; name++)
    if (*name >= 'a' && *name <= 'z')
      return true;
  return false;
}

const CHash *FunSegFind(uintptr_t addr, uint64_t &offset)
{
  offset = 0xFFFF;
  if (!addr)
    return nullptr;
  if (addr >= MEM_HEAP_BASE && addr <= MEM_HEAP_LIMIT) {
    auto *page = MemPageTable(addr);
    if (!page || !(*page & 1))
      return nullptr;
  } else if (addr < MEM_BOOT_BASE || addr >= 0xA0000) {
    return nullptr;
  }
  const CHash *symbol = nullptr;
  for (auto *table = CurrentTask()->hash_table; table; table = table->next) {
    for (int64_t i = 0; i <= table->mask; i++) {
      for (auto *entry = table->body[i]; entry; entry = entry->next) {
        bool is_fun = entry->type & HTT_FUN;
        uint64_t addr0;
        if (is_fun) {
          auto *fun = reinterpret_cast<const CHashFun *>(entry);
          if (fun->cls.flags & (CF_EXTERN | FF_INTERNAL))
            continue;
          addr0 = reinterpret_cast<uintptr_t>(fun->exe_addr);
        } else if (entry->type & HTT_EXPORT_SYS_SYM) {
          addr0 = reinterpret_cast<const CHashExport *>(entry)->val;
        } else {
          continue;
        }
        if (!addr0 || addr0 > addr)
          continue;
        auto delta = addr - addr0;
        if (delta < offset ||
            (delta == offset &&
             (!symbol || !HasLower(symbol->str) ||
              (is_fun && (symbol->type & HTT_EXPORT_SYS_SYM))))) {
          symbol = entry;
          offset = delta;
        }
      }
    }
  }
  return symbol;
}

static int64_t LineFromMap(const CDbgInfo &dbg_info, uintptr_t addr,
                           int64_t count)
{
  if (dbg_info.max_line < dbg_info.min_line || count <= 0)
    return -1;
  uint64_t entries = uint64_t(dbg_info.max_line) - dbg_info.min_line + 1;
  const auto *body = reinterpret_cast<const uint32_t *>(&dbg_info) + 2;
  uint64_t first = 0, last = entries - 1;
  while (first < entries && !body[first])
    first++;
  if (first == entries)
    return -1;
  while (last > first && !body[last])
    last--;
  uint64_t line = first;
  if (addr >= body[last]) {
    line = last;
  } else if (addr >= body[first] && body[last] >= body[first]) {
    line = (addr - body[first]) * (last - first + 1) /
           (uint64_t(body[last]) - body[first] + 1);
    if (line < first)
      line = first;
    if (line > last)
      line = last;
  }
  while (line > first && (!body[line] || body[line] >= addr))
    line--;
  while (line < last && (!body[line] || body[line] < addr))
    line++;
  return addr < uint64_t(body[line]) + uint64_t(count)
             ? dbg_info.min_line + line
             : -1;
}

static bool HasMappedSource(const CDbgInfo &info, uintptr_t addr, int64_t count)
{
  if (info.max_line < info.min_line || count <= 0)
    return false;
  uint64_t lines = uint64_t(info.max_line) - info.min_line + 1;
  const auto *body = reinterpret_cast<const uint32_t *>(&info) + 2;
  for (uint64_t i = 0; i < lines; i++) {
    if (!body[i] || addr < body[i] || addr - body[i] >= uint64_t(count))
      continue;
    uint64_t next = i + 1;
    while (next <= lines && !body[next])
      next++;
    if (next > lines || addr < body[next])
      return true;
  }
  return false;
}

extern "C" int64_t SrcLineNumCpp(uintptr_t addr, int64_t count)
{
  uint64_t offset;
  auto *sym = reinterpret_cast<const CHashSrcSym *>(FunSegFind(addr, offset));
  if (!sym)
    return -1;
  if (auto *map = static_cast<const CDbgInfo *>(sym->dbg_info))
    return LineFromMap(*map, addr, count);
  if (!sym->src_link)
    return -1;
  const char *line = sym->src_link;
  for (const char *ptr = line; *ptr; ptr++)
    if (*ptr == ',')
      line = ptr + 1;
  return Str2I64Cpp(line, 10, nullptr);
}

extern "C" char *SrcFileNameCpp(uintptr_t addr, int64_t count, CTask *mem_task)
{
  uint64_t offset;
  auto *sym = reinterpret_cast<const CHashSrcSym *>(FunSegFind(addr, offset));
  if (!sym || !sym->src_link)
    return nullptr;
  if (auto *map = static_cast<const CDbgInfo *>(sym->dbg_info))
    if (!HasMappedSource(*map, addr, count))
      return nullptr;
  auto *file = StrNew(sym->src_link, MemTaskHeap(mem_task));
  if (file) {
    StrFirstRemCpp(file, reinterpret_cast<const uint8_t *>(":"), nullptr);
    StrLastRemCpp(file, reinterpret_cast<const uint8_t *>(","), nullptr);
  }
  return file;
}

extern "C" char *SrcEdLink(uintptr_t addr, int64_t count, CTask *mem_task)
{
  auto *file = SrcFileNameCpp(addr, count, nullptr);
  if (!file)
    return nullptr;
  auto line = SrcLineNumCpp(addr, count);
  const uint64_t args[]{reinterpret_cast<uintptr_t>(file),
                        line > 0 ? uint64_t(line) : 1};
  auto *link = MStrPrintCpp("FL:%s,%d", 2, args);
  Free(file);
  if (link && mem_task) {
    auto *copy = StrNew(link, MemTaskHeap(mem_task));
    Free(link);
    link = copy;
  }
  return link;
}

extern "C" uint64_t PutSrcLinkCpp(uintptr_t addr, int64_t count, char *buf)
{
  auto *src = SrcEdLink(addr, count);
  if (!src) {
    if (buf)
      *buf = 0;
    return 0;
  }
  uint64_t args[]{addr, reinterpret_cast<uintptr_t>(src)};
  auto *link = StrPrintJoin(buf, "$LK,\"%p\",A=\"%s\"$", 2, args);
  Free(src);
  if (!buf) {
    PutS(link);
    Free(link);
  }
  return 1;
}
