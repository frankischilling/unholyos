I64 startup_exec_result;

U0 StartupExecProbe()
{
  I64 res=0,args[2];
  CBlkDev *bd;
  U8 *file="B:/RunEntry.HC",*src=
      "I64 RunEntry(I64 a,I64 b,I64 c){return 100*a+10*b+c;}\n";
  args[0]=3;
  args[1]=5;
  if (ExePrint("%d*%d;",7,6)==42 && ExePutS2("7*6;")==42 &&
      ExePrint2("%d*%d;",7,6)==42) res|=1;
  ExePrint("I64 RunTop(I64 a,I64 b){return 17*a+b;}\n");
  if (LastFun(2,args)==56) res|=2;
  if (!Let2Drv('B',FALSE)) {
    bd=BlkDevNextFreeSlot('B',BDT_RAM);
    bd->max_blk=511;
    BlkDevAdd(bd,,FALSE,TRUE);
  }
  if (FileWrite(file,src,StrLen(src))) {
    if (RunFile("B:/RunEntry",,3,4,5)==345) res|=4;
    if (RunFile2(file,,6,7,8)==678) res|=8;
    Del(file,,,FALSE);
  }
  if (!ExePrint("throw('Break',TRUE);")) res|=16;
  try {
    ExePrint2("throw('Break',TRUE);");
  } catch {
    if (Fs->except_ch=='Break') res|=32;
    Fs->catch_except=TRUE;
  }
  startup_exec_result=res;
}
StartupExecProbe;

public I64 startup_input_state;

U0 StartupInputFinish(I64 res)
{
  if (startup_input_state==4) res|=8;
  "Startup exec %X input %X\n",startup_exec_result,res;
}

U0 StartupInputProbe()
{
  I64 res=0;
  U8 *dir=DirCur,*file="RunKeys.IN",*src="InStr(\"startup_input_state=3;\");\n";
  InStr("startup_input_state=1;InStr(\"startup_input_state=2;\");");
  TaskWait(Fs);
  if (startup_input_state==2) res|=1;
  if (Cd("B:/") && FileWrite(file,src,StrLen(src))) {
    InFile("RunKeys");
    TaskWait(Fs);
    if (startup_input_state==3) res|=2;
    Del(file,,,FALSE);
  }
  Cd(dir);
  Free(dir);
  XTalkStrWait(Fs,"startup_input_state=%d;",4);
  if (startup_input_state==4) res|=4;
  In("StartupInputFinish(%d);0;\n",res);
}
StartupInputProbe;
