#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr uint32_t RLF_VGA = 1 << 1;
constexpr uint32_t RLF_FULL_HEAPS = 1 << 7;
constexpr uint32_t RLF_INTERRUPTS = 1 << 9;
constexpr uint32_t RLF_DOC = 1 << 13;

struct CTask;
struct CTaskDying;
struct CTSS;

struct CCPU
{
  CCPU *addr;
  int64_t num, cpu_flags, startup_rip, idle_pt_hits;
  double idle_factor;
  int64_t total_jiffies;
  CTask *seth_task, *idle_task;
  int64_t tr, swap_cnter;
  void (*profiler_timer_irq)(CTask *task);
  CTaskDying *next_dying, *last_dying;
  int64_t kill_jiffy;
  CTSS *tss;
  int64_t start_stk[16];
};

static_assert(sizeof(CCPU) == 0x100 && offsetof(CCPU, num) == 8);
static_assert(offsetof(CCPU, seth_task) == 0x38 &&
              offsetof(CCPU, idle_task) == 0x40);
static_assert(__builtin_offsetof(CCPU, next_dying) == 0x60);

constexpr uint32_t MBS_UNUSED_SIGNATURE_VAL = 0x6E55424D; // MBUn
constexpr uint32_t MBS_USED_SIGNATURE_VAL = 0x7355424D;   // MBUs
constexpr uint32_t HEAP_CTRL_SIGNATURE_VAL = 0x56536348;  // HcSV
constexpr uint32_t TASK_SIGNATURE_VAL = 0x536B7354;       // TskS
constexpr uintptr_t SYS_FIXED_AREA = 0x100000;
constexpr size_t MEM_PAGE_SIZE = 0x200000;
constexpr size_t MEM_PAG_BITS = 9;
constexpr size_t MEM_PAG_SIZE = 1 << MEM_PAG_BITS;
constexpr size_t MEM_DFT_STK = MEM_PAG_SIZE * 512;
constexpr size_t MEM_HEAP_HASH_SIZE = 1024;
constexpr size_t MEM_FREE_PAG_HASH_SIZE = 256;
constexpr size_t MEM_EXTRA_HASH2_PAGS = 2;

struct CHash;
struct CDoc;
struct CDocEntryBase;
struct CDirContext;
struct CDrv;
struct CFreeLst;
struct CFAT32FileInfoSect;
struct CFile;
struct CDC;
struct CExcept;
struct CBpt;
struct CCtrl;
struct CMenu;
struct CTaskSettings;
struct CMathODE;
struct CJob;
struct CCmpCtrl;
struct CAOT;
struct CHashFun;
struct CHeapCtrl;

struct CFPU
{
  uint8_t body[512];
};

struct [[gnu::packed]] CMemE820
{
  uint64_t base, len;
  uint8_t type, pad[3];
};

static_assert(sizeof(CMemE820) == 20 && offsetof(CMemE820, type) == 16);

struct CMemBlk
{
  CMemBlk *next, *last;
  uint32_t mb_signature, pags;
};

struct CMemUnused
{
  CMemUnused *next;
  int64_t size;
};

struct CMemUsed
{
  CHeapCtrl *hc;
  int64_t size;
};

struct CBlkPool
{
  uint64_t locked_flags;
  int64_t alloced_u8s, used_u8s;
  CMemBlk *mem_free_lst, *mem_free_2meg_lst;
  CMemBlk *free_pag_hash[MEM_FREE_PAG_HASH_SIZE];
  CMemBlk *free_pag_hash2[64 - MEM_PAG_BITS];
};

struct CHeapCtrl
{
  CBlkPool *bp;
  uint32_t hc_signature, pad;
  uint64_t locked_flags;
  int64_t alloced_u8s, used_u8s;
  CTask *mem_task;
  CMemBlk *next_mem_blk, *last_mem_blk, *last_mergable;
  CMemUnused *malloc_free_lst;
  CMemUsed *next_um, *last_um;
  CMemUnused *heap_hash[MEM_HEAP_HASH_SIZE / sizeof(void *)];
};

struct CMemRange
{
  CMemRange *next, *last;
  uint32_t type, flags;
  uintptr_t base;
  uint64_t size;
};

struct CPCIDev
{
  CPCIDev *next, *last;
  uint16_t vendor, dev_id;
  uint8_t bus, dev, fun, pad, sub_code, base_code, reserved[6];
  uint8_t *vendor_str, *dev_id_str;
};

constexpr unsigned I_DIV_ZERO = 0, I_NMI = 2, I_TIMER = 0x20;
constexpr unsigned I_MP_CRASH = 0x30, I_WAKE = 0x31, I_DBG = 0x32;

constexpr unsigned JIFFY_FREQ = 1000;
constexpr uint16_t SYS_TIMER0_PERIOD = 65536 * 182 / 10 / JIFFY_FREQ;

struct CCntsGlbls
{
  uint64_t jiffies, timer;
  int64_t time_stamp_freq, time_stamp_kHz_freq, time_stamp_freq_initial;
  uint64_t HPET_freq, HPET_kHz_freq, HPET_initial;
  bool time_stamp_calibrated;
};

constexpr uintptr_t HPET_GCAP_ID = 0xFED00000;
constexpr uintptr_t HPET_GEN_CONF = 0xFED00010;
constexpr uintptr_t HPET_MAIN_CNT = 0xFED000F0;
constexpr uint64_t SYS_TIMER_FREQ = 18333 * 65536 / 1000;

constexpr size_t BLK_SIZE = 512, DVD_BLK_SIZE = 2048;
constexpr size_t BIN_HEADER_SIZE = 32;
constexpr uint32_t BIN_SIGNATURE_VAL = 0x42534F54; // TOSB
constexpr uint64_t INVALID_PTR = INT64_MAX;

enum : uint8_t
{
  IET_END = 0,
  IET_REL_I0 = 2,
  IET_REL_I8 = 4,
  IET_REL_I16 = 6,
  IET_REL_I32 = 8,
  IET_IMM_I64 = 11,
  IET_REL32_EXPORT = 16,
  IET_IMM32_EXPORT = 17,
  IET_REL64_EXPORT = 18,
  IET_IMM64_EXPORT = 19,
  IET_ABS_ADDR = 20,
  IET_CODE_HEAP = 21,
  IET_ZEROED_CODE_HEAP = 22,
  IET_DATA_HEAP = 23,
  IET_ZEROED_DATA_HEAP = 24,
  IET_MAIN = 25,
};

struct CBinFile
{
  uint16_t jmp;
  uint8_t module_align_bits, reserved;
  uint32_t bin_signature;
  uint64_t org, patch_table_offset, file_size;
};

static_assert(sizeof(CBinFile) == BIN_HEADER_SIZE &&
              offsetof(CBinFile, org) == 8 &&
              offsetof(CBinFile, patch_table_offset) == 16 &&
              offsetof(CBinFile, file_size) == 24);
constexpr unsigned BOOT_SRC_DVD = 4;
constexpr uint32_t BDT_NULL = 0, BDT_RAM = 1, BDT_ATA = 2,
                   BDT_ISO_FILE_READ = 3, BDT_ISO_FILE_WRITE = 4, BDT_ATAPI = 5;
constexpr uint16_t FSt_REDSEA = 1;
constexpr uint8_t MBR_PT_REDSEA = 0x88;
constexpr uint32_t BD_SIGNATURE_VAL = 0x56534442;  // BDSV
constexpr uint32_t DRV_SIGNATURE_VAL = 0x56535644; // DVSV
constexpr uint32_t BDF_REMOVABLE = 1, BDF_INITIALIZED = 2, BDF_READ_ONLY = 4,
                   BDF_READ_CACHE = 0x20;

struct [[gnu::packed]] CPalindromeU16
{
  uint16_t little,big;
};

struct [[gnu::packed]] CPalindromeU32
{
  uint32_t little,big;
};

constexpr uint8_t ISOT_BOOT_RECORD=0,ISOT_PRI_VOL_DESC=1,
                  ISOT_SUPPLEMENTARY_DESC=2,ISOT_VOL_DRV_DESC=3,
                  ISOT_TERMINATOR=255;

struct [[gnu::packed]] CISODirEntry
{
  uint8_t pad0[2];
  CPalindromeU32 loc;
  uint8_t pad1[24];
};

struct [[gnu::packed]] CISOPriDesc
{
  uint8_t type,id[5],version,pad0[73];
  CPalindromeU32 vol_space_size;
  uint8_t pad1[32];
  CPalindromeU16 vol_set_size;
  CPalindromeU16 vol_seq_num;
  CPalindromeU16 log_block_size;
  uint8_t pad2[20];
  CISODirEntry root_dir_record;
  uint8_t pad3[128],publisher_id[128],pad4[435],file_structure_version,
          pad5[1166];
};

static_assert(sizeof(CPalindromeU16)==4 && sizeof(CPalindromeU32)==8);
static_assert(sizeof(CISODirEntry)==34 && offsetof(CISODirEntry,loc)==2);
static_assert(sizeof(CISOPriDesc)==2044 &&
              offsetof(CISOPriDesc,vol_space_size)==80 &&
              offsetof(CISOPriDesc,vol_set_size)==120 &&
              offsetof(CISOPriDesc,vol_seq_num)==124 &&
              offsetof(CISOPriDesc,log_block_size)==128 &&
              offsetof(CISOPriDesc,root_dir_record)==152 &&
              offsetof(CISOPriDesc,publisher_id)==314 &&
              offsetof(CISOPriDesc,file_structure_version)==877);

constexpr uint16_t RS_ATTR_DIR = 0x10, RS_ATTR_DELETED = 0x100,
                   RS_ATTR_RESIDENT = 0x200, RS_ATTR_COMPRESSED = 0x400,
                   RS_ATTR_CONTIGUOUS = 0x800;
constexpr size_t CDIR_FILENAME_LEN = 38;
constexpr uint64_t INVALID_CLUS = UINT64_MAX;
constexpr char BOOT_DVD_LET = 'T', HOME_DIR[] = "/Home";

struct [[gnu::packed]] CBlkDev
{
  CBlkDev *lock_fwding;
  uint64_t locked_flags;
  uint32_t bd_signature, type, flags;
  uint8_t first_drv_let, unit, pad[2];
  uint32_t base0, base1, blk_size;
  int64_t drv_offset, init_root_dir_blks, max_blk;
  uint16_t *dev_id_record;
  uint8_t *RAM_dsk, *file_dsk_name;
  CFile *file_dsk;
  CTask *owning_task;
  double last_time;
  uint32_t max_reads, max_writes;
};

struct CDrv
{
  uint64_t locked_flags;
  uint32_t dv_signature;
  uint8_t drv_let, pad;
  uint16_t fs_type;
  int64_t drv_offset, size, prt_num, file_system_info_sect, fat1, fat2;
  int64_t root_clus, data_area, spc, fat32_local_time_offset;
  CTask *owning_task;
  CBlkDev *bd;
  CFAT32FileInfoSect *fis;
  int64_t fat_blk_dirty, cur_fat_blk_num;
  uint32_t *cur_fat_blk;
  CFreeLst *next_free, *last_free;
};

constexpr size_t BLKDEVS_NUM = 26, DRVS_NUM = 26;
constexpr char DFT_ISO_FILENAME[] = "::/Tmp/CDDVD.ISO",
               DFT_ISO_C_FILENAME[] = "::/Tmp/CDDVD.ISO.C",
               DFT_TMP_FILENAME[] = "~/Tmp.DD.Z";

struct CCacheBlk;

struct CBlkDevGlbls
{
  CBlkDev *blkdevs;
  char *dft_iso_filename, *dft_iso_c_filename, *tmp_filename, *home_dir;
  CCacheBlk *cache_base, *cache_ctrl, **cache_hash_table;
  int64_t cache_size, read_cnt, write_cnt;
  CDrv *drvs, *let_to_drv[32];
  int64_t mount_ide_auto_cnt, ins_base0, ins_base1;
  uint8_t boot_drv_let, first_hd_drv_let, first_dvd_drv_let;
  bool dvd_boot_is_good, ins_unit;
  uint8_t pad[3];
};

static_assert(sizeof(CBlkDevGlbls) == 384 &&
              offsetof(CBlkDevGlbls, tmp_filename) == 24 &&
              offsetof(CBlkDevGlbls, let_to_drv) == 96 &&
              offsetof(CBlkDevGlbls, boot_drv_let) == 376);

enum : size_t
{
  SEMA_RECORD_MACRO = 1,
  SEMA_DBG_MODE = 4,
  SEMA_SND = 5,
  SEMA_FLUSH_VGA_IMAGE = 9,
  SEMA_SINGLE_USER = 10,
  SEMA_MUTE = 17,
  SEMA_FIX = 20,
  SEMA_SEMAS_NUM = 21,
};

struct CSema
{
  uint64_t flags;
  uint8_t pad[120];
};

static_assert(sizeof(CSema) == 128 && SEMA_SINGLE_USER == 10);

struct [[gnu::packed]] CAUData
{
  uint64_t cdt;
  int8_t ona;
};

struct [[gnu::packed]] CSndData
{
  CSndData *next, *last;
  uint64_t tS;
  int8_t ona;
};

struct [[gnu::packed]] CScrnCastGlbls
{
  CSndData snd_head;
  uint64_t t0_now, t0_tS;
  char *print_fmt;
  CDC *dc, *dc2_alias;
  bool record, just_audio;
  int8_t ona;
};

static_assert(sizeof(CAUData) == 9 && offsetof(CAUData, ona) == 8);
static_assert(sizeof(CSndData) == 25 && offsetof(CSndData, tS) == 16 &&
              offsetof(CSndData, ona) == 24);
static_assert(sizeof(CScrnCastGlbls) == 68 &&
              offsetof(CScrnCastGlbls, t0_now) == 25 &&
              offsetof(CScrnCastGlbls, print_fmt) == 41 &&
              offsetof(CScrnCastGlbls, record) == 65 &&
              offsetof(CScrnCastGlbls, ona) == 67);

struct CDbgInfo
{
  uint32_t min_line, max_line, body[1];
};

struct CMPCrash;
struct CFunSegCache;

struct CDbgGlbls
{
  CTask *focus_task;
  const char *msg;
  int64_t msg_num;
  CMPCrash *mp_crash;
  uint8_t *int_fault_code;
  char *fix_file_line;
  CFunSegCache *fun_seg_cache;
  int64_t fun_seg_cache_index;
  bool panic;
};

static_assert(sizeof(CSema) == 128 && sizeof(CDbgGlbls) == 72 &&
              offsetof(CDbgGlbls, fix_file_line) == 40);

struct CRedSeaBoot
{
  uint8_t jump_and_nop[3], signature, reserved[4];
  uint64_t drv_offset, sects, root_clus, bitmap_sects, unique_id;
  uint8_t code[462];
  uint16_t signature2;
};

struct CRedSeaDirEntry
{
  uint16_t attr;
  char name[CDIR_FILENAME_LEN];
  uint64_t clus, size, datetime;
};

static_assert(sizeof(CBlkDev) == 124 && offsetof(CBlkDev, base0) == 32 &&
              offsetof(CBlkDev, drv_offset) == 44 &&
              offsetof(CBlkDev, max_blk) == 60 &&
              offsetof(CBlkDev, RAM_dsk) == 76);
static_assert(sizeof(CDrv) == 160 && offsetof(CDrv, root_clus) == 64 &&
              offsetof(CDrv, bd) == 104);
static_assert(sizeof(CRedSeaBoot) == BLK_SIZE &&
              offsetof(CRedSeaBoot, signature2) == 510);
static_assert(sizeof(CRedSeaDirEntry) == 64 &&
              offsetof(CRedSeaDirEntry, clus) == 40);

struct CIdtEntry
{
  uint16_t offset_lo, selector, attributes, offset_mid;
  uint32_t offset_hi, reserved;
};

struct [[gnu::packed]] CSysLimitBase
{
  uint16_t limit;
  uintptr_t base;
};

static_assert(sizeof(CIdtEntry) == 16 && offsetof(CIdtEntry, offset_hi) == 8);
static_assert(sizeof(CSysLimitBase) == 10 &&
              offsetof(CSysLimitBase, base) == 2);

struct CDevGlbls
{
  CIdtEntry *idt;
  uintptr_t mem64_ptr, uncached_alias;
  uint8_t mp_apic_ids[128];
  CMemRange mem32_head;
  CPCIDev pci_head;
};

static_assert(sizeof(CMemRange) == 0x28);
static_assert(offsetof(CDevGlbls, uncached_alias) == 0x10);
static_assert(offsetof(CDevGlbls, mem32_head) == 0x98);

static_assert(offsetof(CDevGlbls, pci_head) == 0xC0 &&
              sizeof(CDevGlbls) == 0xF0);

struct CQue
{
  CQue *next, *last;
};

constexpr int64_t QUE_VECT_U8_CNT = 512;

struct CQueVectU8
{
  CQueVectU8 *next, *last;
  int64_t total_cnt, node_cnt, min_idx;
  uint8_t body[QUE_VECT_U8_CNT];
};

static_assert(sizeof(CQue) == 16 && sizeof(CQueVectU8) == 552 &&
              offsetof(CQueVectU8, body) == 40);

struct CTaskStk
{
  CTaskStk *next_stk;
  int64_t stk_size, stk_ptr;
};

struct CExcept
{
  CExcept *next, *last;
  uintptr_t hndlr_catch, hndlr_untry, rsp, rbp;
  uint64_t rflags, rsi, rdi, r10, r11, r12, r13, r14, r15;
};

static_assert(sizeof(CExcept) == 120 && offsetof(CExcept, hndlr_catch) == 16 &&
              offsetof(CExcept, rsp) == 32 && offsetof(CExcept, rflags) == 48 &&
              offsetof(CExcept, rsi) == 56 && offsetof(CExcept, r15) == 112);

constexpr uint32_t HTT_INVALID = 0, HTT_EXPORT_SYS_SYM = 0x00001,
                   HTT_IMPORT_SYS_SYM = 0x00002, HTT_DEFINE_STR = 0x00004,
                   HTT_GLBL_VAR = 0x00008, HTT_CLASS = 0x00010,
                   HTT_INTERNAL_TYPE = 0x00020, HTT_FUN = 0x00040,
                   HTT_WORD = 0x00080, HTT_DICT_WORD = 0x00100,
                   HTT_KEYWORD = 0x00200, HTT_ASM_KEYWORD = 0x00400,
                   HTT_OPCODE = 0x00800, HTT_REG = 0x01000, HTT_FILE = 0x02000,
                   HTT_MODULE = 0x04000, HTT_HELP_FILE = 0x08000,
                   HTT_FRAME_PTR = 0x10000;
constexpr uint32_t HTG_TYPE_MASK = 0x1FFFF, HTG_FLAGS_MASK = 0xFF000000;
constexpr uint32_t HTG_SRC_SYM = HTT_DEFINE_STR | HTT_GLBL_VAR | HTT_FUN |
                                 HTT_CLASS | HTT_EXPORT_SYS_SYM | HTT_HELP_FILE;
constexpr uint32_t HTF_PRIVATE = 0x00800000, HTF_PUBLIC = 0x01000000,
                   HTF_IMPORT = 0x04000000, HTF_IMM = 0x08000000,
                   HTF_UNRESOLVED = 0x40000000;

struct CHash
{
  CHash *next;
  const char *str;
  uint32_t type, use_cnt;
};

struct CHashSrcSym
{
  CHash hash;
  const char *src_link, *idx;
  void *dbg_info;
  const char *import_name;
  void *ie_lst;
};

struct CHashExport
{
  CHashSrcSym sym;
  uint64_t val;
};

struct CHashDefineStr
{
  CHashSrcSym sym;
  char *data;
  const char **sub_idx;
  int64_t cnt;
};

static_assert(sizeof(CHashDefineStr) == 88 &&
              offsetof(CHashDefineStr, data) == 64);

struct CHashImport
{
  CHashSrcSym sym;
  uint8_t *module_base;
  const uint8_t *module_header_entry;
};

struct CHashGeneric
{
  CHash hash;
  uint64_t user_data0, user_data1, user_data2;
};

enum : uint8_t
{
  REGT_R8 = 1,
  REGT_R16,
  REGT_R32,
  REGT_R64,
  REGT_SEG,
  REGT_FSTK,
  REGT_MM,
  REGT_XMM,
};

#pragma pack(push, 1)
struct CHashReg
{
  CHash hash;
  uint8_t reg_num, reg_type;
};
#pragma pack(pop)

struct CInst
{
  uint8_t ins_entry_num, opcode_cnt, opcode[4];
  uint16_t flags;
  uint8_t slash_val, uasm_slash_val, opcode_modifier, arg1, arg2, size1, size2,
      pad;
};

struct CHashOpcode
{
  CHash hash;
  uint16_t inst_entry_cnt, oc_flags, pad[2];
  CInst ins[1];
};

static_assert(sizeof(CHashReg) == 26 && offsetof(CHashReg, reg_num) == 24);
static_assert(sizeof(CInst) == 16 && offsetof(CInst, flags) == 6 &&
              sizeof(CHashOpcode) == 48 && offsetof(CHashOpcode, ins) == 32);

struct CHashClass;

struct CArrayDim
{
  CArrayDim *next;
  int64_t cnt, total_cnt;
};

struct CMemberLstMeta
{
  CMemberLstMeta *next;
  const char *str;
  uint64_t flags, user_data;
};

struct CMemberLst
{
  CMemberLst *next, *left, *right, *left_class_base, *right_class_base;
  const char *str;
  CHashClass *member_class, *member_class_base;
  CMemberLstMeta *meta;
  uint32_t use_cnt;
  uint16_t flags;
  int8_t reg, pad;
  int64_t offset, size;
  CArrayDim dim;
  uint8_t *static_data;
  union {
    int64_t static_data_rip;
    uint64_t dft_val;
  };
  CHashFun *fun_ptr;
};

struct CHashClass
{
  CHashSrcSym sym;
  int64_t size, neg_offset;
  uint32_t member_cnt;
  uint8_t ptr_stars_cnt, raw_type;
  uint16_t flags;
  CMemberLst *member_lst_and_root, *member_class_base_root, *last_in_member_lst;
  CHashClass *base_class, *fwd_class;
};

struct CHashFun
{
  CHashClass cls;
  CHashClass *return_class;
  uint32_t arg_cnt, pad, used_reg_mask, clobbered_reg_mask;
  uint8_t *exe_addr;
  void *ext_lst;
};

struct CHashGlblVar
{
  CHashSrcSym sym;
  int64_t size, flags;
  CHashClass *var_class;
  CHashFun *fun_ptr;
  CArrayDim dim;
  uint8_t *data_addr;
  union {
    int64_t data_addr_rip;
    void *heap_glbl;
  };
};

constexpr uint16_t CF_EXTERN = 1, CF_INTERNAL_TYPE = 1 << 1,
                   FF_INTERNAL = 1 << 12;
constexpr uint16_t FF_ARGPOP = 1 << 10, FF_NOARGPOP = 1 << 11,
                   FF__EXTERN = 1 << 13, FF_DOT_DOT_DOT = 1 << 14,
                   FF_RET1 = 1 << 15;
constexpr uint16_t MLF_DFT_AVAILABLE = 1, MLF_LASTCLASS = 2,
                   MLF_STR_DFT_AVAILABLE = 4, MLF_FUN = 8,
                   MLF_DOT_DOT_DOT = 16, MLF_STATIC = 64;

inline bool FunPopsArgs(const CHashFun &fun)
{
  auto flags = fun.cls.flags;
  return (flags & (FF_RET1 | FF_ARGPOP)) && !(flags & FF_NOARGPOP);
}
constexpr uint64_t MLMF_IS_STR = 1, GVF_IMPORT = 2, GVF_EXTERN = 4, GVF_ALIAS = 16,
                   GVF_ARRAY = 32;

static_assert(sizeof(CMemberLst) == 144 && offsetof(CMemberLst, dim) == 96 &&
              offsetof(CMemberLst, fun_ptr) == 136);
static_assert(sizeof(CHashClass) == 128 && offsetof(CHashClass, flags) == 86 &&
              offsetof(CHashClass, member_lst_and_root) == 88);
static_assert(sizeof(CHashFun) == 168 && offsetof(CHashFun, exe_addr) == 152);
static_assert(sizeof(CHashGlblVar) == 136 &&
              offsetof(CHashGlblVar, data_addr) == 120);

static_assert(sizeof(CHash) == 24 && sizeof(CHashSrcSym) == 64 &&
              sizeof(CHashExport) == 72 && sizeof(CHashImport) == 80);

struct CHashTable
{
  CHashTable *next;
  int64_t mask, locked_flags;
  CHash **body;
};

enum : uint8_t
{
  DOCT_TEXT,
  DOCT_NEW_LINE,
  DOCT_SOFT_NEW_LINE,
  DOCT_TAB,
  DOCT_PAGE_BREAK,
  DOCT_CURSOR,
  DOCT_MARKER,
  DOCT_PMT,
  DOCT_CLEAR,
  DOCT_PAGE_LEN,
  DOCT_LEFT_MARGIN,
  DOCT_RIGHT_MARGIN,
  DOCT_HEADER,
  DOCT_FOOTER,
  DOCT_INDENT,
  DOCT_FOREGROUND,
  DOCT_BACKGROUND,
  DOCT_DFT_FOREGROUND,
  DOCT_DFT_BACKGROUND,
  DOCT_WORD_WRAP,
  DOCT_HIGHLIGHT,
  DOCT_BLINK,
  DOCT_INVERT,
  DOCT_UNDERLINE,
  DOCT_SHIFTED_X,
  DOCT_SHIFTED_Y,
  DOCT_CURSOR_MOVEMENT,
  DOCT_ANCHOR,
  DOCT_LINK,
  DOCT_BTTN,
  DOCT_DATA,
  DOCT_CHECK_BOX,
  DOCT_LST,
  DOCT_MACRO,
  DOCT_MENU_VAL,
  DOCT_HEX_ED,
  DOCT_TREE,
  DOCT_SPRITE,
  DOCT_INS_BIN,
  DOCT_INS_BIN_SIZE,
  DOCT_SONG,
  DOCT_HTML_CODE,
  DOCT_ERROR,
  DOCT_TYPES_NUM
};

constexpr uint64_t DOCF_PLAIN_TEXT = 1, DOCF_PLAIN_TEXT_TABS = 2,
                   DOCF_NO_CURSOR = 8, DOCF_CARRIAGE_RETURN = 16,
                   DOCF_DBL_DOLLARS = 32, DOCF_COLOR_NAMES = 64,
                   DOCF_BREAK_UNLOCKED = 0x400000;
constexpr uint64_t DOCEF_TAG = 1, DOCEF_LEN = 2, DOCEF_AUX_STR = 4,
                   DOCEF_DEFINE = 8, DOCEF_HTML_LINK = 16, DOCEF_LEFT_EXP = 32,
                   DOCEF_LEFT_MACRO = 64, DOCEF_RIGHT_EXP = 128,
                   DOCEF_RIGHT_MACRO = 256, DOCEF_HAS_BIN = 512,
                   DOCEF_BIN_PTR_LINK = 1024, DOCEF_RAW_TYPE = 2048,
                   DOCEF_HIGHLIGHT = 1ull << 26, DOCEF_WORD_WRAP = 1ull << 27,
                   DOCEF_TAG_CB = 1ull << 32,
                   DOCEF_REMALLOC_DATA = 1ull << 48, DOCEF_TREE = 1ull << 52,
                   DOCEF_LST = 1ull << 53, DOCEF_SKIP = 1ull << 54,
                   DOCEF_FILTER_SKIP = 1ull << 57, DOCEF_DFT_LEN = 1ull << 60,
                   DOCEF_DFT_RAW_TYPE = 1ull << 61;
constexpr uint32_t DOCG_BL_IV_UL = 0xB0000000, DOC_SIGNATURE_VAL = 0x53636F44;
constexpr int64_t DOC_DFT = INT32_MIN;

struct CDocSettings
{
  uint32_t final_u32_attr;
  int16_t left_margin, right_margin, indent;
  uint16_t page_len, header, footer;
  int8_t shifted_x, shifted_y;
  uint8_t state, comment_depth, paren_depth, brace_depth, cur_text_attr,
      dft_text_attr;
};

struct CDocBin
{
  CDocBin *next, *last;
  int32_t tmp_use_cnt, renum_num;
  char *tag;
  uint32_t num, flags, size, use_cnt;
  uint8_t *data;
};

struct CDocEntryBase
{
  CDocEntryBase *next, *last;
  char *tag;
  uint32_t type;
  int32_t page_line_num;
  uint64_t de_flags;
  int32_t x, y;
  uint32_t min_col, max_col;
  CDocSettings settings;
  int64_t user_data;

  uint8_t Type() const
  {
    return type;
  }
};

struct CDocEntry
{
  CDocEntryBase base;
  int64_t attr;
  char *left_macro;
  int64_t right_exp;
  char *right_macro;
  uintptr_t tag_cb;
  char *define_str, *aux_str, *bin_ptr_link, *html_link, *my_fmt_data;
  int64_t hex_ed_width;
  int32_t scroll_len, len, bin_num;
  uint8_t raw_type, pad[3];
  CDocBin *bin_data;
  uint8_t *data;
};

struct CEdFindText
{
  char find_text[144], replace_text[144];
  bool replace, scan_fwd, scan_sel_text, match_case, whole_labels, local_var,
      pmt, pad;
  int64_t filter_lines;
};

struct CEdFileName
{
  CDirContext *dirc;
  char name[256];
};

struct CDocUndo
{
  CDocUndo *next, *last;
  int64_t size, doc_flags, time_stamp;
  uint8_t *body;
};

struct CDoc
{
  CDocEntryBase head;
  uint64_t flags, locked_flags;
  CDocEntryBase *cur_entry, *old_cur_entry;
  int32_t cur_col, old_cur_col, line_start_col, top_line_num, dollar_buf_size,
      dollar_buf_ptr;
  char *dollar_buf;
  CTask *win_task, *mem_task, *owning_task;
  int32_t page_line_num, undo_cnt, x, y, min_x, max_x, min_y, max_y;
  int64_t line, col, best_d, old_win_top, old_win_bottom, old_win_left,
      old_win_right, cmd_U8;
  uint32_t doc_signature, cur_bin_num;
  int64_t max_entries, updates_cnt;
  CEdFindText *find_replace;
  CEdFileName filename;
  uint64_t file_attr;
  uintptr_t left_click_link, right_click_link;
  uint8_t *user_put_data;
  uintptr_t user_put_key, user_put_s;
  CDoc *parent_doc;
  uint64_t desc;
  CDocBin bin_head;
  CDocSettings settings_head;
  CDocUndo undo_head;
  int64_t user_data;
};

static_assert(sizeof(CDocSettings) == 24 && sizeof(CEdFindText) == 304);
static_assert(sizeof(CDocBin) == 56 && offsetof(CDocBin, num) == 32 &&
              offsetof(CDocBin, data) == 48);
static_assert(sizeof(CDocEntryBase) == 88 &&
              offsetof(CDocEntryBase, tag) == 16 &&
              offsetof(CDocEntryBase, settings) == 56 &&
              sizeof(CDocEntry) == 208 && offsetof(CDocEntry, bin_data) == 192);
static_assert(sizeof(CDoc) == 768 && offsetof(CDoc, cur_entry) == 104 &&
              offsetof(CDoc, filename) == 304 &&
              offsetof(CDoc, bin_head) == 632);

struct CLexFile
{
  CLexFile *next;
  const char *buf, *buf_ptr;
  int64_t line_num, flags;
  const char *full_name, *line_start;
  CDoc *doc;
  CDocEntryBase *cur_entry;
  int32_t depth;
  uint8_t last_U16, pad[3];
};

struct CLexHashTableContext
{
  CLexHashTableContext *next;
  int64_t old_flags, hash_mask;
  CHashFun *local_var_lst, *fun;
  CHashTable *hash_table_lst, *define_hash_table, *local_hash_table,
      *glbl_hash_table;
};

struct CStreamBlk
{
  CStreamBlk *next, *last;
  char *body;
  bool error;
};

// The prefix through opts is shared with HolyC code.
struct CCmpCtrl
{
  CCmpCtrl *next, *last;
  int64_t token, flags, cur_i64;
  uint64_t cur_f64;
  const char *cur_str;
  int64_t cur_str_len, class_dol_offset;
  const char *dollar_buf;
  int64_t dollar_cnt;
  const char *cur_help_idx;
  int64_t last_U16, min_line, max_line, last_line_num, lock_cnt;
  const uint32_t *char_bmp_alpha_numeric;
  CLexHashTableContext htc;
  CHash *hash_entry;
  uint64_t abs_cnts;
  void *asm_undef_hash;
  CMemberLst *local_var_entry;
  void *lb_leave;
  const char *cur_buf_ptr;
  CLexFile *lex_include_stk, *lex_prs_stk, *fun_lex_file;
  CStreamBlk *next_stream_blk, *last_stream_blk;
  CAOT *aot;
  int64_t pass, opts;
  int64_t jit_exception;
};

static_assert(sizeof(CLexFile) == 80 && offsetof(CLexFile, full_name) == 40);
static_assert(offsetof(CCmpCtrl, cur_help_idx) == 88 &&
              offsetof(CCmpCtrl, lex_include_stk) == 264 &&
              offsetof(CCmpCtrl, opts) == 320);

struct CJobCtrl;

struct CJob
{
  CJob *next, *last;
  CJobCtrl *ctrl;
  int64_t job_code, flags, msg_code;
  int64_t (*addr)(uint8_t *fun_arg);
  uint8_t *fun_arg, *aux_str;
  int64_t aux1, aux2, res;
  CTask *spawned_task, *master_task;
};

struct CJobCtrl
{
  CJob *next_waiting, *last_waiting, *next_done, *last_done;
  int64_t flags;
};

struct CWinScroll
{
  int64_t min, pos, max;
  uint32_t flags;
  uint8_t color, pad[3];
};

constexpr int64_t JOBF_WAKE_MASTER = 1 << 0, JOBF_FOCUS_MASTER = 1 << 1,
                  JOBF_EXIT_ON_COMPLETE = 1 << 2, JOBF_DONT_FILTER = 1 << 3,
                  JOBF_HIGHEST_PRIORITY = 1 << 4, JOBF_DONE = 1 << 5,
                  JOBF_DISPATCHED = 1 << 6, JOBF_FREE_ON_COMPLETE = 1 << 7,
                  JOBF_ADD_TO_QUE = 1 << 8;
enum : int64_t
{
  JOBT_TEXT_INPUT, JOBT_MSG, JOBT_EXE_STR, JOBT_CALL, JOBT_SPAWN_TASK,
};
enum : int64_t
{
  MSG_NULL, MSG_CMD, MSG_KEY_DOWN, MSG_KEY_UP, MSG_MS_MOVE, MSG_MS_L_DOWN,
  MSG_MS_L_UP, MSG_MS_L_D_DOWN, MSG_MS_L_D_UP, MSG_MS_R_DOWN, MSG_MS_R_UP,
  MSG_MS_R_D_DOWN, MSG_MS_R_D_UP,
};
constexpr uint64_t SCF_KEY_DESC = 1ULL << 31;
constexpr uint32_t WIF_SELF_FOCUS = 1, WIF_SELF_KEY_DESC = 1 << 12;

static_assert(sizeof(CJob) == 0x70 && offsetof(CJob, aux1) == 0x48 &&
              offsetof(CJob, master_task) == 0x68);
static_assert(sizeof(CJobCtrl) == 0x28 && offsetof(CJobCtrl, next_done) == 0x10 &&
              offsetof(CJobCtrl, flags) == 0x20);

constexpr uint32_t TASKF_KILL_TASK = 1 << 1, TASKF_SUSPENDED = 1 << 2,
                   TASKF_IDLE = 1 << 3, TASKF_CMD_LINE_PMT = 1 << 4,
                   TASKF_INPUT_FILTER_TASK = 1 << 5, TASKF_FILTER_INPUT = 1 << 6,
                   TASKF_AWAITING_MSG = 1 << 9;
constexpr int64_t DYING_JIFFIES = JIFFY_FREQ / 5;

struct [[gnu::may_alias]] CTaskDying
{
  CTaskDying *next, *last;
  int64_t wake_jiffy;
};
constexpr uint32_t DISPLAYF_NOT_RAW = 1 << 1, DISPLAYF_SILENT = 1 << 2;
constexpr uint8_t TTS_TASK_NAME = 2;

struct CTask
{
  CTask *addr;
  uint32_t task_signature, win_inhibit;
  int64_t wake_jiffy;
  uint32_t task_flags, display_flags;
  CHeapCtrl *code_heap, *data_heap;
  CDoc *put_doc, *display_doc, *border_doc;
  int64_t win_left, win_right, win_top, win_bottom;
  CDrv *cur_dv;
  char *cur_dir;
  CTask *parent_task, *next_task, *last_task;
  CTask *next_input_filter_task, *last_input_filter_task;
  CTask *next_sibling_task, *last_sibling_task, *next_child_task,
      *last_child_task;
  int64_t win_width, win_height, pix_left, pix_right, pix_width;
  int64_t pix_top, pix_bottom, pix_height, scroll_x, scroll_y, scroll_z;
  int64_t rip, rflags, rsp, rsi, rax, rcx, rdx, rbx, rbp, rdi;
  int64_t r8, r9, r10, r11, r12, r13, r14, r15;
  CCPU *gs;
  CFPU *fpu_mmx;
  int64_t swap_cnter;
  void (*draw_it)(CTask *task, CDC *dc);
  uint8_t task_title[144], task_name[32], wallpaper_data[144];
  uint8_t title_src, border_src, text_attr, border_attr;
  uint16_t win_z_num, pad;
  CTaskStk *stk;
  CExcept *next_except, *last_except;
  int64_t except_rbp, except_ch;
  uint8_t *except_callers[8];
  bool catch_except, new_answer;
  uint8_t answer_type, answer_pad[5];
  int64_t answer;
  double answer_time;
  CBpt *bpt_lst;
  CCtrl *next_ctrl, *last_ctrl;
  CMenu *cur_menu;
  CTaskSettings *next_settings;
  CMathODE *next_ode, *last_ode;
  double last_ode_time;
  CHashTable *hash_table;
  CJobCtrl srv_ctrl;
  CCmpCtrl *next_cc, *last_cc;
  CHashFun *last_fun;
  void (*task_end_cb)();
  CTask *song_task, *animate_task;
  int64_t rand_seed, task_num, fault_num, fault_err_code;
  CTask *popup_task, *dbg_task;
  CWinScroll horz_scroll, vert_scroll;
  int64_t user_data;
};

struct alignas(4096) CSysFixedArea
{
  CFPU init_fpu_mmx;
  CCPU boot_cpu;
  CTask adam;
  CBlkPool sys_code_bp;
  CHeapCtrl adam_hc;
};

static_assert(sizeof(CMemBlk) == 0x18 && offsetof(CMemBlk, pags) == 0x14);
static_assert(sizeof(CMemUsed) == 0x10 && sizeof(CMemUnused) == 0x10);
static_assert(offsetof(CBlkPool, mem_free_lst) == 0x18);
static_assert(offsetof(CHeapCtrl, hc_signature) == 8);
static_assert(offsetof(CHeapCtrl, next_um) == 0x50);
static_assert(offsetof(CTask, code_heap) == 0x20 &&
              offsetof(CTask, rsp) == 0x128);
static_assert(offsetof(CTask, task_flags) == 0x18 &&
              offsetof(CTask, next_task) == 0x80);
static_assert(offsetof(CTask, wake_jiffy) == 0x10 &&
              offsetof(CTaskDying, wake_jiffy) == offsetof(CTask, wake_jiffy));
static_assert(sizeof(CTaskStk) == 24 && offsetof(CTaskStk, stk_size) == 8);
static_assert(offsetof(CTask, rip) == 0x118 &&
              offsetof(CTask, rflags) == 0x120);
static_assert(offsetof(CTask, rsi) == 0x130 && offsetof(CTask, rax) == 0x138);
static_assert(offsetof(CTask, rcx) == 0x140 && offsetof(CTask, r15) == 0x1A0);
static_assert(offsetof(CTask, fpu_mmx) == 0x1B0 &&
              offsetof(CTask, swap_cnter) == 0x1B8);
static_assert(offsetof(CTask, fault_num) == 0x440 &&
              offsetof(CTask, fault_err_code) == 0x448);
static_assert(offsetof(CCPU, cpu_flags) == 0x10 &&
              offsetof(CCPU, idle_pt_hits) == 0x20);
static_assert(offsetof(CCPU, total_jiffies) == 0x30 &&
              offsetof(CCPU, swap_cnter) == 0x50);
static_assert(offsetof(CTask, stk) == 0x310 && sizeof(CTask) == 0x4A8);
static_assert(offsetof(CTask, last_except) == 0x320 &&
              offsetof(CTask, catch_except) == 0x378);
static_assert(offsetof(CSysFixedArea, adam) == 0x300);
static_assert(offsetof(CSysFixedArea, sys_code_bp) == 0x7A8);
static_assert(offsetof(CSysFixedArea, adam_hc) == 0x1188);
static_assert(sizeof(CSysFixedArea) == 0x2000);

extern "C" {
extern const uint8_t SYS_KERNEL[], SYS_COMPILE_TIME[];
extern uint32_t SYS_BOOT_SRC, SYS_BOOT_BLK, SYS_RUN_LEVEL, MEM_BOOT_BASE;
extern uint32_t SYS_PCIBIOS_SERVICE_DIR, SYS_PCI_SERVICES;
extern uint32_t SYS_FONT_ADDRESS, SYS_E820_COUNT, MEM_PML2, MEM_PML4,
    MEM_HEAP_BASE;
extern uint64_t MEM_PHYSICAL_SPACE, MEM_MAPPED_SPACE, MEM_HEAP_LIMIT;
extern uint16_t MEM_E801[2];
extern CMemE820 MEM_E820[48];
extern CBlkPool *SYS_CODE_BP, *SYS_DATA_BP;
}

void BootPutS(const char *text);
void SerialPutS(const char *text);
void SerialPutHex(uint64_t value);

inline void OutU8(uint16_t port, uint8_t value)
{
  asm volatile("outb %0,%1" : : "a"(value), "Nd"(port));
}

inline uint8_t InU8(uint16_t port)
{
  uint8_t value;
  asm volatile("inb %1,%0" : "=a"(value) : "Nd"(port));
  return value;
}

inline void OutU16(uint16_t port, uint16_t value)
{
  asm volatile("outw %0,%1" : : "a"(value), "Nd"(port));
}

inline uint16_t InU16(uint16_t port)
{
  uint16_t value;
  asm volatile("inw %1,%0" : "=a"(value) : "Nd"(port));
  return value;
}
