#include "T:/Kernel/KernelA.HH"
#include "T:/Compiler/CompilerA.HH"

_intern IC_FS CTask *ProbeFormatTask();
_import _STRCMP I64 ProbeFormatCmp(U8 *a,U8 *b);
_import _FREE U0 ProbeFormatFree(U8 *ptr);
import U0 SysTry(U8 *catch_start,U8 *untry_start);
import U0 SysUntry();
import U8 *StrPrintJoin(U8 *dst,U8 *fmt,I64 argc,I64 *argv);
import U8 *StrPrint(U8 *dst,U8 *fmt,...);
import U8 *CatPrint(U8 *dst,U8 *fmt,...);
import U8 *MStrPrint(U8 *fmt,...);
import U0 Print(U8 *fmt,...);
import U0 PrintErr(U8 *fmt,...);
import U0 PrintWarn(U8 *fmt,...);
import U0 PutChars(U64 ch);
import Bool IsRaw();
import CHashDefineStr *DefineLstLoad(U8 *name,U8 *list);
import I64 DefineMatch(U8 *needle,U8 *name,I64 flags);
import U8 *DefineSub(I64 index,U8 *name);
import U8 *FileRead(U8 *file,I64 *size,I64 *attr);

public U8 *FormatProbeOutput;
public I64 FormatProbeResult;

Bool ProbeFormatEquals(U8 *actual,U8 *expected)
{
  Bool ok=actual && !ProbeFormatCmp(actual,expected);
  if (!ok && !FormatProbeOutput)
    FormatProbeOutput=actual;
  else
    ProbeFormatFree(actual);
  return ok;
}

Bool ProbeIntegerFormat()
{
  return ProbeFormatEquals(MStrPrint("%d|%u|%08X|%x|%,d|%0*,b|%B|%3td",
      I64_MIN,-1,0x9A,0xBEE,-1234567,12,0xAF,5,-45678),
      "-9223372036854775808|18446744073709551615|0000009A|bee|-1,234,567|00,1010,1111|101|-78");
}

Bool ProbeFormatJoin()
{
  I64 args[4];
  U8 buf[72];
  args[0]=5;
  args[1]="ab";
  args[2]=0x6241;
  args[3]=0;
  return StrPrintJoin(buf,"[%-*s][%h2C][%2s]",4,args)==buf &&
         !ProbeFormatCmp(buf,"[ab   ][ABAB][  ]") &&
         CatPrint(buf,":%s:%%:%o", "z")==buf &&
         !ProbeFormatCmp(buf,"[ab   ][ABAB][  ]:z:%:") &&
         StrPrint(buf,"|%.*,f|",6,1234.125)==buf &&
         !ProbeFormatCmp(buf,"|1,234.125,00|");
}

Bool ProbeFormatGrow()
{
  U8 *p=MStrPrint("%-256s:%0256X","x",0xA),*q=p;
  I64 i;
  if (!p)
    return FALSE;
  Bool ok=*q++=='x';
  for (i=0;i<255;i++)
    ok=ok && *q++==' ';
  ok=ok && *q++==':';
  for (i=0;i<255;i++)
    ok=ok && *q++=='0';
  ok=ok && *q++=='A' && !*q;
  ProbeFormatFree(p);
  return ok;
}

Bool ProbeQuoteFormat()
{
  U8 input[9],*quoted,*plain;
  input[0]=36;
  input[1]='%';
  input[2]='\n';
  input[3]='\r';
  input[4]='\t';
  input[5]='"';
  input[6]='\\';
  input[7]=0x7F;
  input[8]=0;
  quoted=MStrPrint("%$$/Q",input);
  plain=MStrPrint("%/q",quoted);
  return ProbeFormatEquals(plain,input) &&
         ProbeFormatEquals(quoted,"\\d%%\\n\\r\\t\\\"\\\\\\x7F") &&
         ProbeFormatEquals(MStrPrint("%q","abc\\0def"),"abc");
}

Bool ProbeDefineFormat()
{
  U8 *list="red\0@rouge\0green\0\0";
  CHashDefineStr *defined=DefineLstLoad("ST_FORMAT_PROBE",list);
  return defined && defined->cnt==2 && !ProbeFormatCmp(defined->data,"red") &&
         DefineMatch("rouge","ST_FORMAT_PROBE",0)==0 &&
         DefineSub(1,"ST_FORMAT_PROBE")==list+11 &&
         !DefineSub(-1,"ST_FORMAT_PROBE") &&
         ProbeFormatEquals(MStrPrint("%S|%z|%Z|%z","ST_FORMAT_PROBE",
             1,list,1,"ST_FORMAT_PROBE",2,list),"red|green|green|");
}

Bool ProbeDateAndFloatFormat()
{
  CDate date;
  date.date=737848;
  date.time=0x80000000;
  return ProbeFormatEquals(MStrPrint("%D|%T|%.2f|%10e|%8.2g|%8.1h?n|%.2n",
      date,date,-12.125,0.0125,12.25,0.0025,1234000.0),
      "02/29/20|12:00:00|-12.12|1.25000e-2|   12.25|    2.5m|      1.23e6");
}

Bool ProbeEngineeringFormat()
{
  U8 *expected1="1|1.00|1.000000000e-80|1.00000000e1|1.0000000e-4|999.999990e0|1.000000000K";
  U8 *expected2="0.00000000e0|      0.250%c|   0.00e5807|1,245.123,12|        1246|e0";
  U8 want[100];
  StrPrint(want,expected2,0xE6);
  return ProbeFormatEquals(MStrPrint("%f|%.2f|%15e|%e|%g|%n|%h?d",
             0.998,0.998,1.0E-80,9.9999999999999,0.0001,999.99999,1000),expected1) &&
         ProbeFormatEquals(MStrPrint("%n|%.3h-6n|%.2h?u|%.6,f|%h?g|%02n",
             0.0,0.00000025,0xFFFFFFFFFFFFFFFF,1245.1231234,1245.5,123.01),want);
}

Bool ProbePointerAndFile()
{
  U8 buf[40],*data=FileRead("T:/StartOS.HC",NULL,NULL);
  I64 flags=ProbeFormatTask()->display_flags;
  ProbeFormatTask()->display_flags=flags & ~(1<<DISPLAYf_NOT_RAW);
  Bool ok=IsRaw && ProbeFormatEquals(MStrPrint("%p|%,p|%P",1,1,1),"1|.|1") &&
      StrPrint(buf,"%P",1)==buf &&
      !ProbeFormatCmp(buf,"$$LK,\"1\",A=\"AD:0x1\"$$");
  ProbeFormatTask()->display_flags=flags | 1<<DISPLAYf_NOT_RAW;
  ok=ok && !IsRaw &&
     ProbeFormatEquals(MStrPrint("%P",1),"$$LK,\"1\",A=\"AD:0x1\"$$");
  ProbeFormatTask()->display_flags=flags;
  if (data) {
    data[6]=0;
    ok=ProbeFormatEquals(MStrPrint("%6tF","T:/StartOS.HC"),data) && ok;
    ProbeFormatFree(data);
  } else
    ok=FALSE;
  return ok;
}

Bool ProbeFormatException()
{
  Bool ok=FALSE;
  U8 buf[8];
  try {
    StrPrint(buf,"%*d",3);
  } catch {
    ok=ProbeFormatTask()->except_ch=='StrPrint';
    ProbeFormatTask()->catch_except=TRUE;
  }
  if (!ok)
    return FALSE;
  ok=FALSE;
  try {
    StrPrint(buf,"%S","ST_FORMAT_PROBE_MISSING");
  } catch {
    ok=ProbeFormatTask()->except_ch=='UndefDef';
    ProbeFormatTask()->catch_except=TRUE;
  }
  return ok;
}

U0 ProbeFormatPrint()
{
  CTask *task=ProbeFormatTask();
  I64 flags=task->display_flags;
  Print("FORMAT-PRINT %s:%d\n","abc",29);
  PutChars(0x0A42410054);
  PrintErr("FORMAT-ERR %X\n",0x7E);
  PrintWarn("FORMAT-WARN %s\n","xy");
  task->display_flags=flags | 1<<DISPLAYf_SILENT;
  Print("FORMAT-SILENT\n");
  PrintErr("FORMAT-SILENT\n");
  PrintWarn("FORMAT-SILENT\n");
  PutChars('SILENT!');
  task->display_flags=flags;
}

U0 FormatProbeMain()
{
  if (ProbeIntegerFormat)
    FormatProbeResult|=1;
  if (ProbeFormatJoin)
    FormatProbeResult|=2;
  if (ProbeFormatGrow)
    FormatProbeResult|=4;
  if (ProbeQuoteFormat)
    FormatProbeResult|=8;
  if (ProbeDefineFormat)
    FormatProbeResult|=16;
  if (ProbeDateAndFloatFormat)
    FormatProbeResult|=32;
  if (ProbePointerAndFile)
    FormatProbeResult|=64;
  if (ProbeFormatException)
    FormatProbeResult|=128;
  if (ProbeEngineeringFormat)
    FormatProbeResult|=256;
  ProbeFormatPrint;
}
FormatProbeMain;
