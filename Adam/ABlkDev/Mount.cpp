#include "../../Kernel/KernelB.hpp"
#include "../Opt/Boot/DskISORedSea.hpp"

extern "C" void UnmountCpp(uint8_t drv_let)
{
  BlkDevDelCpp(Let2BlkDevCpp(drv_let,true));
}

extern "C" uint64_t MountFileCpp(const char *filename)
{
  if (!filename || !adam_task)
    return 0;
  auto *with_ext=ExtDftCpp(filename,"ISO.C");
  auto *absolute=with_ext?FileNameAbs(with_ext,*CurrentTask()):nullptr;
  Free(with_ext);
  auto *name=absolute?StrNew(absolute,adam_task->data_heap):nullptr;
  Free(absolute);
  if (!name)
    return 0;
  auto *bd=BlkDevNextFreeSlot('M',BDT_ISO_FILE_READ);
  if (!bd) {
    Free(name);
    return 0;
  }
  // System area, volume descriptors, boot catalog and stage-one loader.
  bd->drv_offset=ISO_DRV_OFFSET;
  bd->file_dsk_name=reinterpret_cast<uint8_t *>(name);
  if (BlkDevAddCpp(bd,INT64_MIN,true,false)==1)
    return bd->first_drv_let;
  {
    CMemLock mount(mount_flags);
    if (bd->bd_signature!=BD_SIGNATURE_VAL &&
        bd->file_dsk_name==reinterpret_cast<uint8_t *>(name))
      MemSet(bd,0,sizeof(*bd));
  }
  Free(name);
  return 0;
}
