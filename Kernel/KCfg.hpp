#pragma once

#include <stdint.h>

extern uint8_t sys_mem_init_flag, sys_mem_init_val;
extern uint8_t sys_heap_init_flag, sys_heap_init_val;
extern uint8_t sys_var_init_flag, sys_var_init_val;

extern const char kcfg_home_dir[];
extern const uint8_t kcfg_boot_drv_let, kcfg_first_hd_drv_let,
    kcfg_mount_primary_hd;
extern const uint32_t kcfg_cache_min, kcfg_cache_max, kcfg_cache_limit;

constexpr uint64_t KCfgScale2Mem(uint64_t mem, uint32_t min, uint32_t max,
                                 uint32_t limit)
{
  return mem>=limit?max:min+uint64_t(max-min)*mem/limit;
}

constexpr char KCfgHomeDrive(const char *home,char boot)
{
  if (!home[0] || home[1]!=':' || home[2]!='/')
    return 0;
  char let=home[0]==':'?boot:home[0];
  if (let>='a' && let<='z')
    let-='a'-'A';
  return let>='A' && let<='Z'?let:0;
}
