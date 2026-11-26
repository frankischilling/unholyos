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

static bool FAT32Link(const CDrv &dv,uint64_t c,uint32_t &next)
{
  if (!FAT32ValidClus(dv,c) ||
      !BlkDevReadBytes(*dv.bd,reinterpret_cast<uint8_t *>(&next),
                       uint64_t(dv.fat1)*BLK_SIZE+c*4,4))
    return false;
  next&=0x0FFFFFFF;
  return true;
}

static bool FAT32Next(const CDrv &dv,uint64_t c,uint32_t &next)
{
  if (!FAT32Link(dv,c,next))
    return false;
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

static bool FAT32Writable(const CDrv &dv)
{
  return dv.fs_type==FSt_FAT32 && dv.bd && dv.bd->type==BDT_ATA &&
         !(dv.bd->flags&BDF_READ_ONLY);
}

static uint64_t FAT32ClusPos(const CDrv &dv,uint32_t c)
{
  return (uint64_t(dv.data_area)+uint64_t(c)*dv.spc)*BLK_SIZE;
}

// Update both FAT copies, retaining the reserved high four bits of each.
// The caller holds the drive lock; no persistent FAT cache is used yet.
static bool FAT32SetLink(CDrv &dv,uint32_t c,uint32_t next)
{
  if (!FAT32ValidClus(dv,c) || (next && next<FAT32_EOC && !FAT32ValidClus(dv,next)))
    return false;
  uint64_t p1=uint64_t(dv.fat1)*BLK_SIZE+c*4,p2=uint64_t(dv.fat2)*BLK_SIZE+c*4;
  uint32_t old1,old2;
  if (!BlkDevReadBytes(*dv.bd,reinterpret_cast<uint8_t *>(&old1),p1,4))
    return false;
  old2=old1;
  if (p1!=p2 && !BlkDevReadBytes(*dv.bd,reinterpret_cast<uint8_t *>(&old2),p2,4))
    return false;
  uint32_t value1=(old1&0xF0000000)|next,value2=(old2&0xF0000000)|next;
  if (!BlkDevWriteBytes(*dv.bd,reinterpret_cast<const uint8_t *>(&value1),p1,4))
    return false;
  if (p1!=p2 && !BlkDevWriteBytes(*dv.bd,reinterpret_cast<const uint8_t *>(&value2),p2,4)) {
    BlkDevWriteBytes(*dv.bd,reinterpret_cast<const uint8_t *>(&old1),p1,4);
    return false;
  }
  return true;
}

static bool FAT32InfoRead(CDrv &dv,CFAT32FileInfoSect &fis)
{
  return dv.file_system_info_sect &&
         BlkDevReadBytes(*dv.bd,reinterpret_cast<uint8_t *>(&fis),
                         uint64_t(dv.file_system_info_sect)*BLK_SIZE,sizeof(fis)) &&
         fis.signature1==0x41615252 && fis.signature2==0x61417272 &&
         fis.signature3==0xAA550000;
}

static bool FAT32InfoWrite(CDrv &dv,CFAT32FileInfoSect &fis,uint32_t recent)
{
  if (!dv.file_system_info_sect)
    return true;
  fis.free_clus=UINT32_MAX;
  fis.most_recently_alloced=recent;
  return BlkDevWriteBytes(*dv.bd,reinterpret_cast<const uint8_t *>(&fis),
                          uint64_t(dv.file_system_info_sect)*BLK_SIZE,sizeof(fis));
}

// Scan once, then link the selected free clusters. This avoids publishing a
// partially allocated chain on an out-of-space failure.
static uint32_t FAT32AllocClus(CDrv &dv,uint32_t tail,uint64_t count,bool contiguous)
{
  uint64_t limit=FAT32ClusLimit(dv);
  if (!count || count>limit-2 || count>SIZE_MAX/sizeof(uint32_t) ||
      (contiguous && tail))
    return 0;
  uint32_t old_tail=0;
  if (tail && (!FAT32Next(dv,tail,old_tail) || old_tail<FAT32_EOC))
    return 0;
  CFAT32FileInfoSect fis{};
  if (dv.file_system_info_sect && !FAT32InfoRead(dv,fis))
    return 0;
  auto *chosen=reinterpret_cast<uint32_t *>(MAlloc(count*sizeof(uint32_t),CurrentTask()->data_heap));
  if (!chosen)
    return 0;
  uint32_t c=2,table[BLK_SIZE/4];
  if (!contiguous && FAT32ValidClus(dv,fis.most_recently_alloced))
    c=fis.most_recently_alloced+1;
  uint64_t found=0,cached=UINT64_MAX;
  for (uint64_t i=0;i<limit-2 && found<count;i++,c++) {
    if (c>=limit) {
      if (contiguous)
        break;
      c=2;
    }
    uint64_t block=c/(BLK_SIZE/4);
    if (block!=cached) {
      if (!BlkDevReadBytes(*dv.bd,reinterpret_cast<uint8_t *>(table),
                           (uint64_t(dv.fat1)+block)*BLK_SIZE,BLK_SIZE))
        break;
      cached=block;
    }
    if (!(table[c%(BLK_SIZE/4)]&0x0FFFFFFF))
      chosen[found++]=c;
    else if (contiguous)
      found=0;
  }
  uint64_t linked=0;
  if (found==count) {
    for (;linked<count;linked++)
      if (!FAT32SetLink(dv,chosen[linked],linked+1<count?chosen[linked+1]:0x0FFFFFFF))
        break;
  }
  bool okay=linked==count;
  if (okay && !contiguous)
    okay=FAT32InfoWrite(dv,fis,chosen[count-1]);
  if (okay && tail)
    okay=FAT32SetLink(dv,tail,chosen[0]);
  uint32_t first=okay?chosen[0]:0;
  if (!okay)
    while (linked)
      FAT32SetLink(dv,chosen[--linked],0);
  Free(chosen);
  return first;
}

static bool FAT32FreeClus(CDrv &dv,uint32_t c)
{
  if (!c || c>=FAT32_EOC)
    return true;
  uint64_t count;
  if (c==dv.root_clus || !FAT32ChainLength(dv,c,count))
    return false;
  // Validate the entire chain before changing either FAT.
  uint32_t next=c;
  for (uint64_t i=0;i<count;i++)
    if (next==dv.root_clus || !FAT32Next(dv,next,next))
      return false;
  while (count--) {
    if (!FAT32Next(dv,c,next) || !FAT32SetLink(dv,c,0))
      return false;
    c=next;
  }
  return true;
}

static bool FAT32WriteChain(CDrv &dv,uint32_t c,uint64_t pos,const uint8_t *buf,size_t bytes)
{
  if (!bytes)
    return true;
  uint64_t count,clus_size=uint64_t(dv.spc)*BLK_SIZE;
  if (!FAT32ChainLength(dv,c,count) || pos>count*clus_size || bytes>count*clus_size-pos)
    return false;
  for (uint64_t skip=pos/clus_size;skip;skip--)
    if (!FAT32Next(dv,c,c))
      return false;
  pos%=clus_size;
  uint8_t zero[BLK_SIZE]{};
  while (bytes) {
    size_t n=clus_size-pos;
    if (n>bytes)
      n=bytes;
    if (!buf && n>sizeof(zero))
      n=sizeof(zero);
    if (!BlkDevWriteBytes(*dv.bd,buf?buf:zero,FAT32ClusPos(dv,c)+pos,n))
      return false;
    if (buf)
      buf+=n;
    bytes-=n;
    pos+=n;
    if (pos==clus_size) {
      pos=0;
      if (bytes && !FAT32Next(dv,c,c))
        return false;
    }
  }
  return true;
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

struct CFATDirPos
{
  uint64_t short_pos=0,long_pos[2]{};
  uint8_t long_count=0;
  CFAT32DirEntry raw{};
};

// Directory records may cross a sector or cluster boundary in a long name.
template<class Visit>
static bool FAT32Walk(const CDrv &dv,uint32_t clus,Visit visit,uint64_t &dir_size)
{
  uint64_t count;
  dir_size=0;
  if (!FAT32ChainLength(dv,clus,count))
    return false;
  CFATLongName long_name;
  CFATDirPos pos;
  CFAT32DirEntry entries[BLK_SIZE/sizeof(CFAT32DirEntry)];
  for (uint64_t i=0;i<count;i++) {
    dir_size+=dv.spc*BLK_SIZE;
    for (int64_t sector=0;sector<dv.spc;sector++) {
      uint64_t base=FAT32ClusPos(dv,clus)+sector*BLK_SIZE;
      if (!BlkDevReadBytes(*dv.bd,reinterpret_cast<uint8_t *>(entries),base,BLK_SIZE))
        return false;
      for (const auto &de:entries) {
        uint64_t offset=base+(&de-entries)*sizeof(de);
        if (!de.name[0])
          return true;
        if (de.name[0]==0xE5) {
          long_name={};
          pos.long_count=0;
          continue;
        }
        if ((de.attr&0x3F)==0x0F) {
          CFAT32DirEntryLong ld;
          MemCpy(&ld,&de,sizeof(ld));
          if (ld.ord&0x40)
            pos.long_count=0;
          DirLongNameFill(long_name,ld);
          if (long_name.valid && pos.long_count<2)
            pos.long_pos[pos.long_count++]=offset;
          else
            pos.long_count=0;
          continue;
        }
        CRedSeaDirEntry entry;
        bool valid=!(de.attr&8) && FAT32CDirFill(entry,de,long_name,dv);
        if (!long_name.valid || long_name.next || long_name.xsum!=FATNameXSum(de.name))
          pos.long_count=0;
        long_name={};
        if (valid) {
          pos.short_pos=offset;
          pos.raw=de;
          int res=visit(entry,pos);
          if (res)
            return res>0;
        }
        pos.long_count=0;
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
  return FAT32Walk(dv,dir,[&](const CRedSeaDirEntry &de,const CFATDirPos &) {
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

static bool FAT32NameValid(const char *name)
{
  size_t len=StrLen(name);
  if (!len || len>25 || !FileNameChk(name) ||
      (name[0]=='.' && (len==1 || (len==2 && name[1]=='.'))))
    return false;
  for (size_t i=0;i<len;i++)
    if (uint8_t(name[i])>=127)
      return false;
  return true;
}

static bool FAT32FindPos(const CDrv &dv,uint32_t dir,const char *name,
                         CRedSeaDirEntry &entry,CFATDirPos &pos)
{
  uint64_t size;
  bool found=false;
  return FAT32Walk(dv,dir,[&](const CRedSeaDirEntry &de,const CFATDirPos &p) {
    if (StrCmpCpp(reinterpret_cast<const uint8_t *>(name),
                  reinterpret_cast<const uint8_t *>(de.name)))
      return 0;
    entry=de;
    pos=p;
    found=true;
    return 1;
  },size) && found;
}

static uint8_t FATUpper(uint8_t c)
{
  return c>='a' && c<='z'?c-'a'+'A':c;
}

static void FATNameTo(uint8_t *dst,const char *src)
{
  for (unsigned i=0;i<11;i++) dst[i]=' ';
  if (src[0]=='.' && (!src[1] || (src[1]=='.' && !src[2]))) {
    dst[0]='.';
    if (src[1]) dst[1]='.';
    return;
  }
  unsigned i=0;
  while (i<8 && *src && *src!='.')
    dst[i++]=FATUpper(*src++);
  i=8;
  while (*src && i<11) {
    if (*src!='.') dst[i++]=FATUpper(*src);
    src++;
  }
}

static bool FATAliasFree(const CDrv &dv,uint32_t dir,const uint8_t *alias)
{
  uint64_t size;
  return FAT32Walk(dv,dir,[&](const CRedSeaDirEntry &,const CFATDirPos &p) {
    for (unsigned i=0;i<11;i++)
      if (alias[i]!=p.raw.name[i]) return 0;
    return -1;
  },size);
}

static void CDate2Dos(CFAT32DirEntry &de,uint64_t date,const CDrv &dv)
{
  CDateStruct ds{};
  Date2Struct(ds,DateFromBits(date+uint64_t(dv.fat32_local_time_offset)));
  uint16_t year=ds.year<1980?0:ds.year>2107?127:ds.year-1980;
  de.WrtDate=(year<<9)|(ds.mon<<5)|ds.day_of_mon;
  de.WrtTime=(ds.hour<<11)|(ds.min<<5)|(ds.sec>>1);
}

static void FAT32DirFields(CFAT32DirEntry &de,const CRedSeaDirEntry &entry,const CDrv &dv)
{
  de.attr=entry.attr;
  de.clus_lo=entry.clus;
  de.clus_hi=entry.clus>>16;
  de.size=entry.attr&RS_ATTR_DIR?0:entry.size;
  CDate2Dos(de,entry.datetime?entry.datetime:Now(),dv);
}

static unsigned FAT32DirFill(CFAT32DirEntry (&entries)[3],const CRedSeaDirEntry &entry,
                             const CDrv &dv,uint32_t dir,bool check_alias=true)
{
  CFAT32DirEntry de{};
  FATNameTo(de.name,entry.name);
  if (check_alias && !FATAliasFree(dv,dir,de.name)) {
    // The pinned writer can truncate different names to the same short alias.
    // Keep its first choice, but use a numbered alias rather than duplicate it.
    uint8_t base[11];
    MemCpy(base,de.name,11);
    bool found=false;
    for (unsigned n=1;n<1000;n++) {
      MemCpy(de.name,base,11);
      unsigned j=7,v=n;
      do { de.name[j--]='0'+v%10;v/=10; } while (v);
      de.name[j]='~';
      if (FATAliasFree(dv,dir,de.name)) { found=true;break; }
    }
    if (!found) return 0;
  }
  char short_name[CDIR_FILENAME_LEN];
  unsigned count=0;
  FATFromName(short_name,de.name);
  if (StrCmpCpp(reinterpret_cast<const uint8_t *>(short_name),
                reinterpret_cast<const uint8_t *>(entry.name))) {
    size_t len=StrLen(entry.name);
    unsigned names=(len+12)/13;
    constexpr uint8_t offsets[]{1,3,5,7,9,14,16,18,20,22,24,28,30};
    for (unsigned ord=names;ord;ord--) {
      CFAT32DirEntryLong ld{};
      ld.ord=ord|(ord==names?0x40:0);
      ld.attr=0x0F;
      ld.xsum=FATNameXSum(de.name);
      auto *p=reinterpret_cast<uint8_t *>(&ld);
      for (unsigned i=0;i<13;i++) {
        size_t j=(ord-1)*13+i;
        uint16_t ch=j<len?uint8_t(entry.name[j]):j==len?0:0xFFFF;
        p[offsets[i]]=ch;
        p[offsets[i]+1]=ch>>8;
      }
      MemCpy(&entries[count++],&ld,sizeof(ld));
    }
  }
  FAT32DirFields(de,entry,dv);
  entries[count++]=de;
  return count;
}

static uint32_t FAT32DirGrow(CDrv &dv,uint32_t tail)
{
  auto c=FAT32AllocClus(dv,0,1,false);
  if (!c) return 0;
  if (FAT32WriteChain(dv,c,0,nullptr,dv.spc*BLK_SIZE) && FAT32SetLink(dv,tail,c))
    return c;
  FAT32FreeClus(dv,c);
  return 0;
}

// A new long name fits within one sector, as in FAT32DirNew.HC. If the
// end marker must move to the next sector, publish the new sector first.
static bool FAT32DirNew(CDrv &dv,uint32_t dir,const CRedSeaDirEntry &entry)
{
  CFAT32DirEntry new_entries[3],sector[BLK_SIZE/sizeof(CFAT32DirEntry)];
  unsigned need=FAT32DirFill(new_entries,entry,dv,dir);
  constexpr unsigned slots=BLK_SIZE/sizeof(CFAT32DirEntry);
  uint64_t chain_count;
  if (!need || !FAT32ChainLength(dv,dir,chain_count)) return false;
  uint64_t pad_pos=0;
  CFAT32DirEntry pad[slots];
  bool after_end=false;
  uint32_t c=dir;
  for (uint64_t i=0;i<=chain_count;i++) {
    if (i==chain_count) {
      c=FAT32DirGrow(dv,c);
      if (!c) return false;
      after_end=true;
    }
    for (int64_t s=0;s<dv.spc;s++) {
      uint64_t pos=FAT32ClusPos(dv,c)+s*BLK_SIZE;
      if (after_end) MemSet(sector,0,sizeof(sector));
      else if (!BlkDevReadBytes(*dv.bd,reinterpret_cast<uint8_t *>(sector),pos,sizeof(sector)))
        return false;
      for (unsigned j=0;j<slots;) {
        if (sector[j].name[0]==0) {
          if (slots-j>need) {
            MemCpy(sector+j,new_entries,need*sizeof(CFAT32DirEntry));
            MemSet(sector+j+need,0,sizeof(CFAT32DirEntry));
            if (!BlkDevWriteBytes(*dv.bd,reinterpret_cast<const uint8_t *>(sector),pos,sizeof(sector)))
              return false;
            return !pad_pos || BlkDevWriteBytes(*dv.bd,reinterpret_cast<const uint8_t *>(pad),pad_pos,sizeof(pad));
          }
          MemCpy(pad,sector,sizeof(pad));
          for (unsigned k=j;k<slots;k++) pad[k].name[0]=0xE5;
          pad_pos=pos;
          after_end=true;
          break;
        }
        if (sector[j].name[0]==0xE5) {
          unsigned n=1;
          while (j+n<slots && sector[j+n].name[0]==0xE5) n++;
          if (n>=need) {
            MemCpy(sector+j,new_entries,need*sizeof(CFAT32DirEntry));
            return BlkDevWriteBytes(*dv.bd,reinterpret_cast<const uint8_t *>(sector),pos,sizeof(sector));
          }
          j+=n;
        } else j++;
      }
    }
    if (i+1<chain_count && !FAT32Next(dv,c,c)) return false;
  }
  return false;
}

static bool FAT32ChainSafe(const CDrv &dv,uint32_t c,uint32_t parent)
{
  if (!c || c>=FAT32_EOC) return true;
  uint64_t count;
  if (!FAT32ChainLength(dv,c,count)) return false;
  while (count--) {
    if (c==parent || c==dv.root_clus || !FAT32Next(dv,c,c)) return false;
  }
  return true;
}

static bool FAT32RemoveEntry(CDrv &dv,const CFATDirPos &pos)
{
  const uint8_t deleted=0xE5;
  if (!BlkDevWriteBytes(*dv.bd,&deleted,pos.short_pos,1)) return false;
  for (unsigned i=0;i<pos.long_count;i++)
    if (!BlkDevWriteBytes(*dv.bd,&deleted,pos.long_pos[i],1)) return false;
  return true;
}

static uint32_t FAT32MkDirAt(CDrv &dv,uint32_t dir,const char *name,int64_t entry_cnt)
{
  if (entry_cnt<0) entry_cnt=0;
  uint64_t clus_size=dv.spc*BLK_SIZE;
  if (uint64_t(entry_cnt)>uint64_t(dv.size)*BLK_SIZE/32-2) return 0;
  uint64_t count=((uint64_t(entry_cnt)+2)*32+clus_size-1)/clus_size;
  auto c=FAT32AllocClus(dv,0,count,false);
  if (!c) return 0;
  CRedSeaDirEntry de{};
  de.attr=RS_ATTR_DIR;
  de.datetime=Now();
  de.clus=c;
  CFAT32DirEntry dots[3],parent[3];
  de.name[0]='.';
  FAT32DirFill(dots,de,dv,c,false);
  de.name[1]='.';
  de.clus=dir;
  FAT32DirFill(parent,de,dv,c,false);
  dots[1]=parent[0];
  de.clus=c;
  MemCpy(de.name,name,StrLen(name)+1);
  if (FAT32WriteChain(dv,c,0,nullptr,count*clus_size) &&
      FAT32WriteChain(dv,c,0,reinterpret_cast<const uint8_t *>(dots),2*sizeof(dots[0])) &&
      FAT32DirNew(dv,dir,de))
    return c;
  FAT32FreeClus(dv,c);
  return 0;
}

static uint64_t FAT32Write(CDrv &dv,const char *path,const uint8_t *data,
                           size_t size,uint64_t date,uint16_t attr,bool directory,
                           int64_t entry_cnt=0)
{
  if (!FAT32Writable(dv) || !path || *path++!='/' || size>UINT32_MAX ||
      (size && !data) || (attr&RS_ATTR_DIR)) return 0;
  CMemLock lock(dv.locked_flags);
  uint32_t dir=dv.root_clus;
  for (;;) {
    const char *part=path;
    while (*path && *path!='/') path++;
    size_t len=path-part;
    CRedSeaDirEntry entry{};
    if (!len || len>25) return 0;
    MemCpy(entry.name,part,len);
    if (!FAT32NameValid(entry.name)) return 0;
    CRedSeaDirEntry old{};
    CFATDirPos pos;
    bool exists=FAT32FindPos(dv,dir,entry.name,old,pos);
    if (*path || directory) {
      if (exists) {
        if (!(old.attr&RS_ATTR_DIR)) return 0;
        dir=old.clus;
      } else {
        dir=FAT32MkDirAt(dv,dir,entry.name,*path?0:entry_cnt);
        if (!dir) return 0;
      }
      if (!*path) return dir;
      path++;
      continue;
    }
    if (exists && ((old.attr&(RS_ATTR_DIR|1)) || !FAT32ChainSafe(dv,old.clus,dir))) return 0;
    uint64_t clus_size=dv.spc*BLK_SIZE,count=(size+clus_size-1)/clus_size;
    uint32_t c=count?FAT32AllocClus(dv,0,count,attr&RS_ATTR_CONTIGUOUS):0x0FFFFFFF;
    if (!c) return 0;
    entry.clus=c;entry.size=size;entry.attr=attr;entry.datetime=date;
    bool okay=FAT32WriteChain(dv,c,0,data,size) &&
              FAT32WriteChain(dv,c,size,nullptr,count*clus_size-size);
    if (okay && exists) {
      FAT32DirFields(pos.raw,entry,dv);
      okay=BlkDevWriteBytes(*dv.bd,reinterpret_cast<const uint8_t *>(&pos.raw),pos.short_pos,sizeof(pos.raw));
    } else if (okay) okay=FAT32DirNew(dv,dir,entry);
    if (!okay) {
      FAT32FreeClus(dv,c);
      return 0;
    }
    if (exists && !FAT32FreeClus(dv,old.clus)) return 0;
    return c;
  }
}

uint64_t FAT32FileWrite(CDrv &dv,const char *path,const uint8_t *data,size_t size,
                         uint64_t date,uint16_t attr)
{
  return FAT32Write(dv,path,data,size,date,attr,false);
}

bool FAT32MkDir(CDrv &dv,const char *path,int64_t entry_cnt)
{
  return FAT32Write(dv,path,nullptr,0,Now(),0,true,entry_cnt);
}

extern "C" void PrintCpp(const char *fmt,int64_t argc,const uint64_t *argv);

int64_t FAT32FilesDel(CDrv &dv,const char *cur_dir,const char *mask,uint64_t flags,
                      bool del_dir,bool print_msg)
{
  if (!FAT32Writable(dv)) return 0;
  CMemLock lock(dv.locked_flags);
  uint64_t dir,size;
  if (!Name2DirClus(dv,cur_dir,StrLen(cur_dir),dir)) return 0;
  int64_t res=0;
  FAT32Walk(dv,dir,[&](const CRedSeaDirEntry &de,const CFATDirPos &pos) {
    bool is_dir=de.attr&RS_ATTR_DIR;
    if (de.name[0]=='.' || (de.attr&1) || (!del_dir && is_dir) ||
        !FilesFindMatch(de.name,mask,flags) || !FAT32ChainSafe(dv,de.clus,dir)) return 0;
    if (is_dir) {
      // Refuse nonempty directories; recursive deletion visits children first.
      uint64_t child_size;
      if (!FAT32Walk(dv,de.clus,[&](const CRedSeaDirEntry &child,const CFATDirPos &) {
        return child.name[0]=='.' && (!child.name[1] ||
          (child.name[1]=='.' && !child.name[2]))?0:-1;
      },child_size)) return 0;
    }
    if (!FAT32RemoveEntry(dv,pos) || !FAT32FreeClus(dv,de.clus)) return -1;
    if (!is_dir) res++;
    if (print_msg) {
      uint64_t arg=reinterpret_cast<uintptr_t>(de.name);
      PrintCpp("Del %s\n",1,&arg);
    }
    return 0;
  },size);
  return res;
}

bool FAT32FilesFind(CDrv &dv,uint64_t clus,const char *directory,const char *mask,
                    uint64_t flags,CDirEntry *parent,CDirEntry *&res,uint64_t &dir_size)
{
  if (!FAT32ValidClus(dv,clus))
    return false;
  return FAT32Walk(dv,clus,[&](const CRedSeaDirEntry &de,const CFATDirPos &) {
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
