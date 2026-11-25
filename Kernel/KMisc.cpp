#include "KernelB.hpp"

extern "C" uint64_t RawCpp(uint8_t enabled)
{
  if (enabled)
    return !(__atomic_fetch_and(&CurrentTask()->display_flags,
                                ~DISPLAYF_NOT_RAW, __ATOMIC_RELAXED) &
             DISPLAYF_NOT_RAW);
  VGAFlush();
  return !(__atomic_fetch_or(&CurrentTask()->display_flags, DISPLAYF_NOT_RAW,
                             __ATOMIC_RELAXED) &
           DISPLAYF_NOT_RAW);
}

extern "C" uint64_t IsRawCpp()
{
  // HolyC tests the full return register for Bool calls.
  return !(CurrentTask()->display_flags & DISPLAYF_NOT_RAW);
}

extern "C" uint64_t DbgModeCpp(uint8_t enabled)
{
  auto &flags = sys_semas[SEMA_DBG_MODE].flags;
  return (enabled
              ? __atomic_fetch_or(&flags, 1, __ATOMIC_RELAXED)
              : __atomic_fetch_and(&flags, ~uint64_t{1}, __ATOMIC_RELAXED)) &
         1;
}

extern "C" uint64_t IsDbgModeCpp()
{
  return __atomic_load_n(&sys_semas[SEMA_DBG_MODE].flags, __ATOMIC_RELAXED) & 1;
}

extern "C" void IntCore0TimerHndlr()
{
  __atomic_add_fetch(&cnts.jiffies, uint64_t{1}, __ATOMIC_RELAXED);
  cnts.timer += SYS_TIMER0_PERIOD + 1;
  OutU8(0x20, 0x20);
}

uint64_t SysTimerRead()
{
  uint64_t flags;
  asm volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
  OutU8(0x43, 0);
  uint16_t lo = InU8(0x40);
  int64_t count = lo | (uint16_t{InU8(0x40)} << 8);
  if (count == SYS_TIMER0_PERIOD && (InU8(0x20) & 1))
    count = -1;
  uint64_t result = cnts.timer + SYS_TIMER0_PERIOD - count;
  asm volatile("pushq %0; popfq" : : "r"(flags) : "cc", "memory");
  return result;
}

uint64_t HPET()
{
  return *reinterpret_cast<volatile uint64_t *>(dev.uncached_alias +
                                                HPET_MAIN_CNT);
}

static uint64_t GetTSC()
{
  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi) : : "memory");
  return (uint64_t{hi} << 32) | lo;
}

int64_t TimeCal()
{
  static uint64_t time_stamp_start, timer_start, HPET_start;
  uint64_t flags;
  if (time_stamp_start) {
    asm volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    int64_t elapsed;
    if (HPET_start) {
      cnts.time_stamp_freq = cnts.HPET_freq * (GetTSC() - time_stamp_start);
      elapsed = HPET() - HPET_start;
    } else {
      cnts.time_stamp_freq = SYS_TIMER_FREQ * (GetTSC() - time_stamp_start);
      elapsed = SysTimerRead() - timer_start;
    }
    if (elapsed) {
      cnts.time_stamp_freq /= elapsed;
      cnts.time_stamp_kHz_freq = cnts.time_stamp_freq / 1000;
      cnts.time_stamp_calibrated = true;
    } else {
      SerialPutS("Timer Cal Error\r\n");
    }
    asm volatile("pushq %0; popfq" : : "r"(flags) : "cc", "memory");
  }

  asm volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
  if (cnts.HPET_freq) {
    timer_start = 0;
    HPET_start = HPET();
  } else {
    timer_start = SysTimerRead();
    HPET_start = 0;
  }
  time_stamp_start = GetTSC();
  asm volatile("pushq %0; popfq" : : "r"(flags) : "cc", "memory");
  return cnts.time_stamp_freq;
}
