#include "../Kernel/KernelB.hpp"
#include "CompilerA.hpp"

extern "C" CDocEntryBase *DocPutLineCpp(CDoc *doc, CDocEntryBase *entry);

const char *LexPutLine(const CLexFile &file, const char *start)
{
  if (!start)
    return nullptr;
  if (file.flags & LFSF_DOC) {
    auto *entry = reinterpret_cast<CDocEntryBase *>(const_cast<char *>(start));
    return reinterpret_cast<const char *>(DocPutLineCpp(file.doc, entry));
  }

  auto *p = start;
  while (*p && *p != '\r' && *p != '\n') {
    char text[]{*p++, 0};
    PutS(text);
  }
  if (char eol = *p) {
    ++p;
    if ((*p == '\r' || *p == '\n') && *p != eol)
      ++p;
  }
  PutS("\n");
  return p;
}

extern "C" void PrintErrCpp(const char *fmt, int64_t argc, const uint64_t *argv,
                            const uintptr_t *frame);

extern "C" [[noreturn]] void UndefinedExternCpp(uint64_t caller)
{
  PrintErrCpp("Undefined Extern\nat %P\n", 1, &caller, nullptr);
  RaiseHolyC(0x7478456665646E55); // UndefExt
}
