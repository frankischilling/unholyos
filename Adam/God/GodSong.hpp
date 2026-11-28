#pragma once

#include "GodExt.hpp"

struct [[gnu::packed]] CMakeSongSettings
{
  int64_t complexity;
  bool rests, six_eight;
  int64_t octave, octave_state;
};

static_assert(sizeof(CMakeSongSettings) == 26 &&
              offsetof(CMakeSongSettings, rests) == 8 &&
              offsetof(CMakeSongSettings, six_eight) == 9 &&
              offsetof(CMakeSongSettings, octave) == 10 &&
              offsetof(CMakeSongSettings, octave_state) == 18);

bool GodSongInit();
extern "C" char *GodSongStrCpp();
extern "C" void GodSongCpp();
