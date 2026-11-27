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

static bool RedSeaISOContains(const char *path,const char *dir,bool fold)
{
  auto ch=[fold](char c) {
    return fold && 'a'<=c && c<='z'?char(c-'a'+'A'):c;
  };
  while (*dir && ch(*path)==ch(*dir)) {
    path++;
    dir++;
  }
  return !*dir && (!*path || *path=='/' || path[-1]=='/');
}

static bool RedSeaISOPathEqual(const char *a,const char *b,bool fold)
{
  return fold?!StrICmpCpp(a,b):
      !StrCmpCpp(reinterpret_cast<const uint8_t *>(a),
                 reinterpret_cast<const uint8_t *>(b));
}

static size_t RedSeaBlocks(uint64_t bytes)
{
  return bytes/BLK_SIZE+(bytes%BLK_SIZE!=0);
}

// The reference helper can also be used on an existing image.
extern "C" void RedSeaISO9660Cpp(const char *iso_filename,uint8_t drv_let)
{
  auto *dv=Let2DrvCpp(drv_let,false);
  if (!iso_filename || !dv || !BlkDevChkCpp(dv->bd,false) ||
      dv->root_clus<0 || dv->root_clus>UINT32_MAX)
    return;
  auto *name=FileNameAbs(iso_filename,*CurrentTask());
  if (!name || !*name || name[1]!=':') {
    Free(name);
    return;
  }
  auto *bd=dv->bd;
  if (bd->type==BDT_ISO_FILE_WRITE && bd->file_dsk && bd->file_dsk_name &&
      PathDrive(*CurrentTask(),name[0])==bd->file_dsk->dv &&
      RedSeaISOPathEqual(name+2,reinterpret_cast<char *>(bd->file_dsk_name)+2,
                        bd->file_dsk->dv->fs_type==FSt_FAT32)) {
    RedSeaISO9660Write(*dv);
  } else {
    CDirEntry de;
    if (FileFindCpp(name,&de,FUF_JUST_FILES)) {
      auto size=de.size;
      Free(de.full_name);
      if (size>=int64_t(ISO_DRV_OFFSET*BLK_SIZE) &&
          uint64_t(size)/DVD_BLK_SIZE<=UINT32_MAX) {
        auto *prefix=CAlloc(ISO_DRV_OFFSET*BLK_SIZE,CurrentTask()->data_heap);
        if (prefix) {
          if (auto *f=FOpenCpp(name,"wc+",0)) {
            RedSeaISO9660(prefix,size/DVD_BLK_SIZE,dv->root_clus);
            FBlkWriteCpp(f,prefix,0,ISO_DRV_OFFSET);
            FCloseCpp(f);
          }
          Free(prefix);
        }
      }
    }
  }
  Free(name);
}

static bool RedSeaISO9660Stage1(CDrv &dv,const char *stage2_filename)
{
  if (!stage2_filename)
    return true;
  CDirEntry de;
  if (!FileFindCpp(stage2_filename,&de,FUF_JUST_FILES))
    return false;
  Free(de.full_name);
  if (de.clus<0 || de.size<=0)
    return false;
  auto extent=DvdExtent(de.clus,de.size);
  if (extent.lba+extent.sectors>(uint64_t(dv.bd->max_blk)+1)/4)
    return false;
  uint8_t loader[DVD_BLK_SIZE];
  MemCpy(loader,BDVD_START,sizeof(loader));
  return RedSeaISO9660Stage1(loader,extent)==DVD_EXTENT_OK &&
         BlkDevWriteBytes(*dv.bd,loader,ISO_LOADER_LBA*DVD_BLK_SIZE,
                          sizeof(loader));
}

// Count directory slots and stored file blocks as in RedSeaISOPass1.
static bool RedSeaISOPass1(const CDirEntry *de,size_t &blocks,size_t &dir_blocks,
                           size_t &entries,unsigned depth=0)
{
  if (depth==64)
    return false;
  size_t cnt=0;
  for (;de;de=de->next) {
    if (++cnt>UINT32_MAX || ++entries>UINT32_MAX)
      return false;
    if (de->attr & RS_ATTR_DIR) {
      size_t child_blocks;
      if (!RedSeaISOPass1(de->sub,blocks,child_blocks,entries,depth+1))
        return false;
    } else {
      if (de->size<0 || RedSeaBlocks(de->size)>UINT32_MAX-blocks)
        return false;
      blocks+=RedSeaBlocks(de->size);
    }
  }
  dir_blocks=RedSeaBlocks((cnt+3)*sizeof(CRedSeaDirEntry));
  if (dir_blocks>UINT32_MAX-blocks)
    return false;
  blocks+=dir_blocks;
  return true;
}

static const CDirEntry *RedSeaISOStage2(const CDirEntry *de,const char *name,
                                        bool fold)
{
  for (;de;de=de->next) {
    if (de->attr & RS_ATTR_DIR) {
      if (auto *found=RedSeaISOStage2(de->sub,name,fold))
        return found;
    } else if (RedSeaISOPathEqual(de->full_name+2,name+2,fold))
      return de;
  }
  return nullptr;
}

static bool RedSeaISOFilesFind(CDrv &dv,const char *src_dir,CDirEntry *&tree)
{
  uint64_t clus,parent,size;
  auto len=StrLen(src_dir+2);
  if (!Name2DirClus(dv,src_dir+2,len,clus) ||
      !Name2ParentDirClus(dv,src_dir+2,len,parent))
    return false;
  // FilesFind returns NULL for both an empty directory and a failed scan.
  // Use its filesystem walkers here to distinguish those cases before
  // creating the destination image.
  return dv.fs_type==FSt_FAT32?
      FAT32FilesFind(dv,clus,src_dir,"*",FUF_RECURSE,nullptr,tree,size):
      RedSeaFilesFind(dv,clus,parent,src_dir,"*",FUF_RECURSE,nullptr,tree);
}

static size_t RedSeaISO(CDrv &src,const char *src_dir,const char *out_name,
                        const char *stage2_filename,bool fold)
{
  CDirEntry *tree=nullptr;
  if (!RedSeaISOFilesFind(src,src_dir,tree)) {
    DirTreeDelCpp(tree);
    return 0;
  }
  size_t content=0,root_blocks=0,entries=0,res=0;
  const CDirEntry *stage2=stage2_filename?
      RedSeaISOStage2(tree,stage2_filename,fold):nullptr;
  if ((!stage2_filename || (stage2 && stage2->size>0)) &&
      RedSeaISOPass1(tree,content,root_blocks,entries)) {
    // Reserve cluster zero, and allow one extra DVD sector for the
    // shifted stage-two read, as in the host ISO builder.
    size_t bitmap=1,total;
    for (;;) {
      total=(ISO_DRV_OFFSET+1+bitmap+content+3)&~size_t{3};
      if (stage2)
        total+=4;
      size_t needed=(total-ISO_DRV_OFFSET+8*BLK_SIZE-1)/(8*BLK_SIZE);
      if (needed==bitmap)
        break;
      bitmap=needed;
    }
    char letter='Q';
    while (letter<='S' && (MountedDrive(letter) ||
           blkdev.drvs[letter-'A'].dv_signature==DRV_SIGNATURE_VAL))
      letter++;
    if (letter<='S' && total/4<=UINT32_MAX) {
      auto *name=StrNew(out_name,adam_task->data_heap);
      auto *bd=name?BlkDevNextFreeSlot(letter,BDT_ISO_FILE_WRITE):nullptr;
      if (bd) {
        bd->file_dsk_name=reinterpret_cast<uint8_t *>(name);
        bd->drv_offset=ISO_DRV_OFFSET;
        bd->max_blk=total-1;
        bd->init_root_dir_blks=root_blocks;
        if (BlkDevAddCpp(bd,INT64_MIN,true,true)==1) {
          char dest[]{letter,':','/',0};
          char *stage2_dest=nullptr;
          if (stage2) {
            size_t prefix=StrLen(src_dir);
            if (prefix==3)
              prefix--;
            auto *relative=stage2->full_name+prefix;
            size_t len=StrLen(relative);
            stage2_dest=reinterpret_cast<char *>(
                MAlloc(len+3,CurrentTask()->data_heap));
            if (stage2_dest) {
              stage2_dest[0]=letter;
              stage2_dest[1]=':';
              MemCpy(stage2_dest+2,relative,len+1);
            }
          }
          auto *dv=MountedDrive(letter);
          bool okay=dv && (!stage2 || stage2_dest) &&
              CopyTreeCpp(src_dir,dest,true)==int64_t(entries+1) &&
              RedSeaISO9660Write(*dv) &&
              RedSeaISO9660Stage1(*dv,stage2_dest);
          Free(stage2_dest);
          BlkDevDelCpp(bd);
          if (okay)
            res=content;
          else
            DelCpp(out_name,false,false,false);
        } else {
          // On failure the caller still owns the requested name.
          if (bd->file_dsk_name==reinterpret_cast<uint8_t *>(name))
            bd->file_dsk_name=nullptr;
          Free(name);
        }
      } else
        Free(name);
    }
  }
  DirTreeDelCpp(tree);
  return res;
}

extern "C" int64_t RedSeaISOCpp(const char *iso_filename,const char *src_dir,
                                const char *stage2_filename)
{
  if (!src_dir || !adam_task)
    return 0;
  auto &task=*CurrentTask();
  auto *src=DirNameAbsCpp(src_dir);
  auto *out=ExtChgCpp(iso_filename?iso_filename:blkdev.dft_iso_c_filename,
                      "ISO.C");
  auto *out_path=out?FileNameAbs(out,task):nullptr;
  auto *stage2=stage2_filename?FileNameAbs(stage2_filename,task):nullptr;
  size_t res=0;
  auto *dv=src && *src && src[1]==':'?PathDrive(task,src[0]):nullptr;
  auto *target=out_path && *out_path && out_path[1]==':'?PathDrive(task,out_path[0]):nullptr;
  if (dv && target && ReadableFS(*dv) && WritableFS(*target) &&
      (target!=dv || !RedSeaISOContains(out_path+2,src+2,dv->fs_type==FSt_FAT32)) &&
      IsDirCpp(src) &&
      (!stage2_filename ||
       (stage2 && *stage2 && stage2[1]==':' && PathDrive(task,stage2[0])==dv &&
        RedSeaISOContains(stage2+2,src+2,dv->fs_type==FSt_FAT32))))
    res=RedSeaISO(*dv,src,out_path,stage2,dv->fs_type==FSt_FAT32);
  Free(stage2);
  Free(out_path);
  Free(out);
  Free(src);
  return res;
}

#endif
