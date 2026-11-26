#include "../KernelB.hpp"

constexpr uint32_t FAT32_EOC=0x0FFFFFF8, FAT32_RESERVED=0x0FFFFFF0;

static uint64_t FAT32ClusLimit(const CDrv &dv)
{
  return (uint64_t(dv.drv_offset)+dv.size-dv.data_area)/dv.spc;
}

static bool FAT32ValidClus(const CDrv &dv,uint64_t c)
{
  return c>=2 && c<FAT32_RESERVED && c<FAT32ClusLimit(dv);
}

static bool FAT32Next(const CDrv &dv,uint64_t c,uint32_t &next)
{
  if (!FAT32ValidClus(dv,c) ||
      !BlkDevReadBytes(*dv.bd,reinterpret_cast<uint8_t *>(&next),
                       uint64_t(dv.fat1)*BLK_SIZE+c*4,4))
    return false;
  next&=0x0FFFFFFF;
  return next>=FAT32_EOC || FAT32ValidClus(dv,next);
}

bool FAT32Init(CDrv &dv)
{
  CFAT32Boot br;
  if (!dv.bd || dv.drv_offset<0 || dv.size<=0 ||
      uint64_t(dv.drv_offset)+dv.size>uint64_t(dv.bd->max_blk)+1 ||
      !BlkDevReadBytes(*dv.bd,reinterpret_cast<uint8_t *>(&br),
                       uint64_t(dv.drv_offset)*BLK_SIZE,sizeof(br)) ||
      br.signature!=0xAA55 || br.bytes_per_sect!=BLK_SIZE ||
      !br.sects_per_clus || br.sects_per_clus>128 ||
      (br.sects_per_clus & (br.sects_per_clus-1)) ||
      !br.reserved_sects || !br.sects_per_fat ||
      (br.copies_of_fat!=1 && br.copies_of_fat!=2) ||
      br.max_root_dir_entries || br.old_sects_per_fat || br.old_sects_in_drv ||
      br.version || !br.sects || br.sects>uint64_t(dv.size) ||
      ((br.flags & 0x80) && (br.flags & 15)))
    return false;
  uint64_t first=uint64_t(br.reserved_sects)+uint64_t(br.copies_of_fat)*br.sects_per_fat;
  if (first>=br.sects || first<2*uint64_t(br.sects_per_clus))
    return false;
  uint64_t clusters=(br.sects-first)/br.sects_per_clus;
  if (clusters<65525 || clusters>=FAT32_RESERVED-2 ||
      (clusters+2)*4>uint64_t(br.sects_per_fat)*BLK_SIZE ||
      br.root_clus<2 || br.root_clus>=clusters+2)
    return false;
  if (br.file_system_info_sect!=0xFFFF) {
    if (!br.file_system_info_sect || br.file_system_info_sect>=br.reserved_sects)
      return false;
    CFAT32FileInfoSect fis;
    if (!BlkDevReadBytes(*dv.bd,reinterpret_cast<uint8_t *>(&fis),
        (uint64_t(dv.drv_offset)+br.file_system_info_sect)*BLK_SIZE,sizeof(fis)) ||
        fis.signature1!=0x41615252 || fis.signature2!=0x61417272 ||
        fis.signature3!=0xAA550000)
      return false;
  }
  dv.size=br.sects;
  dv.fat1=dv.drv_offset+br.reserved_sects;
  dv.fat2=dv.fat1+(br.copies_of_fat==2?br.sects_per_fat:0);
  dv.data_area=dv.drv_offset+first-2*br.sects_per_clus;
  dv.spc=br.sects_per_clus;
  dv.root_clus=br.root_clus;
  dv.file_system_info_sect=br.file_system_info_sect==0xFFFF?0:
                           dv.drv_offset+br.file_system_info_sect;
  uint32_t next;
  if (!FAT32Next(dv,dv.root_clus,next))
    return false;
  dv.fs_type=FSt_FAT32;
  return true;
}

static bool FAT32ChainLength(const CDrv &dv,uint64_t c,uint64_t &count)
{
  count=0;
  uint32_t slow=c,fast=c;
  while (FAT32ValidClus(dv,slow) && count<FAT32ClusLimit(dv)) {
    count++;
    if (!FAT32Next(dv,slow,slow))
      return false;
    if (slow>=FAT32_EOC)
      return true;
    if (fast<FAT32_EOC && !FAT32Next(dv,fast,fast))
      return false;
    if (fast<FAT32_EOC && !FAT32Next(dv,fast,fast))
      return false;
    if (fast==slow)
      return false;
  }
  return false;
}

static bool FAT32ReadChain(const CDrv &dv,uint32_t c,uint64_t pos,
                            uint8_t *buf,size_t bytes)
{
  if (!bytes)
    return true;
  uint64_t count,clus_size=uint64_t(dv.spc)*BLK_SIZE;
  if (!FAT32ChainLength(dv,c,count) || pos>count*clus_size ||
      bytes>count*clus_size-pos)
    return false;
  for (uint64_t skip=pos/clus_size;skip;skip--)
    if (!FAT32Next(dv,c,c))
      return false;
  pos%=clus_size;
  while (bytes) {
    size_t n=clus_size-pos;
    if (n>bytes)
      n=bytes;
    if (!BlkDevReadBytes(*dv.bd,buf,(uint64_t(dv.data_area)+uint64_t(c)*dv.spc)*BLK_SIZE+pos,n))
      return false;
    bytes-=n;
    buf+=n;
    pos=0;
    if (bytes && !FAT32Next(dv,c,c))
      return false;
  }
  return true;
}

static CDate Dos2CDate(uint16_t t,uint16_t d)
{
  CDateStruct ds{};
  ds.day_of_mon=d&31;
  ds.mon=(d>>5)&15;
  ds.year=(d>>9)+1980;
  ds.sec=(t&31)*2;
  ds.min=(t>>5)&63;
  ds.hour=t>>11;
  if (!ds.day_of_mon || !ds.mon || ds.mon>12 || ds.sec>59 || ds.min>59 || ds.hour>23)
    return {};
  return Struct2Date(ds);
}

static bool FATFromName(char *dst,const uint8_t *src)
{
  int i=0,j=7,k=0;
  while (j>=0 && src[j]==' ')
    j--;
  for (;i<=j;i++)
    dst[k++]=src[i];
  j=10;
  while (j>=8 && src[j]==' ')
    j--;
  if (*src!='.' && j>=8)
    dst[k++]='.';
  for (i=8;i<=j;i++)
    dst[k++]=src[i];
  dst[k]=0;
  return FileNameChk(dst);
}

static uint8_t FATNameXSum(const uint8_t *src)
{
  uint8_t res=0;
  for (int i=0;i<11;i++)
    res=(res>>1)+(res&1?0x80:0)+src[i];
  return res;
}

struct CFATLongName
{
  char name[CDIR_FILENAME_LEN]{};
  uint8_t next=0,xsum=0;
  bool valid=false;
};

static void DirLongNameFill(CFATLongName &long_name,const CFAT32DirEntryLong &de)
{
  unsigned ord=de.ord&0x3F;
  if (de.ord&0x40) {
    long_name={};
    long_name.valid=ord>=1 && ord<=2;
    long_name.next=ord;
    long_name.xsum=de.xsum;
  }
  if (!long_name.valid || ord!=long_name.next || de.type || de.zero ||
      de.xsum!=long_name.xsum || (de.ord&0x80)) {
    long_name={};
    return;
  }
  constexpr uint8_t offsets[]{1,3,5,7,9,14,16,18,20,22,24,28,30};
  auto *p=reinterpret_cast<const uint8_t *>(&de);
  for (unsigned i=0;i<13;i++) {
    uint16_t ch=p[offsets[i]] | uint16_t(p[offsets[i]+1])<<8;
    size_t index=(ord-1)*13+i;
    if (!ch || ch==0xFFFF)
      break;
    if (ch>127 || index>=25) {
      long_name={};
      return;
    }
    long_name.name[index]=ch;
  }
  long_name.next--;
}

static bool FAT32CDirFill(CRedSeaDirEntry &entry,const CFAT32DirEntry &de,
                           const CFATLongName &long_name,const CDrv &dv)
{
  entry={};
  if (long_name.valid && !long_name.next && long_name.name[0] &&
      long_name.xsum==FATNameXSum(de.name) && FileNameChk(long_name.name))
    MemCpy(entry.name,long_name.name,sizeof(entry.name));
  else if (!FATFromName(entry.name,de.name))
    return false;
  entry.clus=(uint32_t(de.clus_hi)<<16 | de.clus_lo)&0x0FFFFFFF;
  entry.size=de.size;
  entry.attr=de.attr;
  entry.datetime=DateBits(Dos2CDate(de.WrtTime,de.WrtDate))-uint64_t(dv.fat32_local_time_offset);
  return true;
}

// Directory records may cross a sector or cluster boundary in a long name.
template<class Visit>
static bool FAT32Walk(const CDrv &dv,uint32_t clus,Visit visit,uint64_t &dir_size)
{
  uint64_t count;
  dir_size=0;
  if (!FAT32ChainLength(dv,clus,count))
    return false;
  CFATLongName long_name;
  CFAT32DirEntry entries[BLK_SIZE/sizeof(CFAT32DirEntry)];
  for (uint64_t i=0;i<count;i++) {
    dir_size+=dv.spc*BLK_SIZE;
    for (int64_t sector=0;sector<dv.spc;sector++) {
      if (!BlkDevReadBytes(*dv.bd,reinterpret_cast<uint8_t *>(entries),
          (uint64_t(dv.data_area)+uint64_t(clus)*dv.spc+sector)*BLK_SIZE,BLK_SIZE))
        return false;
      for (const auto &de:entries) {
        if (!de.name[0])
          return true;
        if (de.name[0]==0xE5) {
          long_name={};
          continue;
        }
        if ((de.attr&0x3F)==0x0F) {
          CFAT32DirEntryLong ld;
          MemCpy(&ld,&de,sizeof(ld));
          DirLongNameFill(long_name,ld);
          continue;
        }
        CRedSeaDirEntry entry;
        bool valid=!(de.attr&8) && FAT32CDirFill(entry,de,long_name,dv);
        long_name={};
        if (valid) {
          int res=visit(entry);
          if (res)
            return res>0;
        }
      }
    }
    if (i+1<count && !FAT32Next(dv,clus,clus))
      return false;
  }
  return true;
}

bool FAT32FileFind(const CDrv &dv,uint64_t dir,const char *name,
                   bool directory,CRedSeaDirEntry &entry)
{
  entry={};
  if (dv.fs_type!=FSt_FAT32 || !FileNameChk(name) || !FAT32ValidClus(dv,dir))
    return false;
  bool found=false;
  uint64_t size;
  return FAT32Walk(dv,dir,[&](const CRedSeaDirEntry &de) {
    if (bool(de.attr&RS_ATTR_DIR)==directory &&
        !StrCmpCpp(reinterpret_cast<const uint8_t *>(name),
                   reinterpret_cast<const uint8_t *>(de.name))) {
      entry=de;
      found=true;
      return 1;
    }
    return 0;
  },size) && found;
}

bool FAT32FileRead(const CDrv &dv,const CRedSeaDirEntry &entry,uint64_t pos,
                   uint8_t *buf,size_t bytes)
{
  return dv.fs_type==FSt_FAT32 && !(entry.attr&RS_ATTR_DIR) &&
         pos<=entry.size && bytes<=entry.size-pos &&
         FAT32ReadChain(dv,entry.clus,pos,buf,bytes);
}

bool FAT32ReadBlocks(CFile &f,uint8_t *buf,uint64_t blk,uint64_t cnt)
{
  const auto &dv=*f.dv;
  return FAT32ReadChain(dv,f.de.clus,blk*BLK_SIZE,buf,cnt*BLK_SIZE);
}

bool FAT32FilesFind(CDrv &dv,uint64_t clus,const char *directory,const char *mask,
                    uint64_t flags,CDirEntry *parent,CDirEntry *&res,uint64_t &dir_size)
{
  if (!FAT32ValidClus(dv,clus))
    return false;
  return FAT32Walk(dv,clus,[&](const CRedSeaDirEntry &de) {
    bool dir=de.attr&RS_ATTR_DIR,recurse=dir && (flags&FUF_RECURSE);
    if ((recurse && de.name[0]=='.') || (!dir && (flags&FUF_JUST_DIRS)))
      return 0;
    auto *name=DirEntryFullName(directory,de.name);
    if (!name)
      return -1;
    if (!recurse && !FilesFindMatch(name,mask,flags)) {
      Free(name);
      return 0;
    }
    auto *entry=reinterpret_cast<CDirEntry *>(CAlloc(sizeof(CDirEntry),CurrentTask()->data_heap));
    if (!entry) {
      Free(name);
      return -1;
    }
    MemCpy(&entry->attr,&de,sizeof(de));
    entry->full_name=name;
    entry->parent=parent;
    entry->next=res;
    res=entry;
    if (recurse) {
      bool valid=de.clus!=clus && de.clus!=uint64_t(dv.root_clus);
      unsigned depth=0;
      for (auto *p=parent;valid && p;p=p->parent)
        valid=uint64_t(p->clus)!=de.clus && ++depth<64;
      uint64_t size;
      if (!valid || !FAT32FilesFind(dv,de.clus,name,mask,flags,entry,entry->sub,size))
        return -1;
      entry->size=size;
    }
    return 0;
  },dir_size);
}
