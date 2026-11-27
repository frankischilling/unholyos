; Host bootstrap configuration for the DVD kernel.  The defaults follow
; STD_DISTRO_DVD_CFG and retain the current primary IDE disk discovery.
; -1 disables the corresponding initialization; 0..255 selects a byte.
%ifndef KCFG_MEM_INIT
%define KCFG_MEM_INIT -1
%endif
%ifndef KCFG_HEAP_INIT
%define KCFG_HEAP_INIT -1
%endif
%ifndef KCFG_VAR_INIT
%define KCFG_VAR_INIT -1
%endif
%ifndef KCFG_TEXT_MODE
%define KCFG_TEXT_MODE 0
%endif
%ifndef KCFG_BOOT_DRV_LET
%define KCFG_BOOT_DRV_LET 'T'
%endif
%ifndef KCFG_HOME_DIR
%define KCFG_HOME_DIR "::/Home"
%endif
%ifndef KCFG_FIRST_HD_DRV_LET
%define KCFG_FIRST_HD_DRV_LET 'C'
%endif
%ifndef KCFG_MOUNT_PRIMARY_HD
%define KCFG_MOUNT_PRIMARY_HD 1
%endif
%ifndef KCFG_CACHE_MIN
%define KCFG_CACHE_MIN 2048
%endif
%ifndef KCFG_CACHE_MAX
%define KCFG_CACHE_MAX 0x40000
%endif
%ifndef KCFG_CACHE_LIMIT
%define KCFG_CACHE_LIMIT 0x80000000
%endif

%if KCFG_MEM_INIT < -1 || KCFG_MEM_INIT > 255 || KCFG_HEAP_INIT < -1 || KCFG_HEAP_INIT > 255 || KCFG_VAR_INIT < -1 || KCFG_VAR_INIT > 255
%error Kernel initialization values must be -1 or 0..255
%endif
%if KCFG_TEXT_MODE < 0 || KCFG_TEXT_MODE > 1 || KCFG_MOUNT_PRIMARY_HD < 0 || KCFG_MOUNT_PRIMARY_HD > 1
%error Kernel configuration switches must be 0 or 1
%endif
%if KCFG_BOOT_DRV_LET < 'T' || KCFG_BOOT_DRV_LET > 'Z' || KCFG_FIRST_HD_DRV_LET < 'C' || KCFG_FIRST_HD_DRV_LET > 'L'
%error Invalid DVD or ATA drive letter
%endif
%if KCFG_CACHE_MIN < 0 || KCFG_CACHE_MAX < KCFG_CACHE_MIN || KCFG_CACHE_MAX > 0x7FFFFFFF || KCFG_CACHE_LIMIT < 1 || KCFG_CACHE_LIMIT > 0xFFFFFFFF
%error Invalid disk cache scaling range
%endif

%macro KCFG_INIT_BYTE 3
%if %3 < 0
%1: DB 0
%2: DB 0
%else
%1: DB 1
%2: DB %3
%endif
%endmacro
