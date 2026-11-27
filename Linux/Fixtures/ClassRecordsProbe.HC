// Class construction from Terry A. Davis's Compiler/PrsLib.HC, PrsStmt.HC
// and LexLib.HC, TempleOS c26482bb6ad3f80106d28504ec5db3c6a360732c.
_extern _MHEAP_CTRL CHeapCtrl *ClassRecordsOwner(U8 *addr);
_extern _HASH_REM_DEL Bool ClassRecordsRemove(CHash *entry,CHashTable *table,
        I64 instance=1);

asm {
_CLASS_RECORDS_RSP::
  LEA RAX,8[RSP]
  RET
}
_extern _CLASS_RECORDS_RSP U64 ClassRecordsRSP();

#assert sizeof(CHashClass)==128
#assert offset(CHashClass.member_lst_and_root)==88
#assert offset(CHashClass.last_in_member_lst)==104

I64 ClassRecordsWidths=0,ClassRecordsValue=0;
U8 ClassRecordsStorage[29];
U8 *ClassRecordsObject=NULL;

U64 ClassRecordsPointers(CHashClass *base)
{
  CHashClass expected;
  U64 checks=0;
  I64 i;
  if (!base || MSize(base)!=640)
    return 0;
  for (i=1;i<=4;i++) {
    MemSet(&expected,0,sizeof(expected));
    expected.type=16;
    expected.size=8;
    expected.ptr_stars_cnt=i;
    expected.raw_type=10;
    if (!MemCmp(base+i,&expected,sizeof(expected)))
      checks|=1<<(i-1);
  }
  return checks;
}

U0 ClassRecordsProbe()
{
  U8 *name="CClassRecordsLifecycleProbe";
  CHashClass *base=NULL,*completed;
  CMemberLst *middle=NULL,*zulu=NULL,*alpha=NULL;
  CExcept *first=Fs->next_except,*last=Fs->last_except;
  U64 forward=0,pointers=0,complete=0,cleanup=0,heap=0,before,after;
  I64 code_used,data_used,members=0,i;
  Bool heaps=Fs->code_heap!=Fs->data_heap,guards=TRUE,removed=FALSE;
  ClassRecordsObject=ClassRecordsStorage+8;
  MemSet(ClassRecordsStorage,0xA5,sizeof(ClassRecordsStorage));
  before=ClassRecordsRSP;
  code_used=Fs->code_heap->used_u8s;
  data_used=Fs->data_heap->used_u8s;
  if (heaps && !HashFind(name,Fs->hash_table,16)) {
    ExePutS("extern class CClassRecordsLifecycleProbe;");
    base=HashFind(name,Fs->hash_table,16);
    if (base) {
      if ((base->type&0xFFFF)==16 && !StrCmp(base->str,name) &&
          !HashFind(name,Fs->hash_table,16,2))
        forward|=1;
      if (MSize(base)==640)
        forward|=2;
      if (ClassRecordsOwner(base)==Fs->code_heap)
        forward|=4;
      if (!base->size && base->raw_type==10 && !base->ptr_stars_cnt &&
          base->flags==1)
        forward|=8;
      if (!base->member_cnt && !base->member_lst_and_root &&
          !base->member_class_base_root && !base->base_class &&
          !base->fwd_class && !base->neg_offset)
        forward|=16;
      if (base->last_in_member_lst==&base->member_lst_and_root)
        forward|=32;
      pointers=ClassRecordsPointers(base);
      if (pointers==15)
        forward|=64;
      ExePutS("ClassRecordsWidths=sizeof(CClassRecordsLifecycleProbe *)+"
          "sizeof(CClassRecordsLifecycleProbe **)+"
          "sizeof(CClassRecordsLifecycleProbe ***)+"
          "sizeof(CClassRecordsLifecycleProbe ****);");
      if (ClassRecordsWidths==32 && HashFind(name,Fs->hash_table,16)==base &&
          base->flags==1)
        forward|=128;

      ExePutS("class CClassRecordsLifecycleProbe {"
          "U8 middle; I64 zulu; U16 alpha[2];};");
      completed=HashFind(name,Fs->hash_table,16);
      if (completed==base) {
        if (!HashFind(name,Fs->hash_table,16,2) &&
            (base->type&0xFFFF)==16 && !StrCmp(base->str,name))
          complete|=1;
        if (MSize(base)==640 && ClassRecordsOwner(base)==Fs->code_heap)
          complete|=2;
        if (!base->flags && base->size==13 && base->raw_type==10 &&
            !base->ptr_stars_cnt && !base->neg_offset &&
            !base->base_class && !base->fwd_class)
          complete|=4;
        middle=MemberFind("middle",base);
        zulu=MemberFind("zulu",base);
        alpha=MemberFind("alpha",base);
        if (base->member_cnt==3 && middle && zulu && alpha &&
            middle!=zulu && middle!=alpha && zulu!=alpha &&
            !MemberFind("absent",base))
          complete|=8;
        if (middle && zulu && alpha) {
          if (base->member_lst_and_root==middle && middle->next==zulu &&
              zulu->next==alpha && !alpha->next &&
              base->last_in_member_lst==alpha && middle->left==zulu &&
              middle->right==alpha)
            complete|=16;
          if (middle->offset==0 && middle->size==1 &&
              middle->dim.total_cnt==1 && !middle->dim.next)
            members++;
          if (zulu->offset==1 && zulu->size==8 &&
              zulu->dim.total_cnt==1 && !zulu->dim.next)
            members++;
          if (alpha->offset==9 && alpha->size==4 &&
              alpha->dim.total_cnt==2 && alpha->dim.next &&
              alpha->dim.next->cnt==2)
            members++;
          if (members==3)
            complete|=32;
          if (ClassRecordsOwner(middle)==Fs->data_heap &&
              ClassRecordsOwner(zulu)==Fs->data_heap &&
              ClassRecordsOwner(alpha)==Fs->data_heap)
            complete|=64;
        }
        ExePutS("ClassRecordsObject(CClassRecordsLifecycleProbe *)->middle=0x12;"
            "ClassRecordsObject(CClassRecordsLifecycleProbe *)->zulu=-42;"
            "ClassRecordsObject(CClassRecordsLifecycleProbe *)->alpha[0]=0x3456;"
            "ClassRecordsObject(CClassRecordsLifecycleProbe *)->alpha[1]=0x789A;"
            "ClassRecordsValue=sizeof(CClassRecordsLifecycleProbe)==13 && "
            "ClassRecordsObject(CClassRecordsLifecycleProbe *)->middle==0x12 && "
            "ClassRecordsObject(CClassRecordsLifecycleProbe *)->zulu==-42 && "
            "ClassRecordsObject(CClassRecordsLifecycleProbe *)->alpha[0]==0x3456 && "
            "ClassRecordsObject(CClassRecordsLifecycleProbe *)->alpha[1]==0x789A;");
        for (i=0;i<8;i++)
          if (ClassRecordsStorage[i]!=0xA5 || ClassRecordsStorage[21+i]!=0xA5)
            guards=FALSE;
        if (ClassRecordsValue==1 && guards && ClassRecordsObject[0]==0x12 &&
            *(ClassRecordsObject+1)(I64 *)==-42 &&
            *(ClassRecordsObject+9)(U16 *)==0x3456 &&
            *(ClassRecordsObject+11)(U16 *)==0x789A)
          complete|=128;
      }
      if (HashFind(name,Fs->hash_table,16)==base)
        removed=ClassRecordsRemove(base,Fs->hash_table);
    }
  }
  if (removed)
    cleanup|=1;
  if (removed && !HashFind(name,Fs->hash_table,16) &&
      !HashFind(name,Fs->hash_table,16,2))
    cleanup|=2;
  after=ClassRecordsRSP;
  if (before==after && Fs->next_except==first && Fs->last_except==last)
    cleanup|=4;
  if (Fs->code_heap->used_u8s==code_used)
    heap|=1;
  if (Fs->data_heap->used_u8s==data_used)
    heap|=2;
  DocClear(,TRUE);
  "CR heaps %d\n",heaps;
  "CR forward %X pointers %X\n",forward,pointers;
  "CR complete %X members %d\n",complete,members;
  "CR cleanup %X\n",cleanup;
  "CR heap %X\n",heap;
  "CR DONE\n";
}
ClassRecordsProbe;
