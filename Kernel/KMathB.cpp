#include "KernelB.hpp"

extern "C" uint64_t CeilU64Cpp(uint64_t num, uint64_t to)
{
  num += to - 1;
  return num - num % to;
}

// HolyC returns F64 as binary64 bits in RAX.
extern "C" uint64_t Pow10I64Cpp(int64_t exponent)
{
  if (exponent > 308)
    return 0x7FF0000000000000;
  if (exponent < -308)
    return 0;
  return pow10_I64[exponent + 309];
}
