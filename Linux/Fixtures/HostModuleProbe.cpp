#include <cstdint>

extern "C" {
extern uint32_t char_bmp_alpha[16];
void _STRCMP();
int64_t HostModuleCompareDirect(const char *left, const char *right);
int64_t HostModuleCompareIndirect(void (*function)(), const char *left,
                                  const char *right);
uintptr_t HostModuleStackAlign();

alignas(64) volatile uint64_t HostModuleZero[8];
alignas(64) volatile uint64_t HostModuleValues[4]{
    0x0123456789ABCDEF, 0xFEDCBA9876543210, 0x13579BDF2468ACE0, 0};
volatile uint64_t *volatile HostModuleOffset = HostModuleValues + 2;
const uint32_t *volatile HostModuleExternal = char_bmp_alpha;
void (*volatile HostModuleImport)() = _STRCMP;
volatile uint64_t HostModuleResult;
volatile uintptr_t HostModuleAnchor[5];

int64_t HostModuleLocal(int64_t value)
{
  return value * 3 + 7;
}

int64_t (*volatile HostModuleFunction)(int64_t) = HostModuleLocal;

void HostModuleRun()
{
  uint64_t checks = 0;
  bool zero = HostModuleResult == 0;
  for (unsigned i = 0; i < 8; i++)
    zero = zero && HostModuleZero[i] == 0;
  for (unsigned i = 0; i < 5; i++)
    zero = zero && HostModuleAnchor[i] == 0;
  if (zero)
    checks |= 1;
  if (HostModuleValues[0] == 0x0123456789ABCDEF &&
      HostModuleValues[1] == 0xFEDCBA9876543210 &&
      HostModuleValues[2] == 0x13579BDF2468ACE0 && HostModuleValues[3] == 0)
    checks |= 2;
  if (HostModuleOffset == HostModuleValues + 2 &&
      *HostModuleOffset == 0x13579BDF2468ACE0)
    checks |= 4;
  if (HostModuleFunction == HostModuleLocal && HostModuleFunction(37) == 118)
    checks |= 8;
  if (char_bmp_alpha[2] == 0x87FFFFFF && char_bmp_alpha[7] == 0xFFFFFFFF &&
      char_bmp_alpha[8] == 0)
    checks |= 16;
  if (HostModuleExternal == char_bmp_alpha &&
      HostModuleExternal[3] == 0x07FFFFFE)
    checks |= 32;
  if (HostModuleCompareDirect("alpha", "alphb") == -1 &&
      HostModuleCompareDirect("same", "same") == 0)
    checks |= 64;
  if (HostModuleCompareIndirect(HostModuleImport, "beta", "alpha") == 1)
    checks |= 128;
  volatile uintptr_t zero_address = reinterpret_cast<uintptr_t>(HostModuleZero);
  volatile uintptr_t values_address =
      reinterpret_cast<uintptr_t>(HostModuleValues);
  if (!(zero_address & 63) && !(values_address & 63) &&
      HostModuleStackAlign() == 8)
    checks |= 256;

  HostModuleAnchor[0] = reinterpret_cast<uintptr_t>(HostModuleAnchor);
  HostModuleAnchor[1] = reinterpret_cast<uintptr_t>(HostModuleValues);
  HostModuleAnchor[2] = reinterpret_cast<uintptr_t>(HostModuleOffset);
  HostModuleAnchor[3] = reinterpret_cast<uintptr_t>(HostModuleFunction);
  HostModuleAnchor[4] = reinterpret_cast<uintptr_t>(HostModuleExternal);
  HostModuleZero[7] = 0x1020304050607080;
  HostModuleResult = checks;
}
}
