#include "KernelB.hpp"

constexpr unsigned FONT_HEIGHT = 16;

void VGAFlush()
{
  __atomic_fetch_or(&sys_semas[SEMA_FLUSH_VGA_IMAGE].flags, 1,
                    __ATOMIC_RELAXED);
}

void BootPutS(const char *text)
{
  if (SYS_RUN_LEVEL & RLF_VGA) {
    if (!SYS_FONT_ADDRESS)
      return;

    auto *video =
        reinterpret_cast<volatile uint8_t *>(dev.uncached_alias + 0xA0000);
    const auto *font = reinterpret_cast<const uint8_t *>(SYS_FONT_ADDRESS);
    constexpr unsigned row_bytes = GR_WIDTH / 8;

    OutU8(0x3C4, 2);
    OutU8(0x3C5, 0x0F);
    OutU8(0x3CE, 1);
    OutU8(0x3CF, 0);
    OutU8(0x3CE, 3);
    OutU8(0x3CF, 0);
    OutU8(0x3CE, 8);
    OutU8(0x3CF, 0xFF);
    for (unsigned i = 0; i < row_bytes * GR_HEIGHT; i++)
      video[i] = 0;

    for (unsigned col = 0; col < row_bytes && text[col]; col++) {
      const auto *glyph = font + static_cast<uint8_t>(text[col]) * FONT_HEIGHT;
      for (unsigned row = 0; row < FONT_HEIGHT; row++)
        video[row * row_bytes + col] = glyph[row];
    }
  } else {
    auto *video =
        reinterpret_cast<volatile uint16_t *>(dev.uncached_alias + 0xB8000);
    for (unsigned col = 0; col < 80 && text[col]; col++)
      video[col] = 0x0700 | static_cast<uint8_t>(text[col]);
  }
}
