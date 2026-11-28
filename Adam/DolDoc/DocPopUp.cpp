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

extern "C" int64_t PopUp2Cpp(const char *b1, int64_t n1, const char *b2,
                            int64_t n2, const char *header, const char *footer)
{
  auto *doc = DocNewCpp(nullptr, nullptr);
  if (!doc)
    return DOCM_CANCEL;
  int64_t l1 = StrLen(b1), l2 = StrLen(b2), y;
  if (header) {
    DocPutS(*doc, header, StrLen(header));
    y = 4;
  } else {
    const uint64_t args[]{uint64_t(l1 + l2 + 10), reinterpret_cast<uintptr_t>("")};
    DocPrintCpp(doc, "%*s\n", 2, args);
    y = 3;
  }
  const int64_t x = -(l1 + l2 + 3) >> 1;
  const uint64_t args1[]{uint64_t(x), uint64_t(y),
                         reinterpret_cast<uintptr_t>(b1), uint64_t(n1)};
  const uint64_t args2[]{uint64_t(x + l1 + 6),
                         reinterpret_cast<uintptr_t>(b2), uint64_t(n2)};
  DocPrintCpp(doc, "$CM+CX,%d,%d$$BT,\"%s\",LE=%d$", 4, args1);
  DocPrintCpp(doc, "$CM+CX,%d,0$$BT,\"%s\",LE=%d$\n", 3, args2);
  if (footer)
    DocPutS(*doc, footer, StrLen(footer));
  const int64_t res = PopUpMenuCpp(doc, 0);
  DocDelCpp(doc);
  return res;
}

extern "C" bool PopUpOkCpp(const char *header, const char *footer)
{
  return PopUp1Cpp("OKAY", 1, header, footer) > 0;
}

extern "C" bool PopUpCancelOkCpp(const char *header, const char *footer)
{
  return PopUp2Cpp("OKAY", 1, "CANCEL", 0, header, footer) > 0;
}
