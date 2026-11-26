#include "../KernelB.hpp"

extern "C" uint64_t YorNCpp();

// Only an already mounted primary ATA partition can be reformatted here.
// Validate its original extent and retain all unrelated MBR bytes.
bool DrvTypeSet(CDrv &dv, uint8_t type, bool write)
{
  if (!dv.bd || dv.bd->type!=BDT_ATA || (dv.bd->flags&BDF_READ_ONLY) ||
      dv.drv_offset<=0 || dv.size<=0)
    return false;
  uint8_t mbr[BLK_SIZE];
  if (!BlkDevReadBytes(*dv.bd,mbr,0,sizeof(mbr)) || mbr[510]!=0x55 || mbr[511]!=0xAA)
    return false;
  int64_t num=0;
  for (unsigned i=0;i<4;i++) {
    auto *p=mbr+446+16*i;
    if (!p[4]) continue;
    uint32_t offset,size;
    MemCpy(&offset,p+8,4);
    MemCpy(&size,p+12,4);
    if (num++!=dv.prt_num) continue;
    if ((p[4]!=0x0B && p[4]!=0x0C && p[4]!=0x1B && p[4]!=0x1C) ||
        offset!=uint64_t(dv.drv_offset) || size!=uint64_t(dv.size) ||
        uint64_t(offset)+size>uint64_t(dv.bd->max_blk)+1)
      return false;
    for (unsigned j=0;j<4;j++) {
      if (j==i) continue;
      const auto *q=mbr+446+16*j;
      if (!q[4]) continue;
      uint32_t other,count;
      MemCpy(&other,q+8,4);
      MemCpy(&count,q+12,4);
      if (count && other<uint64_t(offset)+size && offset<uint64_t(other)+count)
        return false;
    }
    if (!write) return true;
    p[0]=0x80;
    p[4]=type;
    return BlkDevWriteBytes(*dv.bd,mbr,0,sizeof(mbr));
  }
  return false;
}

extern "C" void FmtCpp(uint8_t drv_let, uint8_t quick, uint8_t confirm, int64_t type)
{
  auto *dv=Let2DrvCpp(drv_let,false);
  if (confirm) {
    PutS("ARE YOU SURE ");
    if (!YorNCpp()) return;
  }
  if (!dv || !dv->bd) {
    PutS("Invalid Drive\n");
    return;
  }
  if (dv->bd->flags&BDF_READ_ONLY) {
    PutS("Drive Read Only\n");
    return;
  }
  bool res=false;
  if (type==FSt_FAT32 && dv->bd->type==BDT_ATA)
    res=FAT32Fmt(*dv,quick);
  else if (dv->bd->type==BDT_RAM && (type==FSt_FAT32 || type==FSt_REDSEA)) {
    if (type==FSt_FAT32) PutS("Using RedSea File System.\n");
    CMemLock lock(dv->locked_flags);
    ResidentDriveClear(dv->drv_let);
    auto root=dv->bd->init_root_dir_blks;
    res=(quick || BlkWriteZero(*dv,dv->drv_offset,dv->size)) &&
        RedSeaFormatRAM(*dv,root>0?root:1);
  } else {
    PutS("File System Not Supported\n");
    return;
  }
  if (!res) {
    PutS("Format Failed\n");
    return;
  }
  char dir[]{char(dv->drv_let),':','/',0};
  CdCpp(dir,false);
}
