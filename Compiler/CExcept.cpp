#include "../Kernel/KernelB.hpp"

extern "C" void PrintErrCpp(const char *fmt, int64_t argc, const uint64_t *argv,
                            const uintptr_t *frame);

extern "C" [[noreturn]] void UndefinedExternCpp(uint64_t caller)
{
  PrintErrCpp("Undefined Extern\nat %P\n", 1, &caller, nullptr);
  RaiseHolyC(0x7478456665646E55); // UndefExt
}
