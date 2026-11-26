#include "DolDoc.hpp"

static CDoc *DocValidate(CDoc *doc)
{
  return doc && doc->doc_signature == DOC_SIGNATURE_VAL ? doc : nullptr;
}

extern "C" CDoc *DocPutCpp(CTask *task)
{
  if (!task)
    task = CurrentTask();
  if (task->task_flags & TASKF_INPUT_FILTER_TASK)
    task = task->parent_task;
  return task ? DocValidate(task->put_doc) : nullptr;
}

CDoc *DocPut()
{
  return DocPutCpp(nullptr);
}

extern "C" CDoc *DocDisplayCpp(CTask *task)
{
  if (!task)
    task = CurrentTask();
  return DocValidate(task->display_doc);
}

extern "C" CDoc *DocBorderCpp(CTask *task)
{
  if (!task)
    task = CurrentTask();
  return DocValidate(task->border_doc);
}
