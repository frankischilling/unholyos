#include "KernelB.hpp"

extern "C" int64_t LstMatchCpp(const uint8_t *needle, const uint8_t *list,
                               uint64_t flags);

// Keep the actual return address of DefinePrint's internal DefineLoad call.
extern "C" [[gnu::noinline]] CHashDefineStr *
DefineLoadCpp(const char *name, const char *text, const uintptr_t *frame)
{
  auto &task = *CurrentTask();
  auto *hc = task.data_heap;
  auto *entry =
      reinterpret_cast<CHashDefineStr *>(CAlloc(sizeof(CHashDefineStr), hc));
  if (!entry)
    return nullptr;
  entry->sym.hash.type = HTT_DEFINE_STR;
  entry->sym.hash.str = StrNew(name, hc);
  entry->data = StrNew(text, hc);
  entry->cnt = -1;
  uint64_t pc = reinterpret_cast<uintptr_t>(
      frame ? Caller(frame, 0, task) : __builtin_return_address(0));
  char link[5 + 16 + 1];
  entry->sym.src_link = StrNew(StrPrintJoin(link, "AD:0x%X", 1, &pc), hc);
  if (!entry->sym.hash.str || !entry->data || !entry->sym.src_link) {
    Free(const_cast<char *>(entry->sym.hash.str));
    Free(entry->data);
    Free(const_cast<char *>(entry->sym.src_link));
    Free(entry);
    return nullptr;
  }
  HashAdd(&entry->sym.hash, task.hash_table);
  return entry;
}

extern "C" CHashDefineStr *DefineLstLoadCpp(const char *name, const char *list,
                                            const uintptr_t *frame)
{
  auto &task = *CurrentTask();
  auto *hc = task.data_heap;
  auto *entry =
      reinterpret_cast<CHashDefineStr *>(CAlloc(sizeof(CHashDefineStr), hc));
  if (!entry)
    return nullptr;
  const char *end = list;
  size_t count = 0;
  while (*end) {
    count += *end != '@';
    end += StrLen(end) + 1;
  }
  entry->sym.hash.str = StrNew(name, hc);
  entry->data = reinterpret_cast<char *>(MAlloc(end - list + 1, hc));
  entry->sub_idx =
      reinterpret_cast<const char **>(MAlloc(count * sizeof(char *), hc));
  uint64_t pc = reinterpret_cast<uintptr_t>(Caller(frame, 0, task));
  char link[5 + 16 + 1];
  entry->sym.src_link = StrNew(StrPrintJoin(link, "AD:0x%X", 1, &pc), hc);
  if (!entry->sym.hash.str || !entry->data || !entry->sub_idx ||
      !entry->sym.src_link) {
    Free(const_cast<char *>(entry->sym.hash.str));
    Free(const_cast<char *>(entry->sym.src_link));
    Free(entry->data);
    Free(entry->sub_idx);
    Free(entry);
    return nullptr;
  }
  MemCpy(entry->data, list, end - list + 1);
  entry->cnt = count;
  auto **index = entry->sub_idx;
  for (auto *p = list; *p; p += StrLen(p) + 1)
    if (*p != '@')
      *index++ = p;
  entry->sym.hash.type = HTT_DEFINE_STR;
  HashAdd(&entry->sym.hash, task.hash_table);
  return entry;
}

static const CHashDefineStr *DefineFind(const char *name, bool &defined)
{
  auto *entry = name ? HashFind(name, CurrentTask()->hash_table, HTT_DEFINE_STR)
                     : nullptr;
  defined = entry || !name;
  return reinterpret_cast<const CHashDefineStr *>(entry);
}

const char *DefineText(const char *name, bool &defined)
{
  auto *entry = DefineFind(name, defined);
  return entry ? entry->data : nullptr;
}

const char *DefineSub(int64_t index, const char *name, bool &defined)
{
  auto *entry = DefineFind(name, defined);
  return entry && 0 <= index && index < entry->cnt ? entry->sub_idx[index]
                                                   : nullptr;
}

[[noreturn]] void UndefinedDefine(const char *name)
{
  PutS("ERROR: Undefined Define: '");
  PutS(name);
  PutS("'.\n");
  RaiseHolyC(UNDEF_DEFINE_ERROR);
}

extern "C" const char *DefineCpp(const char *name)
{
  bool defined;
  auto *res = DefineText(name, defined);
  if (!defined)
    UndefinedDefine(name);
  return res;
}

extern "C" const char *DefineSubCpp(int64_t index, const char *name)
{
  bool defined;
  auto *res = DefineSub(index, name, defined);
  if (!defined)
    UndefinedDefine(name);
  return res;
}

extern "C" int64_t DefineCntCpp(const char *name)
{
  bool defined;
  auto *entry = DefineFind(name, defined);
  if (!defined)
    UndefinedDefine(name);
  return entry ? entry->cnt : -1;
}

extern "C" int64_t DefineMatchCpp(const uint8_t *needle, const char *name,
                                  uint64_t flags)
{
  return LstMatchCpp(needle, reinterpret_cast<const uint8_t *>(DefineCpp(name)),
                    flags);
}

extern "C" void DefinePrintCpp(const char *name, const char *src, int64_t argc,
                               const uint64_t *argv)
{
  auto *buf = StrPrintJoin(nullptr, src, argc, argv);
  DefineLoadCpp(name, buf, nullptr);
  Free(buf);
}

// TempleOS Kernel/KDefine.HC::SysDefinesLoad, Terry A. Davis.
bool SysDefinesLoad()
{
  // Native boot definitions use a null caller frame, recording AD:0x0.
  if (!DefineLstLoadCpp("ST_OFF_ON", "Off\0On\0", nullptr))
    return false;
  if (!DefineLstLoadCpp(
          "ST_HTT_TYPES",
          "ExportSysSym\0ImportSysSym\0DefineStr\0GlbVar\0"
          "Class\0IntType\0Funct\0Word\0DictWord\0KeyWord\0AsmKeyWord\0OpCode\0"
          "Reg\0File\0Module\0HelpFile\0Frame Ptr\0 \0 \0 \0 \0 \0 \0Private\0"
          "Public\0Export\0Import\0Imm\0Goto\0Res\0Unres\0Local\0",
          nullptr))
    return false;
  if (!DefineLstLoadCpp(
          "ST_DAYS_OF_WEEK",
          "Sunday\0Monday\0Tuesday\0Wednesday\0Thursday\0Friday\0Saturday\0",
          nullptr))
    return false;
  if (!DefineLstLoadCpp(
          "ST_MONTHS",
          "January\0February\0March\0April\0May\0"
          "June\0July\0August\0September\0October\0November\0December\0",
          nullptr))
    return false;
  if (!DefineLstLoadCpp("ST_FILE_ATTRS",
                        "R\0H\0S\0V\0D\0A\0 \0 \0X\0T\0Z\0C\0F\0", nullptr))
    return false;
  if (!DefineLstLoadCpp("ST_FILE_UTIL_FLAGS",
                        "r\0d\0i\0a\0c\0R\0p\0m\0x\0s\0"
                        "D\0F\0T\0$\0S\0A\0J\0G\0Z\0O\0P\0f\0l\0lb\0la\0",
                        nullptr))
    return false;
  if (!DefineLstLoadCpp("ST_BLKDEV_TYPES",
                        "NULL\0RAM\0ATA\0FILE_READ\0FILE_WRITE\0ATAPI\0",
                        nullptr))
    return false;
  if (!DefineLstLoadCpp("ST_DRV_TYPES",
                        "NULL\0REDSEA\0FAT32\0ISO9660\0NTFS\0UNKNOWN\0",
                        nullptr))
    return false;
  if (!DefineLstLoadCpp("ST_COLORS",
                        "BLACK\0BLUE\0GREEN\0CYAN\0"
                        "RED\0PURPLE\0BROWN\0LTGRAY\0DKGRAY\0LTBLUE\0LTGREEN\0"
                        "LTCYAN\0LTRED\0LTPURPLE\0YELLOW\0WHITE\0",
                        nullptr))
    return false;
  return DefineLstLoadCpp(
      "ST_INT_NAMES",
      "Divide Error\0SingleStep\0NMI\0Breakpoint\0"
      "Overflow\0BOUND Range Exceeded\0Invalid Opcode\0No Math Coprocessor\0"
      "Double Fault\0Coprocessor Segment Fault\0Invalid TASK\0"
      "Segment Not Present\0Stk Segment Fault\0General Protection\0"
      "Page Fault\0 \0Math Fault\0Alignment Check\0Machine Check\0"
      "SIMD Exception\0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0"
      " \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0MP Crash\0Wake\0Dbg\0",
      nullptr);
}
