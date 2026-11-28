#include "KernelB.hpp"

struct CPCIBIOSRegs
{
  uint32_t eax, ebx, ecx, edx, esi, edi, eflags;
};

static_assert(offsetof(CPCIBIOSRegs, eflags) == 24);

extern "C" {
CPCIBIOSRegs c32;
bool FarCall32(uint32_t entry);
}

static uint64_t far_call32_lock;

static uint32_t PCIRead(uint16_t fn, uint8_t bus, uint8_t device,
                        uint8_t function, uint8_t reg)
{
  CMemLock lock(far_call32_lock);
  c32.eax = fn;
  c32.ebx = (uint32_t{bus} << 8) | (uint32_t{device} << 3) | function;
  c32.edi = reg;
  return FarCall32(SYS_PCI_SERVICES) ? c32.ecx : UINT32_MAX;
}

uint8_t PCIReadU8(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg)
{
  return PCIRead(0xB108, bus, device, function, reg);
}

uint16_t PCIReadU16(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg)
{
  return PCIRead(0xB109, bus, device, function, reg);
}

uint32_t PCIReadU32(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg)
{
  return PCIRead(0xB10A, bus, device, function, reg);
}

int32_t PCIClassFind(uint32_t class_code, uint16_t index)
{
  CMemLock lock(far_call32_lock);
  c32.eax = 0xB103;
  c32.ecx = class_code;
  c32.esi = index;
  if (!FarCall32(SYS_PCI_SERVICES) || (c32.eax & 0xFF00))
    return -1;
  return ((c32.ebx & 0xFF00) << 8) | ((c32.ebx & 0xF8) << 5) | (c32.ebx & 7);
}
