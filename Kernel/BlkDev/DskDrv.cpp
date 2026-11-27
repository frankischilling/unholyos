#include "../KernelB.hpp"

extern "C" void PrintCpp(const char *fmt,int64_t argc,const uint64_t *argv);
extern "C" void PrintErrCpp(const char *fmt,int64_t argc,const uint64_t *argv,
                           const uintptr_t *frame);

bool DrvLockOwner(CDrv *dv)
{
  auto *task=CurrentTask();
  for (;;) {
    {
      CMemLock lock(disk_lock_flags);
      if (!(dv->locked_flags&1)) {
        dv->locked_flags|=1;
        dv->owning_task=task;
        return true;
      }
      if (dv->owning_task==task) return false;
    }
    _YIELD();
  }
}

bool DrvUnlockOwner(CDrv *dv)
{
  CMemLock lock(disk_lock_flags);
  if (!(dv->locked_flags&1) || dv->owning_task!=CurrentTask())
    return false;
  dv->owning_task=nullptr;
  dv->locked_flags&=~uint64_t(1);
  return true;
}

extern "C" uint64_t DrvLockCpp(CDrv *dv)
{
  DrvChkCpp(dv,true);
  BlkDevLockCpp(dv->bd);
  return DrvLockOwner(dv);
}

extern "C" uint64_t DrvUnlockCpp(CDrv *dv,uint8_t rst)
{
  DrvChkCpp(dv,true);
  if (!DrvUnlockOwner(dv)) return false;
  if (auto *bd=BlkDevLockTarget(dv->bd)) BlkDevUnlockOwner(bd,rst);
  _YIELD();
  return true;
}

extern "C" void DrvsReleaseCpp()
{
  if (!blkdev.drvs) return;
  for (size_t i=0;i<DRVS_NUM;i++) {
    auto *dv=&blkdev.drvs[i];
    if (dv->dv_signature==DRV_SIGNATURE_VAL && DrvUnlockOwner(dv)) {
      if (auto *bd=BlkDevLockTarget(dv->bd)) BlkDevUnlockOwner(bd,true);
    }
  }
}

extern "C" CDrv *DrvMakeFreeSlotCpp(uint8_t drv_let)
{
  int64_t i=Let2LetCpp(drv_let)-'A';
  if (!(0<=i && i<static_cast<int64_t>(DRVS_NUM)))
    RaiseHolyC(0x767244); // Drv
  auto *res=&blkdev.drvs[i];
  if (res->locked_flags&1) RaiseHolyC(0x767244);
  DskCacheInvalidate2Cpp(res);
  MemSet(res,0,sizeof(CDrv));
  res->drv_let='A'+i;
  return res;
}

extern "C" uint64_t DrvNextFreeLetCpp(uint8_t first_drv_let)
{
  int64_t i=Let2LetCpp(first_drv_let)-'A',
          type=Let2BlkDevTypeCpp(first_drv_let);
  if (0<=i && i<static_cast<int64_t>(DRVS_NUM))
    do {
      if (blkdev.drvs[i].dv_signature!=DRV_SIGNATURE_VAL) {
        if (Let2BlkDevTypeCpp(i+'A')!=type)
          break;
        return i+'A';
      }
    } while (++i<static_cast<int64_t>(DRVS_NUM));
  RaiseHolyC(0x767244); // Drv
}

extern "C" CDrv *DrvChkCpp(CDrv *dv,uint8_t except)
{
  if (!dv || dv->dv_signature!=DRV_SIGNATURE_VAL) {
    if (except)
      RaiseHolyC(0x767244); // Drv
    return nullptr;
  }
  return dv;
}

extern "C" uint64_t Drv2LetCpp(CDrv *dv)
{
  if (!dv)
    dv=CurrentTask()->cur_dv;
  return DrvChkCpp(dv,true)->drv_let;
}

extern "C" uint64_t Let2LetCpp(uint8_t drv_let)
{
  if (!drv_let)
    drv_let=Drv2LetCpp(nullptr);
  else if (drv_let==':')
    drv_let=blkdev.boot_drv_let;
  else if (drv_let=='~')
    drv_let=*blkdev.home_dir;
  if ('a'<=drv_let && drv_let<='z')
    drv_let-='a'-'A';
  return drv_let;
}

extern "C" int64_t Let2BlkDevTypeCpp(uint8_t drv_let)
{
  drv_let=Let2LetCpp(drv_let);
  if ('A'<=drv_let && drv_let<='B')
    return BDT_RAM;
  if ('C'<=drv_let && drv_let<='L')
    return BDT_ATA;
  if ('M'<=drv_let && drv_let<='P')
    return BDT_ISO_FILE_READ;
  if ('Q'<=drv_let && drv_let<='S')
    return BDT_ISO_FILE_WRITE;
  if ('T'<=drv_let && drv_let<='Z')
    return BDT_ATAPI;
  return BDT_NULL;
}

CDrv *MountedDrive(char letter)
{
  auto *dv='A'<=letter && letter<='Z' ? blkdev.let_to_drv[letter-'A'] :
                                      nullptr;
  return DrvChkCpp(dv,false);
}

extern "C" CDrv *Let2DrvCpp(uint8_t drv_let,uint8_t except)
{
  auto *dv=drv_let ? MountedDrive(Let2LetCpp(drv_let)) :
                     CurrentTask()->cur_dv;
  return DrvChkCpp(dv,except);
}

extern "C" CBlkDev *DrvIsWritableCpp(uint8_t drv_let,uint8_t except)
{
  auto *bd=Let2BlkDevCpp(drv_let,except);
  if (!bd || bd->flags&BDF_READ_ONLY) {
    if (except)
      RaiseHolyC(0x767244); // Drv
    return nullptr;
  }
  return bd;
}

extern "C" uint64_t DrvMapCpp(uint8_t drv_let,CDrv *dv)
{
  drv_let=Let2LetCpp(drv_let);
  if ('A'<=drv_let && drv_let<='Z') {
    blkdev.let_to_drv[drv_let-'A']=dv;
    dv->drv_let=drv_let;
    return true;
  }
  return false;
}

extern "C" uint64_t DrvCpp(uint8_t drv_let)
{
  auto *dv=Let2DrvCpp(drv_let,true);
  BlkDevChkCpp(dv->bd,true);
  auto *task=CurrentTask();
  auto *dir=StrNew("/",task->data_heap);
  if (!dir)
    return false;
  task->cur_dv=dv;
  Free(task->cur_dir);
  task->cur_dir=dir;
  if (ReadableFS(*dv))
    return true;
  PrintErrCpp("File System Not Supported\n",0,nullptr,nullptr);
  return false;
}

static char *DrvIDString(uint8_t drv_let,size_t word,size_t cnt)
{
  auto *bd=Let2BlkDevCpp(drv_let,true);
  if (!bd->dev_id_record)
    return nullptr;
  auto *st=CAlloc(cnt*2+1,CurrentTask()->data_heap);
  if (!st)
    return nullptr;
  for (size_t i=0;i<cnt;i++) {
    auto val=bd->dev_id_record[word+i];
    st[i*2]=val>>8;
    st[i*2+1]=val;
  }
  auto *res=MStrUtilCpp(reinterpret_cast<char *>(st),
                       SUF_REM_LEADING|SUF_REM_TRAILING,0);
  Free(st);
  return res;
}

extern "C" char *DrvSerialNumCpp(uint8_t drv_let)
{
  return DrvIDString(drv_let,10,10);
}

extern "C" char *DrvModelNumCpp(uint8_t drv_let)
{
  return DrvIDString(drv_let,27,20);
}

extern "C" {
uint8_t blkdev_text_attr[6]={0,11,15,10,12,9};
uint8_t drv_text_attr[3]={0,1,4};
}

extern "C" uint64_t DrvTextAttrGetCpp(uint8_t drv_let)
{
  drv_let=Let2LetCpp(drv_let);
  if ('A'<=drv_let && drv_let<='Z')
    return blkdev_text_attr[Let2BlkDevTypeCpp(drv_let)]<<4 |
           drv_text_attr[drv_let%sizeof(drv_text_attr)];
  return 15;
}

extern "C" void DrvRepCpp()
{
  PutS("\nDefined Drives:\n");
  for (size_t i=0;i<DRVS_NUM;i++) {
    auto *dv=&blkdev.drvs[i];
    if (dv->dv_signature!=DRV_SIGNATURE_VAL)
      continue;
    auto *bd=dv->bd;
    auto drv_let=Drv2LetCpp(dv);
    auto attr=DrvTextAttrGetCpp(drv_let);
    const uint64_t args[]{
        attr&15,attr>>4,drv_let,
        uint64_t(dv->fs_type&0x7FFF),reinterpret_cast<uintptr_t>("ST_DRV_TYPES"),
        bd->type,reinterpret_cast<uintptr_t>("ST_BLKDEV_TYPES"),
        bd->base0,bd->base1,bd->unit};
    PrintCpp("$FG,%d$$BG,%d$%C %-8Z %-10Z %04X %04X %02X\n",10,args);
    if (auto *st=DrvModelNumCpp(drv_let)) {
      const uint64_t arg=reinterpret_cast<uintptr_t>(st);
      PrintCpp("   Model# :%s\n",1,&arg);
      Free(st);
    }
    if (auto *st=DrvSerialNumCpp(drv_let)) {
      const uint64_t arg=reinterpret_cast<uintptr_t>(st);
      PrintCpp("   Serial#:%s\n",1,&arg);
      Free(st);
    }
    if (bd->type==BDT_ISO_FILE_READ || bd->type==BDT_ISO_FILE_WRITE) {
      const uint64_t arg=reinterpret_cast<uintptr_t>(bd->file_dsk_name);
      PrintCpp("   File=\"%s\"\n",1,&arg);
    }
    const uint64_t range[]{uint64_t(dv->drv_offset),
                           uint64_t(dv->drv_offset)+uint64_t(dv->size)-1};
    PrintCpp("   %016X-%016X\n$FG$$BG$",2,range);
  }
  const uint64_t home=reinterpret_cast<uintptr_t>(blkdev.home_dir);
  PrintCpp("Home Dir:\"%s\"\n",1,&home);
}
