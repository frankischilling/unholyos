#include "../Adam/DolDoc/DolDoc.hpp"

void PutTaskS(CTask &task, const char *text)
{
  if (!text || (task.display_flags & DISPLAYF_SILENT))
    return;
  if (task.display_flags & DISPLAYF_NOT_RAW) {
    auto *target = task.task_flags & (1 << 5) ? task.parent_task : &task;
    auto *doc = target ? target->put_doc : nullptr;
    if (doc && doc->doc_signature == DOC_SIGNATURE_VAL) {
      DocPutS(*doc, text, StrLen(text));
      return;
    }
  }
  SerialPutS(text);
}

extern "C" void PutS(const char *text)
{
  PutTaskS(*CurrentTask(), text);
}

extern "C" void PutCharsCpp(uint64_t chars)
{
  char text[9];
  char *dst = text;
  for (; chars; chars >>= 8)
    if (static_cast<uint8_t>(chars))
      *dst++ = static_cast<char>(chars);
  *dst = 0;
  PutS(text);
}
