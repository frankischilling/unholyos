// Exercise the original recursive task report against real kernel tasks.
#include "/Adam/AMem.HC"
#include "/Adam/TaskRep.HC"

I64 adam_task_probe_done;

U0 AdamTaskProbeChild(U8 *unused)
{
  while (!adam_task_probe_done)
    Sleep(1);
}

U0 AdamTaskProbe()
{
  I64 res=0,flags;
  CTask *parent,*child;
  CDoc *doc=DocNew,*saved=Fs->put_doc;
  U8 *report,*p,*c,*link,*stats;
  Bool old_raw=Raw(FALSE);
  adam_task_probe_done=0;
  parent=Spawn(&AdamTaskProbeChild,,"Report parent",,Fs);
  child=Spawn(&AdamTaskProbeChild,,"Report child",,parent);
  if (mp_cnt>0 && cpu_structs && &cpu_structs[Gs->num]==Gs &&
      TaskValidate(cpu_structs[0].seth_task)) res|=1;
  if (TaskValidate(parent) && TaskValidate(child) && child->parent_task==parent)
    res|=2;
  if (res==3 && doc) {
    StrCpy(parent->task_title,"Report parent");
    StrCpy(child->task_title,"Report child");
    doc->flags|=DOCF_NO_CURSOR;
    Fs->put_doc=doc;
    flags=GetRFlags;
    TaskRep;
    if (!((GetRFlags^flags)&0x200)) res|=4;
    Fs->put_doc=saved;
    report=DocSave(doc);
    if (report) {
      if (StrMatch("CPU00",report)) res|=8;
      p=StrMatch("Report parent",report);
      c=StrMatch("Report child",report);
      if (p && c && p<c) res|=16;
      link=MStrPrint("Kill(0x%X);",child);
      if (StrMatch(link,report)) res|=32;
      Free(link);
      stats=MStrPrint("%08X %04X:%04X:%08X",TaskMemAlloced(child),
          child->task_flags,child->display_flags,child->win_inhibit);
      if (StrMatch(stats,report)) res|=64;
      Free(stats);
      Free(report);
    }
  }
  Fs->put_doc=saved;
  Raw(old_raw);
  DocDel(doc);
  adam_task_probe_done=1;
  while (TaskValidate(child)) Yield;
  while (TaskValidate(parent)) Yield;
  res|=128;
  "Adam tasks %X\n",res;
}

AdamTaskProbe;
