#include "../Adam/Opt/Boot/DskISORedSea.hpp"
#include "../Kernel/Compress.hpp"
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

struct CImageEntry
{
  std::string name;
  Bytes data;
  uint16_t attr = 0;
  size_t clus = 0;
  std::vector<CImageEntry> children;

  size_t Size() const
  {
    return attr & RS_ATTR_DIR
               ? CeilDiv((3 + children.size()) * 64, BLK_SIZE) * BLK_SIZE
               : data.size();
  }
};

static void AssignExtents(std::vector<CImageEntry> &files, size_t &next)
{
  for (auto &file : files) {
    file.clus = next;
    next += CeilDiv(file.Size(), BLK_SIZE);
    if (file.attr & RS_ATTR_DIR)
      AssignExtents(file.children, next);
  }
}

static void WriteEntries(Bytes &disc, size_t entry, size_t parent,
                         const std::vector<CImageEntry> &files)
{
  for (const auto &file : files) {
    auto size = file.Size(), offset = file.clus * BLK_SIZE;
    RedSeaDirEntry(disc, entry, file.name, file.clus, size, file.attr);
    if (file.attr & RS_ATTR_DIR) {
      RedSeaDirEntry(disc, offset, file.name, file.clus, size, RS_ATTR_DIR);
      RedSeaDirEntry(disc, offset + 64, "..", parent, 0, RS_ATTR_DIR);
      WriteEntries(disc, offset + 128, file.clus, file.children);
    } else
      std::copy(file.data.begin(), file.data.end(), disc.begin() + offset);
    entry += 64;
  }
}

static Bytes RedSeaISO(const Bytes &kernel, std::vector<CImageEntry> &files,
                       size_t &root, CDvdExtent &extent)
{
  // One reserved data block comes before the root directory.
  size_t root_blocks =
      std::max<size_t>(2, CeilDiv((4 + files.size()) * 64, BLK_SIZE));
  size_t bitmap_blocks = 1, data_area, boot, kernel_clus, used_end,
         total_blocks;
  for (;;) {
    data_area = DRV_OFFSET + bitmap_blocks;
    root = data_area + 1;
    boot = root + root_blocks;
    kernel_clus = boot + 1;
    extent = DvdExtent(kernel_clus, kernel.size());
    used_end = 4 * (extent.lba + extent.sectors);
    AssignExtents(files, used_end);
    total_blocks = 4 * CeilDiv(used_end, 4);
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
  size_t used = (files.empty() ? kernel_clus + CeilDiv(kernel.size(), BLK_SIZE)
                               : used_end) -
                data_area;
  for (size_t bit = 0; bit < used; bit++)
    disc[bitmap + bit / 8] |= 1 << (bit % 8);

  size_t root_size = root_blocks * BLK_SIZE;
  base = root * BLK_SIZE;
  RedSeaDirEntry(disc, base, ".", root, root_size, RS_ATTR_DIR);
  RedSeaDirEntry(disc, base + 64, "..", root, 0, RS_ATTR_DIR);
  RedSeaDirEntry(disc, base + 128, "0000Boot", boot, BLK_SIZE, RS_ATTR_DIR);
  WriteEntries(disc, base + 192, root, files);
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

static bool ValidName(std::string_view name)
{
  return !name.empty() && name.size() < 38 && name != "." && name != ".." &&
         name.find_first_of("\\:") == name.npos;
}

static void AddEntry(std::vector<CImageEntry> &root, std::string_view name,
                     const fs::path *path, bool compress = false)
{
  if (name.starts_with('/'))
    name.remove_prefix(1);
  auto *files = &root;
  while (true) {
    auto slash = name.find('/');
    auto part = name.substr(0, slash);
    if (!ValidName(part) || (files == &root && part == "0000Boot"))
      throw std::runtime_error("Invalid RedSea name " + std::string(part));
    auto found =
        std::find_if(files->begin(), files->end(),
                     [part](const auto &file) { return file.name == part; });
    if (slash == name.npos) {
      if (found != files->end())
        throw std::runtime_error("Duplicate RedSea name " + std::string(part));
      if (!path) {
        files->push_back({std::string(part), {}, RS_ATTR_DIR, 0, {}});
        return;
      }
      auto bytes = FileRead(*path);
      if (compress) {
        if (!part.ends_with(".Z"))
          throw std::runtime_error("Compressed RedSea name must end in .Z");
        if (bytes.size() > SIZE_MAX - ARC_WRITE_OVERHEAD)
          throw std::runtime_error("RedSea file too large to compress");
        CArcEncodeCtrl ctrl;
        Bytes packed(bytes.size() + ARC_WRITE_OVERHEAD);
        auto size = ArcCompressBuf(bytes.data(), bytes.size(), packed.data(),
                                   packed.size(), ctrl);
        if (!size)
          throw std::runtime_error("Could not compress " + path->string());
        packed.resize(size);
        bytes = std::move(packed);
      }
      uint16_t attr = 0;
      if (part.ends_with(".Z")) {
        if (bytes.size() < 17 || ReadLE(bytes, 0, 8) != bytes.size() ||
            bytes[16] < 1 || bytes[16] > 3)
          throw std::runtime_error("Invalid archive header in " +
                                   path->string());
        attr = RS_ATTR_COMPRESSED;
      }
      files->push_back({std::string(part), std::move(bytes), attr, 0, {}});
      return;
    }
    if (found == files->end()) {
      files->push_back({std::string(part), {}, RS_ATTR_DIR, 0, {}});
      files = &files->back().children;
    } else {
      if (!(found->attr & RS_ATTR_DIR))
        throw std::runtime_error("RedSea parent is a file: " +
                                 std::string(part));
      files = &found->children;
    }
    name.remove_prefix(slash + 1);
  }
}

int main(int argc, char **argv)
{
  if (argc < 4) {
    std::cerr << "Usage: " << argv[0]
              << " loader kernel output.iso [StartOS.HC.Z]"
                 " [--file path | --file-at guest-path path"
                 " | --compress-at guest-path path | --dir guest-path]...\n";
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

    std::vector<CImageEntry> files;
    int arg = 4;
    if (arg < argc && !std::string_view(argv[arg]).starts_with("--")) {
      const fs::path path = argv[arg++];
      AddEntry(files, "StartOS.HC.Z", &path);
    }
    while (arg < argc) {
      std::string_view option = argv[arg++];
      if (option == "--file" && arg < argc) {
        fs::path path = argv[arg++];
        AddEntry(files, path.filename().string(), &path);
      } else if ((option == "--file-at" || option == "--compress-at") &&
                 arg + 1 < argc) {
        std::string_view name = argv[arg++];
        const fs::path path = argv[arg++];
        AddEntry(files, name, &path, option == "--compress-at");
      } else if (option == "--dir" && arg < argc) {
        AddEntry(files, argv[arg++], nullptr);
      } else
        throw std::runtime_error(
            "Expected --file, --file-at, --compress-at, or --dir");
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
