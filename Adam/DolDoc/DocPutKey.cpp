#include "DolDoc.hpp"

extern "C" void DocPutKeyCpp(CDoc *doc, int64_t ch, int64_t sc)
{
  if (!doc)
    doc = DocPut();
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL)
    return;
  if (doc->user_put_key &&
      CallHolyC4(doc->user_put_key, reinterpret_cast<uintptr_t>(doc),
                 reinterpret_cast<uintptr_t>(doc->user_put_data), ch, sc))
    return;
  if (!ch)
    return;
  CDocLock lock(*doc);
  if (EdCharIns(*doc, ch, sc))
    return;
  const char text[]{static_cast<char>(ch), 0};
  DocPutS(*doc, text, StrLen(text));
}
