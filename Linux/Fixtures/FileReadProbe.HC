#include "T:/Kernel/KernelA.HH"
#include "T:/Compiler/CompilerA.HH"

_intern IC_FS CTask *ProbeFileTask();
_import _FREE U0 ProbeFileFree(U8 *ptr);
_import _MSIZE I64 ProbeFileSize(U8 *ptr);
_import _STRCMP I64 ProbeFileCmp(U8 *a,U8 *b);
_import _HASH_ADD U0 ProbeFileHashAdd(CHash *entry,CHashTable *table);
import CTask *adam_task;
import U8 *DirCur(CTask *task,CTask *mem_task);
import U8 *DirNameAbs(U8 *name);
import U8 *FileNameAbs(U8 *name,I64 flags);
import U8 *DirFile(U8 *name,U8 *file,U8 *ext);
import U8 *ExtDft(U8 *name,U8 *ext);
import U8 *ExtChg(U8 *name,U8 *ext);
import U8 *FileRead(U8 *name,I64 *size,I64 *attr);

Bool ProbePathEquals(U8 *value,U8 *expected)
{
  Bool ok=value && !ProbeFileCmp(value,expected);
  ProbeFileFree(value);
  return ok;
}

Bool ProbeFileNames()
{
  return ProbePathEquals(DirCur(NULL,NULL),"T:/Home") &&
         ProbePathEquals(DirNameAbs(" ::/0000Boot/..//Home/./ "), "T:/Home") &&
         ProbePathEquals(DirNameAbs("X:/lost/~/../Kernel"),"T:/Kernel") &&
         ProbePathEquals(DirNameAbs("t:Kernel/./Mem"),"t:/Kernel/Mem") &&
         ProbePathEquals(FileNameAbs(" ../0000Boot/0000Kernel.BIN.C ",0),
                         "T:/0000Boot/0000Kernel.BIN.C") &&
         ProbePathEquals(FileNameAbs("t:/StartOS.HC",FUF_Z_OR_NOT_Z),
                         "T:/StartOS.HC.Z") &&
         ProbePathEquals(FileNameAbs("T:/0000Boot/StartOS.HC",
                                     FUF_SCAN_PARENTS|FUF_Z_OR_NOT_Z),
                         "T:/StartOS.HC.Z") &&
         ProbePathEquals(FileNameAbs("T:/0000Boot/StartOS.HC",FUF_Z_OR_NOT_Z),
                         "T:/0000Boot/StartOS.HC") &&
         ProbePathEquals(FileNameAbs("/Home/.",0),"T:/Home/.");
}

Bool ProbeExtensions()
{
  return ProbePathEquals(ExtDft("../Compiler/PRJ","PRJ.Z"),"../Compiler/PRJ.PRJ.Z") &&
         ProbePathEquals(ExtDft("a.b/c","HC.Z"),"a.b/c") &&
         ProbePathEquals(ExtChg("a.b/c","HC.Z"),"a.HC.Z") &&
         ProbePathEquals(ExtChg("Name.HC.Z","BIN"),"Name.BIN") &&
         ProbePathEquals(DirFile("::/0000Boot/0000Kernel.BIN.C",NULL,NULL),
                         "T:/0000Boot") &&
         ProbePathEquals(DirFile("/0000Boot","StartOS",".HC.Z"),
                         "T:/0000Boot/StartOS.HC.Z") &&
         ProbePathEquals(DirFile("/","StartOS",".HC.Z"),"T://StartOS.HC.Z") &&
         ProbePathEquals(DirFile("/0000Boot","MissingProbe","HC"),"T:/0000Boot/MissingProbe.HC");
}

Bool ProbeStartOS(U8 *name)
{
  I64 len,attr,i;
  U8 *data=FileRead(name,&len,&attr);
  U64 hash=14695981039346656037;
  Bool ok=data && len==1158 && attr & RS_ATTR_COMPRESSED && !data[len];
  if (ok) {
    for (i=0;i<len;i++)
      hash=(hash ^ data[i])*1099511628211;
    ok=hash==0x68B531597D446B26;
  }
  ProbeFileFree(data);
  return ok;
}

Bool ProbePlainFile()
{
  I64 len,attr;
  U8 *data=FileRead("t:/0000Boot/0000Kernel.BIN.C",&len,&attr);
  Bool ok=data && len>32 && !data[len] &&
          *(data+4)(U32 *)==BIN_SIGNATURE_VAL && attr & RS_ATTR_CONTIGUOUS &&
          !(attr & RS_ATTR_COMPRESSED) && ProbeFileSize(data)>=(len+511 & -512);
  ProbeFileFree(data);
  len=attr=1;
  return ok && !FileRead("T:/MissingProbe.HC",&len,&attr) && !len && !attr;
}

Bool ProbeResidentFile()
{
  CHashGeneric resident;
  resident.str="T:/ResidentProbe.BIN";
  resident.type=HTT_FILE|HTF_PUBLIC;
  resident.use_cnt=0;
  resident.user_data0="Resident!";
  resident.user_data1=9;
  CHashTable *table=adam_task->hash_table;
  ProbeFileHashAdd(&resident,table);
  I64 size,attr,i;
  U8 *data=FileRead("T:/ResidentProbe.BIN.Z",&size,&attr);
  for (i=0;i<=table->mask;i++)
    if (table->body[i]==&resident) {
      table->body[i]=resident.next;
      break;
    }
  Bool ok=data && size==9 && !attr && !ProbeFileCmp(data,"Resident!") &&
          data!=resident.user_data0 && resident.use_cnt==1;
  ProbeFileFree(data);
  return ok;
}

public I64 FileReadProbeResult;
FileReadProbeResult=ProbeFileNames()+2*ProbeExtensions()
    +4*(ProbeStartOS("../StartOS.HC") && ProbeStartOS("T:/StartOS.HC.Z"))
    +8*ProbeStartOS("::/0000Boot/StartOS.HC")
    +16*ProbePlainFile()+32*ProbeResidentFile()
    +64*ProbePathEquals(DirCur(ProbeFileTask(),NULL),"T:/Home");
