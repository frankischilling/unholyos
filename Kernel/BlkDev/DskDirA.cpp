#include "../KernelB.hpp"

extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);

int64_t DirEntryCompareName(const CRedSeaDirEntry &a, const CRedSeaDirEntry &b)
{
  int da = bool(a.attr & RS_ATTR_DIR), db = bool(b.attr & RS_ATTR_DIR);
  if (da != db)
    return db - da;
  const char *ext_a = FileExtDot(a.name), *ext_b = FileExtDot(b.name);
  auto ext_cmp = StrCmpCpp(reinterpret_cast<const uint8_t *>(ext_a ? ext_a+1 : ""),
                           reinterpret_cast<const uint8_t *>(ext_b ? ext_b+1 : ""));
  if (ext_cmp)
    return ext_cmp;
  size_t len_a = ext_a ? size_t(ext_a-a.name) : StrLen(a.name);
  size_t len_b = ext_b ? size_t(ext_b-b.name) : StrLen(b.name);
  for (size_t i = 0; i < len_a && i < len_b; i++)
    if (a.name[i] != b.name[i])
      return uint8_t(a.name[i]) - uint8_t(b.name[i]);
  return (len_a > len_b) - (len_a < len_b);
}

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
