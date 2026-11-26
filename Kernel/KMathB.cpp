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
