#include "../../../Linux/BootMedia.hpp"
#include "../../../Kernel/KernelA.hpp"

#include <algorithm>
#include <bit>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <system_error>
#include <unistd.h>

namespace fs = std::filesystem;

constexpr size_t DRV_OFFSET = 22 * 4, LOADER_LBA = 21, CATALOG_LBA = 20;
constexpr size_t BOOT_RAM_LIMIT = 0x96600 - 0x7C00;

// Kernel/KDate.HC YearStartDate for 2025-01-01 00:00:00.
constexpr uint64_t DATE = (2025ULL * 365 + 2024 / 4 - 2024 / 100 + 2024 / 400)
                          << 32;
constexpr std::string_view LOADER_MARKER = "UNHOLYOS-CD-LBA!";

static_assert(std::endian::native==std::endian::little);

static void FillU16Palindrome(CPalindromeU16 *dst,uint16_t w)
{
  dst->big=__builtin_bswap16(w);
  dst->little=w;
}

static void FillU32Palindrome(CPalindromeU32 *dst,int64_t d)
{
  dst->big=__builtin_bswap32(static_cast<uint32_t>(d));
  dst->little=static_cast<uint32_t>(d);
}

struct CElTorito
{
  uint16_t w[16];
  uint8_t bootable; //88=bootable 00=not bootable
  uint8_t media_type; //0=no emulation 4=hard disk
  uint16_t load_seg; //0000->07C0
  uint8_t sys_type;
  uint8_t zero;
  uint16_t sect_cnt;
  uint32_t load_rba; //start addr of virtual disk
  uint8_t zero2[20];
};

static_assert(sizeof(CElTorito)==64 && offsetof(CElTorito,bootable)==32 &&
              offsetof(CElTorito,sect_cnt)==38 &&
              offsetof(CElTorito,load_rba)==40);

class CDvdExtent
{
public:
  size_t lba, sectors, shift;
};

static CDvdExtent DvdExtent(size_t clus, size_t size)
{
  size_t shift = clus % 4;
  return {clus / 4, CeilDiv(size, DVD_BLK_SIZE) + (shift != 0), shift};
}

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

static void RedSeaISO9660(Bytes &disc, size_t root)
{
  CISOPriDesc iso_pri{},iso_boot{},iso_sup{},iso_term{};
  CElTorito et{};
  CPalindromeU32 root_clus{};
  size_t iso_size=disc.size()/DVD_BLK_SIZE;
  uint32_t catalog=CATALOG_LBA;
  uint16_t j=0;

  iso_pri.type=ISOT_PRI_VOL_DESC;
  std::memcpy(iso_pri.id,"CD001",sizeof(iso_pri.id));
  iso_pri.version=1;
  FillU16Palindrome(&iso_pri.vol_set_size,1);
  FillU16Palindrome(&iso_pri.vol_seq_num,1);
  FillU16Palindrome(&iso_pri.log_block_size,DVD_BLK_SIZE);
  FillU32Palindrome(&iso_pri.vol_space_size,iso_size);
  // The reference writes at the record start, before CISODirEntry::loc.
  FillU32Palindrome(&root_clus,root);
  std::memcpy(&iso_pri.root_dir_record,&root_clus,sizeof(root_clus));
  iso_pri.file_structure_version=1;
  std::memcpy(iso_pri.publisher_id,"TempleOS RedSea",sizeof("TempleOS RedSea"));

  iso_sup=iso_pri;
  iso_sup.type=ISOT_SUPPLEMENTARY_DESC;

  iso_boot.type=ISOT_BOOT_RECORD;
  std::memcpy(iso_boot.id,"CD001",sizeof(iso_boot.id));
  iso_boot.version=1;
  std::memcpy(reinterpret_cast<uint8_t *>(&iso_boot)+7,
              "EL TORITO SPECIFICATION",sizeof("EL TORITO SPECIFICATION"));
  std::memcpy(reinterpret_cast<uint8_t *>(&iso_boot)+0x47,&catalog,
              sizeof(catalog));

  iso_term.type=ISOT_TERMINATOR;
  std::memcpy(iso_term.id,"CD001",sizeof(iso_term.id));
  iso_term.version=1;

  std::memcpy(disc.data()+16*DVD_BLK_SIZE,&iso_pri,sizeof(iso_pri));
  std::memcpy(disc.data()+17*DVD_BLK_SIZE,&iso_boot,sizeof(iso_boot));
  std::memcpy(disc.data()+18*DVD_BLK_SIZE,&iso_sup,sizeof(iso_sup));
  std::memcpy(disc.data()+19*DVD_BLK_SIZE,&iso_term,sizeof(iso_term));

  et.w[0]=1;
  std::memcpy(&et.w[2],"UnholyOS",sizeof("UnholyOS"));
  et.w[15]=0xAA55;
  for (size_t i=0;i<16;i++)
    j+=et.w[i];
  et.w[14]=static_cast<uint16_t>(-j);
  et.bootable=0x88;
  et.media_type=0;
  et.sect_cnt=4;
  et.load_rba=LOADER_LBA;
  std::memcpy(disc.data()+CATALOG_LBA*DVD_BLK_SIZE,&et,sizeof(et));
}

static void RedSeaISO9660Stage1(Bytes &disc, const Bytes &loader,
                                const CDvdExtent &extent)
{
  size_t base = LOADER_LBA * DVD_BLK_SIZE;
  std::copy(loader.begin(), loader.end(), disc.begin() + base);
  size_t off = std::string_view(reinterpret_cast<const char *>(loader.data()),
                                loader.size())
                   .find(LOADER_MARKER);
  if (off == std::string_view::npos ||
      off + LOADER_MARKER.size() + 8 > loader.size())
    throw std::runtime_error("Missing DVD loader extent");

  if (extent.sectors * DVD_BLK_SIZE + extent.shift * BLK_SIZE > BOOT_RAM_LIMIT)
    throw std::runtime_error("Kernel would overwrite the relocated DVD loader");

  off += base + LOADER_MARKER.size();
  WriteLE(disc, off, extent.lba, 4);
  WriteLE(disc, off + 4, extent.sectors, 2);
  WriteLE(disc, off + 6, extent.shift, 2);
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
    RedSeaISO9660(disc, root);
    RedSeaISO9660Stage1(disc, loader, extent);
    WriteImage(argv[3], disc);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
