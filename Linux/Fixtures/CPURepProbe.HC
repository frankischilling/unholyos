// Check the live report without expecting fixed clock or swap counts.
U0 CPURepProbe()
{
  CDoc *saved=Fs->put_doc,*doc=DocNew;
  Bool old_raw=Raw(FALSE);
  I64 checks=0;
  F64 t0,elapsed;
  U8 *summary,*report,*row,*total,*avg,*ratio,*ns;
  CJob *job;

  doc->flags|=DOCF_NO_CURSOR;
  Fs->put_doc=doc;
  CPURep;
  summary=MStrPrint("%03X Cores %6h?nHz\n",mp_cnt,ToF64(cnts.time_stamp_freq));
  report=DocSave(doc);
  if (mp_cnt==1 && cnts.time_stamp_freq>0 && !StrCmp(summary,report))
    checks|=1;
  Free(report);
  DocClear(doc,1);
  CPURep(FALSE);
  report=DocSave(doc);
  if (!StrCmp(summary,report)) checks|=2;
  Free(report);

  DocClear(doc,1);
  t0=tS;
  CPURep(TRUE);
  elapsed=tS-t0;
  report=DocSave(doc);
  if (StrMatch("\n001 Cores ",report) && StrMatch("Hz\n",report) &&
      StrMatch("CPU     Swaps/s         Cycles",report))
    checks|=4;
  if (elapsed>=1.8) checks|=8;
  row=StrMatch("\n 00 ",report);
  total=NULL;
  if (row) total=StrMatch("\n001 ",row);
  avg=StrMatch("\nAvg ",report);
  if (row && Str2I64(row+5)>0 && Str2I64(row+19)>0) checks|=16;
  if (row && total && avg && !MemCmp(row+5,total+5,31) &&
      !MemCmp(row+5,avg+5,31)) checks|=32;
  ratio=StrMatch("Avg Cycles/Swap: ",report);
  ns=StrMatch("Avg Time/Swap  : ",report);
  if (ratio && ns && Str2F64(ratio+17)>0.0 && Str2F64(ns+17)>0.0 &&
      StrMatch("ns\n",ns))
    checks|=64;
  Fs->put_doc=saved;
  Free(report);
  Free(summary);
  DocDel(doc);
  Raw(old_raw);
  job=TaskExe(adam_task,Fs,"17*23;",0);
  if (job && JobResGet(job)==391) checks|=128;
  "CPU reports %X\n",checks;
}

CPURepProbe;
