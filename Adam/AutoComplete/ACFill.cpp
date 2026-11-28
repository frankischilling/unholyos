#include "../DolDoc/DolDoc.hpp"
#include "AutoComplete.hpp"

extern "C" void InCpp(const char *fmt, int64_t argc, const uint64_t *argv);

void ACFillIn(int64_t n)
{
  if (n <= 0 || n > ac.num_fillins)
    return;
  const auto *word = ac.fillin_matches[n - 1]->hash.str;
  if (StrLen(word) > size_t(ac.partial_len)) {
    const uint64_t args[]{reinterpret_cast<uintptr_t>(word + ac.partial_len)};
    InCpp("%s", 1, args);
  }
}

void ACMan(int64_t n, CTask *parent)
{
  if (n <= 0 || n > ac.num_fillins)
    return;
  auto *match = ac.fillin_matches[n - 1];
  auto *sym =
      match ? reinterpret_cast<CHashSrcSym *>(HashFind(
                  match->hash.str, CurrentTask()->hash_table, HTG_SRC_SYM))
            : nullptr;
  if (sym && sym->src_link)
    PopUpEdCpp(sym->src_link, parent, nullptr);
}
