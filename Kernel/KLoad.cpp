#include "KernelB.hpp"

struct CLoadImport
{
  CHashImport entry;
  CLoadedModule *module;
  size_t first, past;
};

struct CLoadRecord
{
  const uint8_t *raw, *offsets;
  const char *name;
  uint64_t bytes;
  uint32_t value;
  uint8_t type;
  uint8_t *storage;
  CHashExport *exported;
  CLoadImport *imported;
};

struct CLoadedModule
{
  uint8_t *image;
  CLoadRecord *records;
  size_t count, unresolved;
  bool owns_image, started;
};

extern "C" {
void CallModuleMain(const uint8_t *entry);
void _BEQU(), _CALL(), _CLAMP_I64();
void _MEMCPY(), _MEMSET(), _MEMSET_I64();
void _MALLOC(), _FREE(), _MHEAP_CTRL(), _MSIZE(), _MSIZE2();
void CAllocHC(), MAllocIdentHC(), StrNewHC(), AStrNewHC();
void _STRCMP(), _STRCPY(), _STRIMATCH(), StrOccHC();
void _HASH_ADD(), _HASH_ADD_AFTER(), _HASH_FIND(), _HASH_SINGLE_TABLE_FIND();
void HashTableNewHC(), HashTypeNumHC(), CeilU64HC(), Pow10I64HC();
void LinkedLstDelHC(), LinkedLstSizeHC(), QueDelHC();
void QueVectU8NewHC(), QueVectU8PutHC(), QueVectU8GetHC(), QueVectU8DelHC();
void QSortHC(), QSortI64HC(), LstMatchHC();
void FileReadHC(), FileNameAbsHC(), DirNameAbsHC(), DirCurHC(), DirFileHC();
void ExtChgHC(), ExtDftHC();
void RedSeaISOHC();
void FileWriteHC(), BlkDevNextFreeSlotHC(), BlkDevAddHC(), Let2DrvHC();
void BlkReadHC(), BlkWriteHC();
void SysTryHC(), SysUntryHC(), ThrowHC(), CallerHC(), PutExceptHC();
void StrPrintJoinHC(), StrPrintHC(), CatPrintHC(), MStrPrintHC(), PrintHC();
void PrintErrHC(), PrintWarnHC(), PutCharsHC(), IsRawHC(), DefineLstLoadHC();
void PutSHC(), PutHexHC();
void PUT_HEX_U64(), PUT_HEX_U32(), PUT_HEX_U16(), PUT_HEX_U8(), PUT_CHARS(),
    PUT_STR();
void CmpHC(), ExePutSHC(), ExeFileHC(), StreamPrintHC(), StreamExePrintHC();
void DefineSubHC(), DefineMatchHC();
void HashValHC(), HashDelHC(), HashTableDelHC(), HashSrcFileSetHC();
void _HASH_REM_DEL(), SysSymImportsResolveHC();
void DocNewHC(), DocDelHC(), DocLoadHC(), DocSizeHC(), DocSaveHC(),
    DocWriteHC();
void DocPrintHC(), Doc2PlainTextHC(), DocBinsValidateHC(), DocPutLineHC();
void PutFileLinkHC(), PutSrcLinkHC(), FixSetHC(), AdamErrHC();
void RawHC(), DbgModeHC(), IsDbgModeHC();
void tSHC(), BusyHC(), SleepUntilHC(), SleepHC(), Ona2FreqHC(), Freq2OnaHC();
void SndHC(), ScrnCastHC(), SndRstHC(), BeepHC(), MuteHC(), IsMuteHC();
}

static bool IsImport(uint8_t type)
{
  return type >= IET_REL_I0 && type <= IET_IMM_I64;
}

static bool IsExport(uint8_t type)
{
  return type >= IET_REL32_EXPORT && type <= IET_IMM64_EXPORT;
}

static bool IsHeap(uint8_t type)
{
  return type >= IET_CODE_HEAP && type <= IET_ZEROED_DATA_HEAP;
}

static unsigned ImportWidth(uint8_t type)
{
  return type < 4 ? 0 : 1U << (type / 2 - 2);
}

static uint64_t ReadWidth(const uint8_t *ptr, size_t width)
{
  uint64_t value = 0;
  MemCpy(&value, ptr, width);
  return value;
}

static bool LoadRecords(const uint8_t *image, const CBinFile &header,
                        CLoadRecord *records, size_t &count)
{
  size_t payload = header.patch_table_offset - BIN_HEADER_SIZE;
  auto *p = image + header.patch_table_offset;
  const auto *end = image + header.file_size;
  count = 0;
  bool in_import = false;
  while (p < end) {
    if (*p == IET_END)
      return true;
    if (end - p < 6)
      return false;
    CLoadRecord rec{};
    rec.raw = p;
    rec.type = *p++;
    rec.value = ReadWidth(p, sizeof(uint32_t));
    p += sizeof(uint32_t);
    rec.name = reinterpret_cast<const char *>(p);
    while (p < end && *p)
      p++;
    if (p == end)
      return false;
    p++;

    if (IsImport(rec.type)) {
      if ((!*rec.name && !in_import) || rec.value > payload ||
          ImportWidth(rec.type) > payload - rec.value)
        return false;
      in_import = true;
    } else {
      in_import = false;
      if (IsExport(rec.type)) {
        if (!*rec.name || (!(rec.type & 1) && rec.value > payload))
          return false;
      } else if (rec.type == IET_MAIN) {
        if (rec.value >= payload)
          return false;
      } else {
        if (rec.type != IET_ABS_ADDR && !IsHeap(rec.type))
          return false;
        size_t width = rec.type >= IET_DATA_HEAP ? 8 : 4;
        if (IsHeap(rec.type)) {
          if (size_t(end - p) < width)
            return false;
          rec.bytes = ReadWidth(p, width);
          if (rec.bytes >> (width * 8 - 1))
            return false;
          p += width;
        }
        if (rec.value > size_t(end - p) / sizeof(uint32_t))
          return false;
        rec.offsets = p;
        for (uint32_t i = 0; i < rec.value; i++) {
          size_t target = ReadWidth(p, sizeof(uint32_t));
          if (target > payload || width > payload - target)
            return false;
          p += sizeof(uint32_t);
        }
      }
    }
    if (records)
      records[count] = rec;
    count++;
  }
  return false;
}

static void LoadDiscard(CLoadedModule *module)
{
  if (module->records) {
    for (size_t i = 0; i < module->count; i++) {
      auto &rec = module->records[i];
      Free(rec.storage);
      HashDel(rec.exported ? &rec.exported->sym.hash : nullptr);
      HashDel(rec.imported ? &rec.imported->entry.sym.hash : nullptr);
    }
    Free(module->records);
  }
  if (module->owns_image)
    Free(module->image);
  Free(module);
}

static uint8_t *ModuleBase(const CLoadedModule &module)
{
  return module.image + BIN_HEADER_SIZE;
}

static bool LoadPrepare(CLoadedModule &module, CTask &task)
{
  for (size_t i = 0; i < module.count; i++) {
    auto &rec = module.records[i];
    if (IsHeap(rec.type)) {
      auto *hc = rec.type < IET_DATA_HEAP ? task.code_heap : task.data_heap;
      rec.storage = MAlloc(rec.bytes, hc);
      if (!rec.storage)
        return false;
      if (rec.type == IET_ZEROED_CODE_HEAP || rec.type == IET_ZEROED_DATA_HEAP)
        MemSet(rec.storage, 0, rec.bytes);
    }
    if ((IsExport(rec.type) || IsHeap(rec.type)) && *rec.name) {
      rec.exported = reinterpret_cast<CHashExport *>(
          MAlloc(sizeof(CHashExport), task.data_heap));
      if (!rec.exported)
        return false;
      MemSet(rec.exported, 0, sizeof(CHashExport));
      rec.exported->sym.hash.str = StrNew(rec.name, task.data_heap);
      if (!rec.exported->sym.hash.str)
        return false;
      rec.exported->sym.hash.type = HTT_EXPORT_SYS_SYM | HTF_IMM;
      rec.exported->val =
          IsHeap(rec.type) ? reinterpret_cast<uintptr_t>(rec.storage)
          : (rec.type & 1)
              ? rec.value
              : reinterpret_cast<uintptr_t>(ModuleBase(module) + rec.value);
    }
    if (IsImport(rec.type) && *rec.name) {
      rec.imported = reinterpret_cast<CLoadImport *>(
          MAlloc(sizeof(CLoadImport), task.data_heap));
      if (!rec.imported)
        return false;
      MemSet(rec.imported, 0, sizeof(CLoadImport));
      rec.imported->entry.sym.hash.str = StrNew(rec.name, task.data_heap);
      if (!rec.imported->entry.sym.hash.str)
        return false;
      rec.imported->entry.sym.hash.type = HTT_IMPORT_SYS_SYM;
      rec.imported->entry.module_base = ModuleBase(module);
      rec.imported->module = &module;
      rec.imported->first = i;
      size_t j = i + 1;
      while (j < module.count && IsImport(module.records[j].type) &&
             !*module.records[j].name)
        j++;
      rec.imported->past = j;
    }
  }
  return true;
}

static void LoadOneImport(const CLoadImport &import, uint64_t address)
{
  auto *base = import.entry.module_base;
  for (size_t i = import.first; i < import.past; i++) {
    const auto &rec = import.module->records[i];
    size_t width = ImportWidth(rec.type);
    auto *target = base + rec.value;
    uint64_t value =
        rec.type & 1 ? address
                     : address - reinterpret_cast<uintptr_t>(target) - width;
    MemCpy(target, &value, width);
  }
}

static CHash *FindLinkSymbol(const char *name, CHashTable *table)
{
  constexpr auto mask = HTT_EXPORT_SYS_SYM | HTT_FUN | HTT_GLBL_VAR;
  for (uint64_t instance = 1;; ++instance) {
    auto *entry = HashFind(name, table, mask, instance);
    if (!entry || !(entry->type & (HTF_IMPORT | HTF_UNRESOLVED)))
      return entry;
  }
}

static uint64_t LinkAddress(const CHash &symbol)
{
  if (symbol.type & HTT_FUN)
    return reinterpret_cast<uintptr_t>(
        reinterpret_cast<const CHashFun *>(&symbol)->exe_addr);
  if (symbol.type & HTT_GLBL_VAR)
    return reinterpret_cast<uintptr_t>(
        reinterpret_cast<const CHashGlblVar *>(&symbol)->data_addr);
  return reinterpret_cast<const CHashExport *>(&symbol)->val;
}

static void SysSymImportsResolve(const char *name, CHashTable *table)
{
  while (auto *entry = HashSingleTableFind(name, table, HTT_IMPORT_SYS_SYM)) {
    auto *symbol = FindLinkSymbol(name, table);
    if (!symbol)
      return;
    auto &import = *reinterpret_cast<CLoadImport *>(entry);
    LoadOneImport(import, LinkAddress(*symbol));
    import.module->unresolved--;
    entry->type = HTT_INVALID;
  }
}

extern "C" void SysSymImportsResolveCpp(const char *name, int64_t)
{
  if (name)
    SysSymImportsResolve(name, CurrentTask()->hash_table);
}

static void LoadPass1(CLoadedModule &module, CHashTable *table, unsigned flags)
{
  auto *base = ModuleBase(module);
  for (size_t i = 0; i < module.count; i++) {
    auto &rec = module.records[i];
    if (rec.exported) {
      HashAdd(&rec.exported->sym.hash, table);
      rec.exported = nullptr;
      if (IsExport(rec.type))
        SysSymImportsResolve(rec.name, table);
    }
    if (rec.imported) {
      if (auto *entry = FindLinkSymbol(rec.name, table)) {
        LoadOneImport(*rec.imported, LinkAddress(*entry));
        HashDel(&rec.imported->entry.sym.hash);
      } else {
        HashAdd(&rec.imported->entry.sym.hash, table);
        module.unresolved++;
        if (!(flags & LDF_SILENT)) {
          SerialPutS("Unresolved Reference:");
          SerialPutS(rec.name);
          SerialPutS("\r\n");
        }
      }
      rec.imported = nullptr;
    }
    if (rec.offsets && (rec.type != IET_ABS_ADDR || !(flags & LDF_NO_ABSS))) {
      size_t width = rec.type >= IET_DATA_HEAP ? 8 : 4;
      uint64_t addend = reinterpret_cast<uintptr_t>(
          rec.type == IET_ABS_ADDR ? base : rec.storage);
      for (uint32_t j = 0; j < rec.value; j++) {
        auto *dest = base + ReadWidth(rec.offsets + size_t{j} * 4, 4);
        uint64_t value = ReadWidth(dest, width) + addend;
        MemCpy(dest, &value, width);
      }
    }
  }
}

bool LoadPass2(CLoadedModule &module)
{
  if (module.unresolved || module.started)
    return false;
  module.started = true;
  for (size_t i = 0; i < module.count; i++)
    if (module.records[i].type == IET_MAIN)
      CallModuleMain(ModuleBase(module) + module.records[i].value);
  return true;
}

static CLoadedModule *LoadImage(const uint8_t *input, size_t size,
                                const char *path, CTask &task, unsigned flags)
{
  CBinFile header;
  if (size < sizeof(header) || size > PTRDIFF_MAX)
    return nullptr;
  MemCpy(&header, input, sizeof(header));
  size_t count;
  if (header.bin_signature != BIN_SIGNATURE_VAL ||
      (header.org != INVALID_PTR && size > UINTPTR_MAX - header.org) ||
      header.file_size != size || header.patch_table_offset < sizeof(header) ||
      header.patch_table_offset >= size || header.module_align_bits >= 63 ||
      !LoadRecords(input, header, nullptr, count) ||
      count > SIZE_MAX / sizeof(CLoadRecord))
    return nullptr;

  auto *module = reinterpret_cast<CLoadedModule *>(
      CAlloc(sizeof(CLoadedModule), task.data_heap));
  if (!module)
    return nullptr;

  module->owns_image = header.org == INVALID_PTR;
  if (module->owns_image) {
    size_t align = size_t{1} << header.module_align_bits;
    if (align < 16)
      align = 16;
    module->image = MAllocAligned(size, align, task.code_heap,
                                  (0 - sizeof(header)) & (align - 1));
  } else {
    module->image = reinterpret_cast<uint8_t *>(header.org);
  }
  if (!module->image) {
    LoadDiscard(module);
    return nullptr;
  }
  module->records = reinterpret_cast<CLoadRecord *>(
      MAlloc(count * sizeof(CLoadRecord), task.data_heap));
  if (!module->records ||
      !LoadRecords(input, header, module->records, module->count) ||
      !LoadPrepare(*module, task)) {
    LoadDiscard(module);
    return nullptr;
  }

  auto *entry = reinterpret_cast<CHashGeneric *>(
      CAlloc(sizeof(CHashGeneric), task.data_heap));
  auto *name = entry ? StrNew(path, task.data_heap) : nullptr;
  if (!name) {
    Free(entry);
    LoadDiscard(module);
    return nullptr;
  }
  size_t len = StrLen(name);
  if (len >= 2 && name[len - 2] == '.' && name[len - 1] == 'Z')
    len -= 2;
  if (len >= 4 && name[len - 4] == '.' && name[len - 3] == 'B' &&
      name[len - 2] == 'I' && name[len - 1] == 'N')
    len -= 4;
  name[len] = 0;
  // A module with ORG uses caller-owned memory. Prepare its patches before
  // replacing those bytes.
  MemCpy(module->image, input, size);
  for (size_t i = 0; i < module->count; i++) {
    auto &rec = module->records[i];
    rec.raw = module->image + (rec.raw - input);
    rec.name = reinterpret_cast<const char *>(rec.raw + 1 + sizeof(rec.value));
    if (rec.offsets)
      rec.offsets = module->image + (rec.offsets - input);
    if (rec.imported)
      rec.imported->entry.module_header_entry = rec.raw;
  }
  entry->hash.str = name;
  entry->hash.type = HTT_MODULE | HTF_PUBLIC;
  entry->user_data0 = reinterpret_cast<uintptr_t>(module->image);
  HashAdd(&entry->hash, task.hash_table);
  LoadPass1(*module, task.hash_table, flags);
  return module;
}

CLoadedModule *Load(const CDrv &dv, const char *path, CTask &task,
                    unsigned flags)
{
  bool extension = false;
  size_t length = 0;
  for (; path[length]; length++) {
    if (path[length] == '/')
      extension = false;
    else if (path[length] == '.')
      extension = true;
  }
  uint8_t *with_extension = nullptr;
  if (!extension) {
    constexpr char suffix[] = ".BIN";
    with_extension = MAlloc(length + sizeof(suffix), task.data_heap);
    if (!with_extension)
      return nullptr;
    MemCpy(with_extension, path, length);
    MemCpy(with_extension + length, suffix, sizeof(suffix));
    path = reinterpret_cast<const char *>(with_extension);
  }
  size_t size;
  auto *data = FileRead(dv, path, size, task.data_heap);
  auto *module = data ? LoadImage(data, size, path, task, flags) : nullptr;
  Free(data);
  Free(with_extension);
  if (module && !(flags & LDF_JUST_LOAD))
    LoadPass2(*module);
  return module;
}

bool LoadKernel(CTask &task)
{
  struct CKernelExport
  {
    const char *name;
    uintptr_t value;
  };
  static const CKernelExport exports[]{
      {"SYS_COMPILE_TIME", reinterpret_cast<uintptr_t>(SYS_COMPILE_TIME)},
      {"cnts", reinterpret_cast<uintptr_t>(&cnts)},
      {"scrncast", reinterpret_cast<uintptr_t>(&scrncast)},
      {"MEM_BOOT_BASE", reinterpret_cast<uintptr_t>(&MEM_BOOT_BASE)},
      {"SYS_RUN_LEVEL", reinterpret_cast<uintptr_t>(&SYS_RUN_LEVEL)},
      {"SYS_VAR_INIT_FLAG", reinterpret_cast<uintptr_t>(&sys_var_init_flag)},
      {"SYS_VAR_INIT_VAL", reinterpret_cast<uintptr_t>(&sys_var_init_val)},
      {"adam_task", reinterpret_cast<uintptr_t>(&adam_task)},
      {"blkdev", reinterpret_cast<uintptr_t>(&blkdev)},
      {"dbg", reinterpret_cast<uintptr_t>(&dbg)},
      {"sys_semas", reinterpret_cast<uintptr_t>(sys_semas)},
      {"_BEQU", reinterpret_cast<uintptr_t>(_BEQU)},
      {"_CALL", reinterpret_cast<uintptr_t>(_CALL)},
      {"Call", reinterpret_cast<uintptr_t>(_CALL)},
      {"_CLAMP_I64", reinterpret_cast<uintptr_t>(_CLAMP_I64)},
      {"_MEMCPY", reinterpret_cast<uintptr_t>(_MEMCPY)},
      {"_MEMSET", reinterpret_cast<uintptr_t>(_MEMSET)},
      {"_MEMSET_I64", reinterpret_cast<uintptr_t>(_MEMSET_I64)},
      {"_MALLOC", reinterpret_cast<uintptr_t>(_MALLOC)},
      {"_FREE", reinterpret_cast<uintptr_t>(_FREE)},
      {"_MHEAP_CTRL", reinterpret_cast<uintptr_t>(_MHEAP_CTRL)},
      {"_MSIZE", reinterpret_cast<uintptr_t>(_MSIZE)},
      {"_MSIZE2", reinterpret_cast<uintptr_t>(_MSIZE2)},
      {"CAlloc", reinterpret_cast<uintptr_t>(CAllocHC)},
      {"MAllocIdent", reinterpret_cast<uintptr_t>(MAllocIdentHC)},
      {"StrNew", reinterpret_cast<uintptr_t>(StrNewHC)},
      {"AStrNew", reinterpret_cast<uintptr_t>(AStrNewHC)},
      {"_STRCMP", reinterpret_cast<uintptr_t>(_STRCMP)},
      {"_STRCPY", reinterpret_cast<uintptr_t>(_STRCPY)},
      {"_STRIMATCH", reinterpret_cast<uintptr_t>(_STRIMATCH)},
      {"StrOcc", reinterpret_cast<uintptr_t>(StrOccHC)},
      {"_HASH_ADD", reinterpret_cast<uintptr_t>(_HASH_ADD)},
      {"_HASH_ADD_AFTER", reinterpret_cast<uintptr_t>(_HASH_ADD_AFTER)},
      {"_HASH_FIND", reinterpret_cast<uintptr_t>(_HASH_FIND)},
      {"_HASH_SINGLE_TABLE_FIND",
       reinterpret_cast<uintptr_t>(_HASH_SINGLE_TABLE_FIND)},
      {"HashTableNew", reinterpret_cast<uintptr_t>(HashTableNewHC)},
      {"HashTypeNum", reinterpret_cast<uintptr_t>(HashTypeNumHC)},
      {"HashVal", reinterpret_cast<uintptr_t>(HashValHC)},
      {"HashDel", reinterpret_cast<uintptr_t>(HashDelHC)},
      {"HashTableDel", reinterpret_cast<uintptr_t>(HashTableDelHC)},
      {"HashSrcFileSet", reinterpret_cast<uintptr_t>(HashSrcFileSetHC)},
      {"_HASH_REM_DEL", reinterpret_cast<uintptr_t>(_HASH_REM_DEL)},
      {"SysSymImportsResolve",
       reinterpret_cast<uintptr_t>(SysSymImportsResolveHC)},
      {"CeilU64", reinterpret_cast<uintptr_t>(CeilU64HC)},
      {"Pow10I64", reinterpret_cast<uintptr_t>(Pow10I64HC)},
      {"LinkedLstDel", reinterpret_cast<uintptr_t>(LinkedLstDelHC)},
      {"LinkedLstSize", reinterpret_cast<uintptr_t>(LinkedLstSizeHC)},
      {"QueDel", reinterpret_cast<uintptr_t>(QueDelHC)},
      {"QueVectU8New", reinterpret_cast<uintptr_t>(QueVectU8NewHC)},
      {"QueVectU8Put", reinterpret_cast<uintptr_t>(QueVectU8PutHC)},
      {"QueVectU8Get", reinterpret_cast<uintptr_t>(QueVectU8GetHC)},
      {"QueVectU8Del", reinterpret_cast<uintptr_t>(QueVectU8DelHC)},
      {"QSort", reinterpret_cast<uintptr_t>(QSortHC)},
      {"QSortI64", reinterpret_cast<uintptr_t>(QSortI64HC)},
      {"LstMatch", reinterpret_cast<uintptr_t>(LstMatchHC)},
      {"FileRead", reinterpret_cast<uintptr_t>(FileReadHC)},
      {"FileNameAbs", reinterpret_cast<uintptr_t>(FileNameAbsHC)},
      {"DirNameAbs", reinterpret_cast<uintptr_t>(DirNameAbsHC)},
      {"DirCur", reinterpret_cast<uintptr_t>(DirCurHC)},
      {"DirFile", reinterpret_cast<uintptr_t>(DirFileHC)},
      {"ExtChg", reinterpret_cast<uintptr_t>(ExtChgHC)},
      {"ExtDft", reinterpret_cast<uintptr_t>(ExtDftHC)},
      {"FileWrite", reinterpret_cast<uintptr_t>(FileWriteHC)},
      {"RedSeaISO", reinterpret_cast<uintptr_t>(RedSeaISOHC)},
      {"BlkDevNextFreeSlot", reinterpret_cast<uintptr_t>(BlkDevNextFreeSlotHC)},
      {"BlkDevAdd", reinterpret_cast<uintptr_t>(BlkDevAddHC)},
      {"Let2Drv", reinterpret_cast<uintptr_t>(Let2DrvHC)},
      {"BlkRead", reinterpret_cast<uintptr_t>(BlkReadHC)},
      {"BlkWrite", reinterpret_cast<uintptr_t>(BlkWriteHC)},
      {"SysTry", reinterpret_cast<uintptr_t>(SysTryHC)},
      {"SysUntry", reinterpret_cast<uintptr_t>(SysUntryHC)},
      {"throw", reinterpret_cast<uintptr_t>(ThrowHC)},
      {"Caller", reinterpret_cast<uintptr_t>(CallerHC)},
      {"PutExcept", reinterpret_cast<uintptr_t>(PutExceptHC)},
      {"StrPrintJoin", reinterpret_cast<uintptr_t>(StrPrintJoinHC)},
      {"StrPrint", reinterpret_cast<uintptr_t>(StrPrintHC)},
      {"CatPrint", reinterpret_cast<uintptr_t>(CatPrintHC)},
      {"MStrPrint", reinterpret_cast<uintptr_t>(MStrPrintHC)},
      {"Print", reinterpret_cast<uintptr_t>(PrintHC)},
      {"PrintErr", reinterpret_cast<uintptr_t>(PrintErrHC)},
      {"PrintWarn", reinterpret_cast<uintptr_t>(PrintWarnHC)},
      {"PUT_HEX_U64", reinterpret_cast<uintptr_t>(PUT_HEX_U64)},
      {"PUT_HEX_U32", reinterpret_cast<uintptr_t>(PUT_HEX_U32)},
      {"PUT_HEX_U16", reinterpret_cast<uintptr_t>(PUT_HEX_U16)},
      {"PUT_HEX_U8", reinterpret_cast<uintptr_t>(PUT_HEX_U8)},
      {"PUT_CHARS", reinterpret_cast<uintptr_t>(PUT_CHARS)},
      {"PUT_STR", reinterpret_cast<uintptr_t>(PUT_STR)},
      {"PutHex", reinterpret_cast<uintptr_t>(PutHexHC)},
      {"PutChars", reinterpret_cast<uintptr_t>(PutCharsHC)},
      {"PutS", reinterpret_cast<uintptr_t>(PutSHC)},
      {"tS", reinterpret_cast<uintptr_t>(tSHC)},
      {"Busy", reinterpret_cast<uintptr_t>(BusyHC)},
      {"SleepUntil", reinterpret_cast<uintptr_t>(SleepUntilHC)},
      {"Sleep", reinterpret_cast<uintptr_t>(SleepHC)},
      {"Ona2Freq", reinterpret_cast<uintptr_t>(Ona2FreqHC)},
      {"Freq2Ona", reinterpret_cast<uintptr_t>(Freq2OnaHC)},
      {"Snd", reinterpret_cast<uintptr_t>(SndHC)},
      {"ScrnCast", reinterpret_cast<uintptr_t>(ScrnCastHC)},
      {"SndRst", reinterpret_cast<uintptr_t>(SndRstHC)},
      {"Beep", reinterpret_cast<uintptr_t>(BeepHC)},
      {"Mute", reinterpret_cast<uintptr_t>(MuteHC)},
      {"IsMute", reinterpret_cast<uintptr_t>(IsMuteHC)},
      {"ExePutS", reinterpret_cast<uintptr_t>(ExePutSHC)},
      {"Cmp", reinterpret_cast<uintptr_t>(CmpHC)},
      {"ExeFile", reinterpret_cast<uintptr_t>(ExeFileHC)},
      {"StreamPrint", reinterpret_cast<uintptr_t>(StreamPrintHC)},
      {"StreamExePrint", reinterpret_cast<uintptr_t>(StreamExePrintHC)},
      {"IsRaw", reinterpret_cast<uintptr_t>(IsRawHC)},
      {"Raw", reinterpret_cast<uintptr_t>(RawHC)},
      {"DbgMode", reinterpret_cast<uintptr_t>(DbgModeHC)},
      {"IsDbgMode", reinterpret_cast<uintptr_t>(IsDbgModeHC)},
      {"PutFileLink", reinterpret_cast<uintptr_t>(PutFileLinkHC)},
      {"PutSrcLink", reinterpret_cast<uintptr_t>(PutSrcLinkHC)},
      {"FixSet", reinterpret_cast<uintptr_t>(FixSetHC)},
      {"AdamErr", reinterpret_cast<uintptr_t>(AdamErrHC)},
      {"DefineLstLoad", reinterpret_cast<uintptr_t>(DefineLstLoadHC)},
      {"DefineSub", reinterpret_cast<uintptr_t>(DefineSubHC)},
      {"DefineMatch", reinterpret_cast<uintptr_t>(DefineMatchHC)},
      {"DocNew", reinterpret_cast<uintptr_t>(DocNewHC)},
      {"DocDel", reinterpret_cast<uintptr_t>(DocDelHC)},
      {"DocLoad", reinterpret_cast<uintptr_t>(DocLoadHC)},
      {"DocSize", reinterpret_cast<uintptr_t>(DocSizeHC)},
      {"DocSave", reinterpret_cast<uintptr_t>(DocSaveHC)},
      {"DocWrite", reinterpret_cast<uintptr_t>(DocWriteHC)},
      {"DocPrint", reinterpret_cast<uintptr_t>(DocPrintHC)},
      {"Doc2PlainText", reinterpret_cast<uintptr_t>(Doc2PlainTextHC)},
      {"DocBinsValidate", reinterpret_cast<uintptr_t>(DocBinsValidateHC)},
      {"DocPutLine", reinterpret_cast<uintptr_t>(DocPutLineHC)},
      {"char_bmp_alpha", reinterpret_cast<uintptr_t>(char_bmp_alpha)},
      {"char_bmp_alpha_numeric",
       reinterpret_cast<uintptr_t>(char_bmp_alpha_numeric)},
      {"char_bmp_alpha_numeric_no_at",
       reinterpret_cast<uintptr_t>(char_bmp_alpha_numeric_no_at)},
      {"char_bmp_word", reinterpret_cast<uintptr_t>(char_bmp_word)},
      {"char_bmp_filename", reinterpret_cast<uintptr_t>(char_bmp_filename)},
      {"char_bmp_dec_numeric",
       reinterpret_cast<uintptr_t>(char_bmp_dec_numeric)},
      {"char_bmp_hex_numeric",
       reinterpret_cast<uintptr_t>(char_bmp_hex_numeric)},
      {"char_bmp_white_space",
       reinterpret_cast<uintptr_t>(char_bmp_white_space)},
      {"char_bmp_non_eol_white_space",
       reinterpret_cast<uintptr_t>(char_bmp_non_eol_white_space)},
      {"char_bmp_zero_cr_nl_cursor",
       reinterpret_cast<uintptr_t>(char_bmp_zero_cr_nl_cursor)},
      {"char_bmp_zero_tab_cr_nl_cursor",
       reinterpret_cast<uintptr_t>(char_bmp_zero_tab_cr_nl_cursor)},
      {"char_bmp_zero_tab_cr_nl_cursor_dollar",
       reinterpret_cast<uintptr_t>(char_bmp_zero_tab_cr_nl_cursor_dollar)},
      {"char_bmp_macro", reinterpret_cast<uintptr_t>(char_bmp_macro)},
      {"char_bmp_printable", reinterpret_cast<uintptr_t>(char_bmp_printable)},
      {"char_bmp_displayable",
       reinterpret_cast<uintptr_t>(char_bmp_displayable)},
      {"char_bmp_safe_dollar",
       reinterpret_cast<uintptr_t>(char_bmp_safe_dollar)},
      {"char_bmp_non_eol", reinterpret_cast<uintptr_t>(char_bmp_non_eol)},
  };
  constexpr char name[] = "/Kernel/Kernel";
  if (HashSingleTableFind(name, task.hash_table, HTT_MODULE))
    return true;
  auto *kernel = reinterpret_cast<CHashGeneric *>(
      CAlloc(sizeof(CHashGeneric), task.data_heap));
  if (!kernel)
    return false;
  kernel->hash.str = StrNew(name, task.data_heap);
  kernel->hash.type = HTT_MODULE | HTF_PUBLIC;
  kernel->user_data0 =
      reinterpret_cast<uintptr_t>(SYS_KERNEL) - BIN_HEADER_SIZE;
  CHash *pending = &kernel->hash;
  auto **tail = &kernel->hash.next;
  bool ok = kernel->hash.str;
  for (size_t i = 0; ok && i < sizeof(exports) / sizeof(exports[0]); i++) {
    auto *symbol = reinterpret_cast<CHashExport *>(
        CAlloc(sizeof(CHashExport), task.data_heap));
    if (!symbol) {
      ok = false;
      break;
    }
    symbol->sym.hash.str = StrNew(exports[i].name, task.data_heap);
    symbol->sym.hash.type = HTT_EXPORT_SYS_SYM | HTF_IMM;
    symbol->val = exports[i].value;
    *tail = &symbol->sym.hash;
    tail = &symbol->sym.hash.next;
    ok = symbol->sym.hash.str;
  }
  while (pending) {
    auto *next = pending->next;
    if (ok)
      HashAdd(pending, task.hash_table);
    else
      HashDel(pending);
    pending = next;
  }
  return ok;
}
