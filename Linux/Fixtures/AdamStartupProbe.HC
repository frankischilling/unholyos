// Run in an ordinary terminal, using the inherited Adam source namespace.
// No include of AMath.HC, AMem.HC or TaskRep.HC is needed.
I64 adam_startup_options=0;
if (!Option(OPTf_NO_REG_VAR,ON) && GetOption(OPTf_NO_REG_VAR))
  adam_startup_options|=1;
I64 AdamStartupStack(I64 x)
{
  I64 y=x+2;
  return y*y+y;
}
if (Option(OPTf_NO_REG_VAR,OFF) && !GetOption(OPTf_NO_REG_VAR))
  adam_startup_options|=2;

U0 AdamStartupChild(U8 *unused)
{
  while (TRUE) Sleep(1);
}

CHash *AdamStartupHash(CHashTable *table,U8 *name,I64 type)
{
  // Room for any of the three record kinds under test.
  CHash *entry=CAlloc(sizeof(CHashFun));
  entry->str=StrNew(name);
  entry->type=type;
  HashAdd(entry,table);
  return entry;
}

U0 AdamStartupProbe()
{
  I64 res=0,before,flags;
  CHashFun *stack_fun=HashFind("AdamStartupStack",Fs->hash_table,HTT_FUN);
  CHashFun *math=HashFind("DistSqrI64",Fs->hash_table,HTT_FUN);
  CHashTable *parent=HashTableNew(16),*table=HashTableNew(16);
  CHash *raw,*invalid,*typed,*other;
  CTask *child;
  CDoc *doc=DocNew,*saved=Fs->put_doc;
  U8 *report,*link,*p;
  Bool old_raw=Raw(FALSE);
  if (adam_startup_options==3 && AdamStartupStack(4)==42 &&
      !(stack_fun->used_reg_mask & 0xF000)) res|=1;
  if (math && !HashSingleTableFind("DistSqrI64",Fs->hash_table,HTT_FUN) &&
      DistSqrI64(1,2,4,6)==25) res|=2;
  before=TaskMemUsed;
  p=MAlloc(128);
  if (TaskMemUsed==before+MSize2(p) && TaskMemAlloced>=TaskMemUsed) res|=4;
  Free(p);
  if (TaskMemUsed==before) res|=8;

  table->next=parent;
  raw=AdamStartupHash(parent,"StartupPurge",HTT_EXPORT_SYS_SYM);
  invalid=AdamStartupHash(table,"StartupInvalid",HTT_INVALID);
  other=AdamStartupHash(table,"StartupOther",HTT_EXPORT_SYS_SYM);
  AdamStartupHash(table,"StartupPurge",HTT_EXPORT_SYS_SYM);
  typed=AdamStartupHash(table,"StartupPurge",HTT_FUN);
  AdamStartupHash(table,"StartupInvalid",HTT_FUN);
  flags=GetRFlags;
  if (HashTablePurge(table)==2 && !((flags^GetRFlags)&0x200) &&
      HashSingleTableFind("StartupPurge",table,HTT_FUN)==typed &&
      !HashSingleTableFind("StartupPurge",table,HTT_EXPORT_SYS_SYM) &&
      HashFind("StartupPurge",table,HTT_EXPORT_SYS_SYM)==raw &&
      HashSingleTableFind("StartupOther",table,HTT_EXPORT_SYS_SYM)==other &&
      HashTablePurge(table)==0 && HashTablePurge(NULL)==0) res|=16;
  HashTableDel(table);
  HashTableDel(parent);

  child=Spawn(&AdamStartupChild,,"Startup child",,Fs);
  if (TaskValidate(child) && !Kill(NULL) && !Kill(Gs->seth_task) &&
      !Kill(sys_winmgr_task)) res|=32;
  if (doc && TaskValidate(child)) {
    StrCpy(child->task_title,"Startup child");
    doc->flags|=DOCF_NO_CURSOR;
    Fs->put_doc=doc;
    TaskRep;
    Fs->put_doc=saved;
    report=DocSave(doc);
    link=MStrPrint("Kill(0x%X);",child);
    if (report && StrMatch("Startup child",report) && StrMatch(link,report))
      res|=64;
    Free(report);
    // Run exactly the command emitted by the original report.
    ExePutS(link);
    Free(link);
  }
  Fs->put_doc=saved;
  Raw(old_raw);
  DocDel(doc);
  if (!TaskValidate(child)) res|=128;
  else Kill(child);
  "Adam startup %X\n",res;
}

AdamStartupProbe;
