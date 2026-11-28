#pragma once

#include "../../Kernel/KernelB.hpp"

constexpr unsigned GOD_BAD_BITS = 4, GOD_GOOD_BITS = 24;
constexpr char GOD_WORD_FILE_MASK[] = "/Adam/God/Vocab.DD*";

struct [[gnu::packed]] CGodGlbls
{
  uint8_t **words, *word_file_mask;
  int64_t word_fuf_flags, num_words;
  CFifoU8 *fifo;
  CDC *doodle_dc;
  int64_t doodle_ch;
  CFifoU8 *doodle_fifo;
  bool doodle_done;
};

static_assert(sizeof(CGodGlbls) == 65 && offsetof(CGodGlbls, fifo) == 32 &&
              offsetof(CGodGlbls, doodle_fifo) == 56 &&
              offsetof(CGodGlbls, doodle_done) == 64);

extern CGodGlbls god;

bool GodExtInit();
extern "C" void GodBitsInsCpp(int64_t num_bits, int64_t n);
extern "C" void GodHexInsCpp(const char *st);
extern "C" int64_t GodBitsCpp(int64_t num_bits, const char *msg);
extern "C" int64_t GodInitCpp(const char *files_find_mask,
                              const char *fu_flags);
extern "C" uint8_t *GodWordStrCpp(int64_t bits);
extern "C" void GodWordCpp(int64_t bits);
