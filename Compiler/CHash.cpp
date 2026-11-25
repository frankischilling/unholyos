#include "../Adam/DolDoc/DolDoc.hpp"
#include "CompilerA.hpp"

extern "C" uint8_t *DocSaveCpp(CDoc *doc, size_t *size);
extern "C" uint64_t FileWriteCpp(const char *name, const uint8_t *data,
                                 uint64_t size, uint64_t date, uint64_t attr);

bool MapFileWrite(const CHashTable &table, const char *name, uint8_t drv_let)
{
  auto *path = ExtDftCpp(name, "MAP.Z");
  auto *doc = path ? DocNewCpp(path, nullptr) : nullptr;
  Free(path);
  if (!doc)
    return false;
  doc->flags |= DOCF_NO_CURSOR;
  bool okay = true;
  for (int64_t i = 0; okay && i <= table.mask; ++i) {
    for (auto *entry = table.body[i]; okay && entry; entry = entry->next) {
      if (!(entry->type & HTG_SRC_SYM) || (entry->type & HTF_PRIVATE))
        continue;
      const auto *sym = reinterpret_cast<const CHashSrcSym *>(entry);
      if (!sym->src_link)
        continue;
      auto *link = StrNew(sym->src_link, CurrentTask()->data_heap);
      if (!link) {
        okay = false;
        break;
      }
      if (drv_let && StrLen(link) >= 4)
        link[3] = drv_let;
      uint64_t args[]{reinterpret_cast<uintptr_t>(entry->str),
                      reinterpret_cast<uintptr_t>(link)};
      auto *line = MStrPrintCpp("$LK,\"%s\",A=\"%s\"$\n", 2, args);
      okay = line && DocPutS(*doc, line, StrLen(line));
      Free(line);
      Free(link);
    }
  }
  size_t size;
  auto *data = okay ? DocSaveCpp(doc, &size) : nullptr;
  okay = data && FileWriteCpp(doc->filename.name, data, size, 0, doc->file_attr);
  Free(data);
  DocDelCpp(doc);
  return okay;
}
