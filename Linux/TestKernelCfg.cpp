#include "../Kernel/KCfg.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>

int main()
{
  static_assert(KCfgHomeDrive("::/Home",'T')=='T');
  static_assert(KCfgHomeDrive("d:/Account",'T')=='D');
  static_assert(!KCfgHomeDrive("",'T'));
  static_assert(!KCfgHomeDrive("::Home",'T'));
  static_assert(!KCfgHomeDrive("1:/Home",'T'));
  static_assert(KCfgScale2Mem(0,2048,0x40000,0x80000000)==2048);
  static_assert(KCfgScale2Mem(0x40000000,2048,0x40000,0x80000000)==132096);
  static_assert(KCfgScale2Mem(0x80000000,2048,0x40000,0x80000000)==0x40000);
  static_assert(KCfgScale2Mem(UINT64_MAX,8192,8192,0x80000000)==8192);

  // This object is assembled with the explicit test options in Makefile.
  assert(sys_mem_init_flag==1 && sys_mem_init_val==0xA5);
  assert(sys_heap_init_flag==1 && sys_heap_init_val==0);
  assert(sys_var_init_flag==0 && sys_var_init_val==0);
  assert(kcfg_boot_drv_let=='U' && kcfg_first_hd_drv_let=='D');
  assert(kcfg_mount_primary_hd==0);
  assert(std::strcmp(kcfg_home_dir,"::/Account")==0);
  assert(KCfgHomeDrive(kcfg_home_dir,kcfg_boot_drv_let)=='U');
  assert(KCfgScale2Mem(0x20000000,kcfg_cache_min,kcfg_cache_max,
                      kcfg_cache_limit)==32768);
  sys_var_init_flag=1;
  sys_var_init_val=0x5A;
  assert(sys_var_init_flag==1 && sys_var_init_val==0x5A);
  std::puts("Kernel configuration checks passed");
}
