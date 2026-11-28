#include "../Kernel/KernelB.hpp"
#include "CompilerA.hpp"

extern "C" void PrintErrCpp(const char *fmt, int64_t argc, const uint64_t *argv,
                            const uintptr_t *frame);

extern "C" char *ExtChgCpp(const char *name, const char *ext);
extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);
extern "C" int64_t LastFunCpp(int64_t argc, const uint64_t *argv);
extern "C" uint64_t FileWriteCpp(const char *name, const uint8_t *data,
                                 uint64_t size, uint64_t date, uint64_t attr);

struct CAOTBinBlk
{
  CAOTBinBlk *next;
  const void *symbol;
  const char *name;
  size_t rip, size;
  bool main;
};

struct CAOTImportExport
{
  CAOTImportExport *next;
  const void *symbol;
  char *name;
  size_t rip;
  uint8_t type;
};

bool AOTStore(CAOT &aot, const void *symbol, const char *name,
              const uint8_t *body, size_t size, bool main)
{
  // Keep even empty declarations at distinct payload addresses.
  size_t step = ((size ? size : 1) + 7) & ~size_t{7};
  if (size > INT32_MAX || step > INT32_MAX - aot.size)
    return false;
  auto *block = reinterpret_cast<CAOTBinBlk *>(
      MAlloc(sizeof(CAOTBinBlk) + size, CurrentTask()->data_heap));
  if (!block)
    return false;
  *block = {nullptr, symbol, name, aot.size, size, main};
  MemCpy(block + 1, body, size);
  if (aot.last)
    aot.last->next = block;
  else
    aot.first = block;
  aot.last = block;
  aot.size += step;
  return true;
}

bool AOTLabel(CAOT &aot, const void *symbol, const char *name, size_t rip)
{
  auto *block = reinterpret_cast<CAOTBinBlk *>(
      MAlloc(sizeof(CAOTBinBlk), CurrentTask()->data_heap));
  if (!block)
    return false;
  *block = {nullptr, symbol, name, rip, 0, false};
  if (aot.last)
    aot.last->next = block;
  else
    aot.first = block;
  aot.last = block;
  return true;
}

bool AOTRelAdd(CAOT &aot, size_t rip, const void *symbol, const char *name,
                uint8_t type)
{
  auto *ie = reinterpret_cast<CAOTImportExport *>(
      MAlloc(sizeof(CAOTImportExport), CurrentTask()->data_heap));
  if (!ie)
    return false;
  auto *copy = name ? StrNew(name, CurrentTask()->data_heap) : nullptr;
  if (name && !copy) {
    Free(ie);
    return false;
  }
  *ie = {nullptr, symbol, copy, rip, type};
  if (aot.last_ie)
    aot.last_ie->next = ie;
  else
    aot.first_ie = ie;
  aot.last_ie = ie;
  return true;
}

void AOTRelResolve(CAOT &aot, const void *before, const void *after)
{
  for (auto *ie = aot.first_ie; ie; ie = ie->next)
    if (!ie->name && ie->symbol == before)
      ie->symbol = after;
}

static bool CmpJoin(CCmpCtrl &cc, CAOT &aot)
{
  auto &task = *CurrentTask();
  cc.aot = &aot;
  QueIns(cc, *task.last_cc);
  Lex(cc);
  bool okay = true;
  while (okay && cc.token != TK_EOF) {
    uint8_t *machine;
    CRawType type;
    okay = LexStmt2Bin(cc, machine, type) && !cc.jit_exception;
  }
  if (!okay && cc.parse_error_count)
    LexPutPos(cc);
  QueRem(cc);
  return okay;
}

static void AOTDel(CAOT &aot, CHashTable *table)
{
  while (aot.first) {
    auto *block = aot.first;
    aot.first = block->next;
    Free(block);
  }
  while (aot.first_ie) {
    auto *ie = aot.first_ie;
    aot.first_ie = ie->next;
    Free(ie->name);
    Free(ie);
  }
  if (table) {
    for (int64_t i = 0; i <= table->mask; i++)
      for (auto *entry = table->body[i]; entry; entry = entry->next)
        if (entry->type & HTT_FUN) {
          auto &fun = *reinterpret_cast<CHashFun *>(entry);
          // Internal declarations store an IC opcode, not allocated code.
          if (!(fun.cls.flags & (FF__EXTERN | FF_INTERNAL)))
            Free(fun.exe_addr);
          for (auto *member = fun.cls.member_lst_and_root; member;
               member = member->next)
            if (member->flags & MLF_STATIC)
              Free(member->static_data);
        }
  }
  HashTableDelCpp(table);
}

static uint8_t *PatchRecord(uint8_t *p, uint8_t type, uint32_t rip,
                            const char *name)
{
  *p++ = type;
  MemCpy(p, &rip, sizeof(rip));
  p += sizeof(rip);
  if (!name)
    name = "";
  size_t len = StrLen(name) + 1;
  MemCpy(p, name, len);
  return p + len;
}

static CBinFile *AOTBinMake(const CAOT &aot)
{
  size_t patches = 1;
  for (auto *b = aot.first; b; b = b->next) {
    if (b->name)
      patches += 6 + StrLen(b->name);
    if (b->main)
      patches += 6;
  }
  uint32_t abs_count = 0;
  for (auto *ie = aot.first_ie; ie; ie = ie->next)
    if (ie->name)
      patches += 6 + StrLen(ie->name);
    else if (ie->type == IET_ABS_ADDR)
      abs_count++;
  if (abs_count)
    patches += 6 + sizeof(uint32_t) * size_t(abs_count);

  size_t patch_offset = sizeof(CBinFile) + aot.size;
  size_t size = (patch_offset + patches + 15) & ~size_t{15};
  auto *bfh = reinterpret_cast<CBinFile *>(
      CAlloc(size, CurrentTask()->data_heap));
  if (!bfh)
    return nullptr;
  *bfh = {0x1EEB, aot.max_align_bits, 0, BIN_SIGNATURE_VAL, aot.org,
          patch_offset, size};

  auto *base = reinterpret_cast<uint8_t *>(bfh + 1);
  auto *patch = base + aot.size;
  for (auto *b = aot.first; b; b = b->next) {
    MemCpy(base + b->rip, b + 1, b->size);
    if (b->name)
      patch = PatchRecord(patch, IET_REL32_EXPORT, b->rip, b->name);
    if (b->main)
      patch = PatchRecord(patch, IET_MAIN, b->rip, nullptr);
  }
  if (abs_count) {
    patch = PatchRecord(patch, IET_ABS_ADDR, abs_count, nullptr);
    for (auto *ie = aot.first_ie; ie; ie = ie->next)
      if (ie->type == IET_ABS_ADDR) {
        uint32_t offset = ie->rip;
        MemCpy(patch, &offset, sizeof(offset));
        patch += sizeof(offset);
      }
  }
  for (auto *ie = aot.first_ie; ie; ie = ie->next) {
    if (ie->name) {
      patch = PatchRecord(patch, ie->type, ie->rip, ie->name);
    } else if (ie->type == IET_ABS_ADDR) {
      continue;
    } else {
      auto *target = aot.first;
      while (target && target->symbol != ie->symbol)
        target = target->next;
      if (!target) {
        Free(bfh);
        return nullptr;
      }
      // A RIP-relative displacement stays valid when Load moves the image.
      uint32_t disp = target->rip - (ie->rip + sizeof(uint32_t));
      MemCpy(base + ie->rip, &disp, sizeof(disp));
    }
  }
  *patch = IET_END;
  return bfh;
}

extern "C" int64_t CmpCpp(const char *filename, const char *map_name,
                          const char *out_name, uint8_t mapfile_drv_let)
{
  auto *path = filename ? ExtDftCpp(filename, "PRJ.Z") : nullptr;
  auto *out = out_name ? ExtDftCpp(out_name, "BIN.Z")
                       : path ? ExtChgCpp(path, "BIN.Z") : nullptr;
  auto *table = path && out ? CmpAsmHashNew() : nullptr;
  const uint64_t arg = reinterpret_cast<uintptr_t>(path);
  auto *source = table ? MStrPrintCpp("#include \"%s\"", 1, &arg) : nullptr;
  CAOT aot;
  CBinFile *bfh = nullptr;
  uint64_t error = 0;
  if (source) {
    CCmpCtrl cc;
    CLexFile file;
    LexSourceInit(cc, file, source, CCF_AOT_COMPILE, path, table);
    if (CmpJoin(cc, aot))
      bfh = AOTBinMake(aot);
    error = cc.parse_error_count;
    LexSourceDel(cc);
  }
  if (!error &&
      (!bfh || (map_name && !MapFileWrite(*table, map_name, mapfile_drv_let)) ||
       !FileWriteCpp(out, reinterpret_cast<uint8_t *>(bfh), bfh->file_size, 0, 0)))
    error = 1;
  uint64_t report[]{error, aot.warning_cnt, aot.size, bfh ? bfh->file_size : 0};
  PrintCpp("Errs:%d Warns:%d Code:%X Size:%X\n", 4, report);
  Free(bfh);
  AOTDel(aot, table);
  Free(source);
  Free(out);
  Free(path);
  return error;
}

CExeResult ExePutS(const char *src, const char *filename, int64_t flags,
                   const CLexHashTableContext *htc)
{
  auto *task = CurrentTask();
  if (!CmpAsmHashLoad(task->hash_table))
    return {};
  auto *path = filename ? FileNameAbs(filename, *task) : nullptr;
  if (filename && !path)
    return {};
  auto *head = reinterpret_cast<CCmpCtrl *>(&task->next_cc);
  CCmpCtrl cc;
  CLexFile file;
  LexSourceInit(cc, file, src, flags, path, task->hash_table);
  if (task->last_cc != head) {
    cc.opts = task->last_cc->opts;
    if (htc) {
      cc.flags = (cc.flags & ~CCF_ASM_EXPRESSIONS) |
                 (htc->old_flags & CCF_ASM_EXPRESSIONS);
      cc.htc = *htc;
    }
  }
  QueIns(cc, *task->last_cc);
  Lex(cc);
  uint64_t value;
  bool okay = ExeCmdLine(cc, value);
  QueRem(cc);
  LexSourceDel(cc);
  Free(path);
  return {okay ? value : 0, cc.jit_exception, okay, cc.parse_error_count != 0};
}

CExeResult ExeFile(const char *name, int64_t flags)
{
  auto *path = ExtDftCpp(name, "HC.Z");
  if (!path)
    return {};
  uint64_t arg = reinterpret_cast<uintptr_t>(path);
  auto *source = MStrPrintCpp("#include \"%s\";", 1, &arg);
  Free(path);
  if (!source)
    return {};
  auto result = ExePutS(source, name, flags);
  Free(source);
  return result;
}

int64_t ExeAnswer(const CExeResult &result)
{
  constexpr int64_t brk = 0x6B61657242; // Break
  if (result.exception && result.exception != COMPILER_ERROR &&
      result.exception != brk)
    RethrowHolyC();
  return static_cast<int64_t>(result.value);
}

extern "C" int64_t ExePutSCpp(const char *src, const char *filename,
                              int64_t flags, const CLexHashTableContext *htc)
{
  return ExeAnswer(ExePutS(src, filename, flags, htc));
}

static CExeResult ExePrint(const char *fmt, int64_t argc, const uint64_t *argv)
{
  auto *source = StrPrintJoin(nullptr, fmt, argc, argv);
  if (!source)
    return {};
  auto result = ExePutS(source, nullptr, 0);
  Free(source);
  return result;
}

extern "C" int64_t ExePrintCpp(const char *fmt, int64_t argc,
                                const uint64_t *argv)
{
  return ExeAnswer(ExePrint(fmt, argc, argv));
}

extern "C" int64_t ExeFileCpp(const char *name, int64_t flags)
{
  return ExeAnswer(ExeFile(name, flags));
}

extern "C" int64_t RunFileCpp(const char *name, int64_t flags, int64_t argc,
                               const uint64_t *argv)
{
  ExeFileCpp(name, flags);
  return LastFunCpp(argc, argv);
}

static int64_t ExeAnswer2(const CExeResult &result)
{
  if (result.exception)
    RethrowHolyC();
  if (!result.okay)
    RaiseHolyC(COMPILER_ERROR);
  return static_cast<int64_t>(result.value);
}

extern "C" int64_t ExePutS2Cpp(const char *src, const char *filename,
                                int64_t flags)
{
  return ExeAnswer2(ExePutS(src, filename, flags));
}

extern "C" int64_t ExePrint2Cpp(const char *fmt, int64_t argc,
                                 const uint64_t *argv)
{
  return ExeAnswer2(ExePrint(fmt, argc, argv));
}

extern "C" int64_t ExeFile2Cpp(const char *name, int64_t flags)
{
  return ExeAnswer2(ExeFile(name, flags));
}

extern "C" int64_t RunFile2Cpp(const char *name, int64_t flags, int64_t argc,
                                const uint64_t *argv)
{
  ExeFile2Cpp(name, flags);
  return LastFunCpp(argc, argv);
}

extern "C" int64_t StreamExePrintCpp(const char *fmt, int64_t argc,
                                     const uint64_t *argv,
                                     const uintptr_t *frame)
{
  auto *task = CurrentTask();
  auto *cc = task->last_cc;
  if (cc == reinterpret_cast<CCmpCtrl *>(&task->next_cc)) {
    PrintErrCpp("Not Compiling\n", 0, nullptr, frame);
    return 0;
  }
  if (!(cc->flags & CCF_EXE_BLK)) {
    PrintErrCpp("StreamExePrint only allowed in AOT compiled #exe{} mode.\n", 0,
                nullptr, frame);
    RaiseHolyC(COMPILER_ERROR);
  }
  if (!cc->htc.next)
    return 0;
  auto *source = StrPrintJoin(nullptr, fmt, argc, argv);
  if (!source)
    return 0;
  auto result = ExePutS(source, nullptr, 0, cc->htc.next);
  Free(source);
  return ExeAnswer(result);
}

int64_t CInitStatus = 0;

extern "C" void CInitCpp()
{
  CInitStatus = -1;
  if (CmpLoadDefines() && AsmHashLoad())
    CInitStatus = 1;
}
