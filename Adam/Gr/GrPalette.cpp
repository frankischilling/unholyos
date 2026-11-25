#include "../../Kernel/KernelB.hpp"

uint8_t gr_rainbow_10[10]{BLACK, BROWN, RED,    LTRED,  YELLOW,
                          GREEN, BLUE,  PURPLE, LTGRAY, WHITE};

bool GrPaletteInit()
{
  return DefineLstLoadCpp(
      "ST_RAINBOW_10",
      "BLACK\0BROWN\0RED\0LTRED\0YELLOW\0GREEN\0BLUE\0PURPLE\0LTGRAY\0WHITE\0",
      nullptr);
}

static uint64_t GrPaletteLock()
{
  auto flags = GetRFlags();
  asm volatile("cli" : : : "memory");
  while (__atomic_fetch_or(&sys_semas[SEMA_VGA].flags, 1, __ATOMIC_ACQUIRE) & 1)
    _YIELD();
  return flags;
}

static void GrPaletteUnlock(uint64_t flags)
{
  __atomic_fetch_and(&sys_semas[SEMA_VGA].flags, ~uint64_t{1},
                     __ATOMIC_RELEASE);
  SetRFlags(flags);
}

void GrPaletteIndicesSet()
{
  if (!(SYS_RUN_LEVEL & RLF_VGA))
    return;
  auto flags = GrPaletteLock();
  InU8(VGAP_INPUT_STAT);
  for (unsigned i = 0; i < COLORS_NUM; i++) {
    OutU8(VGAP_ATTR_INDEX, i);
    OutU8(VGAP_ATTR_DATA_WRITE, i);
  }
  OutU8(VGAP_ATTR_INDEX, 0x20);
  OutU8(VGAP_ATTR_DATA_WRITE, 0);
  InU8(VGAP_INPUT_STAT);
  GrPaletteUnlock(flags);
}

void GrPaletteColorSet(int64_t color_num, CBGR48 bgr48)
{
  if (!(SYS_RUN_LEVEL & RLF_VGA))
    return;
  auto flags = GrPaletteLock();
  OutU8(VGAP_PALETTE_MASK, 0xFF);
  OutU8(VGAP_REG_WRITE, color_num);
  OutU8(VGAP_PALETTE_DATA, bgr48.r >> 10);
  OutU8(VGAP_PALETTE_DATA, bgr48.g >> 10);
  OutU8(VGAP_PALETTE_DATA, bgr48.b >> 10);
  GrPaletteUnlock(flags);
}

CBGR48 GrPaletteColorGet(int64_t color_num)
{
  CBGR48 res{};
  if (!(SYS_RUN_LEVEL & RLF_VGA))
    return res;
  auto flags = GrPaletteLock();
  OutU8(VGAP_PALETTE_MASK, 0xFF);
  OutU8(VGAP_REG_READ, color_num);
  res.r = (0xFFFFULL << 10) * InU8(VGAP_PALETTE_DATA) / 0xFC00;
  res.g = (0xFFFFULL << 10) * InU8(VGAP_PALETTE_DATA) / 0xFC00;
  res.b = (0xFFFFULL << 10) * InU8(VGAP_PALETTE_DATA) / 0xFC00;
  GrPaletteUnlock(flags);
  return res;
}

CBGR48 gr_palette_std[COLORS_NUM]{
0x000000000000,0x00000000AAAA,0x0000AAAA0000,0x0000AAAAAAAA,
0xAAAA00000000,0xAAAA0000AAAA,0xAAAA55550000,0xAAAAAAAAAAAA,
0x555555555555,0x55555555FFFF,0x5555FFFF5555,0x5555FFFFFFFF,
0xFFFF55555555,0xFFFF5555FFFF,0xFFFFFFFF5555,0xFFFFFFFFFFFF};

CBGR48 gr_palette_gray[COLORS_NUM]{
0x000000000000,0x111111111111,0x222222222222,0x333333333333,
0x444444444444,0x555555555555,0x666666666666,0x777777777777,
0x888888888888,0x999999999999,0xAAAAAAAAAAAA,0xBBBBBBBBBBBB,
0xCCCCCCCCCCCC,0xDDDDDDDDDDDD,0xEEEEEEEEEEEE,0xFFFFFFFFFFFF};

extern "C" void GrPaletteGetCpp(CBGR48 *bgr48)
{
  for (unsigned i = 0; i < COLORS_NUM; i++)
    bgr48[i] = GrPaletteColorGet(i);
}

extern "C" void GrPaletteSetCpp(const CBGR48 *bgr48)
{
  for (unsigned i = 0; i < COLORS_NUM; i++)
    GrPaletteColorSet(i, bgr48[i]);
}

extern "C" void PaletteSetStdCpp()
{
  GrPaletteSetCpp(gr_palette_std);
}

extern "C" uint64_t GrPaletteColorGetCpp(int64_t color_num)
{
  return __builtin_bit_cast(uint64_t, GrPaletteColorGet(color_num));
}

extern "C" void GrPaletteColorSetCpp(int64_t color_num, uint64_t bgr48)
{
  GrPaletteColorSet(color_num, CBGR48{bgr48});
}
