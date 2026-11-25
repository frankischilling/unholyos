#pragma once

#include "KernelA.hpp"

inline void MemSet(void *dst, uint8_t value, size_t count)
{
  asm volatile("rep stosb" : "+D"(dst), "+c"(count) : "a"(value) : "memory");
}

inline void MemCpy(void *dst, const void *src, size_t count)
{
  asm volatile("rep movsb" : "+D"(dst), "+S"(src), "+c"(count) : : "memory");
}

inline size_t StrLen(const char *str)
{
  const auto *end = str;
  while (*end)
    end++;
  return end - str;
}

extern "C" int64_t StrCmpCpp(const uint8_t *a, const uint8_t *b);

inline CTask *CurrentTask()
{
  CTask *task;
  asm volatile("movq %%fs:0,%0" : "=r"(task));
  return task;
}

inline CCPU *CurrentCPU()
{
  CCPU *cpu;
  asm volatile("movq %%gs:0,%0" : "=r"(cpu));
  return cpu;
}

inline uint64_t GetRFlags()
{
  uint64_t flags;
  asm volatile("pushfq; popq %0" : "=r"(flags) : : "memory");
  return flags;
}

inline void SetRFlags(uint64_t flags)
{
  asm volatile("pushq %0; popfq" : : "r"(flags) : "cc", "memory");
}

extern "C" void _YIELD();
extern "C" uint64_t TaskValidateCpp(CTask *task);
extern "C" uint64_t SuspendCpp(CTask *task, uint8_t state);
extern "C" CJob *TaskMsgCpp(CTask *srv, CTask *master, int64_t code,
                           int64_t arg1, int64_t arg2, int64_t flags);
extern "C" void KbdMsService();
void KbdPoll();
bool KbdInit();
bool KbdCmdSend(uint16_t port, uint8_t value);
bool KbdCmdRead(uint8_t &value, bool auxiliary = false);
void Ps2PktRead();
void MsHardStageByte(uint8_t raw_byte);
bool MsHardDrvrInstall();
void MsInit();
void MsHardHndlr();
void WinMsUpdate();
int64_t WinQueIPMsgs(bool que);
extern "C" void GridInitCpp();
extern "C" void MsSetCpp(int64_t x, int64_t y, int64_t z, int64_t l, int64_t r);
void MsUpdate(int64_t x, int64_t y, int64_t z, bool l, bool r);
extern "C" uint64_t tSCpp();
extern "C" uint64_t F64FromI64(int64_t value);
extern "C" int64_t F64ToI64(uint64_t value, uint16_t rounding);
extern "C" uint64_t F64MulBits(uint64_t a, uint64_t b);
extern "C" uint64_t F64DivBits(uint64_t a, uint64_t b);
extern "C" uint64_t F64AddBits(uint64_t a, uint64_t b);
extern "C" uint64_t F64SqrtBits(uint64_t value);
void IntEntrySet(unsigned irq, void (*handler)(), uint16_t type = 0xE,
                 uint16_t dpl = 0);
uint64_t GetTSC();
extern CKbdStateGlbls kbd;
extern CMsHardStateGlbls ms_hard, ms_hard_last;
extern CMsStateGlbls ms, ms_last, old_ms;
extern CGridGlbls ms_grid;
extern "C" void PutKeyCpp(int64_t ch, int64_t sc);
void JobCtrlInit(CJobCtrl *ctrl);
void JobQueDel(CJob *head);
extern "C" void JobDelCpp(CJob *job);
extern "C" void TaskRstAwaitingMsgCpp(CTask *task);
extern "C" void TaskWaitCpp(CTask *task, uint8_t cmd_line_pmt);
extern "C" int64_t JobsHndlrCpp(uint64_t run_flags, CTask *task);
extern "C" int64_t FlushMsgsCpp(CTask *task);
extern "C" void InputFilterTaskHC();
extern "C" uint64_t SingleUserCpp(uint8_t val);
extern "C" [[noreturn]] void Exit();
extern "C" void TaskQueInsCpp(CTask *task, CTask *pred);
extern "C" void TaskQueRemCpp(CTask *task);
void TaskQueInsChild(CTask *task);
extern "C" CTask *SpawnCpp(uintptr_t fp_start_addr, uint8_t *data,
                          const char *task_name, int64_t target_cpu,
                          CTask *parent, int64_t stk_size, uint64_t flags);
bool Core0TaskInit();

uint8_t *Caller(const uintptr_t *frame, int64_t depth, const CTask &task);
extern "C" [[noreturn]] void RaiseHolyC(uint64_t tag);
[[noreturn]] void RethrowHolyC();
extern "C" uint64_t CallHolyC0(const uint8_t *entry);
extern "C" bool CallHolyC0Checked(const uint8_t *entry, uint64_t *result);
extern "C" int64_t CallHolyC2(uintptr_t function, uint64_t arg1, uint64_t arg2);

constexpr uint64_t STR_PRINT_ERROR = 0x746E697250727453;    // StrPrint
constexpr uint64_t UNDEF_DEFINE_ERROR = 0x6665446665646E55; // UndefDef

const char *DefineText(const char *name, bool &defined);
const char *DefineSub(int64_t index, const char *name, bool &defined);
[[noreturn]] void UndefinedDefine(const char *name);
const char *LstSub(int64_t index, const char *list);
extern "C" char *StrPrintJoin(char *dst, const char *fmt, int64_t argc,
                              const uint64_t *argv);
const CHash *FunSegFind(uintptr_t addr, uint64_t &offset);
int64_t YearStartDate(int64_t year);
extern "C" uint64_t Pow10I64Cpp(int64_t exponent);
extern "C" uint64_t IsRawCpp();
extern "C" uint64_t DbgModeCpp(uint8_t enabled);
extern "C" uint64_t IsDbgModeCpp();
void VGAFlush();
extern "C" void PutS(const char *text);
void PutTaskS(CTask &task, const char *text);

template <typename T> inline int64_t FifoCnt(const CFifo<T> &f)
{
  return (f.in_ptr - f.out_ptr) & f.mask;
}

template <typename T> inline bool FifoIns(CFifo<T> &f, T value)
{
  auto next = (f.in_ptr + 1) & f.mask;
  if (next == f.out_ptr)
    return false;
  f.buf[f.in_ptr] = value;
  f.in_ptr = next;
  return true;
}

template <typename T> inline bool FifoRem(CFifo<T> &f, T &value)
{
  if (f.in_ptr == f.out_ptr)
    return false;
  value = f.buf[f.out_ptr];
  f.out_ptr = (f.out_ptr + 1) & f.mask;
  return true;
}

template <typename T> void QueInit(T *&next, T *&last)
{
  next = last = reinterpret_cast<T *>(&next);
}

template <typename T> void QueIns(T &entry, T &last)
{
  entry.next = last.next;
  entry.last = &last;
  last.next->last = &entry;
  last.next = &entry;
}

template <typename T> void QueRem(T &entry)
{
  entry.last->next = entry.next;
  entry.next->last = entry.last;
}

class CMemLock
{
public:
  explicit CMemLock(uint64_t &locked_flags) : flags(locked_flags)
  {
    asm volatile("pushfq; popq %0; cli" : "=r"(rflags) : : "memory");
    uint8_t busy;
    do {
      asm volatile("lock btsq $0,%1; setc %0"
                   : "=q"(busy), "+m"(flags)
                   :
                   : "cc", "memory");
      if (busy)
        asm volatile("pause");
    } while (busy);
  }

  ~CMemLock()
  {
    asm volatile("lock btrq $0,%0; pushq %1; popfq"
                 : "+m"(flags)
                 : "r"(rflags)
                 : "cc", "memory");
  }

  CMemLock(const CMemLock &) = delete;
  CMemLock &operator=(const CMemLock &) = delete;

private:
  uint64_t &flags, rflags;
};

CMemBlk *MemPagTaskAlloc(size_t pags, CHeapCtrl *hc);
uint8_t *MAlloc(size_t size, CHeapCtrl *hc);
CHeapCtrl *MemTaskHeap(void *mem_task);
size_t MSize(const void *ptr);
size_t MSize2(const void *ptr);
uint8_t *CAlloc(size_t size, CHeapCtrl *hc);
char *StrNew(const char *src, CHeapCtrl *hc);
uint8_t *MAllocAligned(size_t size, size_t alignment, CHeapCtrl *hc,
                       size_t misalignment = 0);

uint64_t BIOSTotalMem();
bool BlkPoolsInit(CTask *task);
CHeapCtrl *HeapCtrlInit(CTask *task, CBlkPool *bp);
void HeapCtrlDel(CHeapCtrl *hc);

uint64_t *MemPageTable(uintptr_t addr);
void UncachedAliasAlloc();

void Free(void *ptr);
void MemPagTaskFree(CMemBlk *blk, CHeapCtrl *hc);
void MemPagTaskUnlink(CMemBlk *blk, CHeapCtrl *hc);
[[noreturn]] void SysBadFree(const void *ptr);
bool Mem32DevInit(CHeapCtrl *hc);

using CExt = int64_t (*)(...);

extern CTask *adam_task, *sys_focus_task;
extern CBlkDevGlbls blkdev;
extern CDbgGlbls dbg;
extern CSema sys_semas[SEMA_SEMAS_NUM];
extern CScrnCastGlbls scrncast;
extern uint8_t sys_var_init_flag, sys_var_init_val;
extern CJob sys_macro_head;
extern CTask *sys_macro_task;
extern CDevGlbls dev;
extern uint64_t sys_cache_line_width;
extern uint8_t *rev_bits_table, *set_bits_table;
extern uint64_t *pow10_I64;
extern CExt *ext;

extern uint32_t char_bmp_alpha[16], char_bmp_alpha_numeric[16],
    char_bmp_alpha_numeric_no_at[16], char_bmp_word[16], char_bmp_filename[16],
    char_bmp_dec_numeric[16], char_bmp_hex_numeric[16],
    char_bmp_white_space[16], char_bmp_non_eol_white_space[16],
    char_bmp_zero_cr_nl_cursor[16], char_bmp_zero_tab_cr_nl_cursor[16],
    char_bmp_zero_tab_cr_nl_cursor_dollar[16], char_bmp_macro[16],
    char_bmp_printable[16], char_bmp_displayable[16], char_bmp_safe_dollar[16],
    char_bmp_non_eol[16];

extern "C" uint64_t Pow10Bits(int64_t exponent);

bool IntInit1();

extern CCntsGlbls cnts;
bool IntInit2();
void IntsInit();
uint64_t SysTimerRead();
uint64_t HPET();
int64_t TimeCal();
extern "C" uint64_t tSCpp();
extern "C" void BusyCpp(int64_t microseconds);
extern "C" void SleepUntilCpp(int64_t wake_jiffy);
extern "C" void SleepCpp(int64_t ms);
extern "C" uint64_t Ona2FreqCpp(int8_t ona);
extern "C" uint64_t Freq2OnaCpp(uint64_t freq);
extern "C" void SndCpp(int8_t ona);
extern "C" uint64_t ScrnCastCpp(uint8_t val, uint8_t just_audio,
                                const char *print_fmt);
extern "C" void SndRstCpp();
extern "C" void BeepCpp(int8_t ona, uint8_t busy);
extern "C" uint64_t MuteCpp(uint8_t val);
extern "C" uint64_t IsMuteCpp();
uint16_t PCIReadU16(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg);
uint32_t PCIReadU32(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg);
int32_t PCIClassFind(uint32_t class_code, uint16_t index);

uint32_t ATAProbe(const CBlkDev &bd);
bool ATAPIStartStop(const CBlkDev &bd, bool start);
bool ATAPIReadCapacity(CBlkDev &bd);
bool ATAPIReadBlks2(const CBlkDev &bd, uint8_t *buf, uint32_t lba,
                    uint32_t count);
bool BootDVDProbeAll(CBlkDev &bd);
bool ATAPIReadBytes(const CBlkDev &bd, uint8_t *buf, uint64_t pos,
                    size_t bytes);

CDrv *BlkDevsInitAll();
bool BlkDevGlblsInit();
CDrv *MountedDrive(char letter);
bool BlkDevReadBytes(const CBlkDev &bd, uint8_t *buf, uint64_t pos,
                     size_t bytes);
uint64_t Now();
bool ISOInit(CDrv &dv);
bool RedSeaInit(CDrv &dv);
bool RedSeaFormatRAM(CDrv &dv, size_t root_blocks);
bool RedSeaFileFind(const CDrv &dv, uint64_t dir, const char *name,
                    bool directory, CRedSeaDirEntry &entry);
bool RedSeaFileRead(const CDrv &dv, const CRedSeaDirEntry &entry, uint64_t pos,
                    uint8_t *buf, size_t bytes);
bool RedSeaFileLookup(const CDrv &dv, const char *path, bool alternate,
                      bool scan_parents, CRedSeaDirEntry &entry,
                      size_t &prefix);
bool RedSeaDirClus(const CDrv &dv, const char *path, size_t count,
                   uint64_t &dir);
bool RedSeaIsDir(const CDrv &dv, const char *path);
uint64_t RedSeaAllocClus(const CDrv &dv, uint64_t blocks);
CDrv *PathDrive(const CTask &task, char letter);
uint64_t RedSeaFileWrite(CDrv &dv, const char *path, const uint8_t *data,
                         size_t size, uint64_t date, uint16_t attr);
char *FileNameAbs(const char *name, const CTask &task, uint64_t flags = 0);
extern "C" char *ExtDftCpp(const char *name, const char *ext);
const char *FileExtDot(const char *name);
bool IsDotExt(const char *name, char ext);
int64_t DirEntryCompareName(const CRedSeaDirEntry &a, const CRedSeaDirEntry &b);
char *ToggleZorNotZ(const char *name, CHeapCtrl *hc);
void ResidentDriveClear(char letter);
uint8_t *FileRead(const CDrv &dv, const char *path, size_t &size,
                  CHeapCtrl *hc);
extern "C" uint8_t *FileReadCpp(const char *filename, size_t *out_size,
                                uint64_t *out_attr);

CHash *HashSingleTableFind(const char *name, CHashTable *table, uint32_t mask,
                           uint64_t instance = 1);
CHash *HashFind(const char *name, CHashTable *table, uint32_t mask,
                uint64_t instance = 1);
void HashAdd(CHash *entry, CHashTable *table);
void HashRemove(CHash *entry, CHashTable *table);
void HashDel(CHash *entry);
CHashTable *HashTableNew(size_t size, CHeapCtrl *hc);
extern "C" void HashTableDelCpp(CHashTable *table);
extern "C" void HashSrcFileSetCpp(const CCmpCtrl *cc, CHashSrcSym *sym,
                                   int64_t line_offset);
void ClassMemberLstDel(CHashClass *cls);
extern "C" void LinkedLstDelCpp(void *node);
extern "C" char *MStrPrintCpp(const char *fmt, int64_t argc,
                              const uint64_t *argv);

struct CLoadedModule;
constexpr unsigned LDF_NO_ABSS = 1, LDF_JUST_LOAD = 2, LDF_SILENT = 4;
bool LoadKernel(CTask &task);
CLoadedModule *Load(const CDrv &dv, const char *path, CTask &task,
                    unsigned flags = 0);
bool LoadPass2(CLoadedModule &module);
