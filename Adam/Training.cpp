#include "DolDoc/DolDoc.hpp"

static bool TipStart(const CDocEntryBase &entry)
{
  return entry.Type() == DOCT_TEXT && entry.tag && *entry.tag == '*';
}

extern "C" void TipOfDayCpp(const char *tip_file)
{
  const auto random = RandU16Cpp();
  auto *doc = DocReadCpp(tip_file, 0);
  PutS("$WW,1$\n");
  if (!doc)
    return;

  auto *head = &doc->head;
  unsigned count = 0;
  for (auto *entry = head->next; entry != head; entry = entry->next)
    count += TipStart(*entry);

  auto *out = DocPut();
  auto *tip = count && out ? DocNewCpp(nullptr, nullptr) : nullptr;
  if (tip) {
    auto *entry = head->next;
    auto index = random % count;
    while (!TipStart(*entry) || index--)
      entry = entry->next;
    bool copied = true;
    do {
      if (entry->Type() != DOCT_ERROR) {
        auto *copy = DocEntryCopy(*tip, *entry);
        if (!copy) {
          copied = false;
          break;
        }
        DocInsEntry(*tip, *copy);
      }
      entry = entry->next;
    } while (entry != head && !TipStart(*entry));
    if (copied)
      DocInsDoc(*out, *tip);
    DocDelCpp(tip);
  }
  DocDelCpp(doc);
}
