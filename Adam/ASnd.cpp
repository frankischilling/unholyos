#include "ASnd.hpp"
#include "../Kernel/KernelB.hpp"

#include <bit>

extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);

static constexpr uint64_t F64(double value)
{
  return std::bit_cast<uint64_t>(value);
}

static uint64_t Sub(uint64_t a, uint64_t b)
{
  return F64AddBits(a, b ^ (uint64_t{1} << 63));
}

static int64_t ToI64(uint64_t value)
{
  return F64ToI64(value, 0x0C00);
}

extern "C" void SndTaskEndCBHC();

extern "C" [[noreturn]] void SndTaskEndCBCpp()
{
  SndCpp(0);
  Exit();
}

extern "C" uint64_t SndPhaseAddOne(uint64_t rem,uint64_t period);
extern "C" uint64_t SndFullPhase(uint64_t rem,uint64_t period,uint64_t scale,
                                 uint64_t offset);
extern "C" uint64_t SndOneMinusPhase(uint64_t rem,uint64_t period);
extern "C" uint64_t SndScaleRem(uint64_t rem,uint64_t period,uint64_t scale);

extern "C" uint64_t SawCpp(uint64_t t,uint64_t period)
{
  if (period) {
    uint64_t rem=F64RemBits(t,period);
    if (int64_t(t)>=0)
      return F64DivBits(rem,period);
    else
      return SndPhaseAddOne(rem,period);
  } else
    return 0;
}

extern "C" uint64_t FullSawCpp(uint64_t t,uint64_t period)
{
  if (period) {
    uint64_t rem=F64RemBits(t,period);
    if (int64_t(t)>=0)
      return SndFullPhase(rem,period,F64(2.0),F64(-1.0));
    else
      return SndFullPhase(rem,period,F64(2.0),F64(1.0));
  } else
    return 0;
}

extern "C" uint64_t CawCpp(uint64_t t,uint64_t period)
{
  if (period) {
    uint64_t rem=F64RemBits(t,period);
    if (int64_t(t)>=0)
      return SndOneMinusPhase(rem,period);
    else
      return F64DivBits(rem^(uint64_t{1}<<63),period);
  } else
    return F64(1.0);
}

extern "C" uint64_t FullCawCpp(uint64_t t,uint64_t period)
{
  if (period) {
    uint64_t rem=F64RemBits(t,period);
    if (int64_t(t)>=0)
      return SndFullPhase(rem,period,F64(-2.0),F64(1.0));
    else
      return SndFullPhase(rem,period,F64(-2.0),F64(-1.0));
  } else
    return F64(1.0);
}

extern "C" uint64_t TriCpp(uint64_t t,uint64_t period)
{
  if (period) {
    t=SndScaleRem(F64RemBits(F64AbsBits(t),period),period,F64(2.0));
    if (F64CompareBits(t,F64(1.0))<=0)
      return t;
    else
      return F64SubBits(F64(2.0),t);
  } else
    return 0;
}

extern "C" uint64_t FullTriCpp(uint64_t t,uint64_t period)
{
  if (period) {
    t=SndScaleRem(F64RemBits(t,period),period,F64(4.0));
    if (F64CompareBits(t,F64(-1.0))<=0) {
      if (F64CompareBits(t,F64(-3.0))<=0)
        return F64AddBits(t,F64(4.0));
      else
        return F64SubBits(F64(-2.0),t);
    } else {
      if (F64CompareBits(t,F64(1.0))<=0)
        return t;
      else if (F64CompareBits(t,F64(3.0))<=0)
        return F64SubBits(F64(2.0),t);
      else
        return F64SubBits(t,F64(4.0));
    }
  } else
    return 0;
}

CMusicGlbls music{nullptr, nullptr, 4, F64(1.0), {0, 2, 3, 5, 7, 8, 10},
                  false, 4, 4, F64(2.5), F64(0.9), 0, 0, 0, 0};

extern "C" uint64_t tMCpp()
{
  return F64DivBits(F64AddBits(F64FromI64(cnts.jiffies), music.tM_correction),
                    F64FromI64(JIFFY_FREQ));
}

extern "C" uint64_t BeatCpp()
{
  uint64_t flags;
  asm volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
  auto cur_tM = tMCpp(), res = music.last_Beat;
  if (music.tempo & ~(uint64_t{1} << 63))
    res = F64AddBits(res, F64MulBits(Sub(cur_tM, music.last_tM), music.tempo));
  music.last_tM = cur_tM;
  music.last_Beat = res;
  asm volatile("pushq %0; popfq" : : "r"(flags) : "cc", "memory");
  return res;
}

static bool Digit(char ch)
{
  return ch >= '0' && ch <= '9';
}

extern "C" const char *MusicSetOctaveCpp(const char *st)
{
  while (Digit(*st))
    music.octave = *st++ - '0';
  return st;
}

extern "C" const char *MusicSetMeterCpp(const char *st)
{
  while (*st == 'M') {
    st++;
    if (Digit(*st))
      music.meter_top = *st++ - '0';
    if (*st == '/')
      st++;
    if (Digit(*st))
      music.meter_bottom = *st++ - '0';
  }
  return st;
}

extern "C" const char *MusicSetNoteLenCpp(const char *st)
{
  for (;; st++) {
    switch (*st) {
    case 'w': music.note_len = F64(4.0); break;
    case 'h': music.note_len = F64(2.0); break;
    case 'q': music.note_len = F64(1.0); break;
    case 'e': music.note_len = F64(0.5); break;
    // Adam/ASnd.HC's 0.25 literal rounds one binary64 step above C++'s.
    case 's': music.note_len = 0x3FD0000000000001; break;
    case 't':
      music.note_len = F64DivBits(F64MulBits(F64(2.0), music.note_len), F64(3.0));
      break;
    case '.':
      music.note_len = F64MulBits(F64(1.5), music.note_len);
      break;
    default: return st;
    }
  }
}

extern "C" int64_t Note2OnaCpp(int64_t note, int64_t octave)
{
  return static_cast<int8_t>((uint64_t(octave) + (note < 3)) * 12 + note);
}

extern "C" int64_t Ona2NoteCpp(int8_t ona)
{
  return ona % 12;
}

extern "C" int64_t Ona2OctaveCpp(int8_t ona)
{
  return ona / 12 - (ona % 12 < 3);
}

extern "C" void PlayCpp(const char *st, const char *words)
{
  int64_t i = 0;
  music.play_note_num = 0;
  while (*st) {
    int64_t timeout_val = cnts.jiffies;
    bool tie = false;
    const char *last_st;
    do {
      last_st = st;
      if (*st == '(') {
        tie = true;
        st++;
      } else {
        st = MusicSetMeterCpp(st);
        st = MusicSetOctaveCpp(st);
        st = MusicSetNoteLenCpp(st);
      }
    } while (last_st != st);
    if (!*st)
      break;
    int64_t note = *st++ - 'A', ona = 0;
    if (note >= 0 && note < 7) {
      note = music.note_map[note];
      int64_t octave = music.octave;
      if (*st == 'b') {
        if (--note == 2)
          octave--;
        st++;
      } else if (*st == '#') {
        if (++note == 3)
          octave++;
        st++;
      }
      ona = Note2OnaCpp(note, octave);
    }
    if (words) {
      auto *word = LstSub(i++, words);
      if (word && StrCmpCpp(reinterpret_cast<const uint8_t *>(word),
                           reinterpret_cast<const uint8_t *>(" "))) {
        uint64_t arg = reinterpret_cast<uintptr_t>(word);
        PrintCpp("%s", 1, &arg);
      }
    }
    auto d = F64DivBits(F64MulBits(F64FromI64(JIFFY_FREQ), music.note_len),
                       music.tempo);
    auto on = F64MulBits(d, music.stacatto_factor);
    auto off = F64MulBits(d, Sub(F64(1.0), music.stacatto_factor));
    timeout_val = ToI64(F64AddBits(F64FromI64(timeout_val), on));
    auto timeout_val2 = ToI64(F64AddBits(F64FromI64(timeout_val), off));
    if (!music.mute)
      SndCpp(ona);
    SleepUntilCpp(timeout_val);
    music.tM_correction = F64AddBits(music.tM_correction, Sub(on, F64FromI64(ToI64(on))));
    if (!music.mute && !tie)
      SndCpp(0);
    SleepUntilCpp(timeout_val2);
    music.tM_correction = F64AddBits(music.tM_correction, Sub(off, F64FromI64(ToI64(off))));
    music.play_note_num++;
  }
}

extern "C" void MusicSettingsRstCpp()
{
  music.play_note_num = 0;
  music.stacatto_factor = F64(0.9);
  music.tempo = F64(2.5);
  music.octave = 4;
  music.note_len = F64(1.0);
  music.meter_top = music.meter_bottom = 4;
  SndRstCpp();
  uint64_t flags;
  asm volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
  music.last_tM = tMCpp();
  music.last_Beat = 0;
  asm volatile("pushq %0; popfq" : : "r"(flags) : "cc", "memory");
}

extern "C" [[noreturn]] void CurSongTaskCpp()
{
  auto *task = CurrentTask();
  task->task_end_cb = SndTaskEndCBHC;
  for (;;) {
    // A refresh may replace the score while Play sleeps between notes.
    auto *song =
        music.cur_song ? StrNew(music.cur_song, task->data_heap) : nullptr;
    if (song) {
      PlayCpp(song, nullptr);
      Free(song);
      if (music.play_note_num)
        continue;
    }
    SleepCpp(1);
  }
}

extern "C" void SoundEffectEndTaskCBHC();
extern "C" void SoundEffectTaskHC();
extern "C" uint64_t ClampCpp(uint64_t d,uint64_t lo,uint64_t hi);
extern "C" int64_t SoundEffectSweepOna(uint64_t t,int64_t ona1,int64_t ona2);

extern "C" [[noreturn]] void SoundEffectEndTaskCBCpp()
{
  Free(reinterpret_cast<void *>(FramePtrCpp("CSoundEffectFrame",nullptr)));
  music.mute--;
  SndTaskEndCBCpp();
}

static void SoundEffectSleep(int64_t ona,uint64_t timeout)
{
  uint64_t t=ClampCpp(F64DivBits(F64(3000.0),Ona2FreqCpp(ona)),F64(1.0),F64(50.0));
  if (F64CompareBits(F64AddBits(t,tSCpp()),timeout)>0)
    t=F64SubBits(timeout,tSCpp());
  SleepCpp(ToI64(t));
}

extern "C" void SoundEffectTaskCpp(CSoundEffectFrame *ns)
{
  uint64_t t0=tSCpp(),timeout=F64AddBits(t0,ns->duration);
  if (!FramePtrAddCpp("CSoundEffectFrame",reinterpret_cast<intptr_t>(ns),nullptr)) {
    Free(ns);
    music.mute--;
    SndTaskEndCBCpp();
  }
  CurrentTask()->task_end_cb=&SoundEffectEndTaskCBHC;
  switch (ns->type) {
    case SE_NOISE: {
      int64_t i=int64_t(ns->ona2)-ns->ona1;
      if (i<1) i=1;
      while (F64CompareBits(tSCpp(),timeout)<0) {
        int64_t ona=int64_t(RandU16Cpp())%i+ns->ona1;
        SndCpp(ona);
        SoundEffectSleep(ona,timeout);
      }
      break;
    }
    case SE_SWEEP:
      while (F64CompareBits(tSCpp(),timeout)<0) {
        uint64_t t=F64DivBits(F64SubBits(tSCpp(),t0),ns->duration);
        int64_t ona=SoundEffectSweepOna(t,ns->ona1,ns->ona2);
        SndCpp(ona);
        SoundEffectSleep(ona,timeout);
      }
      break;
  }
}

static CTask *SoundEffectSpawn(int64_t mS,uint64_t ona1,uint64_t ona2,int32_t type)
{
  auto *task=CurrentTask();
  auto *ns=reinterpret_cast<CSoundEffectFrame *>(
      MAlloc(sizeof(CSoundEffectFrame),task->data_heap));
  if (!ns)
    return nullptr;
  ns->type=type;
  ns->duration=F64DivBits(F64FromI64(mS),F64(1000.0));
  ns->ona1=ToI64(ona1);
  ns->ona2=ToI64(ona2);
  music.mute++;
  auto *result=SpawnCpp(reinterpret_cast<uintptr_t>(SoundEffectTaskHC),
                        reinterpret_cast<uint8_t *>(ns),"Noise",-1,task,0,
                        JOBF_ADD_TO_QUE);
  if (!result) {
    music.mute--;
    Free(ns);
  }
  return result;
}

extern "C" CTask *NoiseCpp(int64_t mS,uint64_t min_ona,uint64_t max_ona)
{
  if (mS>0)
    return SoundEffectSpawn(mS,min_ona,max_ona,SE_NOISE);
  else
    return nullptr;
}

extern "C" CTask *SweepCpp(int64_t mS,uint64_t ona1,uint64_t ona2)
{
  if (mS>0)
    return SoundEffectSpawn(mS,ona1,ona2,SE_SWEEP);
  else
    return nullptr;
}
