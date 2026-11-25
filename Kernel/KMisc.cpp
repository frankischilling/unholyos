#include "KernelB.hpp"

#include <bit>

extern "C" uint64_t F64MulBits(uint64_t a, uint64_t b);
extern "C" uint64_t F64DivBits(uint64_t a, uint64_t b);
extern "C" uint64_t F64Log2Bits(uint64_t value);
extern "C" uint64_t F64PowerBits(uint64_t base, uint64_t exponent);
extern "C" uint64_t F64FromI64(int64_t value);
extern "C" int64_t F64ToI64(uint64_t value, uint16_t rounding);

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


extern "C" uint64_t tSCpp()
{
  auto freq = cnts.HPET_freq;
  auto ticks = freq ? HPET() - cnts.HPET_initial : SysTimerRead();
  return F64DivBits(F64FromI64(ticks),
                    F64FromI64(freq ? freq : SYS_TIMER_FREQ));
}

extern "C" void BusyCpp(int64_t microseconds)
{
  if (microseconds <= 0)
    return;
  if (auto freq = cnts.HPET_freq) {
    auto start = HPET();
    auto ticks = freq * uint64_t(microseconds) / 1000000;
    while (HPET() - start < ticks)
      asm volatile("pause");
  } else
    for (int64_t i = 0; i < microseconds; i++)
      InU8(0x21);
}

extern "C" void SleepUntilCpp(int64_t wake_jiffy)
{
  auto &task = *CurrentTask();
  auto old = __atomic_fetch_or(&task.task_flags, TASKF_IDLE, __ATOMIC_RELAXED);
  task.wake_jiffy = wake_jiffy;

  _YIELD();
  if (!(old & TASKF_IDLE))
    __atomic_fetch_and(&task.task_flags, ~TASKF_IDLE, __ATOMIC_RELAXED);
}

extern "C" void SleepCpp(int64_t ms)
{
  if (ms) {
    auto delta = int64_t(uint64_t(ms) * JIFFY_FREQ) / 1000;
    SleepUntilCpp(__atomic_load_n(&cnts.jiffies, __ATOMIC_RELAXED) + delta);
  } else
    _YIELD();
}

extern "C" uint64_t Ona2FreqCpp(int8_t ona)
{
  if (!ona)
    return 0;
  constexpr auto octave = std::bit_cast<uint64_t>(12.0);
  constexpr auto two = std::bit_cast<uint64_t>(2.0);
  constexpr auto base = std::bit_cast<uint64_t>(440.0 / 32);
  return F64MulBits(base,
                    F64PowerBits(two, F64DivBits(F64FromI64(ona), octave)));
}

extern "C" uint64_t Freq2OnaCpp(uint64_t freq)
{
  constexpr uint64_t infinity = 0x7FF0000000000000;
  if (!freq || freq > infinity)
    return 0;
  constexpr auto scale = std::bit_cast<uint64_t>(32.0 / 440.0);
  constexpr auto octave = std::bit_cast<uint64_t>(12.0);
  auto ona = F64ToI64(F64MulBits(octave, F64Log2Bits(F64MulBits(scale, freq))),
                      0x0C00);
  return ona < 1 ? 1 : ona > INT8_MAX ? INT8_MAX : ona;
}

extern "C" void SndCpp(int8_t ona)
{
  auto &flags = sys_semas[SEMA_SND].flags;
  if (IsMuteCpp() || (__atomic_fetch_or(&flags, 1, __ATOMIC_ACQUIRE) & 1))
    return;

  if (!ona) {
    scrncast.ona = ona;
    OutU8(0x61, InU8(0x61) & ~3);
  } else if (ona != scrncast.ona) {
    scrncast.ona = ona;
    auto period = F64ToI64(
        F64DivBits(F64FromI64(SYS_TIMER_FREQ), Ona2FreqCpp(ona)), 0x0C00);
    period = period < 1 ? 1 : period > UINT16_MAX ? UINT16_MAX : period;
    OutU8(0x43, 0xB6);
    OutU8(0x42, period);
    OutU8(0x42, period >> 8);
    OutU8(0x61, 3 | InU8(0x61));
  }

  if (!IsDbgModeCpp() && __atomic_load_n(&scrncast.record, __ATOMIC_ACQUIRE)) {
    auto *snd = reinterpret_cast<CSndData *>(
        CAlloc(sizeof(CSndData), adam_task->data_heap));
    if (snd) {
      snd->ona = ona;
      snd->tS = tSCpp();
      QueIns(*snd, *scrncast.snd_head.last);
    }
  }
  __atomic_fetch_and(&flags, ~uint64_t{1}, __ATOMIC_RELEASE);
}

extern "C" uint64_t ScrnCastCpp(uint8_t val, uint8_t just_audio,
                                const char *print_fmt)
{
  scrncast.just_audio = just_audio != 0;
  bool old = __atomic_exchange_n(&scrncast.record, false, __ATOMIC_ACQ_REL);
  if (val && !old) {
    auto *fmt = print_fmt ? StrNew(print_fmt, adam_task->data_heap) : nullptr;
    Free(scrncast.print_fmt);
    scrncast.print_fmt = fmt;
    scrncast.t0_now = Now();
    scrncast.snd_head.tS = scrncast.t0_tS = tSCpp();
    scrncast.snd_head.ona = scrncast.ona;
    __atomic_store_n(&scrncast.record, true, __ATOMIC_RELEASE);
  }
  SndCpp(0);
  return old;
}

extern "C" void SndRstCpp()
{
  auto &flags = sys_semas[SEMA_SND].flags;
  if (__atomic_load_n(&flags, __ATOMIC_RELAXED) & 1) {
    SleepCpp(1);
    if (__atomic_load_n(&flags, __ATOMIC_RELAXED) & 1) {
      SleepCpp(1);
      __atomic_fetch_and(&flags, ~uint64_t{1}, __ATOMIC_RELEASE);
    }
  }
  SndCpp(0);
}

extern "C" void BeepCpp(int8_t ona, uint8_t busy)
{
  SndCpp(ona);
  if (busy)
    BusyCpp(500000);
  else
    SleepCpp(500);
  SndCpp(0);
  if (busy)
    BusyCpp(200000);
  else
    SleepCpp(200);
}

extern "C" uint64_t MuteCpp(uint8_t val)
{
  auto &flags = sys_semas[SEMA_MUTE].flags;
  if (!val)
    return __atomic_fetch_and(&flags, ~uint64_t{1}, __ATOMIC_RELAXED) & 1;
  uint64_t rflags;
  asm volatile("pushfq; popq %0; cli" : "=r"(rflags) : : "memory");
  SndCpp(0);
  auto old = __atomic_fetch_or(&flags, 1, __ATOMIC_RELAXED) & 1;
  asm volatile("pushq %0; popfq" : : "r"(rflags) : "cc", "memory");
  return old;
}

extern "C" uint64_t IsMuteCpp()
{
  return __atomic_load_n(&sys_semas[SEMA_MUTE].flags, __ATOMIC_RELAXED) & 1;
}

extern "C" uint64_t SingleUserCpp(uint8_t val)
{
  auto &flags = sys_semas[SEMA_SINGLE_USER].flags;
  return (val ? __atomic_fetch_or(&flags, 1, __ATOMIC_RELAXED)
              : __atomic_fetch_and(&flags, ~uint64_t{1}, __ATOMIC_RELAXED)) & 1;
}

extern "C" uint64_t IsSingleUserCpp()
{
  return __atomic_load_n(&sys_semas[SEMA_SINGLE_USER].flags, __ATOMIC_RELAXED) & 1;
}
