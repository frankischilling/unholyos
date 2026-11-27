#include <cstdint>

extern "C" {
void PutS(const char *text);
int64_t StrCmpCpp(const uint8_t *left, const uint8_t *right);
void NativeImportPublicPutS();
void NativeImportPutHoly(const char *text);

void (*volatile NativeImportPut)(const char *) = PutS;
int64_t (*volatile NativeImportCompare)(const uint8_t *,
                                        const uint8_t *) = StrCmpCpp;
volatile uint64_t NativeImportResult;
volatile uintptr_t NativeImportBindings[3];

void NativeImportRun()
{
  uint64_t checks = 0;
  bool zero = NativeImportResult == 0;
  for (unsigned i = 0; i < 3; i++)
    zero = zero && NativeImportBindings[i] == 0;
  if (zero)
    checks |= 1;
  const auto *alpha = reinterpret_cast<const uint8_t *>("alpha");
  const auto *beta = reinterpret_cast<const uint8_t *>("beta");
  const uint8_t high[]{0x80, 0}, low[]{0x7F, 0};
  PutS("NI native direct\n");
  if (StrCmpCpp(alpha, beta) == -1 && StrCmpCpp(alpha, alpha) == 0 &&
      StrCmpCpp(high, low) == 1)
    checks |= 2;
  NativeImportPut("NI native indirect\n");
  if (NativeImportCompare(beta, alpha) == 1 &&
      NativeImportCompare(alpha, alpha) == 0 &&
      NativeImportCompare(low, high) == -1)
    checks |= 4;
  NativeImportPutHoly("NI HolyC public\n");
  if (reinterpret_cast<uintptr_t>(NativeImportPut) !=
      reinterpret_cast<uintptr_t>(NativeImportPublicPutS))
    checks |= 8;
  if (NativeImportPut == PutS && NativeImportCompare == StrCmpCpp)
    checks |= 16;
  NativeImportBindings[0] = reinterpret_cast<uintptr_t>(NativeImportPut);
  NativeImportBindings[1] = reinterpret_cast<uintptr_t>(NativeImportCompare);
  NativeImportBindings[2] = reinterpret_cast<uintptr_t>(NativeImportPublicPutS);
  NativeImportResult = checks;
}
}
