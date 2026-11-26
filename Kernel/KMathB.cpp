#include "KernelB.hpp"

extern "C" uint64_t ClampCpp(uint64_t d, uint64_t lo, uint64_t hi)
{
  if (F64CompareBits(d, lo) == -1)
    return lo;
  if (F64CompareBits(d, hi) > 0)
    return hi;
  return d;
}

extern "C" uint64_t MinCpp(uint64_t n1, uint64_t n2)
{
  if (F64CompareBits(n1, n2) <= 0)
    return n1;
  else
    return n2;
}

extern "C" uint64_t MaxCpp(uint64_t n1, uint64_t n2)
{
  if (F64CompareBits(n1, n2) >= 0)
    return n1;
  else
    return n2;
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

extern "C" uint64_t FloorU64Cpp(uint64_t num, uint64_t to)
{
  return num - num % to;
}

extern "C" uint64_t CeilU64Cpp(uint64_t num, uint64_t to)
{
  num += to - 1;
  return num - num % to;
}

extern "C" int64_t RoundI64Cpp(int64_t num, int64_t to)
{
  return static_cast<uint64_t>(num) - num % to;
}

extern "C" int64_t FloorI64Cpp(int64_t num, int64_t to)
{
  if (num >= 0)
    return static_cast<uint64_t>(num) - num % to;
  else {
    num++;
    return static_cast<uint64_t>(num) - num % to - to;
  }
}

extern "C" int64_t CeilI64Cpp(int64_t num, int64_t to)
{
  if (num >= 0) {
    num = static_cast<uint64_t>(num) + to - 1;
    return static_cast<uint64_t>(num) - num % to;
  } else {
    num = static_cast<uint64_t>(num) + to - 1;
    return static_cast<uint64_t>(num) - num % to - to;
  }
}

extern "C" int64_t SignI64Cpp(int64_t i)
{
  return (i > 0) - (i < 0);
}

static uint64_t RandStep()
{
  constexpr uint64_t LIN_CONGRUE_A = 6364136223846793005ULL,
                     LIN_CONGRUE_C = 1442695040888963407ULL;
  auto *task = CurrentTask();
  uint64_t seed = task->rand_seed;
  uint64_t res = (LIN_CONGRUE_A * seed ^ ((seed & 0xFFFFFFFF0000ULL) >> 16)) +
                 LIN_CONGRUE_C;
  if (!(task->task_flags & TASKF_NONTIMER_RAND))
    res ^= GetTSC();
  task->rand_seed = res;
  return res;
}

// The adapter returns each narrow HolyC value extended through RAX.
extern "C" int64_t RandI16Cpp()
{
  return static_cast<int16_t>(RandStep());
}

extern "C" uint64_t RandU16Cpp()
{
  return static_cast<uint16_t>(RandStep());
}

extern "C" int64_t RandI32Cpp()
{
  return static_cast<int32_t>(RandStep());
}

extern "C" uint64_t RandU32Cpp()
{
  return static_cast<uint32_t>(RandStep());
}
extern "C" int64_t RandI64Cpp()
{
  return RandStep();
}
extern "C" uint64_t RandU64Cpp()
{
  return RandStep();
}

extern "C" uint64_t RandCpp()
{
  return F64DivBits(F64FromI64(RandStep() & 0x3FFFFFFFFFFFFFFFULL),
                    F64FromI64(0x4000000000000000LL));
}

extern "C" int64_t SeedCpp(int64_t seed, CTask *task)
{
  if (!task)
    task = CurrentTask();
  if (seed) {
    __atomic_fetch_or(&task->task_flags, TASKF_NONTIMER_RAND, __ATOMIC_RELAXED);
    task->rand_seed = seed;
  } else {
    __atomic_fetch_and(&task->task_flags, ~TASKF_NONTIMER_RAND,
                       __ATOMIC_RELAXED);
    task->rand_seed ^= GetTSC();
  }
  return task->rand_seed;
}
