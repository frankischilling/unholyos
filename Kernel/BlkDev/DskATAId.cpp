#include "../KernelB.hpp"

static bool BootDVDProbe(const CBlkDev &bd)
{
  if (ATAProbe(bd) != BDT_ATAPI || !ATAPIStartStop(bd, true))
    return false;

  uint8_t sector[DVD_BLK_SIZE];
  size_t offset = (SYS_BOOT_SRC >> 16) * BLK_SIZE;
  if (offset >= DVD_BLK_SIZE)
    return false;
  uintptr_t image = reinterpret_cast<uintptr_t>(SYS_KERNEL) - BIN_HEADER_SIZE;
  size_t date_offset = reinterpret_cast<uintptr_t>(SYS_COMPILE_TIME) - image;
  uint64_t expected;
  MemCpy(&expected, SYS_COMPILE_TIME, sizeof(expected));
  for (unsigned retry = 0; retry < 2; retry++) {
    if (!ATAPIReadBlks2(bd, sector, SYS_BOOT_BLK, 1))
      continue;
    uint32_t signature;
    uint64_t compile_time;
    MemCpy(&signature, sector + offset + 4, sizeof(signature));
    MemCpy(&compile_time, sector + offset + date_offset, sizeof(compile_time));
    if (signature == BIN_SIGNATURE_VAL && compile_time == expected)
      return true;
  }
  return false;
}

static bool BootDVDProbeChannel(CBlkDev &bd, uint16_t base0, uint16_t base1)
{
  bd.base0 = base0;
  bd.base1 = base1;
  for (bd.unit = 0; bd.unit < 2; bd.unit++)
    if (BootDVDProbe(bd))
      return true;
  return false;
}

bool BootDVDProbeAll(CBlkDev &bd)
{
  for (unsigned subclass = 0; subclass < 256; subclass++) {
    for (unsigned i = 0; i <= UINT16_MAX; i++) {
      int32_t pci = PCIClassFind(0x010100 | subclass, i);
      if (pci < 0)
        break;
      uint8_t bus = pci >> 16, device = pci >> 8, function = pci;
      constexpr uint8_t bars[]{0x10, 0x18};
      for (uint8_t reg : bars) {
        uint32_t cmd = PCIReadU32(bus, device, function, reg);
        uint32_t ctrl = PCIReadU32(bus, device, function, reg + 4);
        if ((cmd & 1) && (ctrl & 1) && (cmd & ~7U) && cmd <= UINT16_MAX &&
            ctrl <= UINT16_MAX &&
            BootDVDProbeChannel(bd, cmd & ~7U, ctrl & ~3U))
          return true;
      }
    }
  }
  return BootDVDProbeChannel(bd, 0x1F0, 0x3F4) ||
         BootDVDProbeChannel(bd, 0x170, 0x374);
}
