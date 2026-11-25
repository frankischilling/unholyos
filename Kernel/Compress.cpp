#include "Compress.hpp"
#include "KernelB.hpp"

bool ArcReadHeader(const uint8_t *src, size_t src_size, CArcCompress &arc)
{
  if (src_size < sizeof(arc))
    return false;
  MemCpy(&arc, src, sizeof(arc));
  if (arc.compressed_size < sizeof(arc) || arc.compressed_size > src_size ||
      arc.expanded_size >= SIZE_MAX || arc.compression_type < CT_NONE ||
      arc.compression_type > CT_8_BIT)
    return false;
  size_t payload = arc.compressed_size - sizeof(arc);
  if (arc.compression_type == CT_NONE)
    return arc.expanded_size <= payload;

  // A code consumes at least one byte and expands to at most one table chain.
  size_t min_codes = arc.expanded_size / ARC_TABLE_SIZE +
                     (arc.expanded_size % ARC_TABLE_SIZE != 0);
  return min_codes <= payload && payload <= SIZE_MAX / 8;
}

static bool ArcEntryGet(CArcCtrl &c)
{
  c.cur_entry = c.next_entry;
  if (c.next_bits_in_use < ARC_BITS_MAX) {
    c.next_entry = c.free_idx++;
    if (c.free_idx == c.free_limit) {
      c.next_bits_in_use++;
      c.free_limit <<= 1;
    }
    return true;
  }

  // The 12-bit allocator advances before searching for a leaf.
  for (unsigned i = c.min_table_entry; i < c.free_limit; i++) {
    if (++c.free_idx == c.free_limit)
      c.free_idx = c.min_table_entry;
    auto &entry = c.compress[c.free_idx];
    if (entry.children)
      continue;
    c.next_entry = c.free_idx;
    if (entry.linked) {
      c.compress[entry.basecode].children--;
      entry.linked = false;
    }
    return true;
  }
  return false;
}

static bool ArcCodeRead(const uint8_t *src, size_t limit, size_t &pos,
                        unsigned width, unsigned &code)
{
  if (width > limit - pos)
    return false;
  size_t i = pos / 8;
  unsigned shift = pos % 8;
  unsigned bits = src[i];
  if (shift + width > 8)
    bits |= unsigned{src[i + 1]} << 8;
  if (shift + width > 16)
    bits |= unsigned{src[i + 2]} << 16;
  code = (bits >> shift) & ((1U << width) - 1);
  pos += width;
  return true;
}

static bool ArcEncodeEntryGet(CArcEncodeCtrl &c)
{
  if (!ArcEntryGet(c.state))
    return false;
  auto code = c.state.next_entry;
  auto &entry = c.state.compress[code];
  if (entry.assigned) {
    auto *link = &c.heads[entry.basecode];
    while (*link != code)
      link = &c.links[*link];
    *link = c.links[code];
    entry.assigned = false;
  }
  return true;
}

static void ArcCodeWrite(uint8_t *dst, size_t pos, unsigned code)
{
  // The destination starts zeroed, and successive codes share boundary bytes.
  auto *bytes = dst + pos / 8;
  unsigned packed = code << (pos % 8);
  bytes[0] |= packed;
  if (packed >> 8)
    bytes[1] |= packed >> 8;
  if (packed >> 16)
    bytes[2] |= packed >> 16;
}

static size_t ArcEncode(const uint8_t *src, size_t size, uint8_t *dst,
                        unsigned min_bits, CArcEncodeCtrl &c)
{
  if (!size)
    return 0;
  MemSet(&c, 0, sizeof(c));
  auto &state = c.state;
  state.min_table_entry = state.free_idx = 1U << min_bits;
  state.next_bits_in_use = min_bits + 1;
  state.free_limit = 1U << state.next_bits_in_use;
  if (!ArcEncodeEntryGet(c))
    return 0;

  size_t pos = sizeof(CArcCompress) * 8,
         limit = (size + sizeof(CArcCompress)) * 8;
  size_t next = 1;
  unsigned base = src[0], width = state.next_bits_in_use;
  while (next < size && pos + width <= limit) {
    width = state.next_bits_in_use;
    if (!ArcEncodeEntryGet(c))
      return 0;
    while (next < size) {
      uint8_t ch = src[next++];
      unsigned code = c.heads[base];
      while (code && state.compress[code].ch != ch)
        code = c.links[code];
      if (code) {
        base = code;
        continue;
      }

      ArcCodeWrite(dst, pos, base);
      pos += width;
      auto slot = state.cur_entry;
      auto &entry = state.compress[slot];
      entry.basecode = base;
      entry.ch = ch;
      entry.linked = entry.assigned = true;
      state.compress[base].children++;
      c.links[slot] = c.heads[base];
      c.heads[base] = slot;
      base = ch;
      break;
    }
  }
  if (next != size || pos + width > limit)
    return 0;
  ArcCodeWrite(dst, pos, base);
  // ArcFinishCompression counts the next code width at the end of the stream.
  return (pos + state.next_bits_in_use + 7) / 8;
}

size_t ArcCompressBuf(const uint8_t *src, size_t size, uint8_t *dst,
                      size_t dst_size, CArcEncodeCtrl &ctrl)
{
  if (size > SIZE_MAX / 8 - ARC_WRITE_OVERHEAD ||
      dst_size < size + ARC_WRITE_OVERHEAD)
    return 0;
  uint8_t type = CT_7_BIT;
  for (size_t i = 0; i < size; i++) {
    if (src[i] & 0x80) {
      type = CT_8_BIT;
      break;
    }
  }
  MemSet(dst, 0, size + ARC_WRITE_OVERHEAD);
  size_t encoded = ArcEncode(src, size, dst, type == CT_7_BIT ? 7 : 8, ctrl);
  if (!encoded) {
    MemCpy(dst + sizeof(CArcCompress), src, size);
    type = CT_NONE;
    encoded = size + sizeof(CArcCompress);
  }
  CArcCompress arc{encoded, size, type};
  MemCpy(dst, &arc, sizeof(arc));
  return encoded;
}

static bool ArcExpandCodes(const uint8_t *src, size_t bits, uint8_t *dst,
                           size_t size, unsigned min_bits, CArcCtrl &c)
{
  MemSet(&c, 0, sizeof(c));
  c.min_table_entry = c.free_idx = 1U << min_bits;
  c.next_bits_in_use = min_bits + 1;
  c.free_limit = 1U << c.next_bits_in_use;
  if (!ArcEntryGet(c))
    return false;

  size_t pos = 0;
  unsigned previous;
  if (!ArcCodeRead(src, bits, pos, c.next_bits_in_use, previous) ||
      previous >= c.min_table_entry || !ArcEntryGet(c))
    return false;
  uint8_t first = previous;
  *dst++ = first;
  size--;
  while (size) {
    unsigned basecode;
    if (!ArcCodeRead(src, bits, pos, c.next_bits_in_use, basecode))
      return false;
    unsigned code = basecode;
    size_t depth = 0;
    if (code == c.cur_entry) {
      c.stack[depth++] = first;
      code = previous;
    }
    while (code >= c.min_table_entry) {
      const auto &entry = c.compress[code];
      if (!entry.assigned || depth == ARC_TABLE_SIZE - 1)
        return false;
      c.stack[depth++] = entry.ch;
      code = entry.basecode;
    }
    first = code;
    c.stack[depth++] = first;
    if (depth > size)
      return false;

    auto &current = c.compress[c.cur_entry];
    current.basecode = previous;
    current.ch = first;
    current.assigned = current.linked = true;
    c.compress[previous].children++;
    if (!ArcEntryGet(c))
      return false;
    size -= depth;
    while (depth)
      *dst++ = c.stack[--depth];
    previous = basecode;
  }
  return true;
}

bool ArcExpandBuf(const uint8_t *src, size_t src_size, uint8_t *dst,
                  size_t dst_size, CArcCtrl *ctrl)
{
  CArcCompress arc;
  if (!ArcReadHeader(src, src_size, arc) || arc.expanded_size > dst_size)
    return false;
  if (!arc.expanded_size)
    return true;

  const auto *payload = src + sizeof(arc);
  if (arc.compression_type == CT_NONE) {
    MemCpy(dst, payload, arc.expanded_size);
    return true;
  }
  return ctrl &&
         ArcExpandCodes(payload, (arc.compressed_size - sizeof(arc)) * 8, dst,
                        arc.expanded_size,
                        arc.compression_type == CT_7_BIT ? 7 : 8, *ctrl);
}
