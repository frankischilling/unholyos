U0 JobProbeWorker(U8 *opaque)
{
  I64 arg1,arg2,*result=opaque;
  GetMsg(&arg1,&arg2,1<<MSG_CMD);
  *result=arg1+arg2;
}

U0 JobProbeServant(U8 *opaque)
{
  if (!opaque)
    SrvTaskCont;
}

U0 JobProbe()
{
  I64 a,b,down,up,key,scan,none,value=0,result1=0,result2=0,result3=0;
  I64 text_result=0,msg_result=0,state=0;
  CTask *worker;
  CJob *job;
  U8 *text;

  Msg(MSG_CMD,99,900);
  Msg(MSG_KEY_DOWN_UP,65,30);
  down=ScanMsg(&a,&b,1<<MSG_KEY_DOWN);
  key=a;
  up=GetMsg(&a,&b,1<<MSG_KEY_UP);
  scan=b;
  none=ScanMsg(&a,&b);
  "jobs masks %d %d %d %d %d %d %d\n",down,key,up,scan,none,a,b;

  worker=Spawn(&JobProbeWorker,&msg_result,,,Fs);
  TaskWait(worker);
  job=TaskExe(worker,Fs,"6*7;",0);
  if (job && !JobResScan(job,&value) && !value)
    result1=JobResGet(job);
  job=TaskExe(worker,Fs,"23+24;",1<<JOBf_WAKE_MASTER);
  if (job) {
    state=(job->flags&(1<<JOBf_DONE|1<<JOBf_DISPATCHED))==
                       (1<<JOBf_DONE|1<<JOBf_DISPATCHED);
    JobResScan(,&result2);
  }

  text=MStrPrint("*(%d(I64 *))=77;",&text_result);
  TaskText(worker,Fs,text,1<<JOBf_HIGHEST_PRIORITY);
  Free(text);
  TaskWait(worker);
  PostMsgWait(worker,MSG_CMD,9,13);
  "jobs result %d %d %d %d %d\n",result1,result2,state,text_result,msg_result;

  worker=Spawn(&JobProbeServant,,,,Fs);
  TaskWait(worker);
  job=TaskExe(worker,Fs,"93;",1<<JOBf_WAKE_MASTER|1<<JOBf_EXIT_ON_COMPLETE);
  if (job)
    JobResScan(job,&result3);
  "jobs servant %d %d\n",result3,TaskValidate(worker);
}

JobProbe;
