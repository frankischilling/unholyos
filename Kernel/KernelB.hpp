#pragma once

#include "KernelA.hpp"
#include "../Adam/ASnd.hpp"

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
extern "C" int64_t StrICmpCpp(const char *st1, const char *st2);
extern "C" int64_t StrNCmpCpp(const char *st1, const char *st2, int64_t n);
extern "C" int64_t StrNICmpCpp(const char *st1, const char *st2, int64_t n);
extern "C" const char *StrMatchCpp(const char *needle, const char *haystack);
extern "C" int64_t StrOccCpp(const uint8_t *src, int64_t ch);
extern "C" int64_t Spaces2TabsCpp(char *dst, const char *src);
extern "C" char *StrUtilCpp(char *src, int64_t flags);
extern "C" char *StrFirstOccCpp(char *src, const uint8_t *marker);
extern "C" char *StrFirstRemCpp(char *src, const uint8_t *marker, char *dst);
extern "C" char *StrLastOccCpp(char *src, const uint8_t *marker);
extern "C" char *StrLastRemCpp(char *src, const uint8_t *marker, char *dst);
extern "C" const char *StrFindCpp(const char *needle, const char *haystack,
                                  int64_t flags);
extern "C" char *Tabs2SpacesCpp(const char *src);
extern "C" char *ScaleIndentCpp(const char *src, uint64_t indent_scale_factor);
extern "C" char *MStrUtilCpp(const char *src, int64_t flags,
                             uint64_t indent_scale_factor);
extern "C" int64_t Str2I64Cpp(const char *text, int64_t radix,
                              const char **end);
extern "C" uint64_t Str2F64Cpp(const char *text, const char **end);
extern "C" uint64_t Str2DateCpp(const char *src);
extern "C" char *StrScanCpp(char *src, const char *fmt, int64_t argc,
                            const uint64_t *argv);
extern "C" int64_t LstMatchCpp(const uint8_t *needle, const uint8_t *list,
                               uint64_t flags);
extern "C" int64_t DefineMatchCpp(const uint8_t *needle, const char *name,
                                  uint64_t flags);
bool ChkPtr(const void *ptr);
bool ChkPtrRange(const void *ptr, size_t size);
extern "C" uint64_t ChkPtrCpp(const void *ptr);

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
bool KillTask(CTask *task, bool wait = true);
extern "C" uint64_t SuspendCpp(CTask *task, uint8_t state);
extern "C" void MsgCpp(int64_t msg_code, int64_t arg1, int64_t arg2, int64_t flags);
extern "C" CJob *TaskMsgCpp(CTask *srv, CTask *master, int64_t code,
                           int64_t arg1, int64_t arg2, int64_t flags);
extern "C" void KbdMsService();
void KbdPoll();
bool KbdMsInit();
bool KbdInit();
bool KbdCmdSend(uint16_t port, uint8_t value);
bool KbdCmdRead(uint8_t &value, bool auxiliary = false);
void Ps2PktRead();
void MsHardStageByte(uint8_t raw_byte);
bool MsHardDrvrInstall();
void MsInit();
void MsHardHndlr();
void WinMsInput();
void WinMsUpdate();
bool WinGrabScrollInput();
bool WinMenuInput();
bool WinMsCursorVisible();
extern CAutoCompleteGlbls ac;
extern "C" uint64_t AutoCompleteCpp(uint8_t val);
alignas(8) extern CWinMgrGlbls winmgr;
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
extern "C" uint64_t F64SinBits(uint64_t value);
extern "C" uint64_t F64CosBits(uint64_t value);
extern "C" uint64_t F64ArgBits(uint64_t x, uint64_t y);
extern "C" int64_t F64CompareBits(uint64_t n1, uint64_t n2);
void IntEntrySet(unsigned irq, void (*handler)(), uint16_t type = 0xE,
                 uint16_t dpl = 0);
uint64_t GetTSC();
extern CKbdStateGlbls kbd;
extern CTextGlbls text;
extern CMsHardStateGlbls ms_hard, ms_hard_last;
extern CMsStateGlbls ms, ms_last, old_ms;
extern CGridGlbls ms_grid;
extern "C" void PutKeyCpp(int64_t ch, int64_t sc);
extern "C" void PutCharsCpp(uint64_t chars);
extern "C" int64_t GetKeyCpp(int64_t *sc, uint8_t echo, uint8_t raw_cursor);
extern "C" char *GetStrCpp(const char *msg, const char *dft, int64_t flags);
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
extern "C" bool CallHolyC1Checked(uintptr_t function, uint64_t arg1);
extern "C" bool CallHolyC2Checked(uintptr_t function, uint64_t arg1,
                                  uint64_t arg2, uint64_t *result);
extern "C" bool CallHolyC3Checked(uintptr_t function, uint64_t arg1,
                                  uint64_t arg2, uint64_t arg3, uint64_t *result);
extern "C" bool CallHolyC4Checked(uintptr_t function, uint64_t arg1,
                                  uint64_t arg2, uint64_t arg3, uint64_t arg4,
                                  uint64_t *result);
extern "C" int64_t CallHolyC2(uintptr_t function, uint64_t arg1, uint64_t arg2);
extern "C" int64_t CallHolyC3(uintptr_t function, uint64_t arg1, uint64_t arg2,
                              uint64_t arg3);
extern "C" int64_t CallHolyC4(uintptr_t function, uint64_t arg1, uint64_t arg2,
                              uint64_t arg3, uint64_t arg4);

constexpr uint64_t STR_PRINT_ERROR = 0x746E697250727453;    // StrPrint
constexpr uint64_t UNDEF_DEFINE_ERROR = 0x6665446665646E55; // UndefDef

const char *DefineText(const char *name, bool &defined);
const char *DefineSub(int64_t index, const char *name, bool &defined);
[[noreturn]] void UndefinedDefine(const char *name);
const char *LstSub(int64_t index, const char *list);
extern "C" char *StrPrintJoin(char *dst, const char *fmt, int64_t argc,
                              const uint64_t *argv);
extern "C" char *CatPrintCpp(char *dst, const char *fmt, int64_t argc,
                             const uint64_t *argv);
extern "C" void PrintWarnCpp(const char *fmt, int64_t argc,
                             const uint64_t *argv, const uintptr_t *frame);
const CHash *FunSegFind(uintptr_t addr, uint64_t &offset);
int64_t YearStartDate(int64_t year);
CDate Struct2Date(const CDateStruct &ds);
int64_t DayOfWeek(int64_t date);
void Date2Struct(CDateStruct &ds, CDate cdt);
int64_t FirstDayOfMon(int64_t date);
int64_t LastDayOfMon(int64_t date);
int64_t FirstDayOfYear(int64_t date);
int64_t LastDayOfYear(int64_t date);
extern CDate local_time_offset;
extern uint16_t mon_start_days1[12], mon_start_days2[12];
extern "C" uint64_t Pow10I64Cpp(int64_t exponent);
extern "C" uint64_t IsRawCpp();
extern "C" uint64_t RawCpp(uint8_t enabled);
extern "C" uint64_t DbgModeCpp(uint8_t enabled);
extern "C" uint64_t IsDbgModeCpp();
void VGAFlush();
extern CBGR48 gr_palette_std[COLORS_NUM], gr_palette_gray[COLORS_NUM];
extern uint8_t gr_rainbow_10[10];
bool GrPaletteInit();
void GrPaletteIndicesSet();
bool GrInit2();
void GrUpdateScrn();
void BootTextLayer();
extern "C" void WinZBufUpdateCpp();
extern "C" CTask *WinRefocusCpp(CTask *task);
extern "C" int64_t WinToTopCpp(CTask *task, uint8_t update_z_buf);
extern "C" CTask *WinFocusCpp(CTask *task);
extern "C" bool WinHorzCpp(int64_t left, int64_t right, CTask *task);
extern "C" bool WinVertCpp(int64_t top, int64_t bottom, CTask *task);
extern "C" void WinTileHorzCpp();
extern "C" void WinTileVertCpp();
extern "C" void WinMaxCpp(CTask *task);
extern "C" bool WinBorderCpp(uint8_t val, CTask *task);
extern "C" void WinScrollNullCpp(CTask *task, CD3I64 *s);
extern "C" void WinScrollRestoreCpp(CTask *task, CD3I64 *s);
extern "C" void RefreshCpp(int64_t cnt, uint8_t force);
extern "C" void GrScaleZoomCpp(uint64_t scale);
void CtrlAltZ(int64_t sc);
void CtrlAltF(int64_t sc);
void CtrlAltC(int64_t sc);
extern "C" uint32_t sys_ctrl_alt_flags;
extern "C" void BreakCpp();
extern "C" CTask *BreakFocusUser(CTask *next);
bool WinMgrInit();
extern uint64_t sys_font_std[256],sys_font_cyrillic[256];
void WinDerivedValsUpdate(CTask *task);
bool StartUpTasks();
extern "C" uint64_t RandU16Cpp();
extern "C" CDC *DCNewCpp(int64_t width, int64_t height, CTask *task,
                         uint8_t null_bitmap);
extern "C" CDC *DCAliasCpp(CDC *dc, CTask *task);
extern "C" void DCDelCpp(CDC *dc);
extern "C" void DCFillCpp(CDC *dc, uint32_t val);
extern "C" void GrPaletteGetCpp(CBGR48 *bgr48);
extern "C" void GrPaletteSetCpp(const CBGR48 *bgr48);
extern "C" void PutExceptCpp(uint8_t catch_it);
extern "C" uint8_t *MAllocCpp(size_t size, void *mem_task);
extern "C" uint8_t *CAllocCpp(size_t size, void *mem_task);
extern "C" uint8_t *MAllocIdentCpp(const void *src, void *mem_task);
extern "C" uint64_t FileWriteCpp(const char *filename, const uint8_t *data,
                                 uint64_t count, uint64_t date, uint64_t attr);
extern "C" void PaletteSetStdCpp();
extern "C" CHashDefineStr *DefineLoadCpp(const char *name, const char *text,
                                         const uintptr_t *frame);
extern "C" const char *DefineCpp(const char *name);
extern "C" int64_t DefineCntCpp(const char *name);
extern "C" void DefinePrintCpp(const char *name, const char *src, int64_t argc,
                               const uint64_t *argv);
extern "C" CHashDefineStr *DefineLstLoadCpp(const char *name, const char *list,
                                            const uintptr_t *frame);
bool SysDefinesLoad();
extern "C" char *Color2StrCpp(char *buf, uint32_t color);
extern char *str2color_lst;
extern "C" uint64_t Str2ColorU16Cpp(const char *st);
extern "C" uint64_t Str2ColorU32Cpp(char *st);
bool LoadDocDefines();
extern "C" void PutS(const char *text);
extern "C" void StrCpyCpp(char *dst, const char *src);
void PutTaskS(CTask &task, const char *text);

CFifoU8 *FifoU8New(int64_t size, CTask *mem_task = nullptr);
void FifoU8Del(CFifoU8 *f);
CFifoI64 *FifoI64New(int64_t size, CTask *mem_task = nullptr);
void FifoI64Del(CFifoI64 *f);

template <typename T> inline int64_t FifoCnt(const CFifo<T> &f)
{
  uint64_t flags;
  asm volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
  int64_t res = f.out_ptr > f.in_ptr
                    ? f.mask + 1 - (f.out_ptr - f.in_ptr)
                    : f.in_ptr - f.out_ptr;
  SetRFlags(flags);
  return res;
}

template <typename T> inline bool FifoIns(CFifo<T> &f, T value)
{
  uint64_t flags;
  asm volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
  auto next = (f.in_ptr + 1) & f.mask;
  if (next == f.out_ptr) {
    SetRFlags(flags);
    return false;
  }
  f.buf[f.in_ptr] = value;
  f.in_ptr = next;
  SetRFlags(flags);
  return true;
}

template <typename T> inline bool FifoRem(CFifo<T> &f, T *value)
{
  uint64_t flags;
  asm volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
  if (f.in_ptr == f.out_ptr) {
    SetRFlags(flags);
    return false;
  }
  *value = f.buf[f.out_ptr];
  f.out_ptr = (f.out_ptr + 1) & f.mask;
  SetRFlags(flags);
  return true;
}

template <typename T> inline bool FifoRem(CFifo<T> &f, T &value)
{
  return FifoRem(f, &value);
}

template <typename T> inline bool FifoPeek(const CFifo<T> &f, T *value)
{
  uint64_t flags;
  asm volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
  if (f.in_ptr == f.out_ptr) {
    SetRFlags(flags);
    return false;
  }
  *value = f.buf[f.out_ptr];
  SetRFlags(flags);
  return true;
}

template <typename T> inline void FifoFlush(CFifo<T> &f)
{
  uint64_t flags;
  asm volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
  f.out_ptr = f.in_ptr;
  SetRFlags(flags);
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

extern CTask *adam_task, *sys_focus_task, *sys_winmgr_task,
    *sys_task_being_scrn_updated;
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
extern "C" void SndShiftCpp(CSndData *head, uint64_t dt);
extern "C" int64_t AUWriteCpp(const char *filename, CSndData *head,
                             uint64_t t0_now, uint64_t t0_tS);
extern "C" void BeepCpp(int8_t ona, uint8_t busy);
extern "C" uint64_t MuteCpp(uint8_t val);
extern "C" uint64_t IsMuteCpp();
uint16_t PCIReadU16(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg);
uint32_t PCIReadU32(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg);
int32_t PCIClassFind(uint32_t class_code, uint16_t index);

uint32_t ATAProbe(const CBlkDev &bd);
bool ATAReadCapacity(CBlkDev &bd);
bool ATAIdentifyRecord(CBlkDev &bd);
bool ATAReadBytes(CBlkDev &bd, uint8_t *buf, uint64_t pos, size_t bytes);
bool ATAWriteBytes(CBlkDev &bd, const uint8_t *buf, uint64_t pos, size_t bytes);
bool ATAPIStartStop(const CBlkDev &bd, bool start);
bool ATAPIPreventRemoval(const CBlkDev &bd, bool prevent);
bool ATAPIReadCapacity(CBlkDev &bd);
bool ATAPIReadBlks2(const CBlkDev &bd, uint8_t *buf, uint32_t lba,
                    uint32_t count);
bool BootDVDProbeAll(CBlkDev &bd);
bool ATAPIReadBytes(const CBlkDev &bd, uint8_t *buf, uint64_t pos,
                    size_t bytes);

CDrv *BlkDevsInitAll();
bool BlkDevGlblsInit();
CDrv *MountedDrive(char letter);
extern uint64_t disk_lock_flags;
extern uint64_t mount_flags;
CBlkDev *BlkDevLockTarget(CBlkDev *bd);
bool BlkDevLockOwner(CBlkDev *bd);
bool BlkDevUnlockOwner(CBlkDev *bd,bool rst);
bool DrvLockOwner(CDrv *dv);
bool DrvUnlockOwner(CDrv *dv);
extern "C" uint64_t DrvLockCpp(CDrv *dv);
extern "C" uint64_t DrvUnlockCpp(CDrv *dv,uint8_t rst);
extern "C" void DrvsReleaseCpp();
extern "C" void DrvDelCpp(CDrv *dv);
extern "C" void DrvBlkDevDelCpp(CBlkDev *bd);
extern "C" void BlkDevDelCpp(CBlkDev *bd);
extern "C" uint64_t DrvEnableCpp(uint8_t drv_let,uint8_t val);
extern "C" void DskCacheInvalidateCpp(CDrv *dv);
extern "C" void DskChgCpp(uint8_t drv_let);
// Lifecycle helpers require mount_flags and disk_lock_flags held.
bool DrvDeleteReady(const CDrv *dv);
void DrvDelete(CDrv &dv);
bool BlkDevDeleteReady(const CBlkDev *bd);
void BlkDevDelete(CBlkDev &bd,uint8_t *keep_ram=nullptr);
uint8_t *BlkDevOwnedRAM(const CBlkDev *bd);
void BlkDevReleaseRAM(CBlkDev &bd,uint8_t *keep_ram);
bool BlkDevSameBacking(const CBlkDev &a,const CBlkDev &b);
bool BlkDevImageDependsOn(const CBlkDev &image,const CBlkDev &parent);
bool BlkDevImageBacking(const CBlkDev *bd);
bool BlkDevRegistered(const CBlkDev *bd);
bool BlkDevImageChain(const CBlkDev &bd,bool write=false);
bool BlkDevImageCanWrite(const CBlkDev *bd);
bool BlkDevMediaPin(CBlkDev &bd);
void BlkDevMediaUnpin(CBlkDev &bd);
bool BlkDevMediaPinned(const CBlkDev &bd);
bool DrvHasImageDependents(const CDrv &dv);
extern "C" uint64_t BlkDevLockCpp(CBlkDev *bd);
extern "C" uint64_t BlkDevUnlockCpp(CBlkDev *bd,uint8_t rst);
bool BlkDevEnsureInitialized(CBlkDev &bd);
extern "C" uint64_t BlkDevInitCpp(CBlkDev *bd);
extern "C" void BlkDevsReleaseCpp();
extern "C" void DskCacheInitCpp(int64_t size_in_U8s);
extern "C" void DskCacheAddCpp(CDrv *dv,const uint8_t *buf,int64_t blk,int64_t cnt);
extern "C" void DskCacheInvalidate2Cpp(CDrv *dv);
extern "C" void RCacheCpp(CDrv *dv,uint8_t **buf,int64_t *blk,int64_t *cnt);
void DskCacheInvalidateDevice(CBlkDev *bd);
void DskCacheWriteThrough(CBlkDev *bd,const uint8_t *buf,uint64_t pos,
                          size_t bytes,bool okay);
bool DrvReadBytes(const CDrv &drive,uint8_t *buf,uint64_t pos,size_t bytes);

// Native scopes retain existing non-yielding I/O and preserve outer ownership.
// Acquire before disabling interrupts; release without entering the scheduler.
class CDiskLock
{
public:
  explicit CDiskLock(CBlkDev &bd) : CDiskLock(&bd,nullptr) {}
  explicit CDiskLock(CDrv &dv) : CDiskLock(dv.bd,&dv) {}
  ~CDiskLock()
  {
    if (unlock_dv) DrvUnlockOwner(dv);
    if (unlock_bd) BlkDevUnlockOwner(bd,false);
    SetRFlags(rflags);
  }
  CDiskLock(const CDiskLock &)=delete;
  CDiskLock &operator=(const CDiskLock &)=delete;
private:
  CDiskLock(CBlkDev *device,CDrv *drive) :
      bd(BlkDevLockTarget(device)),dv(drive),
      unlock_bd(bd && BlkDevLockOwner(bd)),
      unlock_dv(dv && DrvLockOwner(dv)),rflags(GetRFlags())
  {
    asm volatile("cli" : : : "memory");
  }
  CBlkDev *bd;
  CDrv *dv;
  bool unlock_bd,unlock_dv;
  uint64_t rflags;
};

extern "C" CDrv *DrvMakeFreeSlotCpp(uint8_t drv_let);
extern "C" void DrvFATBlkAllocCpp(CDrv *dv);
extern "C" void DrvFATBlkCleanCpp(CDrv *dv,int64_t fat_sel=3);
extern "C" void DrvFATBlkSetCpp(CDrv *dv,int64_t c,int64_t fat_sel=3);
bool DrvFATCacheInit(CDrv &dv);
void DrvFATCacheDrop(CDrv &dv);
void DrvFATCacheReset(CDrv &dv);
void DrvFATCacheResetDevice(CBlkDev *bd,bool controller=false);
bool DrvFreeListValid(CDrv &dv);
void DrvFreeListClear(CDrv &dv);
bool DrvFATRead(const CDrv &dv,uint64_t index,void *buf,size_t offset,size_t bytes);
bool DrvFATWriteAllowed(CBlkDev *bd,uint64_t pos,size_t bytes,const CDrv *origin=nullptr);
bool DrvFATWriteAllowedImage(CBlkDev *bd,uint64_t pos,size_t bytes,const CDrv *origin);
void DrvFATWriteThrough(CBlkDev *bd,const uint8_t *buf,uint64_t pos,size_t bytes,
                        bool okay,const CDrv *origin=nullptr);
extern "C" uint64_t DrvNextFreeLetCpp(uint8_t first_drv_let);
extern "C" CDrv *DrvChkCpp(CDrv *dv,uint8_t except);
extern "C" uint64_t Drv2LetCpp(CDrv *dv);
extern "C" uint64_t Let2LetCpp(uint8_t drv_let);
extern "C" int64_t Let2BlkDevTypeCpp(uint8_t drv_let);
extern "C" CDrv *Let2DrvCpp(uint8_t letter, uint8_t except);
extern "C" CBlkDev *DrvIsWritableCpp(uint8_t drv_let,uint8_t except);
extern "C" uint64_t DrvMapCpp(uint8_t drv_let,CDrv *dv);
extern "C" uint64_t DrvCpp(uint8_t drv_let);
extern "C" char *DrvSerialNumCpp(uint8_t drv_let);
extern "C" char *DrvModelNumCpp(uint8_t drv_let);
extern "C" uint64_t DrvTextAttrGetCpp(uint8_t drv_let);
extern "C" void DrvRepCpp();
extern "C" uint8_t blkdev_text_attr[6],drv_text_attr[3];
CBlkDev *BlkDevNextFreeSlot(uint8_t first_drv_let,uint32_t type);
extern "C" CBlkDev *BlkDevNextFreeSlotCpp(uint8_t first_drv_let,int64_t type);
extern "C" CBlkDev *BlkDevChkCpp(CBlkDev *bd,uint8_t except);
extern "C" CBlkDev *Let2BlkDevCpp(uint8_t drv_let,uint8_t except);
extern "C" int64_t BlkDevAddCpp(CBlkDev *bd,int64_t prt_num,uint8_t whole_drv,
                                uint8_t make_free);
extern "C" void UnmountCpp(uint8_t drv_let);
extern "C" uint64_t MountFileCpp(const char *filename);
bool BlkDevReadBytes(CBlkDev &bd, uint8_t *buf, uint64_t pos,
                     size_t bytes);
bool BlkDevWriteBytes(CBlkDev &bd, const uint8_t *buf, uint64_t pos,
                      size_t bytes,const CDrv *fat_origin=nullptr);
uint64_t Now();
bool ISOInit(CDrv &dv);
bool RedSeaInit(CDrv &dv);
bool RedSeaFormatRAM(CDrv &dv, size_t root_blocks);
bool RedSeaFormatDrive(CDrv &dv,bool quick);
extern "C" uint64_t RedSeaValidateCpp(uint8_t drv_let);
extern "C" void RedSeaFmtCpp(uint8_t drv_let,uint8_t quick);
extern "C" void RedSeaFreeFreeLstCpp(CDrv *dv);
extern "C" void RedSeaFreeLstBuildCpp(CDrv *dv);
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
bool FAT32Init(CDrv &dv);
bool FAT32Fmt(CDrv &dv, bool quick);
bool DrvTypeSet(CDrv &dv, uint8_t type, bool write);
bool BlkWriteZero(CDrv &dv, uint64_t blk, uint64_t cnt);
bool FAT32FileFind(const CDrv &dv, uint64_t dir, const char *name,
                   bool directory, CRedSeaDirEntry &entry);
bool FAT32FileRead(const CDrv &dv, const CRedSeaDirEntry &entry, uint64_t pos,
                   uint8_t *buf, size_t bytes);
bool FAT32FilesFind(CDrv &dv, uint64_t clus, const char *directory,
                    const char *mask, uint64_t flags, CDirEntry *parent,
                    CDirEntry *&res, uint64_t &dir_size);
bool FAT32ReadBlocks(CFile &f, uint8_t *buf, uint64_t blk, uint64_t cnt);
bool FAT32WriteBlocks(CFile &f, const uint8_t *buf, uint64_t blk, uint64_t cnt);
bool FAT32StreamPrepare(CFile &f, const char *path, int64_t cnt);
bool FAT32StreamCommit(CFile &f, const char *path);
bool FAT32StreamEnsure(CFile &f, uint64_t blocks, bool grow);
bool FAT32StreamCluster(CFile &f, uint64_t index, uint32_t &clus);
bool FAT32ClusRead(CDrv &dv, uint32_t clus, uint8_t *buf, uint64_t blocks);
bool FAT32ClusWrite(CDrv &dv, uint32_t clus, const uint8_t *buf, uint64_t blocks);
uint64_t FAT32FileWrite(CDrv &dv,const char *path,const uint8_t *data,
                         size_t size,uint64_t date,uint16_t attr);
bool FAT32MkDir(CDrv &dv,const char *path,int64_t entry_cnt=0);
int64_t FAT32FilesDel(CDrv &dv,const char *cur_dir,const char *mask,
                      uint64_t flags,bool del_dir,bool print_msg);
inline bool ReadableFS(const CDrv &dv)
{
  return dv.fs_type==FSt_REDSEA || dv.fs_type==FSt_FAT32;
}
inline bool WritableFS(const CDrv &dv)
{
  return dv.bd && BlkDevImageCanWrite(dv.bd) && !BlkDevImageBacking(dv.bd) &&
         ((dv.fs_type==FSt_REDSEA &&
           (dv.bd->type==BDT_RAM || dv.bd->type==BDT_ISO_FILE_WRITE)) ||
          (dv.fs_type==FSt_FAT32 && dv.bd->type==BDT_ATA));
}
inline bool FsMkDir(CDrv &dv,const char *path,int64_t entry_cnt=0);
bool Name2DirClus(const CDrv &dv, const char *path, size_t count, uint64_t &dir);
bool Name2ParentDirClus(const CDrv &dv, const char *path, size_t count,
                        uint64_t &parent);
extern "C" uint64_t Name2DirClusCpp(CDrv *dv,const char *dirname);
extern "C" uint64_t Name2ParentDirClusCpp(CDrv *dv,const char *dirname);
bool FsIsDir(const CDrv &dv, const char *path);
bool FsFileFind(const CDrv &dv, uint64_t dir, const char *name,
                bool directory, CRedSeaDirEntry &entry);
bool FsFileLookup(const CDrv &dv, const char *path, bool alternate,
                   bool scan_parents, CRedSeaDirEntry &entry, size_t &prefix);
uint64_t RedSeaAllocClus(const CDrv &dv, uint64_t blocks);
CDrv *PathDrive(const CTask &task, char letter);
uint64_t RedSeaFileWrite(CDrv &dv, const char *path, const uint8_t *data,
                         size_t size, uint64_t date, uint16_t attr);
bool RedSeaStreamPrepare(CDrv &dv, const char *path, CDirEntry &de, int64_t cnt);
bool RedSeaStreamCommit(CDrv &dv, const char *path, CDirEntry &de, bool new_file);
extern "C" int64_t FSizeCpp(CFile *f);
extern "C" CFile *FOpenCpp(const char *filename, const char *flags, int64_t cnt);
CFile *FOpenImageRead(const char *absolute,CDrv *backing);
struct CImageMountUndo
{
  CRedSeaDirEntry old{};
  uint8_t *data=nullptr;
  bool existed=false,prepared=false,rollback_failed=false;
};
CFile *FOpenImageWrite(const char *absolute,CDrv *backing,int64_t cnt,
                       CImageMountUndo &undo);
bool FDiscardImageWrite(CFile *f,CImageMountUndo &undo);
void FCloseImage(CFile *f);
bool FImageByteRange(const CFile &f,uint64_t pos,size_t bytes,uint64_t &physical);
bool FImageReadBytes(CFile *f,uint8_t *buf,uint64_t pos,size_t bytes);
bool FImageWriteBytes(CFile *f,const uint8_t *buf,uint64_t pos,size_t bytes);
extern "C" void FCloseCpp(CFile *f);
extern "C" uint64_t FBlkReadCpp(CFile *f, uint8_t *buf, int64_t blk, int64_t cnt);
extern "C" uint64_t FBlkWriteCpp(CFile *f, const uint8_t *buf, int64_t blk, int64_t cnt);
extern "C" uint64_t BlkReadCpp(CDrv *dv, uint8_t *buf, int64_t blk, int64_t count);
extern "C" uint64_t BlkWriteCpp(CDrv *dv, const uint8_t *buf, int64_t blk, int64_t count);
extern "C" int64_t ClusNumNextCpp(CDrv *dv,int64_t c,int64_t cnt);
extern "C" int64_t Clus2BlkCpp(CDrv *dv,int64_t c);
extern "C" int64_t ClusBlkReadCpp(CDrv *dv,uint8_t *buf,int64_t c,int64_t blks);
extern "C" int64_t ClusReadCpp(CDrv *dv,uint8_t *buf,int64_t c,int64_t cnt);
extern "C" int64_t ClusBlkWriteCpp(CDrv *dv,const uint8_t *buf,int64_t c,int64_t blks);
extern "C" int64_t ClusWriteCpp(CDrv *dv,const uint8_t *buf,int64_t c,int64_t cnt);
extern "C" int64_t ClusAllocCpp(CDrv *dv,int64_t c,int64_t cnt,uint8_t contiguous);
bool FAT32GetClusLink(const CDrv &dv,uint64_t c,uint32_t &next);
extern "C" int64_t FAT32AllocClusCpp(CDrv *dv,int64_t c,int64_t cnt);
extern "C" int64_t FAT32AllocContiguousClusCpp(CDrv *dv,int64_t cnt);
extern "C" void FAT32FreeClusCpp(CDrv *dv,int64_t c);
extern "C" int64_t RedSeaAllocClusCpp(CDrv *dv,int64_t cnt);
extern "C" void RedSeaFreeClusCpp(CDrv *dv,int64_t c,int64_t cnt);
extern "C" uint64_t FAT32DirNewCpp(CDrv *dv,const char *cur_dir,CDirEntry *entry,
                                   uint8_t free_old_chain);
extern "C" uint64_t RedSeaDirNewCpp(CDrv *dv,const char *cur_dir,CDirEntry *entry,
                                    uint8_t free_old_chain);
extern "C" uint64_t DirNewCpp(CDrv *dv,const char *cur_dir,CDirEntry *entry,
                              uint8_t free_old_chain);
char *FileNameAbs(const char *name, const CTask &task, uint64_t flags = 0);
char *DirNameAbs(const char *name, const CTask &task);
char *PathTrim(const char *name, CHeapCtrl *hc);
bool FileNameChk(const char *name);
extern "C" CDirContext *DirContextNewCpp(const char *mask, uint8_t make_mask,
                                         uint8_t make_dirs, uint8_t no_mask);
extern "C" void DirContextDelCpp(CDirContext *dirc, uint8_t restore);
bool FilesFindMatch(const char *name, const char *mask, uint64_t flags = 0);
extern "C" uint64_t FilesFindMatchCpp(const char *name, const char *mask,
                                      uint64_t flags);
extern "C" char *ExtDftCpp(const char *name, const char *ext);
const char *FileExtDot(const char *name);
extern "C" char *FileExtDotCpp(char *src);
extern "C" char *FileExtRemCpp(char *src,char *dst);
extern "C" uint64_t IsDotZCpp(const char *filename);
extern "C" uint64_t IsDotCCpp(const char *filename);
extern "C" uint64_t FileNameChkCpp(const char *filename);
extern "C" uint64_t CFileNameToCpp(char *dst,const char *src);
extern "C" char *ToggleZorNotZCpp(const char *name);
extern "C" char *FileNameTmpTxtCpp();
extern "C" CDirEntry *Cd2DirEntryCpp(CDirEntry *tmpde,const char *abs_name);
bool IsDotExt(const char *name, char ext);
extern "C" uint64_t FileAttrCpp(const char *name,uint64_t attr);
extern "C" uint64_t CopySingleCpp(const char *f1,const char *f2);
extern "C" int64_t DelCpp(const char *mask, uint8_t make_mask,
                           uint8_t del_dir, uint8_t print_msg);
extern "C" uint64_t CopyCpp(const char *src, const char *dst);
extern "C" uint64_t MoveCpp(const char *f1, const char *f2);
extern "C" int64_t CopyTreeCpp(const char *src, const char *dst,
                                uint8_t no_mask);
extern "C" int64_t DelTreeCpp(const char *mask, const char *fu_flags);
extern "C" void ScanFlagsCpp(uint8_t *dst, const uint8_t *list,
                             const char *src);
int64_t DirEntryCompareName(const CRedSeaDirEntry &a, const CRedSeaDirEntry &b);
bool DirFilesSort(CDirEntry *&first, bool by_cluster);
extern "C" int64_t DirEntryCompareNameCpp(const CDirEntry *a,const CDirEntry *b);
extern "C" int64_t DirEntryCompareClusCpp(const CDirEntry *a,const CDirEntry *b);
extern "C" void DirFilesSortCpp(CDirEntry **first,int64_t key);
void DirFilesFlatten(CDirEntry *&first, uint64_t flags);
extern "C" CDirEntry *DirFilesFlattenCpp(CDirEntry *entry,CDirEntry **res,
                                        uint64_t flags);
extern "C" CDirEntry *FilesFind2Cpp(const char *mask,uint64_t flags);
char *DirEntryFullName(const char *directory, const char *name);
extern "C" void DirEntryDelCpp(CDirEntry *entry);
extern "C" void DirEntryDel2Cpp(CDirEntry *entry);
extern "C" void DirTreeDelCpp(CDirEntry *entry);
extern "C" void DirTreeDel2Cpp(CDirEntry *entry);
extern "C" CDirEntry *FilesFindCpp(const char *mask, uint64_t flags);
extern "C" uint64_t FileFindCpp(const char *name, CDirEntry *entry,
                                uint64_t flags);
extern "C" int64_t DirCpp(const char *mask, uint8_t full);
extern "C" uint64_t CdCpp(const char *name, uint8_t make_dirs);
extern "C" uint64_t IsDirCpp(const char *name);
extern "C" uint64_t DirMkCpp(const char *filename, int64_t entry_cnt);
extern "C" void PutDirLinkCpp(const char *name, const char *full_name);
extern "C" void PutFileLinkCpp(const char *filename, const char *full_name,
                               int64_t line, uint8_t plain_text);
extern "C" char *DirCurCpp(const CTask *task, void *mem_task);
bool RedSeaFilesFind(CDrv &dv, uint64_t clus, uint64_t parent_clus,
                     const char *directory, const char *mask, uint64_t flags,
                     CDirEntry *parent, CDirEntry *&res);
bool RedSeaMkDir(CDrv &dv, const char *path, int64_t entry_cnt = 0);
inline bool FsMkDir(CDrv &dv,const char *path,int64_t entry_cnt)
{
  return dv.fs_type==FSt_FAT32?FAT32MkDir(dv,path,entry_cnt):RedSeaMkDir(dv,path,entry_cnt);
}
int64_t RedSeaFilesDel(CDrv &dv, const char *cur_dir, const char *mask,
                      uint64_t flags, bool del_dir, bool print_msg);
void QSortI64Native(int64_t *base, int64_t count,
                    int64_t (*compare)(int64_t, int64_t));
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
extern "C" uint8_t *LinkedLstCopyCpp(const void *node,CTask *mem_task);
extern "C" int64_t LinkedLstCntCpp(const void *node);
extern "C" uint64_t LinkedLstSizeCpp(const void *node);
extern "C" void QueDelCpp(CQue *head,uint8_t remove);
extern "C" CQue *QueCopyCpp(CQue *head,CTask *mem_task);
extern "C" int64_t QueCntCpp(CQue *head);
extern "C" int64_t QueSizeCpp(CQue *head);
extern "C" char *MStrPrintCpp(const char *fmt, int64_t argc,
                              const uint64_t *argv);

struct CLoadedModule;
constexpr unsigned LDF_NO_ABSS = 1, LDF_JUST_LOAD = 2, LDF_SILENT = 4;
extern "C" void LoadPass2Cpp(const uint8_t *src, uint8_t *module_base,
                             int64_t ld_flags);
extern "C" CBinFile *LoadCpp(const char *filename, int64_t ld_flags,
                             CBinFile *bfh_addr);
bool LoadKernel(CTask &task);
CLoadedModule *Load(const CDrv &dv, const char *path, CTask &task,
                    unsigned flags = 0);
bool LoadPass2(CLoadedModule &module);
bool LoadCompilerImports(CLoadedModule &module, const int64_t *&status);
