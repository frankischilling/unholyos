#include "../DolDoc/DolDoc.hpp"
#include "AutoComplete.hpp"

extern "C" void InCpp(const char *fmt, int64_t argc, const uint64_t *argv);
extern "C" int64_t PopUpCpp(const char *src, CTask *parent, CTask **popup);

static uint8_t *ACDFieldEnd(uint8_t *field, const uint8_t *end)
{
  while (field < end && *field)
    field++;
  return field;
}

static uint8_t *ACDRecord(const char *word, size_t *record_size)
{
  if (!word || !*word || !ac.hash_table)
    return nullptr;
  auto *upper = MStrUtilCpp(word, SUF_TO_UPPER, 0);
  auto *match = upper ? reinterpret_cast<CHashGeneric *>(
                            HashFind(upper, ac.hash_table, HTT_DICT_WORD))
                      : nullptr;
  Free(upper);
  if (!match)
    return nullptr;
  const int64_t start = match->user_data0 * ACD_BLK_SIZE;
  auto *file = FOpenCpp(ACD_DEF_FILENAME, "r", 0);
  if (!file)
    return nullptr;
  const auto size = FSizeCpp(file);
  auto *heap = CurrentTask()->data_heap;
  uint8_t *buffer = nullptr;
  int64_t bytes = 0;
  if (start < size) {
    bytes = size - start;
    if (bytes > ACD_BLK_SIZE * 2)
      bytes = ACD_BLK_SIZE * 2;
    const auto blocks = (bytes + BLK_SIZE - 1) / BLK_SIZE;
    buffer = MAlloc(blocks * BLK_SIZE, heap);
    if (buffer && !FBlkReadCpp(file, buffer, start / BLK_SIZE, blocks)) {
      Free(buffer);
      buffer = nullptr;
    }
  }
  FCloseCpp(file);
  if (!buffer)
    return nullptr;

  auto *end = buffer + bytes;
  uint8_t *result = nullptr;
  for (auto *record = buffer; record < end; record++) {
    if (*record != ACD_WORD_CHAR)
      continue;
    auto *field = ACDFieldEnd(record + 1, end);
    if (field == end)
      break;
    if (!StrICmpCpp(word, reinterpret_cast<char *>(record + 1))) {
      auto *next = field + 1;
      while (next < end && *next != ACD_WORD_CHAR)
        next++;
      const auto length = next - record;
      result = CAlloc(length + 2, heap);
      if (result) {
        MemCpy(result, record, length);
        if (record_size)
          *record_size = length;
      }
      break;
    }
    record = field;
  }
  Free(buffer);
  return result;
}

extern "C" uint8_t *ACDDefsGetCpp(const char *word)
{
  return ACDRecord(word, nullptr);
}

extern "C" char *ACDDefGetCpp(const char *word, int64_t def_num)
{
  if (def_num <= 0)
    return nullptr;
  size_t size;
  auto *defs = ACDRecord(word, &size);
  char *result = nullptr;
  if (defs)
    for (auto *field = defs + 1; field < defs + size; field++)
      if (*field == ACD_DEF_CHAR && !--def_num) {
        result = StrNew(reinterpret_cast<char *>(field + 1),
                        CurrentTask()->data_heap);
        break;
      }
  Free(defs);
  return result;
}

extern "C" void ACDDefsPutCpp(CDoc *doc, const char *word, int64_t num)
{
  if (!word)
    return;
  if (uint8_t(*word) == ACD_WORD_CHAR)
    word++;
  const uint64_t title[]{reinterpret_cast<uintptr_t>(word)};
  DocPrintCpp(doc, "$WW,1$$RED$%s:$FG$\n\n", 1, title);
  if (num >= 0) {
    if (auto *def = ACDDefGetCpp(word, num)) {
      const uint64_t args[]{uint64_t(num), reinterpret_cast<uintptr_t>(def)};
      DocPrintCpp(doc, "$GREEN$(%d)$FG$ %s\n", 2, args);
      Free(def);
    }
    return;
  }
  auto *defs = ACDDefsGetCpp(word);
  int64_t i = 0;
  if (defs)
    for (auto *field = defs; *field;
         field += StrLen(reinterpret_cast<char *>(field + 1)) + 2) {
      const uint64_t text[]{reinterpret_cast<uintptr_t>(field + 1)};
      switch (*field) {
      case ACD_DEF_CHAR: {
        const uint64_t args[]{uint64_t(++i), text[0]};
        DocPrintCpp(doc, "$GREEN$(%d)$FG$ %s\n", 2, args);
        break;
      }
      case ACD_PRONUNCIATION_CHAR:
        DocPrintCpp(doc, "$LTGREEN$%s$FG$\n", 1, text);
        break;
      case ACD_POS_CHAR:
        DocPrintCpp(doc, "$BLACK$%s$FG$\n", 1, text);
        break;
      case ACD_EXTRA_CHAR:
        DocPrintCpp(doc, "$LTBLUE$%s$FG$\n", 1, text);
        break;
      }
    }
  Free(defs);
}

void ACDFillin(int64_t n)
{
  if (n < 0 || n >= acd.num_fillins)
    return;
  auto *word = reinterpret_cast<char *>(acd.fillins[n] + 1);
  if (StrLen(word) > size_t(ac.partial_len)) {
    const uint64_t args[]{reinterpret_cast<uintptr_t>(word + ac.partial_len)};
    InCpp("%s", 1, args);
  }
}

extern "C" void ACDPopUpDefCpp(const char *word, int64_t num, CTask *parent)
{
  if (!word)
    return;
  if (uint8_t(*word) == ACD_WORD_CHAR)
    word++;
  const uint64_t args[]{reinterpret_cast<uintptr_t>(word), uint64_t(num)};
  auto *src = MStrPrintCpp("ACDDefsPut(DocPut,\"%Q\",%d);View;", 2, args);
  if (src) {
    PopUpCpp(src, parent, nullptr);
    Free(src);
  }
}

void ACDDef(int64_t n, CTask *parent)
{
  if (0 <= n && n < acd.num_fillins)
    ACDPopUpDefCpp(reinterpret_cast<char *>(acd.fillins[n]), -1, parent);
}

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
