#include "../Kernel/KernelB.hpp"
#include "CompilerA.hpp"

extern "C" void PrintErrCpp(const char *fmt, int64_t argc, const uint64_t *argv,
                            const uintptr_t *frame);

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
