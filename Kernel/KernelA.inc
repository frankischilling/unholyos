%define BOOT_RAM_BASE		0x7C00
%define BOOT_RAM_LIMIT		0x97000
%define BOOT_STK_SIZE		0x200
%define BOOT_SRC_DVD		4
%define BLK_SIZE		512
%define DVD_BLK_SIZE		2048
%define DVD_BOOT_LOADER_SIZE	DVD_BLK_SIZE
%define BIN_HEADER_SIZE		32
%define SYS_KERNEL_BASE		(BOOT_RAM_BASE+BIN_HEADER_SIZE)
%define SYS_KERNEL_SEG		(SYS_KERNEL_BASE>>4)

%define SYS_START_CR0		0x31
%define RLf_16BIT		0
%define RLf_VGA		1
%define RLf_32BIT		2
%define RLf_PATCHED		3
%define RLf_16MEG_SYS_CODE_BP	4
%define RLf_64BIT		5
%define MEM_E820_ENTRIES_NUM	48
%define MEM_E820_SIZE		20
%define MP_PROCESSORS_NUM	128

%define GDT_BOOT_DS		0x10
%define GDT_BOOT_CS		0x20
%define GDT_CS32		0x30
%define GDT_CS64		0x40
%define GDT_DS			0x60
%define SYS_FIXED_AREA		0x100000
%define SYS_FIXED_BOOT_CPU	(SYS_FIXED_AREA+0x200)
%define SYS_FIXED_ADAM		(SYS_FIXED_AREA+0x300)
%define SYS_FIXED_CODE_BP	(SYS_FIXED_AREA+0x7A8)
%define SYS_FIXED_AREA_SIZE	0x2000
%define SYS_16MEG_AREA_LIMIT	0x1000000

%define MEM_PAG_BITS		9
%define BP_ALLOCED_U8S		8
%define BP_MEM_FREE_LST		24
%define MB_SIGNATURE		16
%define MB_PAGS			20
%define MBS_UNUSED_SIGNATURE_VAL 'MBUn'

%define RLf_16MEG_ADAM_HEAP_CTRL	6
%define SYS_FIXED_ADAM_HC	(SYS_FIXED_AREA+0x1188)
%define MEM_ADAM_STK		(512<<MEM_PAG_BITS)
%define HC_BP			0
%define HC_SIGNATURE		8
%define HC_MEM_TASK		0x28
%define HC_NEXT_UM		0x50
%define HC_LAST_UM		0x58
%define CTask_SIGNATURE		8
%define CTask_CODE_HEAP		0x20
%define CTask_DATA_HEAP		0x28
%define CTask_RSP		0x128

%define CTask_WAKE_JIFFY		0x10
%define CTask_FLAGS		0x18
%define CTask_NEXT_TASK		0x80
%define CTask_STK		0x310
%define CTaskStk_SIZE		8
%define CTaskStk_BASE		24
%define TASKf_KILL_TASK		1
%define TASKf_SUSPENDED		2
%define TASKf_IDLE		3
%define TASKf_AWAITING_MSG	9
%define SEMA_DBG_MODE		4
%define SEMA_SINGLE_USER	10
%define DFT_CACHE_LINE_WIDTH	128
%define CTask_RIP		0x118
%define CTask_RFLAGS		0x120
%define CTask_RSI		0x130
%define CTask_RAX		0x138
%define CTask_RCX		0x140
%define CTask_R15		0x1A0
%define CTask_FPU		0x1B0
%define CTask_SWAP_CNT		0x1B8
%define CTask_FAULT_NUM		0x440
%define CTask_FAULT_ERR_CODE	0x448
%define CTask_LAST_EXCEPT	0x320
%define CTask_CATCH_EXCEPT	0x378
%define CTask_LAST_FUN		0x410
%define CHashFun_EXE_ADDR	152

%define CExcept_HNDLR_CATCH	16
%define CExcept_HNDLR_UNTRY	24
%define CExcept_RSP		32
%define CExcept_RBP		40
%define CExcept_RFLAGS		48
%define CExcept_RSI		56
%define CExcept_RDI		64
%define CExcept_R10		72
%define CExcept_R11		80
%define CExcept_R12		88
%define CExcept_R13		96
%define CExcept_R14		104
%define CExcept_R15		112
%define CExcept_SIZE		120

%define CCPU_FLAGS		0x10
%define CCPU_IDLE_PT_HITS	0x20
%define CCPU_TOTAL_JIFFIES	0x30
%define CCPU_SETH_TASK		0x38
%define CCPU_IDLE_TASK		0x40
%define CCPU_SWAP_CNT		0x50

%define CTask_WIN_LEFT 0x48
%define CTask_WIN_RIGHT 0x50
%define CTask_WIN_TOP 0x58
%define CTask_WIN_BOTTOM 0x60
%define CTask_SCROLL_X 0x100
%define CTask_SCROLL_Y 0x108

; C++ ABI additions: the HolyC reference calls its implementations directly.
; Adapt HolyC stack arguments and callee cleanup to C++.
%macro HC_FRAME_ENTER 0
	PUSH	RBP
	MOV	RBP,RSP
	PUSH	RSI
	PUSH	RDI
	PUSH	R10
	PUSH	R11
%endmacro

%macro HC_FRAME_CALL 1-2 0
	AND	RSP,-16
%if %2>6
	SUB	RSP,((%2-6)*8+15)&~15
%assign %%arg 6
%rep %2-6
	MOV	RAX,[RBP+16+%%arg*8]
	MOV	[RSP+(%%arg-6)*8],RAX
%assign %%arg %%arg+1
%endrep
%endif
	CALL	%1
	LEA	RSP,[RBP-32]
	POP	R11
	POP	R10
	POP	RDI
	POP	RSI
	POP	RBP
%endmacro

; The optional fourth argument widens a SysV U8 or Bool return for HolyC.
%macro HOLYC_CPP 3-4 0
%1:
	HC_FRAME_ENTER
%if %3>=1
	MOV	RDI,[RBP+16]
%endif
%if %3>=2
	MOV	RSI,[RBP+24]
%endif
%if %3>=3
	MOV	RDX,[RBP+32]
%endif
%if %3>=4
	MOV	RCX,[RBP+40]
%endif
%if %3>=5
	MOV	R8,[RBP+48]
%endif
%if %3>=6
	MOV	R9,[RBP+56]
%endif
	HC_FRAME_CALL %2,%3
%if %4
	MOVZX	EAX,AL
%endif
	RET	%3*8
%endmacro

; The caller pops fixed arguments, argc, and the inline argv for variadics.
%macro HOLYC_PRINT 3-4 0
%1:
	HC_FRAME_ENTER
	MOV	RDI,[RBP+16]
%if %3=0
	LEA	RSI,[RBP+24]
%else
	MOV	RSI,[RBP+24]
%if %3=2
	MOV	RDX,[RBP+32]
	LEA	RCX,[RBP+40]
%else
	LEA	RDX,[RBP+32]
%if %4
	MOV	RCX,RBP
%endif
%endif
%endif
	HC_FRAME_CALL %2
	RET
%endmacro
