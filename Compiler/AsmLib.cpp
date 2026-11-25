#include "../Kernel/KernelB.hpp"
#include "CompilerA.hpp"

extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);

void AsmListCode(CAsmListing &listing, uint64_t value, unsigned width)
{
  if (listing.lst_col >= 24) {
    PutS("\n");
    listing.lst_col = 0;
  }
  const char *fmt = "%02X ";
  if (width == 4) {
    value = static_cast<uint32_t>(value);
    fmt = "%08X ";
  } else if (width == 8)
    fmt = "%016X ";
  PrintCpp(fmt, 1, &value);
  listing.lst_col += 2 * width + 1;
}

static void AsmListLinePrefix(CAsmListing &listing)
{
  constexpr char spaces[] = "                        ";
  if (listing.lst_col < sizeof(spaces) - 1)
    PutS(spaces + listing.lst_col);
  const uint64_t rip = listing.lst_last_rip;
  PrintCpp("%08X ", 1, &rip);
  listing.lst_col = 0;
}

void AsmLineLst(CCmpCtrl &cc, CAsmListing &listing, size_t rip)
{
  const auto &file = *cc.lex_include_stk;
  auto *doc = file.flags & LFSF_DOC ? file.doc : nullptr;
  const void *source = doc ? static_cast<const void *>(doc) : file.buf;
  if (listing.lst && listing.lst_last_lfn == &file &&
      listing.lst_last_source == source && source && file.line_start) {
    // Keep line positions because Lex can pop and free an include.
    if (doc) {
      auto *line = reinterpret_cast<const CDocEntryBase *>(file.line_start);
      if (line != &doc->head) {
        if (listing.lst_last_line < static_cast<size_t>(line->y)) {
          auto *entry = line;
          while (entry->last != &doc->head &&
                 entry->last->y >= static_cast<int64_t>(listing.lst_last_line))
            entry = entry->last;
          while (entry && entry != &doc->head && entry->y < line->y) {
            AsmListLinePrefix(listing);
            auto *next =
                LexPutLine(file, reinterpret_cast<const char *>(entry));
            entry = reinterpret_cast<const CDocEntryBase *>(next);
          }
        }
        listing.lst_last_line = line->y;
      }
    } else {
      size_t offset = file.line_start - file.buf;
      if (listing.lst_last_line < offset) {
        auto *line = file.buf + listing.lst_last_line;
        while (line < file.line_start && *line) {
          AsmListLinePrefix(listing);
          line = LexPutLine(file, line);
        }
      }
      listing.lst_last_line = offset;
    }
  } else {
    listing.lst_last_lfn = &file;
    listing.lst_last_source = source;
    listing.lst_last_line = SIZE_MAX;
  }
  listing.lst_last_rip = rip;
}
