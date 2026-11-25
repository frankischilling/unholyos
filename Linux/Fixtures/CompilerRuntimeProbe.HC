#include "T:/Kernel/KernelA.HH"
#include "T:/Compiler/CompilerA.HH"

_intern IC_FS CTask *ProbeCompilerTask();
_import _STRCMP I64 ProbeCompilerCmp(U8 *left,U8 *right);
_import _FREE U0 ProbeCompilerFree(U8 *p);
_import _HASH_ADD U0 ProbeCompilerHashAdd(CHash *entry,CHashTable *table);
_import _HASH_REM_DEL Bool ProbeCompilerRemove(CHash *entry,CHashTable *table,I64 instance=1);
import CBlkDevGlbls blkdev;
import CDbgGlbls dbg;
import CTask *adam_task;
import U8 *CAlloc(I64 size,CTask *task=NULL);
import U8 *StrNew(U8 *s,CTask *task=NULL);
import CDrv *Let2Drv(U8 letter=0,Bool except=TRUE);
import U8 *FileRead(U8 *filename,I64 *size=NULL,I64 *attr=NULL);
import U8 *FileNameAbs(U8 *filename,I64 flags=0);
import CDoc *DocNew(U8 *filename=NULL,CTask *task=NULL);
import U0 DocDel(CDoc *doc);
import U8 *DocSave(CDoc *doc,I64 *size=NULL);
import Bool Raw(Bool enabled);
import Bool IsRaw();
import Bool DbgMode(Bool enabled);
import Bool IsDbgMode();
import U0 FixSet(U8 *filename,I64 line);
import U0 PutFileLink(U8 *filename,U8 *full_name=NULL,I64 line=0,Bool plain=FALSE);
import Bool PutSrcLink(U8 *addr,I64 count=1,U8 *buf=NULL);
import U0 AdamErr(U8 *fmt,...);
import U0 Print(U8 *fmt,...);

#assert sizeof(CBlkDevGlbls)==384 && offset(CBlkDevGlbls.tmp_filename)==24
#assert offset(CBlkDevGlbls.let_to_drv)==96 && offset(CBlkDevGlbls.boot_drv_let)==376
#assert sizeof(CDbgGlbls)==72 && offset(CDbgGlbls.fix_file_line)==40

public I64 CompilerRuntimeProbeResult;

Bool ProbeCompilerString(U8 *actual,U8 *expected)
{
  Bool ok=actual && !ProbeCompilerCmp(actual,expected);
  ProbeCompilerFree(actual);
  return ok;
}

Bool ProbeDriveGlobals()
{
  CDrv *t=Let2Drv('t',FALSE),*b=Let2Drv('b',FALSE);
  I64 i;
  if (!t || !b)
    return FALSE;
  Bool ok=blkdev.boot_drv_let=='T' && blkdev.first_hd_drv_let=='C' &&
      blkdev.first_dvd_drv_let=='T' && blkdev.dvd_boot_is_good &&
      !ProbeCompilerCmp(blkdev.dft_iso_filename,"::/Tmp/CDDVD.ISO") &&
      !ProbeCompilerCmp(blkdev.dft_iso_c_filename,"::/Tmp/CDDVD.ISO.C") &&
      !ProbeCompilerCmp(blkdev.tmp_filename,"~/Tmp.DD.Z") &&
      t==blkdev.let_to_drv['T'-'A'] && b==blkdev.let_to_drv['B'-'A'] &&
      Let2Drv(':',FALSE)==t && Let2Drv('~',FALSE)==t && !Let2Drv('Z',FALSE) &&
      t->bd->type==BDT_ATAPI && b->bd->type==BDT_RAM &&
      blkdev.blkdevs<=t->bd<blkdev.blkdevs+BLKDEVS_NUM &&
      blkdev.blkdevs<=b->bd<blkdev.blkdevs+BLKDEVS_NUM;
  U8 *old_home=blkdev.home_dir,*old_tmp=blkdev.tmp_filename;
  blkdev.home_dir="B:/HomeData";
  blkdev.tmp_filename="B:/Probe.DD";
  CDoc *doc=DocNew;
  ok=ok && Let2Drv('~',FALSE)==b && doc &&
      !ProbeCompilerCmp(doc->filename.name,"B:/Probe.DD") &&
      ProbeCompilerString(FileNameAbs("~/leaf.HC"),"B:/HomeData/leaf.HC");
  DocDel(doc);
  blkdev.home_dir=old_home;
  blkdev.tmp_filename=old_tmp;
  I64 before=blkdev.read_cnt,size;
  U8 *source=FileRead("T:/StartOS.HC",&size);
  ok=ok && source && size==0x486 && blkdev.read_cnt>=before && blkdev.read_cnt>0;
  ProbeCompilerFree(source);
  return ok;
}

Bool ProbeCompilerModes()
{
  Bool old_raw=Raw(FALSE),old_dbg=DbgMode(FALSE);
  Bool ok=!IsRaw && !IsDbgMode && !Raw(TRUE) && IsRaw &&
      !DbgMode(TRUE) && IsDbgMode && Raw(FALSE) && !IsRaw &&
      DbgMode(FALSE) && !IsDbgMode;
  Raw(old_raw);
  DbgMode(old_dbg);
  return ok;
}

Bool ProbeFileLinks()
{
  CDoc *doc=DocNew,*saved=ProbeCompilerTask()->put_doc;
  Bool old_raw=Raw(FALSE);
  doc->flags|=DOCF_NO_CURSOR;
  ProbeCompilerTask()->put_doc=doc;
  PutFileLink("StartOS.HC","T:/StartOS.HC",17);
  Print("\n");
  PutFileLink("StartOS.HC","T:/StartOS.HC",0,TRUE);
  Print("\n");
  PutFileLink("StartOS.HC","T:/StartOS.HC",33,TRUE);
  Print("\n");
  PutFileLink("A0:0x1234",NULL,7,TRUE);
  Print("\n");
  PutFileLink("A0:0x1234");
  Print("\n");
  PutFileLink("T:/StartOS.HC");
  ProbeCompilerTask()->put_doc=saved;
  Raw(TRUE);
  PutFileLink("RAW-LOCATION.HC",NULL,27,TRUE);
  Print("\n");
  Raw(old_raw);
  Bool ok=ProbeCompilerString(DocSave(doc),
      "$$LK,\"StartOS.HC,0017\",A=\"FL:T:/StartOS.HC,17\"$$\n"
      "$$LK,\"StartOS.HC\",A=\"PI:T:/StartOS.HC\"$$\n"
      "$$LK,\"StartOS.HC,0033\",A=\"PL:T:/StartOS.HC,33\"$$\n"
      "$$LK,\"0x1234,0007\",A=\"AL:0x1234,7\"$$\n"
      "$$LK,\"0x1234\",A=\"AI:0x1234\"$$\n"
      "$$LK,\"T:/StartOS.HC\",A=\"FI:T:/StartOS.HC\"$$");
  DocDel(doc);
  return ok;
}

I64 ProbeSourceBody(I64 value)
{
  return value+23;
}

Bool ProbeSourceLinks()
{
  CTask *task=ProbeCompilerTask();
  CHashFun *fun=CAlloc(sizeof(CHashFun));
  U8 *pc=&ProbeSourceBody;
  fun->str=StrNew("ProbeSource");
  fun->type=HTT_FUN;
  fun->exe_addr=pc;
  fun->src_link=StrNew("FL:T:/Home/Probe.HC,19");
  ProbeCompilerHashAdd(fun,task->hash_table);
  U8 text[128];
  Bool ok=PutSrcLink(pc,1,text) &&
      !ProbeCompilerCmp(text,"$$LK,\"&ProbeSource+0x0000\",A=\"FL:T:/Home/Probe.HC,19\"$$");
  CDbgInfo *map=CAlloc(32);
  map->min_line=40;
  map->max_line=44;
  map->body[1]=pc;
  map->body[3]=pc+8;
  map->body[4]=pc+16;
  map->body[5]=pc+20;
  fun->dbg_info=map;
  ok=ok && PutSrcLink(pc,1,text) &&
      !ProbeCompilerCmp(text,"$$LK,\"&ProbeSource+0x0000\",A=\"FL:T:/Home/Probe.HC,41\"$$") &&
      PutSrcLink(pc+1,4,text) &&
      !ProbeCompilerCmp(text,"$$LK,\"&ProbeSource+0x0001\",A=\"FL:T:/Home/Probe.HC,43\"$$") &&
      !PutSrcLink(pc+5,1,text) && !*text;
  ProbeCompilerRemove(fun,task->hash_table);
  return ok;
}

Bool ProbeFixSet()
{
  FixSet("T:/Home/First.HC",11);
  if (!dbg.fix_file_line || ProbeCompilerCmp(dbg.fix_file_line,"FL:T:/Home/First.HC,11"))
    return FALSE;
  FixSet("T:/Home/Second.HC",-2);
  return dbg.fix_file_line && !ProbeCompilerCmp(dbg.fix_file_line,"FL:T:/Home/Second.HC,-2");
}

Bool ProbeAdamErr()
{
  CDoc *doc=DocNew(NULL,adam_task),*saved=adam_task->put_doc;
  I64 display=adam_task->display_flags;
  doc->flags|=DOCF_NO_CURSOR;
  adam_task->put_doc=doc;
  adam_task->display_flags=display | 1<<DISPLAYf_NOT_RAW;
  AdamErr("COMPILER-ERROR %X\n",0x1A);
  adam_task->put_doc=saved;
  adam_task->display_flags=display;
  Bool ok=ProbeCompilerString(DocSave(doc),
      "$$FG,12$$$$BK,1$$ERROR:$$FG$$$$BK,0$$ COMPILER-ERROR 1A\n");
  DocDel(doc);
  return ok;
}

U0 CompilerRuntimeProbeMain()
{
  if (ProbeDriveGlobals)
    CompilerRuntimeProbeResult|=1;
  if (ProbeCompilerModes)
    CompilerRuntimeProbeResult|=2;
  if (ProbeFileLinks)
    CompilerRuntimeProbeResult|=4;
  if (ProbeSourceLinks)
    CompilerRuntimeProbeResult|=8;
  if (ProbeFixSet)
    CompilerRuntimeProbeResult|=16;
  if (ProbeAdamErr)
    CompilerRuntimeProbeResult|=32;
}
CompilerRuntimeProbeMain;
