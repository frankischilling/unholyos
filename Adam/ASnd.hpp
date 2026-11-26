#pragma once

#include "../Kernel/KernelA.hpp"

struct [[gnu::packed]] CMusicGlbls
{
  char *cur_song;
  CTask *cur_song_task;
  int64_t octave;
  uint64_t note_len;
  uint8_t note_map[7];
  bool mute;
  int64_t meter_top, meter_bottom;
  uint64_t tempo, stacatto_factor;
  int64_t play_note_num;
  uint64_t tM_correction, last_Beat, last_tM;
};

static_assert(sizeof(CMusicGlbls) == 104 &&
              offsetof(CMusicGlbls, note_map) == 32 &&
              offsetof(CMusicGlbls, mute) == 39 &&
              offsetof(CMusicGlbls, tempo) == 56 &&
              offsetof(CMusicGlbls, last_tM) == 96);

extern CMusicGlbls music;
extern "C" [[noreturn]] void SndTaskEndCBCpp();
extern "C" uint64_t tMCpp();
extern "C" uint64_t BeatCpp();
extern "C" const char *MusicSetOctaveCpp(const char *st);
extern "C" const char *MusicSetMeterCpp(const char *st);
extern "C" const char *MusicSetNoteLenCpp(const char *st);
extern "C" int64_t Note2OnaCpp(int64_t note, int64_t octave);
extern "C" int64_t Ona2NoteCpp(int8_t ona);
extern "C" int64_t Ona2OctaveCpp(int8_t ona);
extern "C" void PlayCpp(const char *st, const char *words);
extern "C" void MusicSettingsRstCpp();
