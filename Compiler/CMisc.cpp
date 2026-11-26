#include "../Kernel/KernelB.hpp"
#include "CompilerA.hpp"

extern "C" void PrintErrCpp(const char *fmt, int64_t argc, const uint64_t *argv,
                            const uintptr_t *frame);
extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);
extern "C" char *DirCurCpp(const CTask *task, void *mem_task);

char *CmdLinePmt()
{
  auto *task = CurrentTask();
  if (task->new_answer) {
    uint64_t args[3]{};
    MemCpy(args, &task->answer_time, sizeof(task->answer_time));
    args[1] = args[2] = task->answer;
    if (task->answer_type == RT_F64)
      PrintCpp("%8.6fs ansf=%15.7g\n", 2, args);
    else if ((task->answer_type & ~1) != (RT_U0 & ~1))
      PrintCpp("%8.6fs ans=0x%08X=%d\n", 3, args);
    else {
      PrintCpp("%8.6fs\n", 1, args);
      task->answer = 0;
    }
    task->new_answer = false;
  }
  if (auto *path = DirCurCpp(task, task)) {
    PutS(path);
    Free(path);
  }
  PutS(">");
  task->task_flags |= TASKF_CMD_LINE_PMT;
  auto *st = GetStrCpp(nullptr, nullptr, GSF_SHIFT_ESC_EXIT);
  task->task_flags &= ~TASKF_CMD_LINE_PMT;
  if (!st)
    return nullptr;
  size_t len = StrLen(st);
  while (len && static_cast<uint8_t>(st[len - 1]) <= ' ')
    --len;
  auto *res = reinterpret_cast<char *>(MAlloc(len + 3, task->data_heap));
  if (res) {
    MemCpy(res, st, len);
    if (len && res[len - 1] == ';')
      res[len++] = ';'; // Lex reads one token ahead.
    res[len++] = '\n';
    res[len] = 0;
  }
  Free(st);
  return res;
}

extern "C" void StreamPrintCpp(const char *fmt, int64_t argc,
                               const uint64_t *argv, const uintptr_t *frame)
{
  auto *task = CurrentTask();
  auto *cc = task->last_cc;
  if (cc == reinterpret_cast<CCmpCtrl *>(&task->next_cc) ||
      cc->last_stream_blk ==
          reinterpret_cast<CStreamBlk *>(&cc->next_stream_blk)) {
    PrintErrCpp("No exe{} blk\n", 0, nullptr, frame);
    return;
  }
  auto &stream = *cc->last_stream_blk;
  auto *text = StrPrintJoin(nullptr, fmt, argc, argv);
  if (!text) {
    stream.error = true;
    return;
  }
  if (!stream.body) {
    stream.body = text;
    return;
  }
  size_t prefix = StrLen(stream.body), suffix = StrLen(text) + 1;
  auto *body =
      suffix <= SIZE_MAX - prefix
          ? reinterpret_cast<char *>(MAlloc(prefix + suffix, task->data_heap))
          : nullptr;
  if (body) {
    MemCpy(body, stream.body, prefix);
    MemCpy(body + prefix, text, suffix);
    Free(stream.body);
    stream.body = body;
  } else
    stream.error = true;
  Free(text);
}
