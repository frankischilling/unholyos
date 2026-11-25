#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr unsigned ARC_BITS_MAX = 12;
constexpr unsigned ARC_TABLE_SIZE = 1 << ARC_BITS_MAX;
constexpr uint8_t CT_NONE = 1, CT_7_BIT = 2, CT_8_BIT = 3;

struct [[gnu::packed]] CArcCompress
{
  uint64_t compressed_size, expanded_size;
  uint8_t compression_type;
};

static_assert(sizeof(CArcCompress) == 17);

struct CArcEntry
{
  uint16_t basecode, children;
  uint8_t ch;
  bool linked, assigned;
};

struct CArcCtrl
{
  unsigned min_table_entry, free_idx, free_limit, next_bits_in_use;
  unsigned cur_entry, next_entry;
  CArcEntry compress[ARC_TABLE_SIZE];
  uint8_t stack[ARC_TABLE_SIZE];
};

struct CArcEncodeCtrl
{
  CArcCtrl state;
  uint16_t heads[ARC_TABLE_SIZE], links[ARC_TABLE_SIZE];
};

// Allow one byte beyond the stored size for a final code-width transition.
constexpr size_t ARC_WRITE_OVERHEAD = sizeof(CArcCompress) + 1;

bool ArcReadHeader(const uint8_t *src, size_t src_size, CArcCompress &arc);
bool ArcExpandBuf(const uint8_t *src, size_t src_size, uint8_t *dst,
                  size_t dst_size, CArcCtrl *ctrl);
size_t ArcCompressBuf(const uint8_t *src, size_t size, uint8_t *dst,
                      size_t dst_size, CArcEncodeCtrl &ctrl);
