#include "KernelB.hpp"

CHashTable *HashTableNew(size_t size, CHeapCtrl *hc)
{
  if (size > SIZE_MAX / sizeof(CHash *))
    return nullptr;
  auto *table = reinterpret_cast<CHashTable *>(CAlloc(sizeof(CHashTable), hc));
  if (!table)
    return nullptr;
  table->body = reinterpret_cast<CHash **>(CAlloc(size * sizeof(CHash *), hc));
  if (!table->body) {
    Free(table);
    return nullptr;
  }
  table->mask = size - 1;
  return table;
}

extern "C" CHashTable *HashTableNewCpp(size_t size, void *mem_task)
{
  return HashTableNew(size, MemTaskHeap(mem_task));
}

extern "C" int64_t HashTypeNumCpp(const CHash *entry)
{
  uint32_t type = entry ? entry->type & HTG_TYPE_MASK : 0;
  return type ? __builtin_ctz(type) : -1;
}

extern "C" uint64_t HashValCpp(const CHash *entry)
{
  if (!entry)
    return 0;
  uint32_t types = entry->type & HTG_TYPE_MASK;
  switch (types & (0 - types)) {
  case HTT_EXPORT_SYS_SYM:
    return reinterpret_cast<const CHashExport *>(entry)->val;
  case HTT_IMPORT_SYS_SYM:
    return reinterpret_cast<uintptr_t>(
        reinterpret_cast<const CHashImport *>(entry)->module_base);
  case HTT_DEFINE_STR:
  case HTT_CLASS:
  case HTT_INTERNAL_TYPE:
  case HTT_WORD:
  case HTT_DICT_WORD:
  case HTT_OPCODE:
  case HTT_HELP_FILE:
    return reinterpret_cast<uintptr_t>(entry);
  case HTT_GLBL_VAR: {
    auto *var = reinterpret_cast<const CHashGlblVar *>(entry);
    return var->flags & GVF_EXTERN
               ? reinterpret_cast<uintptr_t>(&var->data_addr)
               : reinterpret_cast<uintptr_t>(var->data_addr);
  }
  case HTT_FUN: {
    auto *fun = reinterpret_cast<const CHashFun *>(entry);
    return fun->cls.flags & CF_EXTERN
               ? reinterpret_cast<uintptr_t>(fun)
               : reinterpret_cast<uintptr_t>(fun->exe_addr);
  }
  case HTT_REG: {
    auto *reg = reinterpret_cast<const CHashReg *>(entry);
    return reg->reg_num | (uint64_t(reg->reg_type) << 8);
  }
  case HTT_KEYWORD:
  case HTT_ASM_KEYWORD:
  case HTT_MODULE:
  case HTT_FILE:
  case HTT_FRAME_PTR:
    return reinterpret_cast<const CHashGeneric *>(entry)->user_data0;
  default:
    return 0;
  }
}

void HashDel(CHash *entry)
{
  if (!entry)
    return;
  uint32_t type = entry->type;
  if (!(type & HTT_DICT_WORD))
    Free(const_cast<char *>(entry->str));
  if (type & HTG_SRC_SYM) {
    auto *sym = reinterpret_cast<CHashSrcSym *>(entry);
    Free(const_cast<char *>(sym->src_link));
    Free(const_cast<char *>(sym->idx));
    Free(const_cast<char *>(sym->import_name));
    LinkedLstDelCpp(sym->ie_lst);
    if (type & (HTT_FUN | HTT_EXPORT_SYS_SYM))
      Free(sym->dbg_info);
    if (type & (HTT_FUN | HTT_CLASS)) {
      ClassMemberLstDel(reinterpret_cast<CHashClass *>(entry));
    } else if (type & HTT_DEFINE_STR) {
      Free(reinterpret_cast<CHashDefineStr *>(entry)->data);
    } else if (type & HTT_GLBL_VAR) {
      auto *var = reinterpret_cast<CHashGlblVar *>(entry);
      if (!(var->flags & GVF_ALIAS))
        Free(var->data_addr);
      LinkedLstDelCpp(var->dim.next);
      if (var->fun_ptr) {
        auto *fun = var->fun_ptr - var->fun_ptr->cls.ptr_stars_cnt;
        HashDel(&fun->cls.sym.hash);
      }
    }
  } else if (type & HTT_FILE) {
    Free(reinterpret_cast<void *>(
        reinterpret_cast<CHashGeneric *>(entry)->user_data0));
  }
  Free(entry);
}

extern "C" void HashDelCpp(CHash *entry)
{
  HashDel(entry);
}

extern "C" void HashTableDelCpp(CHashTable *table)
{
  if (!table)
    return;
  for (int64_t i = 0; i <= table->mask; i++) {
    for (auto *entry = table->body[i]; entry;) {
      auto *next = entry->next;
      HashDel(entry);
      entry = next;
    }
  }
  Free(table->body);
  Free(table);
}

extern "C" void HashSrcFileSetCpp(const CCmpCtrl *cc, CHashSrcSym *sym,
                                  int64_t line_offset)
{
  constexpr uint64_t OPTF_KEEP_PRIVATE = uint64_t{1} << 0x21;
  const auto *file = cc->lex_include_stk;
  int64_t line = static_cast<int64_t>(uint64_t(file->line_num) + line_offset);
  uint64_t args[]{reinterpret_cast<uintptr_t>(file->full_name),
                  line < 1 ? 1 : uint64_t(line)};
  auto *link = MStrPrintCpp("FL:%s,%d", 2, args);
  if (!link)
    return;
  char *idx = nullptr;
  if (cc->cur_help_idx && *cc->cur_help_idx) {
    idx = StrNew(cc->cur_help_idx, CurrentTask()->data_heap);
    if (!idx) {
      Free(link);
      return;
    }
  }
  Free(const_cast<char *>(sym->src_link));
  sym->src_link = link;
  if (cc->opts & OPTF_KEEP_PRIVATE)
    sym->hash.type |= HTF_PRIVATE;
  Free(const_cast<char *>(sym->idx));
  sym->idx = idx;
}
