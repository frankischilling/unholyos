#include "../Kernel/KernelB.hpp"
#include "CompilerA.hpp"

extern "C" CDocEntryBase *DocPutLineCpp(CDoc *doc, CDocEntryBase *entry);
extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);
extern "C" void PrintErrCpp(const char *fmt, int64_t argc, const uint64_t *argv,
                            const uintptr_t *frame);
extern "C" void AdamErrCpp(const char *fmt, int64_t argc, const uint64_t *argv);
extern "C" void FixSetCpp(const char *filename, int64_t line);

static void LexPutToken(const CCmpCtrl &cc)
{
  PutS("\"");
  const auto token = cc.token;
  const uint64_t str = reinterpret_cast<uintptr_t>(cc.cur_str);
  const uint64_t integer = cc.cur_i64;
  switch (token) {
  case TK_IDENT:
  case TK_STR:
    PrintCpp("%s", 1, &str);
    break;
  case TK_I64:
    PrintCpp("INT:%X", 1, &integer);
    break;
  case TK_CHAR_CONST:
    PrintCpp("U8:%X", 1, &integer);
    break;
  case TK_F64:
    PrintCpp("FLOAT:%e", 1, &cc.cur_f64);
    break;
  case '\n':
    PutS("<NEW_LINE>");
    break;
  default:
    if (uint64_t(token) < 512 &&
        (char_bmp_displayable[token / 32] >> (token & 31) & 1)) {
      const char text[]{char(token), 0};
      PutS(text);
    } else {
      const uint64_t value = token;
      PrintCpp("T:%X", 1, &value);
    }
    break;
  }
  PutS("\" ");
}

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

static void LexPutPos(const CCmpCtrl &cc)
{
  LexPutToken(cc);
  if (const auto *file = cc.lex_include_stk) {
    const uint64_t args[]{reinterpret_cast<uintptr_t>(file->full_name),
                          uint64_t(file->line_num)};
    FixSetCpp(file->full_name, file->line_num);
    if (IsRawCpp())
      PrintCpp("%s,%d ", 2, args);
    else {
      PutFileLinkCpp(file->full_name, nullptr, file->line_num, false);
      AdamErrCpp("%s,%d\n", 2, args);
      PutS(" ");
    }
    LexPutLine(*file, file->line_start);
  }
}

void LexExceptReport(const CCmpCtrl &cc, const char *message)
{
  const uint64_t args[]{reinterpret_cast<uintptr_t>(CurrentTask()),
                        reinterpret_cast<uintptr_t>(message)};
  if (message)
    PrintErrCpp("%s", 1, args + 1, nullptr);
  if (!IsRawCpp())
    AdamErrCpp("Task:%08X %s\n", 2, args);
  LexPutPos(cc);
}

extern "C" [[noreturn]] void UndefinedExternCpp(uint64_t caller)
{
  PrintErrCpp("Undefined Extern\nat %P\n", 1, &caller, nullptr);
  RaiseHolyC(0x7478456665646E55); // UndefExt
}
