// List and queue routines from Terry A. Davis's Kernel/KDataTypes.HC,
// TempleOS c26482bb6ad3f80106d28504ec5db3c6a360732c.
_extern _MHEAP_CTRL CHeapCtrl *ListUtilitiesOwner(U8 *addr);

class CListUtilitiesNode
{
  CListUtilitiesNode *next;
  I64 value;
};

class CListUtilitiesQueue:CQue
{
  I64 value;
};

I64 ListUtilitiesLinked()
{
  CListUtilitiesNode *a,*b,*copy,*other;
  I64 checks=0,before=Fs->data_heap->used_u8s;
  a=MAlloc(sizeof(CListUtilitiesNode));
  b=MAlloc(sizeof(CListUtilitiesNode)+17);
  MemSet(a,0xA5,MSize(a));
  MemSet(b,0x5A,MSize(b));
  a->next=b;
  b->next=NULL;
  a->value=17;
  b->value=-29;
  if (!LinkedLstCopy(NULL) && !LinkedLstCnt(NULL) && !LinkedLstSize(NULL))
    checks|=1;
  if (LinkedLstCnt(a)==2 && LinkedLstSize(a)==MSize2(a)+MSize2(b))
    checks|=2;
  copy=LinkedLstCopy(a);
  other=LinkedLstCopy(a,Fs);
  if (copy && other && copy!=a && other!=a && copy!=other &&
      copy->next!=b && other->next!=b && copy->next!=other->next &&
      !copy->next->next && !other->next->next && a->next==b)
    checks|=4;
  if (copy && other && LinkedLstCnt(copy)==2 &&
      LinkedLstSize(copy)==LinkedLstSize(a) &&
      MSize(copy)==MSize(a) && MSize(copy->next)==MSize(b) &&
      !MemCmp(&copy->value,&a->value,MSize(a)-8) &&
      !MemCmp(copy->next,b,MSize(b)) &&
      !MemCmp(other->next,b,MSize(b)))
    checks|=8;
  if (copy && other && ListUtilitiesOwner(copy)==Fs->data_heap &&
      ListUtilitiesOwner(copy->next)==Fs->data_heap &&
      ListUtilitiesOwner(other)==Fs->data_heap &&
      ListUtilitiesOwner(other->next)==Fs->data_heap)
    checks|=16;
  if (copy && other) {
    copy->next->value=9;
    if (b->value==-29 && other->next->value==-29)
      checks|=32;
  }
  LinkedLstDel(copy);
  LinkedLstDel(other);
  LinkedLstDel(a);
  LinkedLstDel(NULL);
  if (Fs->data_heap->used_u8s==before)
    checks|=64;
  return checks;
}

I64 ListUtilitiesQueue()
{
  CListUtilitiesQueue *head,*a,*b,*copy,*empty;
  I64 checks=0,before=Fs->data_heap->used_u8s;
  head=MAlloc(sizeof(CListUtilitiesQueue)+9);
  a=MAlloc(sizeof(CListUtilitiesQueue));
  b=MAlloc(sizeof(CListUtilitiesQueue)+17);
  MemSet(head,0x11,MSize(head));
  MemSet(a,0x22,MSize(a));
  MemSet(b,0x33,MSize(b));
  QueInit(head);
  empty=QueCopy(head);
  if (empty && empty!=head && empty->next==empty && empty->last==empty &&
      !QueCnt(empty) && !QueSize(empty) &&
      !MemCmp(&empty->value,&head->value,MSize(head)-16))
    checks|=1;
  if (empty)
    Free(empty);
  QueIns(a,head);
  QueIns(b,a);
  if (QueCnt(head)==2 && QueSize(head)==MSize2(a)+MSize2(b))
    checks|=2;
  copy=QueCopy(head,Fs);
  if (copy && copy!=head && copy->next!=a && copy->last!=b &&
      copy->next->last==copy && copy->last->next==copy &&
      copy->next->next==copy->last && copy->last->last==copy->next &&
      head->next==a && head->last==b)
    checks|=4;
  if (copy && QueCnt(copy)==2 && QueSize(copy)==QueSize(head) &&
      MSize(copy)==MSize(head) &&
      MSize(copy->next)==MSize(a) && MSize(copy->last)==MSize(b) &&
      !MemCmp(copy(U8 *)+16,head(U8 *)+16,MSize(head)-16) &&
      !MemCmp(copy->next(U8 *)+16,a(U8 *)+16,MSize(a)-16) &&
      !MemCmp(copy->last(U8 *)+16,b(U8 *)+16,MSize(b)-16))
    checks|=8;
  if (copy && ListUtilitiesOwner(copy)==Fs->data_heap &&
      ListUtilitiesOwner(copy->next)==Fs->data_heap &&
      ListUtilitiesOwner(copy->last)==Fs->data_heap)
    checks|=16;
  if (copy) {
    QueDel(copy,TRUE);
    if (copy->next==copy && copy->last==copy && !QueCnt(copy) &&
        !QueSize(copy) && QueCnt(head)==2)
      checks|=32;
    Free(copy);
  }
  QueDel(head);
  Free(head);
  if (Fs->data_heap->used_u8s==before)
    checks|=64;
  return checks;
}

"LU linked %X\n",ListUtilitiesLinked;
"LU queue %X\n",ListUtilitiesQueue;
"LU DONE\n";
