#include "DolDoc.hpp"

static CDoc *DocBorderNew(CTask *task)
{
  auto *doc = DocNewCpp(nullptr, task);
  if (doc)
    doc->flags |= DOCF_BORDER_DOC;
  return doc;
}

extern "C" void DocTermNewCpp()
{
  auto *task = CurrentTask();
  auto *pdoc = DocNewCpp(nullptr, task);
  if (!pdoc)
    return;
  auto *bdoc = DocBorderNew(task);
  if (!bdoc) {
    DocDelCpp(pdoc);
    return;
  }
  pdoc->max_entries = 4096;
  pdoc->desc = 0x6D726554; // Term
  task->border_src = 1;    // BDS_CUR_DRV
  task->put_doc = task->display_doc = pdoc;
  task->border_doc = bdoc;
  RawCpp(false);
}
