#include "KernelB.hpp"

static void *LinkedLstNext(const void *node)
{
  void *next;
  MemCpy(&next, node, sizeof(next));
  return next;
}

extern "C" void LinkedLstDelCpp(void *node)
{
  while (node) {
    auto *next = LinkedLstNext(node);
    Free(node);
    node = next;
  }
}

extern "C" uint8_t *LinkedLstCopyCpp(const void *node,CTask *mem_task)
{
  uint8_t *res=nullptr,*tmp;
  void *slot=&res;
  while (node) {
    tmp=MAllocIdentCpp(node,mem_task);
    if (!tmp) {
      LinkedLstDelCpp(res);
      return nullptr;
    }
    void *next=nullptr;
    MemCpy(tmp,&next,sizeof(next));
    MemCpy(slot,&tmp,sizeof(tmp));
    slot=tmp;
    node=LinkedLstNext(node);
  }
  return res;
}

extern "C" int64_t LinkedLstCntCpp(const void *node)
{
  int64_t res=0;
  while (node) {
    res++;
    node=LinkedLstNext(node);
  }
  return res;
}

extern "C" uint64_t LinkedLstSizeCpp(const void *node)
{
  uint64_t size = 0;
  for (; node; node = LinkedLstNext(node))
    size += MSize2(node);
  return size;
}

template <typename T> static void QueDel(T *head, bool remove)
{
  for (auto *entry = head->next; entry != head;) {
    auto *next = entry->next;
    if (remove) {
      entry->last->next = next;
      next->last = entry->last;
    }
    Free(entry);
    entry = next;
  }
}

extern "C" void QueDelCpp(CQue *head, uint8_t remove)
{
  QueDel(head, remove != 0);
}

extern "C" CQue *QueCopyCpp(CQue *head,CTask *mem_task)
{
  auto *res=reinterpret_cast<CQue *>(MAllocIdentCpp(head,mem_task));
  if (!res)
    return nullptr;
  QueInit(res->next,res->last);
  CQue *tmpq=head->next,*tmpq1;
  while (tmpq!=head) {
    tmpq1=reinterpret_cast<CQue *>(MAllocIdentCpp(tmpq,mem_task));
    if (!tmpq1) {
      QueDel(res,false);
      Free(res);
      return nullptr;
    }
    QueIns(*tmpq1,*res->last);
    tmpq=tmpq->next;
  }
  return res;
}

extern "C" int64_t QueCntCpp(CQue *head)
{
  CQue *tmpq=head->next;
  int64_t res=0;
  while (tmpq!=head) {
    res++;
    tmpq=tmpq->next;
  }
  return res;
}

extern "C" int64_t QueSizeCpp(CQue *head)
{
  CQue *tmpq=head->next;
  uint64_t res=0;
  while (tmpq!=head) {
    res+=MSize2(tmpq);
    tmpq=tmpq->next;
  }
  return static_cast<int64_t>(res);
}

extern "C" CQueVectU8 *QueVectU8NewCpp(int64_t min_idx)
{
  auto *v = reinterpret_cast<CQueVectU8 *>(
      MAlloc(sizeof(CQueVectU8), MemTaskHeap(nullptr)));
  if (!v)
    return nullptr;
  QueInit(v->next, v->last);
  v->total_cnt = v->node_cnt = 0;
  v->min_idx = min_idx;
  return v;
}

static uint8_t &QueVectU8At(CQueVectU8 *v, int64_t idx)
{
  while (idx >= v->node_cnt) {
    idx -= v->node_cnt;
    v = v->next;
  }
  return v->body[idx];
}

extern "C" void QueVectU8PutCpp(CQueVectU8 *v, int64_t idx, uint8_t ch)
{
  idx = static_cast<int64_t>(uint64_t(idx) - uint64_t(v->min_idx));
  if (idx < 0)
    return;
  if (idx < v->total_cnt) {
    QueVectU8At(v, idx) = ch;
    return;
  }
  auto *tail = v->last;
  uint64_t remaining = uint64_t(idx - v->total_cnt) + 1;
  while (remaining) {
    if (tail->node_cnt == QUE_VECT_U8_CNT) {
      auto *next = reinterpret_cast<CQueVectU8 *>(
          MAlloc(sizeof(CQueVectU8), MemTaskHeap(nullptr)));
      if (!next)
        return;
      next->next = v;
      next->last = tail;
      tail->next = next;
      v->last = tail = next;
      next->node_cnt = 0;
    }
    size_t count = QUE_VECT_U8_CNT - tail->node_cnt;
    if (remaining < count)
      count = remaining;
    MemSet(tail->body + tail->node_cnt, 0, count);
    tail->node_cnt += count;
    v->total_cnt += count;
    remaining -= count;
  }
  tail->body[tail->node_cnt - 1] = ch;
}

extern "C" void QueVectU8DelCpp(CQueVectU8 *v)
{
  if (v) {
    QueDel(v, false);
    Free(v);
  }
}

extern "C" int64_t QueVectU8GetCpp(CQueVectU8 *v, int64_t idx)
{
  idx = static_cast<int64_t>(uint64_t(idx) - uint64_t(v->min_idx));
  return 0 <= idx && idx < v->total_cnt ? QueVectU8At(v, idx) : 0;
}

template <typename T> static CFifo<T> *FifoNew(int64_t size, CTask *mem_task)
{
  if (size <= 0 || uint64_t(size) > SIZE_MAX / sizeof(T))
    return nullptr;
  auto *heap = MemTaskHeap(mem_task);
  auto *f = reinterpret_cast<CFifo<T> *>(MAlloc(sizeof(CFifo<T>), heap));
  if (!f)
    return nullptr;
  f->buf = reinterpret_cast<T *>(MAlloc(size * sizeof(T), heap));
  if (!f->buf) {
    Free(f);
    return nullptr;
  }
  f->mask = size - 1;
  f->in_ptr = f->out_ptr = 0;
  return f;
}

CFifoU8 *FifoU8New(int64_t size, CTask *mem_task)
{
  return FifoNew<uint8_t>(size, mem_task);
}

void FifoU8Del(CFifoU8 *f)
{
  if (f) {
    Free(f->buf);
    Free(f);
  }
}

CFifoI64 *FifoI64New(int64_t size, CTask *mem_task)
{
  return FifoNew<int64_t>(size, mem_task);
}

void FifoI64Del(CFifoI64 *f)
{
  if (f) {
    Free(f->buf);
    Free(f);
  }
}

extern "C" CFifoU8 *FifoU8NewCpp(int64_t size, CTask *mem_task)
{
  return FifoU8New(size, mem_task);
}

extern "C" void FifoU8DelCpp(CFifoU8 *f)
{
  FifoU8Del(f);
}

extern "C" bool FifoU8InsCpp(CFifoU8 *f, uint8_t value)
{
  return FifoIns(*f, value);
}

extern "C" bool FifoU8RemCpp(CFifoU8 *f, uint8_t *value)
{
  return FifoRem(*f, value);
}

extern "C" bool FifoU8PeekCpp(CFifoU8 *f, uint8_t *value)
{
  return FifoPeek(*f, value);
}

extern "C" void FifoU8FlushCpp(CFifoU8 *f)
{
  FifoFlush(*f);
}

extern "C" int64_t FifoU8CntCpp(CFifoU8 *f)
{
  return FifoCnt(*f);
}

extern "C" CFifoI64 *FifoI64NewCpp(int64_t size, CTask *mem_task)
{
  return FifoI64New(size, mem_task);
}

extern "C" void FifoI64DelCpp(CFifoI64 *f)
{
  FifoI64Del(f);
}

extern "C" bool FifoI64InsCpp(CFifoI64 *f, int64_t value)
{
  return FifoIns(*f, value);
}

extern "C" bool FifoI64RemCpp(CFifoI64 *f, int64_t *value)
{
  return FifoRem(*f, value);
}

extern "C" bool FifoI64PeekCpp(CFifoI64 *f, int64_t *value)
{
  return FifoPeek(*f, value);
}

extern "C" void FifoI64FlushCpp(CFifoI64 *f)
{
  FifoFlush(*f);
}

extern "C" int64_t FifoI64CntCpp(CFifoI64 *f)
{
  return FifoCnt(*f);
}
