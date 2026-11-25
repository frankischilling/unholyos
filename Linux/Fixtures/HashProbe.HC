#include "T:/Kernel/KernelA.HH"
#include "T:/Compiler/CompilerA.HH"

_intern IC_FS CTask *ProbeHashTask();
_import _HASH_ADD U0 ProbeHashAdd(CHash *entry,CHashTable *table);
_import _HASH_FIND CHash *ProbeHashFind(U8 *name,CHashTable *table,I64 mask,I64 instance=1);
_import _HASH_REM_DEL Bool ProbeHashRemove(CHash *entry,CHashTable *table,I64 instance=1);
_import _STRCMP I64 ProbeHashCmp(U8 *a,U8 *b);
_import _FREE U0 ProbeHashFree(U8 *p);
import U8 *CAlloc(I64 size,CTask *mem_task=NULL);
import U8 *StrNew(U8 *str,CTask *mem_task=NULL);
import CHashTable *HashTableNew(I64 size,CTask *mem_task=NULL);
import U0 HashTableDel(CHashTable *table);
import U0 HashDel(CHashSrcSym *entry);
import I64 HashVal(CHash *entry);
import U0 HashSrcFileSet(CCmpCtrl *cc,CHashSrcSym *entry,I64 line_offset=0);
import U0 SysSymImportsResolve(U8 *name,I64 ld_flags=0);
import U8 *StrPrint(U8 *buf,U8 *fmt,...);
import I64 HashCallTarget(I64 value);

#assert sizeof(CMemberLst)==144 && offset(CMemberLst.fun_ptr)==136
#assert sizeof(CHashClass)==128 && offset(CHashClass.flags)==86
#assert sizeof(CHashFun)==168 && offset(CHashFun.exe_addr)==152
#assert sizeof(CHashGlblVar)==136 && offset(CHashGlblVar.data_addr)==120
#assert sizeof(CLexFile)==80 && offset(CLexFile.full_name)==40
#assert offset(CCmpCtrl.cur_help_idx)==88 && offset(CCmpCtrl.lex_include_stk)==264 && offset(CCmpCtrl.opts)==320

public I64 HashProbeResult;

Bool ProbeHashSource()
{
  CTask *task=ProbeHashTask();
  I64 before=task->data_heap->used_u8s;
  CCmpCtrl *cc=CAlloc(sizeof(CCmpCtrl));
  CLexFile *file=CAlloc(sizeof(CLexFile));
  CHashExport *sym=CAlloc(sizeof(CHashExport));
  cc->lex_include_stk=file;
  cc->opts=1<<OPTf_KEEP_PRIVATE;
  cc->cur_help_idx="Compiler/Fixture";
  file->full_name="T:/Compiler/Lex.HC";
  file->line_num=17;
  sym->type=HTT_EXPORT_SYS_SYM;
  sym->str=StrNew("ProbeSource");
  sym->src_link=StrNew("AD:0x33");
  sym->idx=StrNew("Old index");
  HashSrcFileSet(cc,sym,-99);
  Bool ok=!ProbeHashCmp(sym->src_link,"FL:T:/Compiler/Lex.HC,1") &&
      !ProbeHashCmp(sym->idx,cc->cur_help_idx) && sym->idx!=cc->cur_help_idx &&
      sym->type==(HTT_EXPORT_SYS_SYM|HTF_PRIVATE);
  cc->opts=0;
  cc->cur_help_idx="";
  HashSrcFileSet(cc,sym,3);
  ok=ok && !ProbeHashCmp(sym->src_link,"FL:T:/Compiler/Lex.HC,20") &&
      !sym->idx && sym->type==(HTT_EXPORT_SYS_SYM|HTF_PRIVATE);
  HashDel(sym);
  ProbeHashFree(file);
  ProbeHashFree(cc);
  return ok && task->data_heap->used_u8s==before;
}

CHashGeneric *ProbeHashGeneric(CHashTable *table,U8 *name,I64 type,I64 value)
{
  CHashGeneric *entry=CAlloc(sizeof(CHashGeneric));
  if (type==HTT_DICT_WORD)
    entry->str=name;
  else
    entry->str=StrNew(name);
  entry->type=type;
  entry->user_data0=value;
  ProbeHashAdd(entry,table);
  return entry;
}

Bool ProbeHashTables()
{
  CTask *task=ProbeHashTask();
  I64 before=task->data_heap->used_u8s;
  CHashTable *parent=HashTableNew(8),*child=HashTableNew(8);
  child->next=parent;
  CHashGeneric *ancestral=ProbeHashGeneric(parent,"duplicate",HTT_KEYWORD,11);
  CHashGeneric *first=ProbeHashGeneric(child,"duplicate",HTT_KEYWORD,21);
  CHashGeneric *second=ProbeHashGeneric(child,"duplicate",HTT_KEYWORD,22);
  CHashGeneric *file=ProbeHashGeneric(child,"ProbeCache",HTT_FILE,CAlloc(19));
  CHashGeneric *word=ProbeHashGeneric(child,"Borrowed",HTT_DICT_WORD,0);
  Bool ok=!ProbeHashRemove(first,child) && second->use_cnt==1 &&
      ProbeHashRemove(first,child,2) &&
      ProbeHashFind("duplicate",child,HTT_KEYWORD,2)==ancestral &&
      HashVal(second)==22 && HashVal(file)==file->user_data0 && HashVal(word)==word;
  HashTableDel(child);
  ok=ok && ProbeHashFind("duplicate",parent,HTT_KEYWORD)==ancestral &&
      HashVal(ancestral)==11;
  HashTableDel(parent);
  HashTableDel(NULL);
  HashDel(NULL);
  return ok && task->data_heap->used_u8s==before;
}

CHashFun *ProbeHashFun(U8 *name)
{
  CHashFun *fun=CAlloc(sizeof(CHashFun)*(PTR_STARS_NUM+1));
  fun->str=StrNew(name);
  fun->type=HTT_FUN;
  fun[2].ptr_stars_cnt=2;
  return fun;
}

Bool ProbeHashMembers()
{
  CTask *task=ProbeHashTask();
  I64 before=task->data_heap->used_u8s;
  CHashFun *fun=ProbeHashFun("owner"),*nested=ProbeHashFun("callback");
  CMemberLst *member=CAlloc(sizeof(CMemberLst));
  CMemberLstMeta *meta=CAlloc(sizeof(CMemberLstMeta));
  fun->member_lst_and_root=member;
  fun->src_link=StrNew("FL:T:/Home/Fixture.HC,2");
  fun->import_name=StrNew("ownerImport");
  fun->dbg_info=CAlloc(sizeof(CDbgInfo));
  fun->ie_lst=CAlloc(sizeof(CAOTImportExport));
  member->str=StrNew("member");
  member->flags=MLF_STR_DFT_AVAILABLE|MLF_FUN;
  member->dft_val=StrNew("default");
  member->dim.next=CAlloc(sizeof(CArrayDim));
  member->dim.next->next=CAlloc(sizeof(CArrayDim));
  member->fun_ptr=nested+2;
  member->meta=meta;
  meta->flags=MLMF_IS_STR;
  meta->str=StrNew("tag");
  meta->user_data=StrNew("value");
  HashDel(fun);
  return task->data_heap->used_u8s==before;
}

Bool ProbeHashGlobals()
{
  CTask *task=ProbeHashTask();
  I64 before=task->data_heap->used_u8s;
  U8 *borrowed=CAlloc(17);
  *borrowed=73;
  I64 after_borrow=task->data_heap->used_u8s;
  CHashGlblVar *alias=CAlloc(sizeof(CHashGlblVar));
  alias->str=StrNew("alias");
  alias->type=HTT_GLBL_VAR;
  alias->flags=GVF_ALIAS|GVF_EXTERN;
  alias->data_addr=borrowed;
  Bool ok=HashVal(alias)==&alias->data_addr;
  HashDel(alias);
  ok=ok && *borrowed==73 && task->data_heap->used_u8s==after_borrow;
  CHashGlblVar *owned=CAlloc(sizeof(CHashGlblVar));
  owned->str=StrNew("owned");
  owned->type=HTT_GLBL_VAR;
  owned->data_addr=borrowed;
  owned->dim.next=CAlloc(sizeof(CArrayDim));
  owned->fun_ptr=ProbeHashFun("ownedFun")+2;
  ok=ok && HashVal(owned)==borrowed;
  HashDel(owned);
  return ok && task->data_heap->used_u8s==before;
}

I64 HashTargetBody(I64 value)
{
  return value*3;
}

I64 HashProbeData=40;

Bool ProbeHashImports()
{
  CHashTable *table=ProbeHashTask()->hash_table;
  CHashImport *waiting_fn=ProbeHashFind("HashTargetFn",table,HTT_IMPORT_SYS_SYM);
  CHashImport *waiting_var=ProbeHashFind("HashTargetVar",table,HTT_IMPORT_SYS_SYM);
  if (!waiting_fn || !waiting_var || HashVal(waiting_fn)!=waiting_fn->module_base)
    return FALSE;
  CHashFun *fun=ProbeHashFun("HashTargetFn");
  fun->exe_addr=&HashTargetBody;
  fun->flags=1<<Cf_EXTERN;
  ProbeHashAdd(fun,table);
  Bool ok=HashVal(fun)==fun;
  SysSymImportsResolve(fun->str);
  fun->flags=0;
  CHashExport *other=CAlloc(sizeof(CHashExport));
  other->str=StrNew("HashExportAlias");
  other->type=HTT_EXPORT_SYS_SYM|HTF_IMM;
  other->val=fun->exe_addr;
  ProbeHashAdd(other,table);
  CHashGlblVar *var=CAlloc(sizeof(CHashGlblVar));
  var->str=StrNew("HashTargetVar");
  var->type=HTT_GLBL_VAR;
  var->flags=GVF_ALIAS;
  var->data_addr=&HashProbeData;
  ProbeHashAdd(var,table);
  SysSymImportsResolve(var->str);
  U8 buf[80];
  StrPrint(buf,"%,p|%10tp",fun->exe_addr,fun->exe_addr+1);
  ok=ok && waiting_fn->type==HTT_INVALID && waiting_var->type==HTT_INVALID &&
      HashVal(fun)==fun->exe_addr && HashCallTarget(2)==46 &&
      !ProbeHashCmp(buf,"&HashTargetFn|&Ha+0x0001");
  ProbeHashRemove(other,table);
  ProbeHashRemove(fun,table);
  ProbeHashRemove(var,table);
  return ok;
}

Bool ProbeHashLoadedEntries()
{
  CHashTable *table=ProbeHashTask()->hash_table;
  CHash *module=ProbeHashFind("/HashCaller",table,HTT_MODULE),
        *fun=ProbeHashFind("HashCallTarget",table,HTT_EXPORT_SYS_SYM);
  return module && fun && ProbeHashRemove(module,table) &&
      ProbeHashRemove(fun,table) && !ProbeHashFind("/HashCaller",table,HTT_MODULE) &&
      !ProbeHashFind("HashCallTarget",table,HTT_EXPORT_SYS_SYM) &&
      HashCallTarget(3)==49;
}

U0 HashProbeMain()
{
  if (ProbeHashSource)
    HashProbeResult|=1;
  if (ProbeHashTables)
    HashProbeResult|=2;
  if (ProbeHashMembers)
    HashProbeResult|=4;
  if (ProbeHashGlobals)
    HashProbeResult|=8;
  if (ProbeHashImports) {
    HashProbeResult|=16;
    if (ProbeHashLoadedEntries)
      HashProbeResult|=32;
  }
}
HashProbeMain;
