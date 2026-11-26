#include "../Linux/BootProbe.hpp"
#include "KernelB.hpp"

static bool SysGlblsInit()
{
  uint32_t eax = 1, ebx, ecx = 0, edx;
  asm volatile("cpuid" : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx));
  sys_cache_line_width = ((ebx >> 8) & 255) * 8;
  sys_focus_task = adam_task;
  DbgModeCpp(true);
  QueInit(sys_macro_head.next, sys_macro_head.last);
  scrncast.snd_head.next = scrncast.snd_head.last = &scrncast.snd_head;

  auto *hc = adam_task->data_heap;
  rev_bits_table = MAlloc(256, hc);
  set_bits_table = MAlloc(256, hc);
  if (!rev_bits_table || !set_bits_table)
    return false;
  for (unsigned i = 0; i < 256; i++) {
    unsigned rev = 0, count = 0;
    for (unsigned j = 0; j < 8; j++) {
      rev |= ((i >> (7 - j)) & 1) << j;
      count += (i >> j) & 1;
    }
    rev_bits_table[i] = rev;
    set_bits_table[i] = count;
  }

  constexpr unsigned EXT_EXTS_NUM = 5;
  ext = reinterpret_cast<CExt *>(MAlloc(EXT_EXTS_NUM * sizeof(*ext), hc));
  if (!ext)
    return false;
  MemSet(ext, 0, EXT_EXTS_NUM * sizeof(*ext));

  constexpr int limit = 308, bias = limit + 1;
  pow10_I64 = reinterpret_cast<uint64_t *>(
      MAlloc((2 * limit + 2) * sizeof(*pow10_I64), hc));
  if (!pow10_I64)
    return false;
  pow10_I64[0] = 0;
  for (int i = -limit; i <= limit; i++)
    pow10_I64[i + bias] = Pow10Bits(i);

  QueInit(dev.pci_head.next, dev.pci_head.last);
  dev.mem64_ptr = MEM_MAPPED_SPACE;
  return true;
}

static bool SysGrInit()
{
  text.font=sys_font_std;
  text.aux_font=sys_font_cyrillic;
  text.vga_alias=reinterpret_cast<uint8_t *>(dev.uncached_alias+0xA0000);
  text.vga_text_alias=reinterpret_cast<uint8_t *>(dev.uncached_alias+0xB8000);
  text.cols=TEXT_COLS;
  if (!(SYS_RUN_LEVEL & RLF_VGA)) {
    text.rows=25;
    MemSet(text.vga_text_alias,0,text.rows*text.cols*2);
    const uint8_t border[]{0xC4,0xCD,0xB3,0xBA,0xDA,0xC9,0xBF,0xBB,
                           0xC0,0xC8,0xD9,0xBC};
    MemCpy(text.border_chars+2,border,sizeof(border));
  } else {
    text.rows=TEXT_ROWS;
    OutU8(0x3C4,2);
    OutU8(0x3C5,0x0F);
    MemSet(text.vga_alias,0,GR_WIDTH*GR_HEIGHT/8);
    text.raw_scrn_image=CAlloc(GR_WIDTH*GR_HEIGHT/8,adam_task->data_heap);
    if (!text.raw_scrn_image)
      return false;
    for (unsigned i=2;i<14;i++)
      text.border_chars[i]=i;
  }
  return true;
}

static void TimersInit()
{
  OutU8(0x43, 0x34);
  OutU8(0x40, SYS_TIMER0_PERIOD & 255);
  OutU8(0x40, SYS_TIMER0_PERIOD >> 8);

  if (PCIReadU16(0, 31, 0, 0) == 0x8086) {
    uintptr_t rcba = PCIReadU32(0, 31, 0, 0xF0) & ~0x3FFFU;
    auto *hpet_config = reinterpret_cast<volatile uint32_t *>(rcba + 0x3404);
    *hpet_config = (*hpet_config & 3) | 0x80;
  }

  uintptr_t alias = dev.uncached_alias;
  uint64_t capabilities =
      *reinterpret_cast<volatile uint64_t *>(alias + HPET_GCAP_ID);
  uint64_t period = capabilities >> 32;
  if (period > 100000 && period < 1000000000) {
    cnts.HPET_freq = 1000000000000000ULL / period;
    cnts.HPET_kHz_freq = 1000000000000ULL / period;
    auto *config = reinterpret_cast<volatile uint64_t *>(alias + HPET_GEN_CONF);
    *config = *config | 1;
    cnts.HPET_initial = HPET();
  } else {
    cnts.HPET_freq = 0;
    cnts.HPET_kHz_freq = 0;
    cnts.HPET_initial = 0;
  }
}

extern "C" void KMainCpp()
{
  OutU8(0x61, InU8(0x61) & ~3);
  asm volatile("movq %%fs:0,%0" : "=r"(adam_task));
  if (!BlkPoolsInit(adam_task)) {
    SerialPutS("UnholyOS: Could not initialize memory pools\r\n");
    return;
  }
  if (!SysGlblsInit()) {
    SerialPutS("UnholyOS: Could not initialize kernel globals\r\n");
    return;
  }
  if (!Mem32DevInit(adam_task->data_heap)) {
    SerialPutS("UnholyOS: Could not initialize device memory ranges\r\n");
    return;
  }
  UncachedAliasAlloc();
  if (!SysGrInit()) {
    SerialPutS("UnholyOS: Could not initialize text globals\r\n");
    return;
  }
  if (!IntInit1()) {
    SerialPutS("UnholyOS: Could not initialize the interrupt table\r\n");
    return;
  }
  TimersInit();
  IntsInit();
  if (!IntInit2()) {
    SerialPutS("UnholyOS: Could not initialize fault handlers\r\n");
    return;
  }
  SYS_RUN_LEVEL |= RLF_INTERRUPTS;
  asm volatile("sti" : : : "memory");
  TimeCal();
  if (!LoadKernel(*adam_task)) {
    SerialPutS("UnholyOS: Could not register kernel symbols\r\n");
    return;
  }
  if (!BlkDevGlblsInit()) {
    SerialPutS("UnholyOS: Could not allocate drive tables\r\n");
    return;
  }
  adam_task->cur_dv = BlkDevsInitAll();
  if (!Core0TaskInit()) {
    SerialPutS("UnholyOS: Could not initialize the idle task\r\n");
    return;
  }
  MsInit();
  if (!KbdInit())
    SerialPutS("UnholyOS: Could not initialize the PS/2 keyboard\r\n");
  if (MsHardDrvrInstall())
    KbdMsService();
  else
    SerialPutS("UnholyOS: Could not initialize the PS/2 mouse\r\n");
  if (!DefineInitColors() || !GrPaletteInit()) {
    SerialPutS("UnholyOS: Could not register color names\r\n");
    return;
  }
  if (!DefineInitFileFlags()) {
    SerialPutS("UnholyOS: Could not register file utility flags\r\n");
    return;
  }
  if (!LoadDocDefines()) {
    SerialPutS("UnholyOS: Could not register document definitions\r\n");
    return;
  }
  if (!GrInit2()) {
    SerialPutS("UnholyOS: Could not allocate graphics device contexts\r\n");
    return;
  }
  BootPutS("UnholyOS kernel entered in 64-bit mode");
  if (!WinMgrInit()) {
    SerialPutS("UnholyOS: Could not start the window manager\r\n");
    return;
  }

  BootProbe();
  adam_task->win_inhibit |= WIF_SELF_FOCUS;
  SYS_RUN_LEVEL |= RLF_DOC;
  adam_task->display_flags |= DISPLAYF_CHILDREN_NOT_ON_TOP;
  DbgModeCpp(false);
  if (!StartUpTasks())
    SerialPutS("UnholyOS: Could not start the user task\r\n");
  SerialPutS("UnholyOS: kernel entered in 64-bit mode\r\n");
}
