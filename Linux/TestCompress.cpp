#include "../Kernel/Compress.hpp"
#include "BootMedia.hpp"

#include <algorithm>
#include <array>
#include <iostream>

static void Check(bool ok, const char *message)
{
  if (!ok)
    throw std::runtime_error(message);
}

static void TestStored()
{
  constexpr uint8_t stored[]{20, 0, 0, 0, 0, 0, 0,       0,   3, 0,
                             0,  0, 0, 0, 0, 0, CT_NONE, 'A', 0, 0xFF};
  std::array<uint8_t, 3> bytes;
  Check(ArcExpandBuf(stored, sizeof(stored), bytes.data(), bytes.size(),
                     nullptr) &&
            bytes == std::array<uint8_t, 3>{'A', 0, 0xFF},
        "Stored archive differs from the original bytes");
  Check(!ArcExpandBuf(stored, sizeof(stored) - 1, bytes.data(), bytes.size(),
                      nullptr),
        "Accepted a truncated stored archive");

  constexpr uint8_t empty[]{17, 0, 0, 0, 0, 0, 0, 0,      0,
                            0,  0, 0, 0, 0, 0, 0, CT_NONE};
  Check(ArcExpandBuf(empty, sizeof(empty), nullptr, 0, nullptr),
        "Empty stored archive did not expand");
  CArcEncodeCtrl encode;
  Bytes packed(ARC_WRITE_OVERHEAD);
  Check(ArcCompressBuf(nullptr, 0, packed.data(), packed.size(), encode) ==
                sizeof(empty) &&
            std::equal(std::begin(empty), std::end(empty), packed.begin()),
        "Empty input did not produce a stored archive");
}

static void TestArchive(const char *path, size_t size, uint8_t type,
                        uint64_t checksum)
{
  auto arc = FileRead(path);
  CArcCompress header;
  Check(ArcReadHeader(arc.data(), arc.size(), header) &&
            header.compressed_size == arc.size() &&
            header.expanded_size == size && header.compression_type == type,
        "Unexpected archive header");

  CArcCtrl ctrl;
  Bytes out(size + 1, 0xA5);
  Check(ArcExpandBuf(arc.data(), arc.size(), out.data(), size, &ctrl) &&
            out.back() == 0xA5,
        "Archive did not expand within the output buffer");
  uint64_t hash = 14695981039346656037ULL;
  for (size_t i = 0; i < size; i++)
    hash = (hash ^ out[i]) * 1099511628211ULL;
  Check(hash == checksum, "Expanded bytes differ from TempleOS");

  CArcEncodeCtrl encode;
  Bytes packed(size + ARC_WRITE_OVERHEAD);
  const size_t packed_size =
      ArcCompressBuf(out.data(), size, packed.data(), packed.size(), encode);
  Check(packed_size == arc.size() &&
            std::equal(arc.begin(), arc.end(), packed.begin()),
        "Re-encoded archive differs from TempleOS");

  Check(!ArcExpandBuf(arc.data(), arc.size(), out.data(), size - 1, &ctrl),
        "Accepted an undersized output buffer");
  arc.resize(arc.size() / 2);
  for (size_t i = 0; i < 8; i++)
    arc[i] = arc.size() >> (i * 8);
  Check(!ArcExpandBuf(arc.data(), arc.size(), out.data(), size, &ctrl),
        "Accepted a truncated compressed stream");
  arc[16] = CT_8_BIT + 1;
  Check(!ArcReadHeader(arc.data(), arc.size(), header),
        "Accepted an unknown compression type");
}

int main(int argc, char **argv)
{
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " StartOS.HC.Z KDbg.HC.Z\n";
    return 2;
  }
  try {
    TestStored();
    TestArchive(argv[1], 1158, CT_7_BIT, 0x68B531597D446B26);
    TestArchive(argv[2], 14463, CT_8_BIT, 0x131A1E9E8E8A790D);
    std::cout
        << "Verified TempleOS archive expansion and byte-for-byte encoding.\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
