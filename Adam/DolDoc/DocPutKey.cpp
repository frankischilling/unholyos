#include "DolDoc.hpp"

extern "C" void DocPutKeyCpp(CDoc *doc, int64_t ch, int64_t sc)
{
  if (!doc)
    doc = DocPut();
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL || !ch)
    return;
  CDocLock lock(*doc);
  if (EdCharIns(*doc, ch, sc))
    return;
  const char text[]{static_cast<char>(ch), 0};
  DocPutS(*doc, text, StrLen(text));
}
