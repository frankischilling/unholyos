I64 ins_reg_probe_key=CH_ESC;

U0 InsRegProbeInput(CTask *owner)
{
  CTask *popup;
  do {
    Yield;
    popup=owner->popup_task;
  } while (!popup || !Bt(&popup->task_flags,TASKf_AWAITING_MSG));
  TaskMsg(popup,NULL,MSG_KEY_DOWN,ins_reg_probe_key,0,0);
}

U0 InsRegProbe()
{
  I64 res=0;
  CHashSrcSym *num;
  if (sizeof(CInsReg)==33 && offset(CInsReg.registered)==0 &&
      offset(CInsReg.host_os)==1 && offset(CInsReg.host_vm)==9 &&
      offset(CInsReg.initials)==17 && offset(CInsReg.machine_num)==25)
    res|=1;
  if (!ins_reg.registered && RegCnt("Adam/InsReg")>2 &&
      DefineCnt("ST_HOST_OSES")==4 && DefineCnt("ST_HOST_VMS")==5 &&
      !StrCmp(DefineSub(OS_LINUX,"ST_HOST_OSES"),"Linux") &&
      !StrCmp(DefineSub(VM_QEMU,"ST_HOST_VMS"),"QEMU")) res|=2;
  if (Spawn(&InsRegProbeInput,Fs) && InsReg && ins_reg.registered &&
      !ins_reg.host_os && !ins_reg.host_vm && !*ins_reg.initials &&
      !ins_reg.machine_num) res|=4;
  if (!InsReg && ins_reg.registered) res|=8;
  num=HashFind("INS_REG_MACHINE_NUM",Fs->hash_table,HTT_DEFINE_STR);
  if (num && num->type&HTF_PUBLIC && num->idx &&
      !StrCmp(num->idx,"Registry/Install Registration")) res|=16;
  ins_reg_probe_key=CH_SHIFT_ESC;
  if (Spawn(&InsRegProbeInput,Fs) && !InsRereg && ins_reg.registered) res|=32;
  if (InsReg(,RR_UNREGISTER) && !ins_reg.registered &&
      !ins_reg.host_os && !ins_reg.host_vm && !*ins_reg.initials &&
      !ins_reg.machine_num && InsUnreg) res|=64;
  "InsReg probe %X\n",res;
}
InsRegProbe;
