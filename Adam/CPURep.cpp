#include "../Kernel/KernelB.hpp"

extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);
extern "C" void MPCPURepEntry();

struct __attribute__((packed)) CCPURep
{
  bool mp_start, mp_end;
  uint64_t mp_not_done_flags;
  int64_t *swaps, *cycles;
};

static_assert(sizeof(CCPURep) == 26 &&
              offsetof(CCPURep, mp_not_done_flags) == 2 &&
              offsetof(CCPURep, swaps) == 10 &&
              offsetof(CCPURep, cycles) == 18);

extern "C" void MPCPURep(CCPURep *cr)
{
  int64_t swaps = 0;
  while (!__atomic_load_n(&cr->mp_start, __ATOMIC_ACQUIRE))
    _YIELD();
  auto cycles_0 = GetTSC();
  while (!__atomic_load_n(&cr->mp_end, __ATOMIC_ACQUIRE)) {
    swaps++;
    _YIELD();
  }
  auto cycles_f = GetTSC();
  uint64_t cpu = CurrentCPU()->num;
  cr->swaps[cpu] = swaps;
  cr->cycles[cpu] = cycles_f - cycles_0;
  asm volatile("lock btrq %1,%0"
               : "+m"(cr->mp_not_done_flags)
               : "r"(cpu)
               : "cc", "memory");
}

template <class... T> static void RepPrint(const char *fmt, T... args)
{
  uint64_t values[]{uint64_t(args)...};
  PrintCpp(fmt, sizeof...(args), values);
}

extern "C" void CPURep(uint8_t full)
{
  int64_t count = mp_cnt;
  if (!full) {
    RepPrint("%03X Cores %6h?nHz\n", count, F64FromI64(cnts.time_stamp_freq));
    return;
  }
  if (count < 1 || count > 64)
    return;

  // Keep the packed completion word within a single cache line.
  alignas(16) CCPURep cr{};
  auto *task = CurrentTask();
  cr.swaps = reinterpret_cast<int64_t *>(
      CAlloc(sizeof(int64_t) * count, task->data_heap));
  cr.cycles = reinterpret_cast<int64_t *>(
      CAlloc(sizeof(int64_t) * count, task->data_heap));
  if (!cr.swaps || !cr.cycles) {
    Free(cr.swaps);
    Free(cr.cycles);
    return;
  }

  int64_t i = 0;
  for (; i < count; i++) {
    if (!SpawnCpp(reinterpret_cast<uintptr_t>(MPCPURepEntry),
                  reinterpret_cast<uint8_t *>(&cr), nullptr, i, task, 0,
                  JOBF_ADD_TO_QUE))
      break;
    cr.mp_not_done_flags |= uint64_t{1} << i;
  }

  auto t0 = tSCpp();
  bool complete = i == count;
  if (!complete)
    __atomic_store_n(&cr.mp_end, true, __ATOMIC_RELEASE);
  __atomic_store_n(&cr.mp_start, true, __ATOMIC_RELEASE);
  if (complete) {
    SleepCpp(2000);
    __atomic_store_n(&cr.mp_end, true, __ATOMIC_RELEASE);
  }
  for (;;) {
    uint64_t pending;
    asm volatile("movq %1,%0"
                 : "=r"(pending)
                 : "m"(cr.mp_not_done_flags)
                 : "memory");
    if (!pending)
      break;
    _YIELD();
  }
  auto tf = tSCpp();

  if (complete) {
    uint64_t elapsed = F64AddBits(tf, t0 ^ (uint64_t{1} << 63));
    auto per_second = [elapsed](int64_t value) {
      return F64DivBits(F64FromI64(value), elapsed);
    };
    auto to_i64 = [](uint64_t value) { return F64ToI64(value, 0x0C00); };
    RepPrint("\n%03X Cores %6h?nHz\n", count, F64FromI64(cnts.time_stamp_freq));
    PutS("\n         Context\n"
         "CPU     Swaps/s         Cycles\n"
         "--- ------------- -----------------\n");
    int64_t total_swaps = 0, total_cycles = 0;
    for (i = 0; i < count; i++) {
      RepPrint(" %02X %13,d %17,d\n", i, to_i64(per_second(cr.swaps[i])),
               to_i64(per_second(cr.cycles[i])));
      total_swaps += cr.swaps[i];
      total_cycles += cr.cycles[i];
    }
    PutS("--- ------------- -----------------\n");
    auto swaps = per_second(total_swaps), cycles = per_second(total_cycles);
    auto cores = F64FromI64(i);
    RepPrint("%03X %13,d %17,d\n", i, to_i64(swaps), to_i64(cycles));
    RepPrint("Avg %13,d %17,d\n\n", to_i64(F64DivBits(swaps, cores)),
             to_i64(F64DivBits(cycles, cores)));
    auto raw_swaps = F64FromI64(total_swaps);
    RepPrint("Avg Cycles/Swap: %12.6f\n",
             F64DivBits(F64FromI64(total_cycles), raw_swaps));
    RepPrint("Avg Time/Swap  : %12.6fns\n\n",
             F64DivBits(
                 F64MulBits(F64MulBits(elapsed, F64FromI64(1000000000)), cores),
                 raw_swaps));
  }
  Free(cr.swaps);
  Free(cr.cycles);
}
