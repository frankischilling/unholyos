#include "T:/Kernel/KernelA.HH"

_import _MALLOC U8 *ProbeWriteAlloc(I64 size,CTask *task);
_import _FREE U0 ProbeWriteFree(U8 *ptr);
_import _STRCPY U0 ProbeWriteCopy(U8 *dst,U8 *src);
_import _STRCMP I64 ProbeWriteCmp(U8 *a,U8 *b);
import CBlkDev *BlkDevNextFreeSlot(U8 letter,I64 type);
import I64 BlkDevAdd(CBlkDev *bd,I64 part,Bool whole,Bool make_free);
import CDrv *Let2Drv(U8 letter,Bool except);
import Bool BlkRead(CDrv *dv,U8 *buf,I64 blk,I64 count);
import Bool BlkWrite(CDrv *dv,U8 *buf,I64 blk,I64 count);
import U8 *FileRead(U8 *name,I64 *size,I64 *attr);
import I64 FileWrite(U8 *name,U8 *data,I64 size,CDate date,I64 attr);
import U8 *DirCur(CTask *task,CTask *mem_task);

class CProbeWriteEntry
{
  U16 attr;
  U8 name[38];
  I64 clus,size,datetime;
};

CDrv *ProbeWriteMount()
{
  CBlkDev *bd=BlkDevNextFreeSlot('B',BDT_RAM);
  if (!bd || bd->type!=BDT_RAM || bd->blk_size!=BLK_SIZE)
    return NULL;
  bd->max_blk=2047;
  if (BlkDevAdd(bd,I64_MIN,FALSE,TRUE)!=1)
    return NULL;
  CDrv *dv=Let2Drv('b',FALSE);
  if (!dv || dv->bd!=bd || dv->fs_type!=FSt_REDSEA || dv->drv_let!='B')
    return NULL;
  return dv;
}

Bool ProbeWriteMedia(CDrv *dv)
{
  CRedSeaBoot boot;
  U8 buf[BLK_SIZE];
  CProbeWriteEntry *dir=buf;
  if (!BlkRead(dv,&boot,0,1) || boot.signature!=MBR_PT_REDSEA ||
      boot.signature2!=0xAA55 || boot.sects!=dv->size ||
      boot.bitmap_sects!=1 || boot.root_clus!=2 || !boot.unique_id ||
      !BlkRead(dv,buf,dv->root_clus,1) ||
      ProbeWriteCmp(dir[0].name,".") || ProbeWriteCmp(dir[1].name,"..") ||
      dir[0].size!=BLK_SIZE || !dir[0].datetime ||
      !BlkRead(dv,buf,1,1) || buf[0]!=3)
    return FALSE;
  buf[0]=0x3C;
  buf[511]=0xDA;
  if (!BlkWrite(dv,buf,dv->bd->max_blk,1))
    return FALSE;
  buf[0]=buf[511]=0;
  return BlkRead(dv,buf,dv->bd->max_blk,1) && buf[0]==0x3C && buf[511]==0xDA;
}

Bool ProbeWriteRead(U8 *name,U8 *src,I64 len,I64 flags)
{
  I64 size,attr,i;
  U8 *data=FileRead(name,&size,&attr);
  Bool ok=data && size==len && !data[size] && (attr & flags)==flags;
  for (i=0;ok && i<size;i++)
    ok=data[i]==src[i];
  ProbeWriteFree(data);
  return ok;
}

Bool ProbeWritePlain(U8 *buf)
{
  I64 i;
  for (i=0;i<1300;i++)
    buf[i]=(i*43)^0xAC;
  if (FileWrite("b:/Build/Units/Source.HC",buf,1300,0xB4A1C33333333,0)<=0 ||
      !ProbeWriteRead("B:/Build/Units/Source.HC",buf,1300,RS_ATTR_CONTIGUOUS))
    return FALSE;
  buf[0]=0x96;
  I64 clus=FileWrite("B:/Build/Units/Source.HC",buf,619,0,0);
  return clus>0 &&
         ProbeWriteRead("B:/Build/Units/Source.HC",buf,619,RS_ATTR_CONTIGUOUS) &&
         FileWrite("B:/Blank.HC",buf,0,0,0)==INVALID_CLUS &&
         ProbeWriteRead("B:/Blank.HC",buf,0,RS_ATTR_CONTIGUOUS);
}

Bool ProbeWriteArchive(CDrv *dv,U8 *buf)
{
  I64 size,i;
  U8 *src=FileRead("T:/StartOS.HC",&size,NULL);
  if (!src || size!=1158)
    return FALSE;
  I64 clus=FileWrite("B:/Source.HC.Z",src,size,0,0);
  Bool ok=clus>0 &&
          ProbeWriteRead("B:/Source.HC",src,size,RS_ATTR_COMPRESSED|RS_ATTR_CONTIGUOUS) &&
          BlkRead(dv,buf,clus,2);
  if (ok) {
    CArcCompress *header=buf;
    U64 hash=14695981039346656037;
    for (i=0;i<header->compressed_size && i<1024;i++)
      hash=(hash ^ buf[i])*1099511628211;
    ok=header->expanded_size==1158 && header->compression_type==CT_7_BIT &&
       header->compressed_size==889 && hash==0x0A1353327EF5DBE6;
  }
  ProbeWriteFree(src);
  return ok;
}

Bool ProbeWriteGrowth(CDrv *dv,U8 *buf)
{
  U8 name[40];
  I64 i,root=dv->root_clus;
  ProbeWriteCopy(name,"B:/RootA.HC");
  for (i=0;i<9;i++) {
    name[7]='A'+i;
    buf[0]=i;
    if (FileWrite(name,buf,1,0,0)<=0)
      return FALSE;
  }
  if (dv->root_clus==root)
    return FALSE;
  ProbeWriteCopy(name,"B:/Build/Units/ItemA.HC");
  for (i=0;i<10;i++) {
    name[19]='A'+i;
    buf[0]=i+2;
    if (FileWrite(name,buf,1,0,0)<=0)
      return FALSE;
  }
  buf[0]=11;
  return ProbeWriteRead("B:/Build/Units/ItemJ.HC",buf,1,RS_ATTR_CONTIGUOUS);
}

Bool ProbeWriteCache(U8 *buf)
{
  buf[0]=3;
  if (FileWrite("B:/Resident.HC",buf,1,0,RS_ATTR_RESIDENT)<=0 ||
      !ProbeWriteRead("B:/Resident.HC",buf,1,0))
    return FALSE;
  buf[0]=5;
  return FileWrite("B:/Resident.HC",buf,1,0,0)>0 &&
         ProbeWriteRead("B:/Resident.HC",buf,1,RS_ATTR_CONTIGUOUS);
}

I64 ProbeWriteAll()
{
  CDrv *dv=ProbeWriteMount();
  if (!dv)
    return 0;
  U8 *buf=ProbeWriteAlloc(2048,NULL);
  I64 result=1+2*ProbeWriteMedia(dv)+4*ProbeWritePlain(buf)
    +8*ProbeWriteArchive(dv,buf)+16*ProbeWriteGrowth(dv,buf)+32*ProbeWriteCache(buf);
  U8 *dir=DirCur(NULL,NULL);
  if (dir && !ProbeWriteCmp(dir,"T:/Home"))
    result+=64;
  ProbeWriteFree(dir);
  ProbeWriteFree(buf);
  return result;
}

public I64 FileWriteProbeResult;
FileWriteProbeResult=ProbeWriteAll();
