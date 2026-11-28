#include "::/Demo/RegistryDemo.HC"

public U0 RegistryProbe()
{
  I64 res=0,old_flags=sys_msg_flags[0];
  CBlkDev *bd;
  CDoc *registry=sys_registry_doc,*drive_doc;
  if (registry && registry_version==sys_os_version &&
      RegCnt("Adam/SysRegVer")>2) res|=1;
  if (!RegDft("RegistryProbe/Empty","") &&
      RegDft("RegistryProbe/Empty","98;\n") &&
      RegCnt("RegistryProbe/Empty")==2 &&
      !RegCnt("RegistryProbe/Unknown")) res|=2;
  if (!RegWrite("RegistryProbe/Value","%d+%d;\n",10,3) &&
      RegExe("RegistryProbe/Value")==13 &&
      RegAppend("RegistryProbe/Value","%d*%d;\n",7,9) &&
      RegExe("RegistryProbe/Value")==63 &&
      RegWrite("RegistryProbe/Value","%d;\n",17) &&
      RegExe("RegistryProbe/Value")==17) res|=4;
  if (Adam("%d+%d;",12,13)==25 &&
      !RegDft("RegistryProbe/Adam",
        "if (Fs==adam_task) sys_msg_flags[0]=71;\n",TRUE) &&
      sys_msg_flags[0]==71 &&
      RegDft("RegistryProbe/Adam","sys_msg_flags[0]=99;\n",TRUE) &&
      sys_msg_flags[0]==71) res|=8;
  sys_msg_flags[0]=old_flags;
  if (RegExe("DemoCompany/Game1/SimpleVal")==1235) res|=16;
  if (registry && !DrvIsWritable(*registry->filename.name)) res|=32;
  if (!Let2Drv('B',FALSE)) {
    bd=BlkDevNextFreeSlot('B',BDT_RAM);
    bd->max_blk=511;
    BlkDevAdd(bd,,FALSE,TRUE);
  }
  if (DrvIsWritable('B')) {
    Del("B:/RegistryProbe.HC.Z",,,FALSE);
    drive_doc=DocNew("B:/RegistryProbe.HC.Z");
    sys_registry_doc=drive_doc;
    if (!RegDft("Disk/Value","11;\n") &&
        DocTreeFExe("B:/RegistryProbe.HC.Z","Disk/Value")==11 &&
        RegWrite("Disk/Value","%d;\n",29) &&
        RegAppend("Disk/Value","%d;\n",43) &&
        DocTreeFExe("B:/RegistryProbe.HC.Z","Disk/Value")==43) res|=64;
    sys_registry_doc=registry;
    DocDel(drive_doc);
    Del("B:/RegistryProbe.HC.Z",,,FALSE);
  }
  "Registry probe %X flags %d\n",res,old_flags;
}

RegistryProbe;
