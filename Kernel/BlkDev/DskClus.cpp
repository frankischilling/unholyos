#include "../KernelB.hpp"

namespace {
constexpr uint64_t DRV_EXCEPT = 0x767244;
constexpr uint64_t FILE_EXCEPT = 0x656C6946;
constexpr int64_t FAT32_EOC = 0x0FFFFFF8;
}

extern "C" int64_t ClusNumNextCpp(CDrv *dv,int64_t c,int64_t cnt)
{
  DrvChkCpp(dv,true);
  if (cnt<=0) return c;
  switch (dv->fs_type) {
    case FSt_REDSEA:
      return int64_t(uint64_t(c)+uint64_t(cnt));
    case FSt_FAT32:
      while (cnt-->0 && 0<c && c<FAT32_EOC) {
        uint32_t next;
        if (!FAT32GetClusLink(*dv,c,next))
          RaiseHolyC(DRV_EXCEPT);
        c=next;
      }
      return 0<c && c<FAT32_EOC ? c : int64_t(INVALID_CLUS);
    default:
      RaiseHolyC(DRV_EXCEPT);
  }
}

extern "C" int64_t Clus2BlkCpp(CDrv *dv,int64_t c)
{
  DrvChkCpp(dv,true);
  switch (dv->fs_type) {
    case FSt_REDSEA:
      return c;
    case FSt_FAT32:
      return int64_t(uint64_t(dv->data_area)+uint64_t(c)*dv->spc);
    default:
      RaiseHolyC(DRV_EXCEPT);
  }
}

extern "C" int64_t ClusBlkReadCpp(CDrv *dv,uint8_t *buf,int64_t c,int64_t blks)
{
  DrvChkCpp(dv,true);
  if (blks<=0) return c;
  switch (dv->fs_type) {
    case FSt_REDSEA:
      if (!BlkReadCpp(dv,buf,c,blks)) RaiseHolyC(DRV_EXCEPT);
      return int64_t(uint64_t(c)+uint64_t(blks));
    case FSt_FAT32:
      if (dv->spc<=0) RaiseHolyC(DRV_EXCEPT);
      while (blks && 0<c && c<FAT32_EOC) {
        int64_t i=blks<dv->spc ? blks : dv->spc;
        if (!BlkReadCpp(dv,buf,Clus2BlkCpp(dv,c),i))
          RaiseHolyC(DRV_EXCEPT);
        buf+=i*BLK_SIZE;
        c=ClusNumNextCpp(dv,c,1);
        blks-=i;
      }
      if (blks) RaiseHolyC(DRV_EXCEPT);
      return c;
    default:
      RaiseHolyC(DRV_EXCEPT);
  }
}

extern "C" int64_t ClusReadCpp(CDrv *dv,uint8_t *buf,int64_t c,int64_t cnt)
{
  DrvChkCpp(dv,true);
  return ClusBlkReadCpp(dv,buf,c,int64_t(uint64_t(cnt)*dv->spc));
}

extern "C" int64_t ClusBlkWriteCpp(CDrv *dv,const uint8_t *buf,int64_t c,
                                   int64_t blks)
{
  DrvChkCpp(dv,true);
  if (blks<=0) return c;
  switch (dv->fs_type) {
    case FSt_REDSEA:
      if (!BlkWriteCpp(dv,buf,c,blks)) RaiseHolyC(DRV_EXCEPT);
      return 0;
    case FSt_FAT32:
      if (dv->spc<=0) RaiseHolyC(DRV_EXCEPT);
      while (blks) {
        if (!(0<c && c<FAT32_EOC)) RaiseHolyC(DRV_EXCEPT);
        int64_t i=blks<dv->spc ? blks : dv->spc;
        if (!BlkWriteCpp(dv,buf,Clus2BlkCpp(dv,c),i))
          RaiseHolyC(DRV_EXCEPT);
        buf+=i*BLK_SIZE;
        c=ClusNumNextCpp(dv,c,1);
        blks-=i;
      }
      return c;
    default:
      RaiseHolyC(DRV_EXCEPT);
  }
}

extern "C" int64_t ClusWriteCpp(CDrv *dv,const uint8_t *buf,int64_t c,
                                int64_t cnt)
{
  DrvChkCpp(dv,true);
  return ClusBlkWriteCpp(dv,buf,c,int64_t(uint64_t(cnt)*dv->spc));
}

extern "C" int64_t ClusAllocCpp(CDrv *dv,int64_t c,int64_t cnt,
                                uint8_t contiguous)
{
  DrvChkCpp(dv,true);
  if (cnt<=0) return c;
  switch (dv->fs_type) {
    case FSt_REDSEA:
      return RedSeaAllocClusCpp(dv,cnt);
    case FSt_FAT32:
      if (contiguous) {
        if (c) RaiseHolyC(FILE_EXCEPT);
        return FAT32AllocContiguousClusCpp(dv,cnt);
      }
      return FAT32AllocClusCpp(dv,c,cnt);
    default:
      RaiseHolyC(DRV_EXCEPT);
  }
}
