#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr uint32_t RLF_VGA = 1 << 1;
constexpr uint32_t RLF_FULL_HEAPS = 1 << 7;
constexpr uint32_t RLF_INTERRUPTS = 1 << 9;
constexpr uint32_t RLF_COMPILER = 1 << 12;
constexpr uint32_t RLF_DOC = 1 << 13;
constexpr uint32_t RLF_REGISTRY = 1 << 15;
constexpr uint32_t RLF_AUTO_COMPLETE = 1 << 17;
constexpr uint32_t RLF_ADAM_SERVER = 1 << 18;
constexpr uint32_t RLF_ONCE_ADAM = 1 << 19;
constexpr uint32_t RLF_ONCE_USER = 1 << 20;

struct CTask;
struct CTaskDying;
struct CTSS;

struct CDate
{
  uint32_t time;
  int32_t date;
};

static_assert(sizeof(CDate) == 8 && offsetof(CDate, date) == 4);

inline uint64_t DateBits(CDate cdt)
{
  return (uint64_t(uint32_t(cdt.date)) << 32) | cdt.time;
}

inline CDate DateFromBits(uint64_t bits)
{
  return {uint32_t(bits), int32_t(bits >> 32)};
}

struct CDateStruct
{
  uint8_t sec10000, sec100, sec, min, hour, day_of_week, day_of_mon, mon;
  int32_t year;
};

static_assert(sizeof(CDateStruct) == 12 && offsetof(CDateStruct, year) == 8);

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
constexpr uintptr_t SYS_16MEG_AREA_LIMIT = 0x1000000;
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
struct CTaskSettings;
struct CMenu;
struct CMathODE;
struct CJob;
struct CCmpCtrl;
struct CAOT;
struct CAOTHeapGlbl;
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

static_assert(sizeof(CPCIDev) == 0x30 &&
              offsetof(CPCIDev, vendor) == 0x10 &&
              offsetof(CPCIDev, sub_code) == 0x18 &&
              offsetof(CPCIDev, vendor_str) == 0x20);

constexpr unsigned I_DIV_ZERO = 0, I_NMI = 2, I_TIMER = 0x20, I_KBD = 0x21,
                   I_MS = 0x2C;
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
constexpr uint16_t FSt_REDSEA = 1, FSt_FAT32 = 2;
constexpr uint16_t FSG_TYPE_MASK = 0x7FFF;
constexpr unsigned FStf_DISABLE = 15;
constexpr uint8_t MBR_PT_REDSEA = 0x88;
constexpr uint32_t BD_SIGNATURE_VAL = 0x56534442;  // BDSV
constexpr uint32_t DRV_SIGNATURE_VAL = 0x56535644; // DVSV
constexpr uint32_t BDF_REMOVABLE = 1, BDF_INITIALIZED = 2, BDF_READ_ONLY = 4,
                   BDF_READ_ONLY_OVERRIDE = 8, BDF_LAST_WAS_WRITE = 0x10,
                   BDF_READ_CACHE = 0x20, BDF_INIT_IN_PROGRESS = 0x80;

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
constexpr uint64_t FUF_RECURSE = 1, FUF_SINGLE = 0x200, FUF_JUST_DIRS = 0x400,
                   FUF_JUST_FILES = 0x800, FUF_JUST_TXT = 0x1000,
                   FUF_JUST_DD = 0x2000, FUF_JUST_SRC = 0x4000,
                   FUF_JUST_AOT = 0x8000, FUF_JUST_JIT = 0x10000,
                   FUF_JUST_GR = 0x20000, FUF_Z_OR_NOT_Z = 0x40000,
                   FUF_CLUS_ORDER = 0x80000, FUF_SCAN_PARENTS = 0x100000,
                   FUF_FLATTEN_TREE = 0x200000;
constexpr uint64_t FUG_FILES_FIND =
    FUF_RECURSE | FUF_SINGLE | FUF_CLUS_ORDER | FUF_JUST_DIRS | FUF_JUST_FILES |
    FUF_JUST_TXT | FUF_JUST_DD | FUF_JUST_SRC | FUF_JUST_AOT | FUF_JUST_JIT |
    FUF_JUST_GR | FUF_FLATTEN_TREE;
constexpr uint64_t FUG_FILE_FIND =
    FUF_JUST_DIRS | FUF_JUST_FILES | FUF_Z_OR_NOT_Z | FUF_SCAN_PARENTS;
constexpr char FILEMASK_JIT[] = "*.HC*;*.HH*",
               FILEMASK_AOT[] = "*.HC*;*.HH*;*.PRJ*",
               FILEMASK_SRC[] = "*.HC*;*.HH*;*.IN*;*.PRJ*",
               FILEMASK_DD[] = "*.HC*;*.HH*;*.IN*;*.PRJ*;*.DD*",
               FILEMASK_TXT[] = "*.HC*;*.HH*;*.IN*;*.PRJ*;*.DD*;*.TXT*",
               FILEMASK_GR[] = "*.GR*";

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

struct CFreeLst
{
  CFreeLst *next, *last;
  int64_t start, size;
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

constexpr size_t DSK_CACHE_HASH_SIZE = 0x2000;

struct CCacheBlk
{
  CCacheBlk *next_lru, *last_lru;
  CCacheBlk *next_hash, *last_hash;
  CDrv *dv;
  int64_t blk;
  uint8_t body[BLK_SIZE];
};

static_assert(sizeof(CCacheBlk) == 560 &&
              offsetof(CCacheBlk, next_lru) == 0 &&
              offsetof(CCacheBlk, last_lru) == 8 &&
              offsetof(CCacheBlk, next_hash) == 16 &&
              offsetof(CCacheBlk, last_hash) == 24 &&
              offsetof(CCacheBlk, dv) == 32 &&
              offsetof(CCacheBlk, blk) == 40 &&
              offsetof(CCacheBlk, body) == 48);

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
  SEMA_REFRESH_IN_PROGRESS = 8,
  SEMA_FLUSH_VGA_IMAGE = 9,
  SEMA_SINGLE_USER = 10,
  SEMA_DSK_CACHE = 11,
  SEMA_DEV_MEM = 13,
  SEMA_VGA = 14,
  SEMA_UPDATE_WIN_Z_BUF = 15,
  SEMA_MUTE = 17,
  SEMA_JUST_PUMP_MSGS = 18,
  SEMA_FIX = 20,
  SEMA_SEMAS_NUM = 21,
};

struct CSema
{
  uint64_t flags;
  uint8_t pad[120];
};

static_assert(sizeof(CSema) == 128 && SEMA_SINGLE_USER == 10);

enum : uint8_t
{
  BLACK,BLUE,GREEN,CYAN,RED,PURPLE,BROWN,LTGRAY,
  DKGRAY,LTBLUE,LTGREEN,LTCYAN,LTRED,LTPURPLE,YELLOW,WHITE,
  COLORS_NUM,
};

struct CBGR48
{
  uint16_t b,g,r,pad;

  constexpr CBGR48(uint64_t value = 0)
    : b(value), g(value>>16), r(value>>32), pad(value>>48) {}
};

static_assert(sizeof(CBGR48) == 8 && offsetof(CBGR48, b) == 0 &&
              offsetof(CBGR48, g) == 2 && offsetof(CBGR48, r) == 4 &&
              offsetof(CBGR48, pad) == 6);

constexpr uint8_t TRANSPARENT = 0xFF, COLOR_INVALID = 16, COLOR_MONO = 0xFF;
constexpr uint8_t ROPB_EQU = 0, ROPB_XOR = 1, ROPB_COLLISION = 2, ROPB_MONO = 3,
                  ROPBF_HALF_RANGE_COLOR = 0x10, ROPBF_TWO_SIDED = 0x20,
                  ROPBF_DITHER = 0x40, ROPBF_PROBABILITY_DITHER = 0x80;
constexpr uint32_t ROPF_HALF_RANGE_COLOR = 0x1000, ROPF_TWO_SIDED = 0x2000,
                   ROPF_DITHER = 0x40000000,
                   ROPF_PROBABILITY_DITHER = 0x80000000;

struct CColorROPU16
{
  uint8_t color, rop;
};

struct CColorROPU32
{
  CColorROPU16 c0, c1;
};

struct CD3I32
{
  int32_t x, y, z;
};

constexpr int64_t GR_WIDTH = 640, GR_HEIGHT = 480;

constexpr int64_t PROGRESS_BARS_NUM = 4, PROGRESS_DESC_LEN = 64 - 8 - 8;
struct CProgress
{
  int64_t val, max;
  uint64_t t0, tf;
  char desc[PROGRESS_DESC_LEN];
};
static_assert(sizeof(CProgress) == 80 && offsetof(CProgress, max) == 8 &&
              offsetof(CProgress, t0) == 16 && offsetof(CProgress, tf) == 24 &&
              offsetof(CProgress, desc) == 32);
constexpr int64_t TEXT_COLS = 80, TEXT_ROWS = 60;
constexpr int64_t GR_SCALE = int64_t{1} << 32;
enum : uint32_t
{
  DCF_COMPRESSED = 1,
  DCF_PALETTE = 2,
  DCF_NO_TRANSPARENTS = 4,
  DCF_TRANSFORMATION = 0x100,
  DCF_SYMMETRY = 0x200,
  DCF_JUST_MIRROR = 0x400,
  DCF_LOCATE_NEAREST = 0x800,
  DCF_DONT_DRAW = 0x1000,
  DCF_ALIAS = 0x2000,
  DCF_SCRN_BITMAP = 0x4000,
  DCF_FILL_NOT_COLOR = 0x8000,
  DCF_RECORD_EXTENTS = 0x10000,
  DCF_ON_TOP = 0x20000,
  DCSF_COMPRESSED = 1,
  DCSF_PALETTE_GET = 2,
  DCS_SIGNATURE_VAL = 0x53437644, // 'DvCS'
};

struct CGrSym
{
  int32_t sx, sy, sz, pad;
  int64_t snx, sny, snz;
};

struct CDC
{
  uint64_t cdt;
  int32_t x0, y0, width, width_internal, height, flags;
  CBGR48 palette[COLORS_NUM];
  CColorROPU32 color, bkcolor, color2;
  CD3I32 ls;
  uint64_t dither_probability_u16;
  CDC *brush;
  int64_t *r, r_norm;
  int32_t x, y, z, thick;
  // These callbacks use the HolyC stack convention.
  void (*transform)(CDC *dc, int64_t *x, int64_t *y, int64_t *z);
  void (*lighting)(CDC *dc, CD3I32 *p1, CD3I32 *p2, CD3I32 *p3, uint32_t color);
  CGrSym sym;
  int32_t cur_x, cur_y, cur_z, pad;
  int64_t collision_cnt, nearest_dist, min_x, max_x, min_y, max_y;
  uint32_t dc_signature, signature_pad;
  CTask *mem_task, *win_task;
  CDC *alias;
  uint8_t *body;
  int32_t *depth_buf;
  int64_t db_z;
};

static_assert(sizeof(CColorROPU16) == 2 && sizeof(CColorROPU32) == 4 &&
              sizeof(CD3I32) == 12 && sizeof(CGrSym) == 40);
static_assert(offsetof(CColorROPU16, color) == 0 &&
              offsetof(CColorROPU16, rop) == 1 &&
              offsetof(CColorROPU32, c0) == 0 &&
              offsetof(CColorROPU32, c1) == 2);
static_assert(sizeof(CDC) == 408 && offsetof(CDC, flags) == 28 &&
              offsetof(CDC, palette) == 32 && offsetof(CDC, color) == 160 &&
              offsetof(CDC, ls) == 172 && offsetof(CDC, r) == 200 &&
              offsetof(CDC, transform) == 232 && offsetof(CDC, sym) == 248 &&
              offsetof(CDC, dc_signature) == 352 &&
              offsetof(CDC, mem_task) == 360 && offsetof(CDC, body) == 384 &&
              offsetof(CDC, depth_buf) == 392 && offsetof(CDC, db_z) == 400);

enum : uint16_t
{
  VGAP_ATTR_INDEX = 0x03C0,
  VGAP_ATTR_DATA_WRITE = 0x03C0,
  VGAP_PALETTE_MASK = 0x03C6,
  VGAP_REG_READ = 0x03C7,
  VGAP_REG_WRITE = 0x03C8,
  VGAP_PALETTE_DATA = 0x03C9,
  VGAP_INPUT_STAT = 0x03DA,
};

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

struct [[gnu::packed]] CFAT32Boot
{
  uint8_t jump_and_nop[3], oem_name[8];
  uint16_t bytes_per_sect;
  uint8_t sects_per_clus;
  uint16_t reserved_sects;
  uint8_t copies_of_fat;
  uint16_t max_root_dir_entries, old_sects_in_drv;
  uint8_t media_desc;
  uint16_t old_sects_per_fat, sects_per_track, num_heads;
  uint32_t hidden_sects, sects, sects_per_fat;
  uint16_t flags, version;
  uint32_t root_clus;
  uint16_t file_system_info_sect, backup_boot_sect;
  uint8_t reserved[12], log_drv_num, unused, ext_signature;
  uint32_t serial_num;
  uint8_t vol_name[11], fat_name[8], code[420];
  uint16_t signature;
};

struct [[gnu::packed]] CFAT32FileInfoSect
{
  uint32_t signature1;
  uint8_t unknown[480];
  uint32_t signature2, free_clus, most_recently_alloced;
  uint8_t reserved[12];
  uint32_t signature3;
};

struct [[gnu::packed]] CFAT32DirEntry
{
  uint8_t name[11], attr, NTres, CrtTimeTenth;
  uint16_t CrtTime, CrtDate, LstAccDate, clus_hi, WrtTime, WrtDate, clus_lo;
  uint32_t size;
};

struct [[gnu::packed]] CFAT32DirEntryLong
{
  uint8_t ord;
  uint16_t name1[5];
  uint8_t attr, type, xsum;
  uint16_t name2[6], zero, name3[2];
};

static_assert(sizeof(CFAT32Boot)==512 && offsetof(CFAT32Boot,root_clus)==44 &&
              offsetof(CFAT32Boot,signature)==510);
static_assert(sizeof(CFAT32FileInfoSect)==512 && sizeof(CFAT32DirEntry)==32 &&
              offsetof(CFAT32DirEntry,WrtTime)==22 &&
              sizeof(CFAT32DirEntryLong)==32);

struct CRedSeaDirEntry
{
  uint16_t attr;
  char name[CDIR_FILENAME_LEN];
  uint64_t clus, size, datetime;
};

struct CDirEntry
{
  CDirEntry *next, *parent, *sub;
  char *full_name;
  int64_t user_data, user_data2;
  uint16_t attr;
  char name[CDIR_FILENAME_LEN];
  int64_t clus, size;
  CDate datetime;
};

constexpr int64_t FFB_NEXT_BLK = INT64_MAX;
constexpr int64_t FF_WRITE = 1, FF_NEW_FILE = 2, FF_BUF_DIRTY = 4,
                  FF_NEEDS_WRITE = 8, FF_CONTIGUOUS = 16,
                  FF_USE_OLD_DATETIME = 32;

struct CFile
{
  int64_t flags;
  CDirEntry de;
  CDrv *dv;
  int64_t fblk_num, clus, file_clus_num, max_blk;
  uint8_t *clus_buf;
};

static_assert(sizeof(CFile) == 168 && offsetof(CFile, de) == 8 &&
              offsetof(CFile, dv) == 120 && offsetof(CFile, fblk_num) == 128 &&
              offsetof(CFile, clus) == 136 && offsetof(CFile, file_clus_num) == 144 &&
              offsetof(CFile, max_blk) == 152 && offsetof(CFile, clus_buf) == 160);

struct CDirContext
{
  CDrv *old_dv, *dv;
  char *old_dir, *mask;
};

static_assert(sizeof(CDirContext) == 32 && offsetof(CDirContext, dv) == 8 &&
              offsetof(CDirContext, old_dir) == 16 &&
              offsetof(CDirContext, mask) == 24);

static_assert(sizeof(CDirEntry) == 112 && offsetof(CDirEntry, attr) == 48 &&
              offsetof(CDirEntry, name) == 50 &&
              offsetof(CDirEntry, clus) == 88 &&
              offsetof(CDirEntry, datetime) == 104);

static_assert(sizeof(CBlkDev) == 124 && offsetof(CBlkDev, base0) == 32 &&
              offsetof(CBlkDev, drv_offset) == 44 &&
              offsetof(CBlkDev, max_blk) == 60 &&
              offsetof(CBlkDev, RAM_dsk) == 76);
static_assert(sizeof(CDrv) == 160 && offsetof(CDrv, root_clus) == 64 &&
              offsetof(CDrv, bd) == 104);
static_assert(sizeof(CFreeLst) == 32 && offsetof(CFreeLst, start) == 16 &&
              offsetof(CFreeLst, size) == 24);
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

struct CHashAC
{
  CHash hash;
  int32_t num;
  uint32_t hits;
};

static_assert(sizeof(CHashAC) == 32 && offsetof(CHashAC, hits) == 28);

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
    CAOTHeapGlbl *heap_glbl;
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
constexpr uint64_t MLMF_IS_STR = 1, GVF_FUN = 1, GVF_IMPORT = 2,
                   GVF_EXTERN = 4, GVF_DATA_HEAP = 8, GVF_ALIAS = 16,
                   GVF_ARRAY = 32;

static_assert(sizeof(CMemberLstMeta) == 32 && offsetof(CMemberLstMeta, user_data) == 24);
static_assert(sizeof(CMemberLst) == 144 && offsetof(CMemberLst, meta) == 64 &&
              offsetof(CMemberLst, dim) == 96 &&
              offsetof(CMemberLst, fun_ptr) == 136);
static_assert(sizeof(CHashClass) == 128 && offsetof(CHashClass, flags) == 86 &&
              offsetof(CHashClass, member_lst_and_root) == 88);
static_assert(sizeof(CHashFun) == 168 && offsetof(CHashFun, exe_addr) == 152);
static_assert(sizeof(CHashGlblVar) == 136 &&
              offsetof(CHashGlblVar, data_addr) == 120 &&
              offsetof(CHashGlblVar, heap_glbl) == 128);

static_assert(sizeof(CHash) == 24 && sizeof(CHashSrcSym) == 64 &&
              sizeof(CHashExport) == 72 && sizeof(CHashImport) == 80);
static_assert(sizeof(CHashGeneric) == 48 &&
              offsetof(CHashGeneric, user_data0) == 24 &&
              offsetof(CHashGeneric, user_data2) == 40);

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
                   DOCF_BORDER_DOC = 0x100, DOCF_FORM = 0x200,
                   DOCF_SIZE_MIN = 0x400, DOCF_HIDE_CURSOR = 0x800,
                   DOCF_DONT_HIGHLIGHT_CURSOR = 0x1000,
                   DOCF_NO_SCROLL_BARS = 0x2000, DOCF_DONT_SHOW = 0x8000,
                   DOCF_HAS_SONG = 0x10000, DOCF_BWD_MOVEMENT = 0x40000,
                   DOCF_DO_FULL_REFRESH = 0x200000,
                   DOCF_BREAK_UNLOCKED = 0x400000,
                   DOCF_OVERSTRIKE = 1ull << 32, DOCF_IN_DOLLAR = 0x200000000;
constexpr uint64_t DOCEF_TAG = 1, DOCEF_LEN = 2, DOCEF_AUX_STR = 4,
                   DOCEF_DEFINE = 8, DOCEF_HTML_LINK = 16, DOCEF_LEFT_EXP = 32,
                   DOCEF_LEFT_MACRO = 64, DOCEF_RIGHT_EXP = 128,
                   DOCEF_RIGHT_MACRO = 256, DOCEF_HAS_BIN = 512,
                   DOCEF_BIN_PTR_LINK = 1024, DOCEF_RAW_TYPE = 2048,
                   DOCEF_LEFT_CB = 1ull << 16, DOCEF_LEFT_IN_STR = 1ull << 17,
                   DOCEF_RIGHT_CB = 1ull << 18, DOCEF_RIGHT_IN_STR = 1ull << 19,
                   DOCEF_LEFT_X = 1ull << 20, DOCEF_CENTER_X = 1ull << 21,
                   DOCEF_RIGHT_X = 1ull << 22, DOCEF_TOP_Y = 1ull << 23,
                   DOCEF_CENTER_Y = 1ull << 24, DOCEF_BOTTOM_Y = 1ull << 25,
                   DOCEF_HIGHLIGHT = 1ull << 26, DOCEF_WORD_WRAP = 1ull << 27,
                   DOCEF_TAG_CB = 1ull << 32, DOCEF_PAGE_REL_Y = 1ull << 33,
                   DOCEF_MARGIN_REL_X = 1ull << 34, DOCEF_LINK = 1ull << 36,
                   DOCEF_ESC = 1ull << 37, DOCEF_QUIT = 1ull << 38,
                   DOCEF_FROM_START = 1ull << 39, DOCEF_HAS_BORDER = 1ull << 40,
                   DOCEF_SOLID_BORDER = 1ull << 41, DOCEF_BORDER_PLOT = 1ull << 42,
                   DOCEF_CHECKED_COLLAPSED = 1ull << 43,
                   DOCEF_CHECK_COLLAPSABLE = 1ull << 44,
                   DOCEF_REFRESH_DATA = 1ull << 45,
                   DOCEF_UPDATE_DATA = 1ull << 46,
                   DOCEF_DEREF_DATA = 1ull << 47,
                   DOCEF_REMALLOC_DATA = 1ull << 48,
                   DOCEF_HAS_TERMINATOR = 1ull << 49, DOCEF_HOLD = 1ull << 51,
                   DOCEF_TREE = 1ull << 52, DOCEF_POPUP = 1ull << 55,
                   DOCEF_LST = 1ull << 53, DOCEF_SKIP = 1ull << 54,
                   DOCEF_SKIP_IN_FORM = 1ull << 56,
                   DOCEF_FILTER_SKIP = 1ull << 57, DOCEF_NO_CLICK_ON = 1ull << 58,
                   DOCEF_DONT_DRAW = 1ull << 59,
                   DOCEF_DFT_LEN = 1ull << 60,
                   DOCEF_DFT_RAW_TYPE = 1ull << 61;
constexpr uint32_t ATTRF_BLINK = 0x10000000, ATTRF_INVERT = 0x20000000,
                   ATTRF_SEL = 0x40000000, ATTRF_UNDERLINE = 0x80000000;
constexpr uint32_t DOCG_BL_IV_UL = ATTRF_BLINK | ATTRF_INVERT | ATTRF_UNDERLINE,
                   DOC_SIGNATURE_VAL = 0x53636F44;
constexpr uint32_t RECALCt_NORMAL = 0, RECALCt_FIND_CURSOR = 1,
                   RECALCt_TO_SCRN = 2, RECALCG_MASK = 0xFF,
                   RECALCF_HAS_CURSOR = 0x100;
constexpr int64_t DOC_DFT = INT32_MIN;
constexpr int64_t DOCM_CANCEL = -1;
constexpr uint64_t DOF_SIZE_MIN = 1, DOF_INTERCEPT_TASK_END = 2,
                   DOF_DONT_HOME = 4, DOF_WIN_MAX = 8,
                   DOF_DONT_TEXT_ATTR = 16, DOF_DONT_WINMGR_SYNC = 32,
                   DOF_DONT_SHOW = 64;
constexpr uint64_t EDF_BAIL = 1 << 8, EDF_COLLAPSE = 1 << 9,
                   EDF_UNCOLLAPSE = 1 << 10, EDF_WAS_WRITE = 1 << 11;

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
  union {
    int64_t attr;
    int64_t left_exp;
    uintptr_t left_cb;
  };
  char *left_macro;
  union {
    int64_t right_exp;
    uintptr_t right_cb;
  };
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
static_assert(sizeof(CEdFileName) == 264 && offsetof(CEdFileName, name) == 8);
static_assert(sizeof(CDocBin) == 56 && offsetof(CDocBin, num) == 32 &&
              offsetof(CDocBin, data) == 48);
static_assert(sizeof(CDocEntryBase) == 88 &&
              offsetof(CDocEntryBase, tag) == 16 &&
              offsetof(CDocEntryBase, x) == 40 && offsetof(CDocEntryBase, y) == 44 &&
              offsetof(CDocEntryBase, settings) == 56 &&
              sizeof(CDocEntry) == 208 && offsetof(CDocEntry, left_exp) == 88 &&
              offsetof(CDocEntry, right_exp) == 104 && offsetof(CDocEntry, bin_data) == 192);
static_assert(sizeof(CDoc) == 768 && offsetof(CDoc, head) == 0 &&
              offsetof(CDoc, cur_entry) == 104 &&
              offsetof(CDoc, x) == 184 && offsetof(CDoc, y) == 188 &&
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

struct CAOTHeapGlblRef
{
  CAOTHeapGlblRef *next;
  int64_t rip;
};

struct CAOTHeapGlbl
{
  CAOTHeapGlbl *next;
  char *str;
  int64_t size;
  CAOTHeapGlblRef *references;
};

static_assert(sizeof(CAOTHeapGlblRef) == 16 &&
              offsetof(CAOTHeapGlblRef, rip) == 8);
static_assert(sizeof(CAOTHeapGlbl) == 32 && offsetof(CAOTHeapGlbl, str) == 8 &&
              offsetof(CAOTHeapGlbl, size) == 16 &&
              offsetof(CAOTHeapGlbl, references) == 24);

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
  char *pmt_buf;
  const char *parse_error;
  unsigned parse_error_count, warning_cnt;
  bool expression_error;
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

enum : int64_t
{
  CTRLT_GENERIC, CTRLT_WIN_HSCROLL, CTRLT_WIN_VSCROLL, CTRLT_VIEWING_ANGLES,
};
constexpr int64_t CTRLF_SHOW = 1, CTRLF_BORDER = 2,
                  CTRLF_CAPTURE_LEFT_MS = 4, CTRLF_CAPTURE_RIGHT_MS = 8,
                  CTRLF_CLICKED = 16;
constexpr uint32_t WSSF_SET_TO_POS = 1;

struct CWinScroll
{
  int64_t min, pos, max;
  uint32_t flags;
  uint8_t color, pad[3];
};

struct CViewAngles
{
  int64_t sx, sy, sz;
  uint64_t ax, ay, az;
  CColorROPU32 cx, cy, cz, cbd, cbg, cfg;
};

struct CCtrl
{
  CCtrl *next, *last;
  CTask *win_task;
  int64_t type, flags;
  int64_t left, right, top, bottom;
  int64_t scrn_left, scrn_right, scrn_top, scrn_bottom;
  void *state;
  uintptr_t update_derived_vals, draw_it, inside_ctrl;
  uintptr_t left_click, right_click, wheel_chg;
};

static_assert(sizeof(CWinScroll) == 32 && offsetof(CWinScroll, flags) == 24 &&
              offsetof(CWinScroll, color) == 28);
static_assert(sizeof(CViewAngles) == 72 &&
              offsetof(CViewAngles, ax) == 24 &&
              offsetof(CViewAngles, cx) == 48 &&
              offsetof(CViewAngles, cfg) == 68);
static_assert(sizeof(CCtrl) == 160 && offsetof(CCtrl, win_task) == 16 &&
              offsetof(CCtrl, left) == 40 && offsetof(CCtrl, scrn_left) == 72 &&
              offsetof(CCtrl, state) == 104 &&
              offsetof(CCtrl, update_derived_vals) == 112 &&
              offsetof(CCtrl, wheel_chg) == 152);

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
constexpr uint16_t KBD_PORT = 0x60, KBD_CTRL = 0x64;
constexpr char CH_CURSOR = 0x05;
constexpr uint8_t CH_BACKSPACE = 0x08, CH_ESC = 0x1B, CH_SHIFT_ESC = 0x1C,
                  CH_SHIFT_SPACE = 0x1F;
constexpr uint32_t SCF_E0_PREFIX = 1 << 7, SCF_KEY_UP = 1 << 8,
                   SCF_SHIFT = 1 << 9, SCF_CTRL = 1 << 10, SCF_ALT = 1 << 11,
                   SCF_CAPS = 1 << 12, SCF_NUM = 1 << 13, SCF_SCROLL = 1 << 14,
                   SCF_NEW_KEY = 1 << 15, SCF_MS_L_DOWN = 1 << 16,
                   SCF_MS_R_DOWN = 1 << 17, SCF_DELETE = 1 << 18, SCF_INS = 1 << 19,
                   SCF_NO_SHIFT = 1U << 30, SCF_KEY_DESC = 1U << 31;
constexpr uint8_t SC_ENTER = 0x1C, SC_SHIFT = 0x2A, SC_CTRL = 0x1D,
                  SC_ALT = 0x38, SC_CAPS = 0x3A, SC_NUM = 0x45,
                  SC_F1 = 0x3B, SC_F6 = 0x40, SC_F7 = 0x41, SC_F10 = 0x44,
                  SC_SCROLL = 0x46, SC_HOME = 0x47, SC_CURSOR_UP = 0x48,
                  SC_PAGE_UP = 0x49, SC_CURSOR_LEFT = 0x4B, SC_CURSOR_RIGHT = 0x4D,
                  SC_END = 0x4F, SC_CURSOR_DOWN = 0x50, SC_PAGE_DOWN = 0x51,
                  SC_INS = 0x52, SC_DELETE = 0x53, SC_PRTSCRN1 = 0xAA,
                  SC_GUI = 0xDB;
constexpr int64_t GSF_SHIFT_ESC_EXIT = 1, GSF_WITH_NEW_LINE = 2;

constexpr int64_t KDF_HAS_DESCS = 1;
struct CKeyDevEntry
{
  CKeyDevEntry *next, *last;
  int64_t priority, flags;
  uintptr_t put_key, put_s;
};
struct CKeyDevGlbls
{
  CKeyDevEntry put_key_head;
  uintptr_t *fp_ctrl_alt_cbs;
  uint64_t ctrl_alt_in_irq_flags;
  uintptr_t **ctrl_alt_ret_addr;
  char **ctrl_alt_no_shift_descs, **ctrl_alt_shift_descs;
  char desc[144];
  uintptr_t hndlr;
};
static_assert(sizeof(CKeyDevEntry) == 48 && offsetof(CKeyDevEntry, put_key) == 32);
static_assert(sizeof(CKeyDevGlbls) == 240 && offsetof(CKeyDevGlbls, desc) == 88 &&
              offsetof(CKeyDevGlbls, hndlr) == 232);

constexpr int64_t SUF_REM_CTRL_CHARS = 0x001, SUF_REM_LEADING = 0x002,
                  SUF_REM_TRAILING = 0x004, SUF_REM_SPACES = 0x008,
                  SUF_SINGLE_SPACE = 0x010, SUF_TO_UPPER = 0x020,
                  SUF_TO_LOWER = 0x040, SUF_S2T = 0x080, SUF_T2S = 0x100,
                  SUF_SCALE_INDENT = 0x200, SUF_SAFE_DOLLAR = 0x400;
constexpr int64_t SFF_IGNORE_CASE = 1, SFF_WHOLE_LABELS_BEFORE = 2,
                  SFF_WHOLE_LABELS_AFTER = 4,
                  SFG_WHOLE_LABELS =
                      SFF_WHOLE_LABELS_BEFORE | SFF_WHOLE_LABELS_AFTER;

template <typename T> struct CFifo
{
  T *buf;
  int64_t mask, in_ptr, out_ptr;
};

using CFifoU8 = CFifo<uint8_t>;
using CFifoI64 = CFifo<int64_t>;

struct [[gnu::packed]] CKbdStateGlbls
{
  int64_t scan_code, last_down_scan_code, cnt, timestamp, new_key_timestamp;
  CFifoU8 *fifo, *fifo2;
  CFifoI64 *scan_code_fifo;
  uint32_t down_bitmap[8], down_bitmap2[8];
  bool rst, irqs_working;
};

static_assert(sizeof(CFifoU8) == 32 && sizeof(CFifoI64) == 32 &&
              offsetof(CFifoU8, buf) == 0 && offsetof(CFifoU8, mask) == 8 &&
              offsetof(CFifoU8, in_ptr) == 16 && offsetof(CFifoU8, out_ptr) == 24 &&
              offsetof(CFifoI64, buf) == 0 && offsetof(CFifoI64, mask) == 8 &&
              offsetof(CFifoI64, in_ptr) == 16 && offsetof(CFifoI64, out_ptr) == 24);
static_assert(sizeof(CKbdStateGlbls) == 130 &&
              offsetof(CKbdStateGlbls, fifo) == 40 &&
              offsetof(CKbdStateGlbls, down_bitmap) == 64 &&
              offsetof(CKbdStateGlbls, irqs_working) == 129);

struct [[gnu::packed]] CTextGlbls
{
  int64_t raw_col,raw_flags;
  uint8_t *raw_scrn_image;
  int64_t rows,cols;
  uint64_t *font,*aux_font;
  uint8_t *vga_alias,*vga_text_alias;
  uint8_t border_chars[16];
};
static_assert(sizeof(CTextGlbls)==88 &&
              offsetof(CTextGlbls,font)==40 &&
              offsetof(CTextGlbls,aux_font)==48 &&
              offsetof(CTextGlbls,border_chars)==72);

struct CD3I64
{
  int64_t x, y, z;
};

struct CD3
{
  uint64_t x, y, z;
};

struct Complex
{
  uint64_t x, y; // binary64 bit patterns
};
static_assert(sizeof(Complex)==16 && offsetof(Complex,y)==8);

struct [[gnu::packed]] CMsHardStateGlbls
{
  CD3I64 pos, prescale;
  CD3 scale;
  uint64_t speed;
  int64_t timestamp, install_attempts, pkt_size;
  CFifoU8 *fifo, *fifo2;
  bool bttns[5], has_wheel, has_ext_bttns, evt, installed, install_in_progress,
       irqs_working;
};

struct CMsStateGlbls
{
  CD3I64 pos, pos_text, presnap, offset;
  CD3 scale;
  uint64_t speed;
  int64_t timestamp;
  uint64_t dbl_time, left_dbl_time, right_dbl_time;
  bool lb, rb, show, has_wheel, left_dbl, right_dbl, left_down_sent,
       right_down_sent;
};

constexpr uint64_t ACF_INIT_IN_PROGRESS = 1;
constexpr int64_t AC_FILLINS_NUM = 10;

struct CAutoCompleteGlbls
{
  int64_t num_words;
  CHashTable *hash_table;
  uint8_t *cur_word;
  uint64_t flags;
  CTask *task;
  int64_t partial_len, num_fillins, fillin_hits[AC_FILLINS_NUM + 1];
  CHashAC *fillin_matches[AC_FILLINS_NUM + 1];
};

static_assert(sizeof(CAutoCompleteGlbls) == 232 &&
              offsetof(CAutoCompleteGlbls, task) == 32);

constexpr int64_t ACD_BLK_SIZE = 0x4000, ACD_FILLINS_NUM = 10;
constexpr int64_t ACD_H1 = 0, ACD_H1_END = 1, ACD_DEF = 2, ACD_DEF_END = 3,
                  ACD_PRONUNCIATION = 4, ACD_PRONUNCIATION_END = 5,
                  ACD_POS = 6, ACD_POS_END = 7, ACD_EXTRA = 8, ACD_EXTRA_END = 9;
constexpr uint8_t ACD_END_CHAR = 0, ACD_WORD_CHAR = 1, ACD_DEF_CHAR = 2,
                  ACD_PRONUNCIATION_CHAR = 3, ACD_POS_CHAR = 4,
                  ACD_EXTRA_CHAR = 5;

#pragma pack(push, 1)
struct CAutoCompleteDictGlbls
{
  uint8_t *word_lst;
  int64_t word_lst_size, num_words, num_fillins;
  uint8_t *fillins[ACD_FILLINS_NUM];
  bool has_words, has_defs;
};
#pragma pack(pop)

static_assert(sizeof(CAutoCompleteDictGlbls) == 114 &&
              offsetof(CAutoCompleteDictGlbls, fillins) == 32 &&
              offsetof(CAutoCompleteDictGlbls, has_words) == 112);

struct CWinMgrTimingGlbls;

struct [[gnu::packed]] CWinMgrGlbls
{
  int64_t updates;
  uint64_t ode_time, last_ode_time, fps, ideal_refresh_tS, last_refresh_tS;
  CWinMgrTimingGlbls *t;
  bool show_menu, grab_scroll, grab_scroll_closed;
};

static_assert(sizeof(CWinMgrGlbls) == 59 &&
              offsetof(CWinMgrGlbls, updates) == 0 &&
              offsetof(CWinMgrGlbls, grab_scroll) == 57);

struct CGridGlbls
{
  bool snap, show, coord;
  uint8_t pad[5];
  uint64_t x, y, z, x_offset, y_offset, z_offset, x_speed, y_speed, z_speed;
};

static_assert(sizeof(CD3I64) == 24 && sizeof(CD3) == 24);
static_assert(sizeof(CMsHardStateGlbls) == 131 &&
              offsetof(CMsHardStateGlbls, pkt_size) == 96 &&
              offsetof(CMsHardStateGlbls, bttns) == 120 &&
              offsetof(CMsHardStateGlbls, irqs_working) == 130);
static_assert(sizeof(CMsStateGlbls) == 168 &&
              offsetof(CMsStateGlbls, scale) == 96 &&
              offsetof(CMsStateGlbls, lb) == 160);
static_assert(sizeof(CGridGlbls) == 80 && offsetof(CGridGlbls, x) == 8);

constexpr uint32_t WIF_SELF_FOCUS = 1 << 0, WIF_SELF_CTRLS = 1 << 2,
                   WIF_SELF_MS_L = 1 << 3, WIF_SELF_MS_R = 1 << 5,
                   WIF_SELF_MS_WHEEL = 1 << 7, WIF_SELF_BORDER = 1 << 8,
                   WIF_SELF_GRAB_SCROLL = 1 << 9, WIF_SELF_DOC = 1 << 10,
                   WIF_SELF_ODE = 1 << 11, WIF_SELF_KEY_DESC = 1 << 12,
                   WIF_FOCUS_TASK_MENU = 1 << 17,
                   WIF_FOCUS_TASK_CTRLS = 1 << 18,
                   WIF_FOCUS_TASK_MS_L = 1 << 19,
                   WIF_FOCUS_TASK_MS_L_D = 1 << 20,
                   WIF_FOCUS_TASK_MS_R = 1 << 21,
                   WIF_FOCUS_TASK_MS_R_D = 1 << 22,
                   WIF_FOCUS_TASK_MS_WHEEL = 1 << 23,
                   WIF_FOCUS_TASK_BORDER = 1 << 24,
                   WIF_FOCUS_TASK_GRAB_SCROLL = 1 << 25;

constexpr uint32_t WIG_DBL_CLICK =
    WIF_FOCUS_TASK_MS_L_D | WIF_FOCUS_TASK_MS_R_D;
constexpr uint32_t WIG_TASK_DFT = WIF_FOCUS_TASK_MENU | WIG_DBL_CLICK |
                                  (0xFFFF - WIF_SELF_DOC - WIF_SELF_ODE);
constexpr uint32_t WIG_NO_FOCUS_TASK_DFT = WIG_TASK_DFT - WIF_SELF_BORDER -
                                           WIF_SELF_MS_L - WIF_SELF_MS_R -
                                           WIG_DBL_CLICK;
constexpr uint32_t WIG_USER_TASK_DFT = WIF_SELF_KEY_DESC;

static_assert(sizeof(CJob) == 0x70 && offsetof(CJob, aux1) == 0x48 &&
              offsetof(CJob, master_task) == 0x68);
static_assert(sizeof(CJobCtrl) == 0x28 && offsetof(CJobCtrl, next_done) == 0x10 &&
              offsetof(CJobCtrl, flags) == 0x20);

constexpr uint32_t TASKF_KILL_TASK = 1 << 1, TASKF_SUSPENDED = 1 << 2,
                   TASKF_IDLE = 1 << 3, TASKF_CMD_LINE_PMT = 1 << 4,
                   TASKF_INPUT_FILTER_TASK = 1 << 5, TASKF_FILTER_INPUT = 1 << 6,
                   TASKF_HAS_SONG = 1 << 7, TASKF_AWAITING_MSG = 1 << 9,
                   TASKF_BREAK_LOCKED = 1 << 10,
                   TASKF_PENDING_BREAK = 1 << 11, TASKF_BREAK_TO_SHIFT_ESC = 1 << 12,
                   TASKF_NONTIMER_RAND = 1 << 14;
constexpr int64_t DYING_JIFFIES = JIFFY_FREQ / 5;

struct [[gnu::may_alias]] CTaskDying
{
  CTaskDying *next, *last;
  int64_t wake_jiffy;
};
constexpr uint32_t DISPLAYF_SHOW = 1, DISPLAYF_NOT_RAW = 1 << 1,
                   DISPLAYF_SILENT = 1 << 2, DISPLAYF_NO_BORDER = 1 << 3,
                   DISPLAYF_WIN_ON_TOP = 1 << 4,
                   DISPLAYF_CHILDREN_NOT_ON_TOP = 1 << 5;
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

constexpr int64_t TSF_SAME_SONG = 1;

struct CMenuEntry
{
  CMenuEntry *next, *sub;
  char name[32];
  int64_t msg_code, arg1, arg2;
  bool checked, dir, pad[6];
};

struct CMenu
{
  CMenu *next;
  CMenuEntry *sub;
  CTask *task;
  int64_t flags;
  uint8_t attr, pad[7];
};

static_assert(sizeof(CMenuEntry) == 80 &&
              offsetof(CMenuEntry, name) == 16 &&
              offsetof(CMenuEntry, msg_code) == 48 &&
              offsetof(CMenuEntry, checked) == 72 &&
              offsetof(CMenuEntry, dir) == 73);
static_assert(sizeof(CMenu) == 40 && offsetof(CMenu, sub) == 8 &&
              offsetof(CMenu, task) == 16 && offsetof(CMenu, attr) == 32);

struct CTaskSettings
{
  CTaskSettings *next;
  char *cur_dir;
  int64_t left, right, top, bottom;
  void (*draw_it)(CTask *task, CDC *dc);
  void (*task_end_cb)();
  CTask *song_task, *animate_task;
  int64_t scroll_x, scroll_y, scroll_z;
  CBGR48 palette[COLORS_NUM];
  uint32_t win_inhibit;
  uint8_t text_attr, title_src, border_attr, border_src, task_title[144];
  bool border, hide_cursor, highlight_cursor, scroll, autocomplete, pad[3];
};

static_assert(sizeof(CTaskSettings) == 392 &&
              offsetof(CTaskSettings, draw_it) == 48 &&
              offsetof(CTaskSettings, scroll_x) == 80 &&
              offsetof(CTaskSettings, palette) == 104 &&
              offsetof(CTaskSettings, task_title) == 240 &&
              offsetof(CTaskSettings, autocomplete) == 388);
static_assert(offsetof(CTask, draw_it) == 0x1C0);

struct [[gnu::packed]] CInsReg
{
  bool registered;
  int64_t host_os, host_vm;
  uint8_t initials[8];
  int64_t machine_num;
};

static_assert(sizeof(CInsReg) == 33 && offsetof(CInsReg, registered) == 0 &&
              offsetof(CInsReg, host_os) == 1 && offsetof(CInsReg, host_vm) == 9 &&
              offsetof(CInsReg, initials) == 17 && offsetof(CInsReg, machine_num) == 25);

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
static_assert(offsetof(CTask, win_z_num) == 0x30C);
static_assert(offsetof(CTask, next_except) == 0x318 &&
              offsetof(CTask, last_except) == 0x320 &&
              offsetof(CTask, except_rbp) == 0x328 &&
              offsetof(CTask, except_ch) == 0x330 &&
              offsetof(CTask, except_callers) == 0x338 &&
              offsetof(CTask, catch_except) == 0x378);
static_assert(offsetof(CTask, win_left) == 0x48 &&
              offsetof(CTask, win_right) == 0x50 &&
              offsetof(CTask, win_top) == 0x58 &&
              offsetof(CTask, win_bottom) == 0x60 &&
              offsetof(CTask, scroll_x) == 0x100 &&
              offsetof(CTask, scroll_y) == 0x108 &&
              offsetof(CTask, scroll_z) == 0x110);
static_assert(offsetof(CTask, fault_num) == 0x440 &&
              offsetof(CTask, fault_err_code) == 0x448);
static_assert(offsetof(CTask, last_fun) == 0x410);
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
extern uint32_t SYS_FONT_ADDRESS, SYS_E820_COUNT, MEM_PML2, MEM_PML3, MEM_PML4,
    MEM_HEAP_BASE;
extern uint64_t MEM_PHYSICAL_SPACE, MEM_MAPPED_SPACE, MEM_HEAP_LIMIT;
extern uint64_t MEM_2MEG_NUM, MEM_1GIG_NUM, MEM_512GIG_NUM;
extern uint16_t SYS_PCI_BUSES;
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
