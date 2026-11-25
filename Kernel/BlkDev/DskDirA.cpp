#include "../KernelB.hpp"

extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);

extern "C" void PutFileLinkCpp(const char *filename, const char *full_name,
                               int64_t line, uint8_t plain_text)
{
  if (!filename)
    return;
  uint64_t args[5];
  args[0] = reinterpret_cast<uintptr_t>(filename);
  if (IsRawCpp()) {
    args[1] = line;
    PrintCpp(line ? "%s,%04d" : "%s", line ? 2 : 1, args);
    return;
  }
  bool address = filename[0] == 'A' && filename[1] && filename[2] == ':';
  char *abs = nullptr;
  if (address) {
    filename += 3;
    full_name = filename;
  } else if (!full_name) {
    abs = FileNameAbs(filename, *CurrentTask());
    full_name = abs;
  }
  const char *kind = address      ? (line ? "AL" : "AI")
                     : plain_text ? (line ? "PL" : "PI")
                                  : (line ? "FL" : "FI");
  args[0] = reinterpret_cast<uintptr_t>(filename);
  size_t n = 1;
  if (line)
    args[n++] = line;
  args[n++] = reinterpret_cast<uintptr_t>(kind);
  args[n++] = reinterpret_cast<uintptr_t>(full_name);
  if (line)
    args[n++] = line;
  PrintCpp(line ? "$LK,\"%s,%04d\",A=\"%s:%s,%d\"$" : "$LK,\"%s\",A=\"%s:%s\"$",
           n, args);
  Free(abs);
}
