#include "KernelB.hpp"

CTask *adam_task, *sys_focus_task, *sys_winmgr_task,
    *sys_task_being_scrn_updated;
CCPU *cpu_structs;
int64_t mp_cnt;
CAutoCompleteGlbls ac;
CBlkDevGlbls blkdev;
CDate local_time_offset;
CDbgGlbls dbg;
CSema sys_semas[SEMA_SEMAS_NUM];
CScrnCastGlbls scrncast;
CKbdStateGlbls kbd;
CTextGlbls text;
CMsHardStateGlbls ms_hard, ms_hard_last;
CMsStateGlbls ms, ms_last, old_ms;
CGridGlbls ms_grid;
CJob sys_macro_head;
CTask *sys_macro_task;
CDevGlbls dev;
CCntsGlbls cnts{1, 0, 2676302000, 2676302, 2676302000, 0, 0, 0, false};
uint64_t sys_cache_line_width;
uint64_t sys_os_version = __builtin_bit_cast(uint64_t, 5.030);
uint8_t *rev_bits_table, *set_bits_table;
uint64_t *pow10_I64;
CExt *ext;

extern "C" {
uint64_t MEM_HEAP_LIMIT;
}
