// Function records from Terry A. Davis's Compiler/PrsLib.HC, PrsStmt.HC,
// PrsVar.HC and Kernel/KHashB.HC, TempleOS c26482bb6ad3f80106d28504ec5db3c6a360732c.
#ifaot
_import _MHEAP_CTRL CHeapCtrl *FunctionRecordsOwner(U8 *addr);
_import _HASH_REM_DEL Bool FunctionRecordsRemove(CHash *entry,CHashTable *table,
        I64 instance=1);
#else
_extern _MHEAP_CTRL CHeapCtrl *FunctionRecordsOwner(U8 *addr);
_extern _HASH_REM_DEL Bool FunctionRecordsRemove(CHash *entry,CHashTable *table,
        I64 instance=1);
#endif

asm {
_FUNCTION_RECORDS_RSP::
  LEA RAX,8[RSP]
  RET
}
_extern _FUNCTION_RECORDS_RSP U64 FunctionRecordsRSP();

#assert sizeof(CHashFun)==168
#assert offset(CHashFun.member_lst_and_root)==88
#assert offset(CHashFun.last_in_member_lst)==104
#assert offset(CHashFun.return_class)==128
#assert offset(CHashFun.exe_addr)==152

public I64 FunctionRecordsEffects=0;

public I64 FunctionRecordsBody(I64 value=7)
{
  FunctionRecordsEffects++;
  return value+35;
}

public U0 FunctionRecordsTouch()
{
  FunctionRecordsEffects+=10;
}

U64 FunctionRecordsCompanions(CHashFun *base)
{
  CHashFun expected;
  U64 checks=0;
  I64 i;
  if (!base || MSize(base)!=840)
    return 0;
  for (i=1;i<=4;i++) {
    MemSet(&expected,0,sizeof(expected));
    expected.type=64;
    expected.size=8;
    expected.ptr_stars_cnt=i;
    expected.raw_type=10;
    if (!MemCmp(base+i,&expected,sizeof(expected)))
      checks|=1<<(i-1);
  }
  return checks;
}

Bool FunctionRecordsAllocation(CHashFun *base)
{
  return base && MSize(base)==840 && MSize2(base)==856 &&
      FunctionRecordsOwner(base)==Fs->code_heap &&
      (base->type&0x1FFFF)==64 && !base->ptr_stars_cnt &&
      base->raw_type==10 && !base->base_class && !base->fwd_class &&
      base->return_class &&
      base->return_class==HashFind("I64",Fs->hash_table,16);
}

Bool FunctionRecordsArguments(CHashFun *base,I64 defaults)
{
  CMemberLst *left,*right;
  CHashClass *i64=HashFind("I64",Fs->hash_table,16);
  if (!base || !i64 || base->arg_cnt!=2 || base->member_cnt!=2 || base->size)
    return FALSE;
  left=base->member_lst_and_root;
  if (!left)
    return FALSE;
  right=left->next;
  if (!right || right->next || base->last_in_member_lst!=right ||
      StrCmp(left->str,"left") || StrCmp(right->str,"right") ||
      left->offset!=16 || right->offset!=24 || left->size!=8 || right->size!=8 ||
      left->dim.total_cnt!=1 || right->dim.total_cnt!=1 ||
      left->dim.next || right->dim.next ||
      left->member_class!=i64 || right->member_class!=i64 ||
      FunctionRecordsOwner(left)!=Fs->data_heap ||
      FunctionRecordsOwner(right)!=Fs->data_heap)
    return FALSE;
  if (((left->flags&1)!=0)!=((defaults&1)!=0) ||
      ((right->flags&1)!=0)!=((defaults&2)!=0))
    return FALSE;
  if ((defaults&1) && left->dft_val!=4 ||
      (defaults&2) && right->dft_val!=2)
    return FALSE;
  return TRUE;
}

U0 FunctionRecordsProbe()
{
  U8 *names[4],*slots[4],*call,*code,*body=&FunctionRecordsBody,
      *touch=&FunctionRecordsTouch;
  CHashFun *fun[4],*signature,*jit_body,*jit_touch,*saved_last=Fs->last_fun;
  CHashClass *callbacks=NULL;
  CMemberLst *member;
  CHash *body_export,*touch_export;
  CBinFile *module;
  CExcept *first=Fs->next_except,*last=Fs->last_except;
  I64 (*empty)(),(*args)(I64 left,I64 right),
      (*dft)(I64 left=4,I64 right=2),(*invoke)(),(*compiled)(I64 value=7);
  U0 (*compiled_touch)();
  U64 named=0,pointers=0,arguments=0,callback=0,callback_pointers=0,
      behavior=0,stack=0,cleanup=0,heap=0,image=0,before,after,direct[4];
  I64 code_used,data_used,i,j,value0,value1,phase=0;
  Bool heaps=Fs->code_heap!=Fs->data_heap,unused=TRUE,unique;
  names[0]="FunctionRecordsNewEmpty";
  names[1]="FunctionRecordsNewArgs";
  names[2]="FunctionRecordsNewDefault";
  names[3]="FunctionRecordsNewCallbacks";
  slots[0]="one";
  slots[1]="two";
  slots[2]="three";
  slots[3]="four";
  MemSet(fun,0,sizeof(fun));
  MemSet(direct,0,sizeof(direct));
  for (i=0;i<4;i++)
    if (HashFind(names[i],Fs->hash_table,64))
      unused=FALSE;
  if (HashFind("CFunctionRecordsCallbacks",Fs->hash_table,16))
    unused=FALSE;
  code_used=Fs->code_heap->used_u8s;
  data_used=Fs->data_heap->used_u8s;
  if (heaps && unused) {
    ExePutS("I64 FunctionRecordsNewEmpty(){return 11;}"
        "I64 FunctionRecordsNewArgs(I64 left,I64 right){return left*10+right;}"
        "I64 FunctionRecordsNewDefault(I64 left=4,I64 right=2){"
        "return left*10+right;}"
        "class CFunctionRecordsCallbacks {"
        "I64 (*one)(I64 left,I64 right=2);"
        "I64 (**two)(I64 left,I64 right=2);"
        "I64 (***three)(I64 left,I64 right=2);"
        "I64 (****four)(I64 left,I64 right=2);};"
        "I64 FunctionRecordsNewCallbacks(){"
        "CFunctionRecordsCallbacks box;"
        "I64 (*p)(I64 left,I64 right=2);"
        "I64 (**pp)(I64 left,I64 right=2);"
        "I64 (***ppp)(I64 left,I64 right=2);"
        "p=&FunctionRecordsNewArgs;pp=&p;ppp=&pp;"
        "box.one=p;box.two=pp;box.three=ppp;box.four=&ppp;"
        "return box.one(4)+(*box.two)(4)+(**box.three)(4)+"
        "(***box.four)(4);}");
    for (i=0;i<4;i++) {
      fun[i]=HashFind(names[i],Fs->hash_table,64);
      if (FunctionRecordsAllocation(fun[i]) &&
          !(fun[i]->flags&1) && fun[i]->exe_addr &&
          FunctionRecordsOwner(fun[i]->exe_addr)==Fs->code_heap &&
          !StrCmp(fun[i]->str,names[i]) &&
          !HashFind(names[i],Fs->hash_table,64,2))
        named|=1<<i;
      pointers|=FunctionRecordsCompanions(fun[i])<<(i*4);
    }
    if (fun[0] && !fun[0]->size && !fun[0]->arg_cnt && !fun[0]->member_cnt &&
        !fun[0]->member_lst_and_root &&
        fun[0]->last_in_member_lst==&fun[0]->member_lst_and_root)
      arguments|=1;
    if (FunctionRecordsArguments(fun[1],0))
      arguments|=2;
    if (FunctionRecordsArguments(fun[2],3))
      arguments|=4;
    callbacks=HashFind("CFunctionRecordsCallbacks",Fs->hash_table,16);
    if (callbacks && callbacks->size==32 && callbacks->member_cnt==4)
      for (i=0;i<4;i++) {
        member=MemberFind(slots[i],callbacks);
        if (member && (member->flags&8) && member->size==8 &&
            member->offset==i*8 && member->dim.total_cnt==1 &&
            member->fun_ptr && member->fun_ptr->ptr_stars_cnt==i+1) {
          signature=member->fun_ptr-(i+1);
          if (FunctionRecordsAllocation(signature) && !signature->str &&
              FunctionRecordsArguments(signature,2))
            callback|=1<<i;
          callback_pointers|=FunctionRecordsCompanions(signature)<<(i*4);
        }
      }
    if (named==15) {
      before=FunctionRecordsRSP;
      call=MStrPrint("*0x%X(U64 *)=FunctionRecordsNewEmpty();"
          "*0x%X(U64 *)=FunctionRecordsNewArgs(4,2);"
          "*0x%X(U64 *)=FunctionRecordsNewDefault();"
          "*0x%X(U64 *)=FunctionRecordsNewDefault(8,1);",
          &direct[0],&direct[1],&direct[2],&direct[3]);
      ExePutS(call);
      Free(call);
      after=FunctionRecordsRSP;
      if (before==after)
        stack|=1;
      if (direct[0]==11) behavior|=1;
      if (direct[1]==42) behavior|=2;
      if (direct[2]==42) behavior|=4;
      if (direct[3]==81) behavior|=8;
      empty=fun[0]->exe_addr;
      args=fun[1]->exe_addr;
      dft=fun[2]->exe_addr;
      invoke=fun[3]->exe_addr;
      before=FunctionRecordsRSP;
      if (empty()==11) behavior|=16;
      if (args(4,2)==42) behavior|=32;
      if (dft()==42 && dft(6)==62) behavior|=64;
      after=FunctionRecordsRSP;
      if (before==after)
        stack|=2;
      before=FunctionRecordsRSP;
      if (invoke()==168)
        behavior|=128;
      after=FunctionRecordsRSP;
      if (before==after)
        stack|=4;
    }
    Fs->last_fun=saved_last;
    for (i=3;i>=0;i--)
      if (fun[i] && HashFind(names[i],Fs->hash_table,64)==fun[i]) {
        code=fun[i]->exe_addr;
        unique=TRUE;
        for (j=i+1;j<4;j++)
          if (fun[j] && direct[j]==code)
            unique=FALSE;
        direct[i]=code;
        if (code && unique && !(fun[i]->flags&1) &&
            FunctionRecordsOwner(code)==Fs->code_heap)
          Free(code);
        if (FunctionRecordsRemove(fun[i],Fs->hash_table) &&
            !HashFind(names[i],Fs->hash_table,64))
          cleanup|=1<<i;
      }
    if (callbacks && HashFind("CFunctionRecordsCallbacks",Fs->hash_table,16)==
        callbacks && FunctionRecordsRemove(callbacks,Fs->hash_table) &&
        !HashFind("CFunctionRecordsCallbacks",Fs->hash_table,16))
      cleanup|=16;
  }
  if (Fs->code_heap->used_u8s==code_used)
    heap|=1;
  if (Fs->data_heap->used_u8s==data_used)
    heap|=2;

  compiled=&FunctionRecordsBody;
  compiled_touch=&FunctionRecordsTouch;
  FunctionRecordsEffects=0;
  before=FunctionRecordsRSP;
  value0=FunctionRecordsBody();
  value1=compiled(5);
  FunctionRecordsTouch;
  compiled_touch();
  after=FunctionRecordsRSP;
  if (before==after && Fs->next_except==first && Fs->last_except==last &&
      Fs->last_fun==saved_last)
    stack|=8;
  jit_body=HashFind("FunctionRecordsBody",Fs->hash_table,64);
  jit_touch=HashFind("FunctionRecordsTouch",Fs->hash_table,64);
  body_export=HashFind("FunctionRecordsBody",Fs->hash_table,1);
  touch_export=HashFind("FunctionRecordsTouch",Fs->hash_table,1);
  module=HashVal(HashFind("/FunctionRecordsProbe",Fs->hash_table,0x4000));
  if (module)
    phase=1;
  if (HashVal(HashFind("FunctionRecordsBody",Fs->hash_table,65))==body &&
      HashVal(HashFind("FunctionRecordsTouch",Fs->hash_table,65))==touch)
    image|=1;
  if (jit_body && jit_touch) {
    if (!phase) {
      if (jit_body->exe_addr==body && jit_touch->exe_addr==touch)
        image|=2;
      if (!body_export && !touch_export)
        image|=8;
    } else {
      if (module->bin_signature==BIN_SIGNATURE_VAL &&
          module->patch_table_offset>sizeof(CBinFile) &&
          module->patch_table_offset<module->file_size &&
          body>=module(U8 *)+sizeof(CBinFile) &&
          body<module(U8 *)+module->patch_table_offset &&
          touch>=module(U8 *)+sizeof(CBinFile) &&
          touch<module(U8 *)+module->patch_table_offset)
        image|=2;
      if (body_export && touch_export && HashVal(body_export)==body &&
          HashVal(touch_export)==touch && jit_body->exe_addr!=body &&
          jit_touch->exe_addr!=touch)
        image|=8;
    }
  }
  if (value0==42 && value1==40 && FunctionRecordsEffects==22)
    image|=4;
  DocClear(,TRUE);
  "FR heaps %d\n",heaps;
  "FR named %X pointers %X\n",named,pointers;
  "FR args %X\n",arguments;
  "FR callbacks %X pointers %X\n",callback,callback_pointers;
  "FR behavior %X stack %X\n",behavior,stack;
  "FR cleanup %X heap %X\n",cleanup,heap;
  "FR image %X phase %d\n",image,phase;
  "FR DONE\n";
}
FunctionRecordsProbe;
