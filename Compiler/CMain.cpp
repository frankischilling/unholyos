#include "../Kernel/KernelB.hpp"
#include "CompilerA.hpp"

extern "C" void PrintErrCpp(const char *fmt, int64_t argc, const uint64_t *argv,
                            const uintptr_t *frame);

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
  return {okay ? value : 0, cc.jit_exception, okay};
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

extern "C" int64_t ExeFileCpp(const char *name, int64_t flags)
{
  return ExeAnswer(ExeFile(name, flags));
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
