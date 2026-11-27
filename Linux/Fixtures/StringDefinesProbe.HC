// Checks Terry A. Davis's Kernel/KDefine.HC at TempleOS
// c26482bb6ad3f80106d28504ec5db3c6a360732c.
_extern _HASH_REM_DEL Bool StringDefinesRemove(CHash *entry,CHashTable *table,
        I64 instance=1);

asm {
_STRING_DEFINES_RSP::
  LEA RAX,8[RSP]
  RET
}

_extern _STRING_DEFINES_RSP U64 StringDefinesRSP();

#assert sizeof(CHashDefineStr)==88
#assert offset(CHashDefineStr.data)==64
#assert offset(CHashDefineStr.sub_idx)==72
#assert offset(CHashDefineStr.cnt)==80

Bool StringDefinesRecord(CHashDefineStr *entry,U8 *name,U8 *value)
{
  return entry && entry->type==4 && entry->cnt==-1 && !entry->sub_idx &&
      entry->str && entry->data && entry->src_link &&
      !StrCmp(entry->str,name) && !StrCmp(entry->data,value);
}

U64 StringDefinesCaller(CHashDefineStr *entry)
{
  U8 *end;
  U64 pc;
  if (!entry || !entry->src_link || StrNCmp(entry->src_link,"AD:0x",5))
    return 0;
  pc=Str2I64(entry->src_link+5,16,&end);
  if (!end || end==entry->src_link+5 || *end)
    return 0;
  return pc;
}

Bool StringDefinesHeapRoundTrip()
{
  CHashDefineStr *plain,*printed;
  I64 used=Fs->data_heap->used_u8s;
  Bool okay;
  plain=DefineLoad("ST_SD_HEAP_LOAD","heap");
  DefinePrint("ST_SD_HEAP_PRINT","%s:%d","heap",42);
  printed=HashFind("ST_SD_HEAP_PRINT",Fs->hash_table,4);
  okay=StringDefinesRecord(plain,"ST_SD_HEAP_LOAD","heap") &&
      StringDefinesRecord(printed,"ST_SD_HEAP_PRINT","heap:42");
  if (!StringDefinesRemove(printed,Fs->hash_table))
    okay=FALSE;
  if (!StringDefinesRemove(plain,Fs->hash_table))
    okay=FALSE;
  return okay && !HashFind("ST_SD_HEAP_LOAD",Fs->hash_table,4) &&
      !HashFind("ST_SD_HEAP_PRINT",Fs->hash_table,4) &&
      Fs->data_heap->used_u8s==used;
}

U0 StringDefinesProbe()
{
  CHashDefineStr *owned[7],*entry,*list;
  CExcept *first=Fs->next_except,*last=Fs->last_except;
  U8 name[32],text[32],*value,*formatted,*saved_data,*saved_link;
  U64 copy=0,values=0,duplicate=0,printed=0,caller=0,errors=0,cleanup=0;
  U64 direct1,direct2,internal1,internal2,before,after;
  I64 i,count;
  Bool removed=TRUE,value_stack;
  MemSet(owned,0,sizeof(owned));
  MemSet(name,0x5A,sizeof(name));
  MemSet(text,0x5A,sizeof(text));
  StrCpy(name,"ST_SD_COPY");
  StrCpy(text,"alpha");
  before=StringDefinesRSP;
  entry=owned[0]=DefineLoad(name,text);
  after=StringDefinesRSP;
  if (before==after && StringDefinesRecord(entry,"ST_SD_COPY","alpha"))
    copy|=1;
  before=StringDefinesRSP;
  value=Define("ST_SD_COPY");
  after=StringDefinesRSP;
  value_stack=before==after;
  before=StringDefinesRSP;
  count=DefineCnt("ST_SD_COPY");
  after=StringDefinesRSP;
  if (entry && entry->str!=name && entry->data!=text &&
      value==entry->data && count==-1 && value_stack && before==after &&
      !DefineSub(0,"ST_SD_COPY"))
    copy|=2;
  name[0]='X';
  text[0]='Z';
  if (StringDefinesRecord(entry,"ST_SD_COPY","alpha") &&
      HashFind("ST_SD_COPY",Fs->hash_table,4)==entry &&
      !HashFind("XT_SD_COPY",Fs->hash_table,4))
    copy|=4;
  if (entry && (value=Define("ST_SD_COPY"))) {
    *value='A';
    if (value==entry->data && !StrCmp(Define("ST_SD_COPY"),"Alpha"))
      copy|=8;
  }
  if (entry && entry->str && entry->data && entry->src_link &&
      entry(U64 *)[-2]==Fs->data_heap &&
      entry->str(U64 *)[-2]==Fs->data_heap &&
      entry->data(U64 *)[-2]==Fs->data_heap &&
      entry->src_link(U64 *)[-2]==Fs->data_heap &&
      MSize(entry)>=sizeof(CHashDefineStr) &&
      MSize(entry->str)>=11 && MSize(entry->data)>=6 &&
      name[31]==0x5A && text[31]==0x5A)
    copy|=16;

  owned[1]=DefineLoad("ST_SD_EMPTY","");
  owned[2]=DefineLoad("ST_SD_NULL",NULL);
  if (StringDefinesRecord(owned[1],"ST_SD_EMPTY","") &&
      Define("ST_SD_EMPTY")==owned[1]->data && DefineCnt("ST_SD_EMPTY")==-1)
    values|=1;
  if (StringDefinesRecord(owned[2],"ST_SD_NULL","") &&
      Define("ST_SD_NULL")==owned[2]->data && DefineCnt("ST_SD_NULL")==-1 &&
      owned[1]->data!=owned[2]->data)
    values|=2;
  if (!Define(NULL) && DefineCnt(NULL)==-1 && !DefineSub(0,NULL))
    values|=4;
  list=HashFind("ST_U8_REGS",Fs->hash_table,4);
  if (list && Define("ST_U8_REGS")==list->data && DefineCnt("ST_U8_REGS")==16 &&
      !StrCmp(DefineSub(4,"ST_U8_REGS"),"AH") && DefineMatch("AH","ST_U8_REGS")==4)
    values|=8;

  owned[3]=DefineLoad("ST_SD_DUP","old");
  owned[4]=DefineLoad("ST_SD_DUP","new");
  if (owned[3] && owned[4] && owned[3]!=owned[4] &&
      HashFind("ST_SD_DUP",Fs->hash_table,4)==owned[4] &&
      HashFind("ST_SD_DUP",Fs->hash_table,4,2)==owned[3] &&
      !HashFind("ST_SD_DUP",Fs->hash_table,4,3))
    duplicate|=1;
  if (StringDefinesRecord(owned[3],"ST_SD_DUP","old") &&
      StringDefinesRecord(owned[4],"ST_SD_DUP","new") &&
      Define("ST_SD_DUP")==owned[4]->data)
    duplicate|=2;
  if (StringDefinesRemove(owned[4],Fs->hash_table)) {
    owned[4]=NULL;
    if (HashFind("ST_SD_DUP",Fs->hash_table,4)==owned[3] &&
        Define("ST_SD_DUP")==owned[3]->data && !StrCmp(Define("ST_SD_DUP"),"old"))
      duplicate|=4;
  }
  if (!owned[4] && StringDefinesRemove(owned[3],Fs->hash_table)) {
    owned[3]=NULL;
    if (!HashFind("ST_SD_DUP",Fs->hash_table,4))
      duplicate|=8;
  }

  before=StringDefinesRSP;
  DefinePrint("ST_SD_PRINT_A","[%S]|%s|%d|%08X|%c|%%|%0.2f",
      "ST_SD_COPY","mix",-7,0x2A,'Q',1.5);
  after=StringDefinesRSP;
  owned[5]=HashFind("ST_SD_PRINT_A",Fs->hash_table,4);
  if (before==after &&
      StringDefinesRecord(owned[5],"ST_SD_PRINT_A","[Alpha]|mix|-7|0000002A|Q|%|1.50"))
    printed|=1;
  if (owned[5] && Define("ST_SD_PRINT_A")==owned[5]->data &&
      DefineCnt("ST_SD_PRINT_A")==-1 && !DefineSub(0,"ST_SD_PRINT_A"))
    printed|=2;
  before=StringDefinesRSP;
  DefinePrint("ST_SD_PRINT_B","plain%%");
  after=StringDefinesRSP;
  owned[6]=HashFind("ST_SD_PRINT_B",Fs->hash_table,4);
  if (before==after && StringDefinesRecord(owned[6],"ST_SD_PRINT_B","plain%"))
    printed|=4;
  formatted=MStrPrint("<%S>|%S","ST_SD_PRINT_A","ST_SD_PRINT_B");
  if (formatted && !StrCmp(formatted,"<[Alpha]|mix|-7|0000002A|Q|%|1.50>|plain%"))
    printed|=8;
  Free(formatted);

  direct1=StringDefinesCaller(owned[0]);
  direct2=StringDefinesCaller(owned[1]);
  internal1=StringDefinesCaller(owned[5]);
  internal2=StringDefinesCaller(owned[6]);
  if (direct1 && direct2)
    caller|=1;
  if (direct1 && direct2 && direct1!=direct2)
    caller|=2;
  if (internal1 && internal1==internal2)
    caller|=4;
  if (internal1 && internal1!=direct1 && internal1!=direct2 &&
      owned[5]->src_link!=owned[6]->src_link &&
      !StrCmp(owned[5]->src_link,owned[6]->src_link))
    caller|=8;

  DocClear(,TRUE);
  "SD copy %X\n",copy;
  "SD values %X\n",values;
  "SD duplicate %X\n",duplicate;
  "SD print %X\n",printed;
  "SD caller %X\n",caller;
  try {
    Define("ST_SD_MISSING_VALUE");
  } catch {
    if (Fs->except_ch=='UndefDef')
      errors|=1;
    Fs->catch_except=TRUE;
  }
  try {
    DefineCnt("ST_SD_MISSING_COUNT");
  } catch {
    if (Fs->except_ch=='UndefDef')
      errors|=2;
    Fs->catch_except=TRUE;
  }
  saved_data=owned[5]->data;
  saved_link=owned[5]->src_link;
  try {
    DefinePrint("ST_SD_PRINT_A",NULL);
  } catch {
    if (Fs->except_ch=='StrPrint' &&
        HashFind("ST_SD_PRINT_A",Fs->hash_table,4)==owned[5] &&
        !HashFind("ST_SD_PRINT_A",Fs->hash_table,4,2) &&
        owned[5]->data==saved_data && owned[5]->src_link==saved_link)
      errors|=4;
    Fs->catch_except=TRUE;
  }
  try {
    DefinePrint("ST_SD_PRINT_A","%d %d",7);
  } catch {
    if (Fs->except_ch=='StrPrint' &&
        HashFind("ST_SD_PRINT_A",Fs->hash_table,4)==owned[5] &&
        !HashFind("ST_SD_PRINT_A",Fs->hash_table,4,2) &&
        StringDefinesRecord(owned[5],"ST_SD_PRINT_A","[Alpha]|mix|-7|0000002A|Q|%|1.50") &&
        owned[5]->data==saved_data && owned[5]->src_link==saved_link)
      errors|=8;
    Fs->catch_except=TRUE;
  }

  for (i=6;i>=0;i--)
    if (owned[i] && !StringDefinesRemove(owned[i],Fs->hash_table))
      removed=FALSE;
  if (removed && !HashFind("ST_SD_COPY",Fs->hash_table,4) &&
      !HashFind("ST_SD_EMPTY",Fs->hash_table,4) &&
      !HashFind("ST_SD_NULL",Fs->hash_table,4) &&
      !HashFind("ST_SD_DUP",Fs->hash_table,4) &&
      !HashFind("ST_SD_PRINT_A",Fs->hash_table,4) &&
      !HashFind("ST_SD_PRINT_B",Fs->hash_table,4))
    cleanup|=1;
  if (StringDefinesHeapRoundTrip)
    cleanup|=2;
  if (Fs->next_except==first && Fs->last_except==last)
    cleanup|=4;
  "SD errors %X\n",errors;
  "SD cleanup %X\n",cleanup;
  "SD DONE\n";
}

StringDefinesProbe;
