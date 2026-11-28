#include "DolDoc.hpp"

extern "C" int64_t PopUp1Cpp(const char *b1, int64_t n1, const char *header,
                             const char *footer)
{
  auto *doc = DocNewCpp(nullptr, nullptr);
  if (!doc)
    return DOCM_CANCEL;
  if (header)
    DocPutS(*doc, header, StrLen(header));
  const int64_t l1 = StrLen(b1);
  const uint64_t args[]{uint64_t(-l1 / 2), reinterpret_cast<uintptr_t>(b1),
                         uint64_t(n1)};
  DocPrintCpp(doc, "$CM+CX,%d,4$$BT,\"%s\",LE=%d$\n", 3, args);
  if (footer)
    DocPutS(*doc, footer, StrLen(footer));
  const int64_t i = PopUpMenuCpp(doc, 0);
  DocDelCpp(doc);
  return i;
}

extern "C" bool PopUpOkCpp(const char *header, const char *footer)
{
  return PopUp1Cpp("OKAY", 1, header, footer) > 0;
}
