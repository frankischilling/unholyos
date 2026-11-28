%include "Kernel/KernelA.inc"
%include "Kernel/KCfg.inc"

GLOBAL CORE0_32BIT_INIT
GLOBAL sys_mem_init_flag,sys_mem_init_val,sys_heap_init_flag,sys_heap_init_val
GLOBAL sys_var_init_flag,sys_var_init_val
GLOBAL kcfg_home_dir,kcfg_boot_drv_let,kcfg_first_hd_drv_let,kcfg_mount_primary_hd
GLOBAL kcfg_cache_min,kcfg_cache_max,kcfg_cache_limit
EXTERN SYS_RUN_LEVEL,__sys_run_level_offset
EXTERN SYS_INIT_PAGE_TABLES,SYS_INIT_16MEG_SYS_CODE_BP,SYS_ENTER_LONG_MODE
EXTERN SYS_SERIAL_INIT,__bss_start,__bss_end
EXTERN SYS_FIND_PCIBIOS_SERVICE_DIR,SYS_FIND_PCI_SERVICES,CORE0_64BIT_INIT

SECTION .boot32 progbits alloc exec nowrite align=16
BITS 32

CORE0_32BIT_INIT:
	PUSH	DWORD 0
	POPFD
	MOV	AX,GDT_BOOT_DS
	MOV	DS,AX
	BTS	DWORD [__sys_run_level_offset],RLf_32BIT

	MOV	AX,GDT_DS
	MOV	DS,AX
	MOV	ES,AX
	MOV	FS,AX
	MOV	GS,AX
	MOV	SS,AX
	MOV	ESP,BOOT_RAM_LIMIT
	JMP	GDT_CS32:.flat

.flat:
	CLD
	XOR	EAX,EAX
	MOV	EDI,__bss_start
	MOV	ECX,__bss_end
	SUB	ECX,EDI
	REP STOSB

	; The linker resolves startup addresses at the CD load address.
	BTS	DWORD [SYS_RUN_LEVEL],RLf_PATCHED
	CALL	SYS_SERIAL_INIT
	CALL	SYS_FIND_PCIBIOS_SERVICE_DIR
	CALL	SYS_FIND_PCI_SERVICES
	CALL	SYS_INIT_PAGE_TABLES
	CALL	SYS_INIT_16MEG_SYS_CODE_BP
	PUSH	DWORD 0
	CALL	SYS_ENTER_LONG_MODE
BITS 64
	JMP	CORE0_64BIT_INIT

SECTION .data
	KCFG_INIT_BYTE sys_mem_init_flag,sys_mem_init_val,KCFG_MEM_INIT
	KCFG_INIT_BYTE sys_heap_init_flag,sys_heap_init_val,KCFG_HEAP_INIT
	KCFG_INIT_BYTE sys_var_init_flag,sys_var_init_val,KCFG_VAR_INIT

SECTION .rodata
kcfg_boot_drv_let:	DB KCFG_BOOT_DRV_LET
kcfg_first_hd_drv_let:	DB KCFG_FIRST_HD_DRV_LET
kcfg_mount_primary_hd:	DB KCFG_MOUNT_PRIMARY_HD
kcfg_home_dir:		DB KCFG_HOME_DIR,0
ALIGN 4
kcfg_cache_min:		DD KCFG_CACHE_MIN
kcfg_cache_max:		DD KCFG_CACHE_MAX
kcfg_cache_limit:	DD KCFG_CACHE_LIMIT

SECTION .bss
ALIGNB 8
GLOBAL SYS_PROGRESSES
SYS_PROGRESSES:
%macro PROGRESS_RECORD 1
GLOBAL SYS_PROGRESS%1,SYS_PROGRESS%1_MAX,SYS_PROGRESS%1_T0
GLOBAL SYS_PROGRESS%1_TF,SYS_PROGRESS%1_DESC
SYS_PROGRESS%1:		RESQ 1
SYS_PROGRESS%1_MAX:	RESQ 1
SYS_PROGRESS%1_T0:	RESQ 1
SYS_PROGRESS%1_TF:	RESQ 1
SYS_PROGRESS%1_DESC:	RESB 64 - 8 - 8
%endmacro

	PROGRESS_RECORD 1
	PROGRESS_RECORD 2
	PROGRESS_RECORD 3
	PROGRESS_RECORD 4

SECTION .note.GNU-stack noalloc noexec nowrite progbits
