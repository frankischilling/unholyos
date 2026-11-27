#include "DskISORedSea.hpp"

#if __STDC_HOSTED__
#include <cstring>
static constexpr auto MemCpy=std::memcpy;
static constexpr auto MemSet=std::memset;
#else
#include "../../../Kernel/KernelB.hpp"
#endif

static_assert(__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__);

static constexpr size_t BOOT_RAM_LIMIT=0x96600-0x7C00;
static constexpr char loader_marker[]="UNHOLYOS-CD-LBA!";

static void FillU16Palindrome(CPalindromeU16 *dst,uint16_t w)
{
  dst->big=__builtin_bswap16(w);
  dst->little=w;
}

static void FillU32Palindrome(CPalindromeU32 *dst,int64_t d)
{
  dst->big=__builtin_bswap32(static_cast<uint32_t>(d));
  dst->little=static_cast<uint32_t>(d);
}

struct CElTorito
{
  uint16_t w[16];
  uint8_t bootable,media_type;
  uint16_t load_seg;
  uint8_t sys_type,zero;
  uint16_t sect_cnt;
  uint32_t load_rba;
  uint8_t zero2[20];
};

static_assert(sizeof(CElTorito)==64 && offsetof(CElTorito,bootable)==32 &&
              offsetof(CElTorito,sect_cnt)==38 &&
              offsetof(CElTorito,load_rba)==40);

CDvdExtent DvdExtent(size_t clus,size_t size)
{
  size_t shift=clus%4;
  return {clus/4,size/DVD_BLK_SIZE+(size%DVD_BLK_SIZE!=0)+(shift!=0),shift};
}

CDvdExtentResult RedSeaISO9660Stage1(uint8_t *loader,const CDvdExtent &extent)
{
  constexpr size_t marker_size=sizeof(loader_marker)-1, patch_size=8;
  size_t i=0,j=0;
  for (;i+marker_size+patch_size<=DVD_BLK_SIZE;i++) {
    for (j=0;j<marker_size && loader[i+j]==loader_marker[j];j++)
      ;
    if (j==marker_size)
      break;
  }
  if (j!=marker_size)
    return DVD_EXTENT_MARKER;
  if (extent.lba>UINT32_MAX || !extent.sectors || extent.shift>3 ||
      extent.sectors>(BOOT_RAM_LIMIT-extent.shift*BLK_SIZE)/DVD_BLK_SIZE)
    return DVD_EXTENT_RANGE;

  uint32_t lba=extent.lba;
  uint16_t sectors=extent.sectors,shift=extent.shift;
  auto *patch=loader+i+marker_size;
  MemCpy(patch,&lba,sizeof(lba));
  MemCpy(patch+4,&sectors,sizeof(sectors));
  MemCpy(patch+6,&shift,sizeof(shift));
  return DVD_EXTENT_OK;
}

void RedSeaISO9660(uint8_t *disc,uint32_t iso_size,uint32_t root_clus)
{
  CISOPriDesc iso_pri;
  CPalindromeU32 root;
  CElTorito et;
  uint32_t catalog=ISO_CATALOG_LBA;
  uint16_t j=0;

  // The destination includes the zeroed system area and full DVD sectors.
  MemSet(disc,0,(ISO_LOADER_LBA+1)*DVD_BLK_SIZE);
  MemSet(&iso_pri,0,sizeof(iso_pri));
  MemSet(&et,0,sizeof(et));
  iso_pri.type=ISOT_PRI_VOL_DESC;
  MemCpy(iso_pri.id,"CD001",sizeof(iso_pri.id));
  iso_pri.version=1;
  FillU16Palindrome(&iso_pri.vol_set_size,1);
  FillU16Palindrome(&iso_pri.vol_seq_num,1);
  FillU16Palindrome(&iso_pri.log_block_size,DVD_BLK_SIZE);
  FillU32Palindrome(&iso_pri.vol_space_size,iso_size);
  // TempleOS writes the cluster at the start of the root record.
  FillU32Palindrome(&root,root_clus);
  MemCpy(&iso_pri.root_dir_record,&root,sizeof(root));
  iso_pri.file_structure_version=1;
  MemCpy(iso_pri.publisher_id,"TempleOS RedSea",
                   sizeof("TempleOS RedSea"));

  MemCpy(disc+16*DVD_BLK_SIZE,&iso_pri,sizeof(iso_pri));
  MemCpy(disc+18*DVD_BLK_SIZE,&iso_pri,sizeof(iso_pri));
  disc[18*DVD_BLK_SIZE]=ISOT_SUPPLEMENTARY_DESC;

  auto *iso_boot=disc+17*DVD_BLK_SIZE;
  iso_boot[0]=ISOT_BOOT_RECORD;
  MemCpy(iso_boot+1,"CD001\x01",6);
  MemCpy(iso_boot+7,"EL TORITO SPECIFICATION",
                   sizeof("EL TORITO SPECIFICATION"));
  MemCpy(iso_boot+0x47,&catalog,sizeof(catalog));

  auto *iso_term=disc+19*DVD_BLK_SIZE;
  iso_term[0]=ISOT_TERMINATOR;
  MemCpy(iso_term+1,"CD001\x01",6);

  et.w[0]=1;
  MemCpy(&et.w[2],"TempleOS",sizeof("TempleOS"));
  et.w[15]=0xAA55;
  for (size_t i=0;i<16;i++)
    j+=et.w[i];
  et.w[14]=static_cast<uint16_t>(-j);
  et.bootable=0x88;
  et.media_type=0;
  et.sect_cnt=4;
  et.load_rba=ISO_LOADER_LBA;
  MemCpy(disc+ISO_CATALOG_LBA*DVD_BLK_SIZE,&et,sizeof(et));
}

#if !__STDC_HOSTED__

extern "C" const uint8_t BDVD_START[];
extern "C" char *DirNameAbsCpp(const char *name);
extern "C" char *ExtChgCpp(const char *name,const char *ext);
extern "C" uint64_t FileWriteCpp(const char *name,const uint8_t *data,
                                 uint64_t count,uint64_t date,uint64_t attr);

bool RedSeaISO9660Write(CDrv &dv)
{
  if (!dv.bd || dv.drv_offset!=int64_t(ISO_DRV_OFFSET) ||
      dv.bd->max_blk<int64_t(ISO_DRV_OFFSET) ||
      uint64_t(dv.bd->max_blk)+1>uint64_t(UINT32_MAX)*DVD_BLK_SIZE/BLK_SIZE ||
      dv.root_clus>UINT32_MAX)
    return false;
  auto *prefix=CAlloc(ISO_DRV_OFFSET*BLK_SIZE,CurrentTask()->data_heap);
  if (!prefix) return false;
  RedSeaISO9660(prefix,(uint64_t(dv.bd->max_blk)+1)*BLK_SIZE/DVD_BLK_SIZE,
                dv.root_clus);
  bool res=BlkDevWriteBytes(*dv.bd,prefix,0,ISO_DRV_OFFSET*BLK_SIZE);
  Free(prefix);
  return res;
}

struct CISOEntry
{
  CISOEntry *next,*sub;
  CRedSeaDirEntry de;
  size_t dir_blocks;
};

static void DirTreeDel(CISOEntry *node)
{
  while (node) {
    auto *next=node->next;
    DirTreeDel(node->sub);
    Free(node);
    node=next;
  }
}

static CISOEntry *DirFilesSort(CISOEntry *first,size_t count)
{
  if (count<2)
    return first;
  auto *middle=first;
  for (size_t i=1;i<count/2;i++)
    middle=middle->next;
  auto *right=middle->next;
  middle->next=nullptr;
  auto *left=DirFilesSort(first,count/2);
  right=DirFilesSort(right,count-count/2);
  CISOEntry *res=nullptr,**tail=&res;
  while (left && right) {
    auto **entry=DirEntryCompareName(left->de,right->de)<=0?&left:&right;
    *tail=*entry;
    *entry=(*entry)->next;
    tail=&(*tail)->next;
  }
  *tail=left?left:right;
  return res;
}

static size_t RedSeaBlocks(uint64_t bytes)
{
  return bytes/BLK_SIZE+(bytes%BLK_SIZE!=0);
}

static bool RedSeaISOPass1(const CDrv &dv,CISOEntry &dir,size_t &blocks,
                           unsigned depth)
{
  // Bound recursion when a malformed directory links back to an ancestor.
  CRedSeaDirEntry head;
  if (depth==64 || !RedSeaFileFind(dv,dir.de.clus,".",true,head))
    return false;

  CHeapCtrl *hc=CurrentTask()->data_heap;
  auto *block=reinterpret_cast<CRedSeaDirEntry *>(MAlloc(BLK_SIZE,hc));
  if (!block)
    return false;
  size_t count=0;
  bool okay=true,end=false;
  auto **tail=&dir.sub;
  for (uint64_t pos=0;okay && !end && pos<head.size;pos+=BLK_SIZE) {
    if (!BlkDevReadBytes(*dv.bd,reinterpret_cast<uint8_t *>(block),
                         dir.de.clus*BLK_SIZE+pos,BLK_SIZE)) {
      okay=false;
      break;
    }
    for (size_t i=pos?0:2;i<BLK_SIZE/sizeof(*block);i++) {
      const auto &de=block[i];
      if (!de.name[0]) {
        end=true;
        break;
      }
      if ((de.attr & RS_ATTR_DELETED) ||
          ((de.attr & RS_ATTR_DIR) && de.name[0]=='.'))
        continue;
      if (de.name[sizeof(de.name)-1]) {
        okay=false;
        break;
      }
      auto *entry=reinterpret_cast<CISOEntry *>(CAlloc(sizeof(CISOEntry),hc));
      if (!entry) {
        okay=false;
        break;
      }
      entry->de=de;
      *tail=entry;
      tail=&entry->next;
      count++;
    }
  }
  Free(block);
  if (!okay)
    return false;

  dir.sub=DirFilesSort(dir.sub,count);
  dir.dir_blocks=RedSeaBlocks((count+3)*sizeof(CRedSeaDirEntry));
  blocks+=dir.dir_blocks;
  for (auto *entry=dir.sub;entry;entry=entry->next) {
    if (entry->de.attr & RS_ATTR_DIR) {
      if (!RedSeaISOPass1(dv,*entry,blocks,depth+1))
        return false;
    } else {
      if (entry->de.size>uint64_t(dv.size)*BLK_SIZE)
        return false;
      blocks+=RedSeaBlocks(entry->de.size);
    }
    if (blocks>UINT32_MAX)
      return false;
  }
  return true;
}

static bool CopyTree2(const CDrv &src,CDrv &dst,const CISOEntry &dir,
                      uint64_t clus,uint64_t stage2_src,uint64_t &stage2_dst)
{
  auto *out=reinterpret_cast<CRedSeaDirEntry *>(dst.bd->RAM_dsk+clus*BLK_SIZE);
  size_t index=2;
  for (auto *entry=dir.sub;entry;entry=entry->next) {
    auto de=entry->de;
    de.attr|=RS_ATTR_CONTIGUOUS;
    if (de.attr & RS_ATTR_DIR) {
      de.attr=RS_ATTR_DIR|RS_ATTR_CONTIGUOUS;
      de.size=entry->dir_blocks*BLK_SIZE;
      de.datetime=Now();
    } else if (IsDotExt(de.name,'Z')) {
      de.attr|=RS_ATTR_COMPRESSED;
    } else {
      de.attr&=~RS_ATTR_COMPRESSED;
    }
    size_t blocks=RedSeaBlocks(de.size);
    de.clus=blocks?RedSeaAllocClus(dst,blocks):INVALID_CLUS;
    if (blocks && de.clus==INVALID_CLUS)
      return false;
    out[index++]=de;

    if (de.attr & RS_ATTR_DIR) {
      auto *child=reinterpret_cast<CRedSeaDirEntry *>(
          dst.bd->RAM_dsk+de.clus*BLK_SIZE);
      child[0]=de;
      child[1]={de.attr,"..",clus,0,de.datetime};
      if (!CopyTree2(src,dst,*entry,de.clus,stage2_src,stage2_dst))
        return false;
    } else {
      if (blocks && !RedSeaFileRead(src,entry->de,0,
                                    dst.bd->RAM_dsk+de.clus*BLK_SIZE,de.size))
        return false;
      if (stage2_src!=INVALID_CLUS && stage2_src==entry->de.clus)
        stage2_dst=de.clus;
    }
  }
  return true;
}

static bool RedSeaISOContains(const char *path,const char *dir)
{
  while (*dir && *path==*dir) {
    path++;
    dir++;
  }
  return !*dir && (!*path || *path=='/' || path[-1]=='/');
}

static size_t RedSeaISO(const CDrv &src,const char *out_name,uint64_t root_clus,
                        const CRedSeaDirEntry *stage2)
{
  CISOEntry root{};
  root.de.clus=root_clus;
  size_t content=0;
  if (!RedSeaISOPass1(src,root,content,0)) {
    DirTreeDel(root.sub);
    return 0;
  }

  // Reserve cluster zero of the data area, and room for the shifted stage-two
  // read if its final DVD sector would otherwise extend past the volume.
  size_t bitmap=1;
  size_t total;
  for (;;) {
    total=(ISO_DRV_OFFSET+1+bitmap+content+3)&~size_t{3};
    if (stage2)
      total+=4;
    size_t needed=(total-ISO_DRV_OFFSET+8*BLK_SIZE-1)/(8*BLK_SIZE);
    if (needed==bitmap)
      break;
    bitmap=needed;
  }
  auto *iso=CAlloc(total*BLK_SIZE,CurrentTask()->data_heap);
  CBlkDev bd;
  CDrv dst;
  MemSet(&bd,0,sizeof(bd));
  MemSet(&dst,0,sizeof(dst));
  bd.type=BDT_RAM;
  bd.RAM_dsk=iso;
  bd.max_blk=total-1;
  dst.bd=&bd;
  dst.drv_offset=ISO_DRV_OFFSET;
  dst.size=total-ISO_DRV_OFFSET;
  uint64_t stage2_clus=INVALID_CLUS;
  bool okay=iso && RedSeaFormatRAM(dst,root.dir_blocks) &&
            CopyTree2(src,dst,root,dst.root_clus,
                      stage2?stage2->clus:INVALID_CLUS,stage2_clus);
  if (okay) {
    RedSeaISO9660(iso,total/4,dst.root_clus);
    if (stage2) {
      auto *loader=iso+ISO_LOADER_LBA*DVD_BLK_SIZE;
      MemCpy(loader,BDVD_START,DVD_BLK_SIZE);
      auto extent=DvdExtent(stage2_clus,stage2->size);
      okay=stage2_clus!=INVALID_CLUS && extent.lba+extent.sectors<=total/4 &&
           RedSeaISO9660Stage1(loader,extent)==DVD_EXTENT_OK;
    }
  }
  if (okay)
    okay=FileWriteCpp(out_name,iso,total*BLK_SIZE,0,0)!=0;
  Free(iso);
  DirTreeDel(root.sub);
  return okay?content:0;
}

extern "C" int64_t RedSeaISOCpp(const char *iso_filename,const char *src_dir,
                                const char *stage2_filename)
{
  if (!src_dir)
    return 0;
  auto &task=*CurrentTask();
  auto *src=DirNameAbsCpp(src_dir);
  auto *out=ExtChgCpp(iso_filename?iso_filename:blkdev.dft_iso_c_filename,
                      "ISO.C");
  auto *out_path=out?FileNameAbs(out,task):nullptr;
  auto *stage2=stage2_filename?FileNameAbs(stage2_filename,task):nullptr;
  size_t res=0,prefix;
  uint64_t root;
  CRedSeaDirEntry kernel;
  auto *dv=src && *src && src[1]==':'?PathDrive(task,src[0]):nullptr;
  auto *target=out_path && *out_path && out_path[1]==':'?PathDrive(task,out_path[0]):nullptr;
  if (dv && target && target->bd->type==BDT_RAM &&
      !(target->bd->flags & BDF_READ_ONLY) &&
      (target!=dv || !RedSeaISOContains(out_path+2,src+2)) &&
      RedSeaDirClus(*dv,src+2,StrLen(src+2),root) &&
      (!stage2_filename ||
       (stage2 && *stage2 && stage2[1]==':' && PathDrive(task,stage2[0])==dv &&
        RedSeaISOContains(stage2+2,src+2) &&
        RedSeaFileLookup(*dv,stage2+2,false,false,kernel,prefix) &&
        kernel.size && kernel.clus!=INVALID_CLUS)))
    res=RedSeaISO(*dv,out_path,root,stage2_filename?&kernel:nullptr);
  Free(stage2);
  Free(out_path);
  Free(out);
  Free(src);
  return res;
}

#endif
