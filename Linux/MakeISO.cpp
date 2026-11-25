#include "../Adam/Opt/Boot/DskISORedSea.hpp"
#include "BootMedia.hpp"

#include <algorithm>
#include <cerrno>
#include <iostream>
#include <system_error>
#include <unistd.h>

namespace fs = std::filesystem;

constexpr size_t DRV_OFFSET = (ISO_LOADER_LBA + 1) * DVD_BLK_SIZE / BLK_SIZE;

// Kernel/KDate.HC YearStartDate for 2025-01-01 00:00:00.
constexpr uint64_t DATE = (2025ULL * 365 + 2024 / 4 - 2024 / 100 + 2024 / 400)
                          << 32;

static void WriteLE(Bytes &disc, size_t offset, uint64_t val, size_t width)
{
  for (size_t i = 0; i < width; i++) {
    disc.at(offset + i) = static_cast<uint8_t>(val);
    val >>= 8;
  }
}

static void WriteString(Bytes &disc, size_t offset, std::string_view text)
{
  std::copy(text.begin(), text.end(), disc.begin() + offset);
}

static void RedSeaDirEntry(Bytes &disc, size_t offset, std::string_view name,
                           size_t clus, size_t size, uint16_t attr = 0)
{
  WriteLE(disc, offset, attr | RS_ATTR_CONTIGUOUS, 2);
  WriteString(disc, offset + 2, name);
  WriteLE(disc, offset + 40, clus, 8);
  WriteLE(disc, offset + 48, size, 8);
  WriteLE(disc, offset + 56, DATE, 8);
}

struct CRootFile
{
  std::string name;
  Bytes data;
  uint16_t attr;
  size_t clus;
};

static Bytes RedSeaISO(const Bytes &kernel, std::vector<CRootFile> &files,
                       size_t &root, CDvdExtent &extent)
{
  // One reserved data block comes before the root directory.
  size_t root_blocks =
      std::max<size_t>(2, CeilDiv((4 + files.size()) * 64, BLK_SIZE));
  size_t bitmap_blocks = 1, data_area, boot, kernel_clus, total_blocks;
  for (;;) {
    data_area = DRV_OFFSET + bitmap_blocks;
    root = data_area + 1;
    boot = root + root_blocks;
    kernel_clus = boot + 1;
    extent = DvdExtent(kernel_clus, kernel.size());
    total_blocks = 4 * (extent.lba + extent.sectors);
    for (auto &file : files) {
      file.clus = total_blocks;
      total_blocks += CeilDiv(file.data.size(), BLK_SIZE);
    }
    total_blocks = 4 * CeilDiv(total_blocks, 4);
    size_t needed = CeilDiv(total_blocks - DRV_OFFSET, BLK_SIZE * 8);
    if (needed == bitmap_blocks)
      break;
    bitmap_blocks = needed;
  }

  Bytes disc(total_blocks * BLK_SIZE);
  size_t base = DRV_OFFSET * BLK_SIZE;
  disc[base + 3] = 0x88;
  WriteLE(disc, base + 8, DRV_OFFSET, 8);
  WriteLE(disc, base + 16, total_blocks - DRV_OFFSET, 8);
  WriteLE(disc, base + 24, root, 8);
  WriteLE(disc, base + 32, bitmap_blocks, 8);
  WriteLE(disc, base + 40, DATE, 8);
  WriteLE(disc, base + 510, 0xAA55, 2);

  size_t bitmap = base + BLK_SIZE;
  size_t used =
      (files.empty()
           ? kernel_clus + CeilDiv(kernel.size(), BLK_SIZE)
           : files.back().clus + CeilDiv(files.back().data.size(), BLK_SIZE)) -
      data_area;
  for (size_t bit = 0; bit < used; bit++)
    disc[bitmap + bit / 8] |= 1 << (bit % 8);

  size_t root_size = root_blocks * BLK_SIZE;
  base = root * BLK_SIZE;
  RedSeaDirEntry(disc, base, ".", root, root_size, RS_ATTR_DIR);
  RedSeaDirEntry(disc, base + 64, "..", root, 0, RS_ATTR_DIR);
  RedSeaDirEntry(disc, base + 128, "0000Boot", boot, BLK_SIZE, RS_ATTR_DIR);
  size_t entry = base + 192;
  for (const auto &file : files) {
    RedSeaDirEntry(disc, entry, file.name, file.clus, file.data.size(),
                   file.attr);
    std::copy(file.data.begin(), file.data.end(),
              disc.begin() + file.clus * BLK_SIZE);
    entry += 64;
  }
  base = boot * BLK_SIZE;
  RedSeaDirEntry(disc, base, "0000Boot", boot, BLK_SIZE, RS_ATTR_DIR);
  RedSeaDirEntry(disc, base + 64, "..", root, 0, RS_ATTR_DIR);
  RedSeaDirEntry(disc, base + 128, "0000Kernel.BIN.C", kernel_clus,
                 kernel.size());

  std::copy(kernel.begin(), kernel.end(),
            disc.begin() + kernel_clus * BLK_SIZE);
  return disc;
}

static void WriteImage(const fs::path &output, const Bytes &disc)
{
  std::string name = output.string() + ".XXXXXX";
  int fd = mkstemp(name.data());
  if (fd < 0)
    throw std::system_error(errno, std::generic_category(), name);
  try {
    size_t written = 0;
    while (written < disc.size()) {
      ssize_t count = write(fd, disc.data() + written, disc.size() - written);
      if (count < 0 && errno == EINTR)
        continue;
      if (count <= 0)
        throw std::system_error(errno, std::generic_category(), name);
      written += static_cast<size_t>(count);
    }
    int rc = close(fd);
    fd = -1;
    if (rc < 0)
      throw std::system_error(errno, std::generic_category(), name);
    fs::rename(name, output);
  } catch (...) {
    if (fd >= 0)
      close(fd);
    unlink(name.c_str());
    throw;
  }
}

static void AddRootFile(std::vector<CRootFile> &files, std::string name,
                        const fs::path &path)
{
  if (name.empty() || name.size() >= 38 || name == "." || name == ".." ||
      name == "0000Boot" || name.find_first_of("/\\:") != std::string::npos ||
      std::any_of(files.begin(), files.end(),
                  [&name](const auto &file) { return file.name == name; }))
    throw std::runtime_error("Invalid or duplicate RedSea filename " + name);
  auto bytes = FileRead(path);
  uint16_t attr = 0;
  if (name.ends_with(".Z")) {
    if (bytes.size() < 17 || ReadLE(bytes, 0, 8) != bytes.size() ||
        bytes[16] < 1 || bytes[16] > 3)
      throw std::runtime_error("Invalid archive header in " + path.string());
    attr = RS_ATTR_COMPRESSED;
  }
  files.push_back({std::move(name), std::move(bytes), attr, 0});
}

int main(int argc, char **argv)
{
  if (argc < 4) {
    std::cerr << "Usage: " << argv[0]
              << " loader kernel output.iso [StartOS.HC.Z] [--file path]...\n";
    return 2;
  }
  try {
    const fs::path loader_path = argv[1], kernel_path = argv[2];
    Bytes loader = FileRead(loader_path), kernel = FileRead(kernel_path);
    if (loader.size() != DVD_BLK_SIZE)
      throw std::runtime_error("The DVD loader must occupy 2048 bytes");
    if (kernel.size() < 32 || !HasBytes(kernel, 0, "\xEB\x1E") ||
        !HasBytes(kernel, 4, "TOSB"))
      throw std::runtime_error("Invalid CBinFile header");

    uint64_t patch_table = ReadLE(kernel, 16, 8);
    if (ReadLE(kernel, 24, 8) != kernel.size() || patch_table <= 32 ||
        patch_table >= kernel.size() || kernel.size() % 16)
      throw std::runtime_error("Invalid CBinFile extent");

    std::vector<CRootFile> files;
    int arg = 4;
    if (arg < argc && std::string_view(argv[arg]) != "--file")
      AddRootFile(files, "StartOS.HC.Z", argv[arg++]);
    while (arg < argc) {
      if (std::string_view(argv[arg++]) != "--file" || arg == argc)
        throw std::runtime_error("Expected --file followed by a path");
      fs::path path = argv[arg++];
      AddRootFile(files, path.filename().string(), path);
    }
    size_t root;
    CDvdExtent extent;
    Bytes disc = RedSeaISO(kernel, files, root, extent);
    RedSeaISO9660(disc.data(), disc.size() / DVD_BLK_SIZE, root);
    switch (RedSeaISO9660Stage1(loader.data(), extent)) {
    case DVD_EXTENT_OK:
      break;
    case DVD_EXTENT_MARKER:
      throw std::runtime_error("Missing DVD loader extent");
    case DVD_EXTENT_RANGE:
      throw std::runtime_error("Kernel would overwrite the relocated DVD loader");
    }
    std::copy(loader.begin(), loader.end(),
              disc.begin() + ISO_LOADER_LBA * DVD_BLK_SIZE);
    WriteImage(argv[3], disc);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
