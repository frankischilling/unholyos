U0 OnceProbe()
{
  I64 res=0,old_flags=sys_msg_flags[0],old_level=sys_run_level;
  Bool caught=FALSE;
  CDoc *registry=sys_registry_doc,*doc;
  CBlkDev *bd;
  if (!Let2Drv('B',FALSE)) {
    bd=BlkDevNextFreeSlot('B',BDT_RAM);
    bd->max_blk=511;
    BlkDevAdd(bd,,FALSE,TRUE);
  }
  if (DrvIsWritable('B')) {
    Del("B:/OnceProbe.HC.Z",,,FALSE);
    doc=DocNew("B:/OnceProbe.HC.Z");
    sys_registry_doc=doc;
    sys_msg_flags[0]=0;
    LBtr(&sys_run_level,RLf_ONCE_ADAM);
    LBtr(&sys_run_level,RLf_ONCE_USER);
    AOnce("sys_msg_flags[0]+=1000;");
    AOnce("if (Fs==adam_task) sys_msg_flags[0]+=%d;",5);
    Once("sys_msg_flags[0]+=1000;");
    Once("if (Fs!=adam_task) sys_msg_flags[0]+=%d;",7);
    OnceExe;
    if (sys_msg_flags[0]==12 && Bt(&sys_run_level,RLf_ONCE_ADAM) &&
        Bt(&sys_run_level,RLf_ONCE_USER)) res|=1;
    if (RegCnt("Once/Adam")==2 && RegCnt("Once/User")==2) res|=2;

    AOnce("if (Fs==adam_task) sys_msg_flags[0]=%d;",5);
    AOnce("sys_msg_flags[0]+=7;");
    Once("if (Fs!=adam_task) sys_msg_flags[0]+=%d;",11);
    OnceDrv(,"sys_msg_flags[0]*=%d;",2);
    OnceExe;
    if (sys_msg_flags[0]==46 && RegCnt("Once/Adam")==2 &&
        RegCnt("Once/User")==2) res|=4;
    OnceExe;
    if (sys_msg_flags[0]==46) res|=8;

    *doc->filename.name='T';
    AOnceDrv('B',"if (Fs==adam_task) sys_msg_flags[0]+=%d;",10);
    OnceDrv('B',"if (Fs!=adam_task) sys_msg_flags[0]+=%d;",22);
    DocTreeFExe("B:/OnceProbe.HC.Z","Once/User");
    if (*doc->filename.name=='T' && sys_msg_flags[0]==68) res|=16;
    *doc->filename.name='B';
    OnceExe;
    if (sys_msg_flags[0]==100) res|=32;

    Once("throw('OnceErr',TRUE);");
    Once("sys_msg_flags[0]=999;");
    LBtr(&sys_run_level,RLf_ONCE_ADAM);
    LBtr(&sys_run_level,RLf_ONCE_USER);
    try OnceExe;
    catch {
      caught=Fs->except_ch=='OnceErr';
      Fs->catch_except=TRUE;
    }
    if (caught && Bt(&sys_run_level,RLf_ONCE_ADAM) &&
        Bt(&sys_run_level,RLf_ONCE_USER) &&
        sys_msg_flags[0]==100 && RegCnt("Once/Adam")==2 &&
        RegCnt("Once/User")==2) res|=64;
    AOnce("sys_msg_flags[0]=999;");
    Once("sys_msg_flags[0]=999;");
    AOnceFlush;
    OnceFlush;
    OnceExe;
    if (sys_msg_flags[0]==100 && RegCnt("Once/Adam")==2 &&
        RegCnt("Once/User")==2) res|=128;
    sys_registry_doc=registry;
    sys_msg_flags[0]=old_flags;
    sys_run_level=old_level;
    DocDel(doc);
    Del("B:/OnceProbe.HC.Z",,,FALSE);
  }
  "Once probe %X\n",res;
}

OnceProbe;
