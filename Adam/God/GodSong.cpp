#include "GodSong.hpp"
#include "../ASnd.hpp"
#include "../DolDoc/DolDoc.hpp"

bool GodSongInit()
{
  return DefineLstLoadCpp("ST_RHYTHM_COMPLEXITY", "Simple\0Normal\0Complex\0",
                          nullptr);
}

static void InsNote(CMakeSongSettings &mss, char *buf, int64_t k, int64_t &j)
{
  if (!k && mss.rests) {
    buf[j++] = 'R';
  } else {
    k /= 2;
    if (k < 3) {
      if (mss.octave_state != mss.octave) {
        mss.octave_state = mss.octave;
        buf[j++] = mss.octave_state + '0';
      }
      buf[j++] = k ? k - 1 + 'A' : 'G';
    } else {
      if (mss.octave_state != mss.octave + 1) {
        mss.octave_state = mss.octave + 1;
        buf[j++] = mss.octave_state + '0';
      }
      buf[j++] = k - 1 + 'A';
    }
  }
}

enum : uint8_t
{
  DUR_4,
  DUR_8_8,
  DUR_3_3_3,
  DUR_16_16_16_16,
  DUR_8DOT_16,
  DUR_8_16_16,
  DUR_16_16_8
};

static constexpr uint8_t god_simple_songs[5]{DUR_4, DUR_4, DUR_4, DUR_4,
                                             DUR_8_8};
static constexpr uint8_t god_normal_songs[5]{DUR_4, DUR_4, DUR_8_8, DUR_3_3_3,
                                             DUR_16_16_16_16};
static constexpr uint8_t god_complex_songs[9]{
    DUR_4,     DUR_4,       DUR_8_8,     DUR_8_8,        DUR_8DOT_16,
    DUR_3_3_3, DUR_8_16_16, DUR_16_16_8, DUR_16_16_16_16};

extern "C" char *GodSongStrCpp()
{
  CMakeSongSettings mss{};
  mss.complexity = 1;
  mss.octave = music.octave;
  if (!PopUpFormCpp(reinterpret_cast<uint8_t *>(&mss), "CMakeSongSettings",
                    DOF_SIZE_MIN, nullptr, nullptr))
    return nullptr;

  auto *buf = reinterpret_cast<char *>(CAlloc(256, CurrentTask()->data_heap));
  if (!buf)
    return nullptr;
  music.octave = mss.octave = mss.octave < 1   ? 1
                              : mss.octave > 7 ? 7
                                               : mss.octave;
  auto &progress4 = sys_progresses[3];
  progress4.val = 0;
  int64_t len = progress4.max = mss.six_eight ? 6 : 8;

  int64_t j = 0, last_duration = -1;
  mss.octave_state = mss.octave + 1;
  buf[j++] = '0' + mss.octave_state;
  if (mss.six_eight) {
    buf[j++] = 'M';
    buf[j++] = '6';
    buf[j++] = '/';
    buf[j++] = '8';
  }
  FifoFlush(*god.fifo);
  for (int64_t i = 0; i < len; i++) {
    auto n = GodBitsCpp(8, nullptr);
    auto duration = mss.complexity == 2   ? god_complex_songs[n % 9]
                    : mss.complexity == 1 ? god_normal_songs[n % 5]
                                          : god_simple_songs[n % 5];
    switch (duration) {
    case DUR_8_8:
      if (last_duration != DUR_8_8)
        buf[j++] = 'e';
      InsNote(mss, buf, GodBitsCpp(4, nullptr), j);
      InsNote(mss, buf, GodBitsCpp(4, nullptr), j);
      break;
    case DUR_8DOT_16:
      buf[j++] = 'e';
      buf[j++] = '.';
      InsNote(mss, buf, GodBitsCpp(4, nullptr), j);
      buf[j++] = 's';
      InsNote(mss, buf, GodBitsCpp(4, nullptr), j);
      duration = DUR_16_16_16_16;
      break;
    case DUR_3_3_3:
      if (last_duration != DUR_3_3_3) {
        buf[j++] = 'e';
        buf[j++] = 't';
      }
      InsNote(mss, buf, GodBitsCpp(4, nullptr), j);
      InsNote(mss, buf, GodBitsCpp(4, nullptr), j);
      InsNote(mss, buf, GodBitsCpp(4, nullptr), j);
      break;
    case DUR_8_16_16:
      if (last_duration != DUR_8_8)
        buf[j++] = 'e';
      InsNote(mss, buf, GodBitsCpp(4, nullptr), j);
      buf[j++] = 's';
      InsNote(mss, buf, GodBitsCpp(4, nullptr), j);
      InsNote(mss, buf, GodBitsCpp(4, nullptr), j);
      duration = DUR_16_16_16_16;
      break;
    case DUR_16_16_8:
      if (last_duration != DUR_16_16_16_16)
        buf[j++] = 's';
      InsNote(mss, buf, GodBitsCpp(4, nullptr), j);
      InsNote(mss, buf, GodBitsCpp(4, nullptr), j);
      buf[j++] = 'e';
      InsNote(mss, buf, GodBitsCpp(4, nullptr), j);
      duration = DUR_8_8;
      break;
    case DUR_16_16_16_16: {
      if (last_duration != DUR_16_16_16_16)
        buf[j++] = 's';
      auto k = GodBitsCpp(4, nullptr), k2 = GodBitsCpp(4, nullptr);
      InsNote(mss, buf, k, j);
      InsNote(mss, buf, k2, j);
      InsNote(mss, buf, k, j);
      InsNote(mss, buf, k2, j);
      break;
    }
    default:
      if (last_duration != DUR_4)
        buf[j++] = 'q';
      InsNote(mss, buf, GodBitsCpp(4, nullptr), j);
    }
    last_duration = duration;
    progress4.val++;
  }
  buf[j] = 0;
  progress4.val = progress4.max = 0;
  return buf;
}

extern "C" void GodSongCpp()
{
  auto *st1 = GodSongStrCpp(), *st2 = GodSongStrCpp();
  if (st1 && st2) {
    const uint64_t a = reinterpret_cast<uintptr_t>(st1);
    const uint64_t b = reinterpret_cast<uintptr_t>(st2);
    const uint64_t args[]{a, a, b, b};
    DocPrintCpp(DocPut(), "$SO,\"<Song>\",A=\"%s%s%s%s\"$", 4, args);
  }
  Free(st1);
  Free(st2);
}
