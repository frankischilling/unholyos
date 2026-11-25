ENTRY(SYS_KERNEL)

PHDRS
{
  text PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
}

SECTIONS
{
  . = 0x7C00;
  .boot16 : { KEEP(*(.boot16)) } :text
  .boot32 : { KEEP(*(.boot32)) }
  .boot64 : { KEEP(*(.boot64)) }
  .text : { *(.text .text.*) }
  .rodata : { *(.rodata .rodata.*) }
  .data : { *(.data .data.*) } :data
  .patch_table : {
    BYTE(0)
    . = ALIGN(16);
  } :data
  __core0_32bit_offset = CORE0_32BIT_INIT - SYS_KERNEL;
  __sys_run_level_offset = SYS_RUN_LEVEL - SYS_KERNEL;
  __patch_table_offset = ADDR(.patch_table) - ADDR(.boot16);
  __kernel_file_size = . - ADDR(.boot16);

  .bss (NOLOAD) : ALIGN(16) {
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    __bss_end = .;
  } :data

  /DISCARD/ : { *(.comment .eh_frame .note*) }
}

ASSERT(SYS_KERNEL == 0x7C20, "Kernel boot header address differs from the DVD loader")
ASSERT(SYS_COMPILE_TIME + 8 - ADDR(.boot16) <= 512, "DVD probe timestamp does not fit in the first kernel block")
ASSERT(ADDR(.boot16) + SIZEOF(.boot16) <= 0x17C20, "Real-mode kernel exceeds its segment")
ASSERT(__bss_end <= 0x96600, "Kernel overlaps the relocated DVD loader")
