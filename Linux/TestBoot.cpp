#include "BootMedia.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <map>
#include <poll.h>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;
using namespace std::string_view_literals;

constexpr std::string_view BOOT_MARKER =
    "UnholyOS: kernel entered in 64-bit mode";

static void Check(bool ok, const char *msg)
{
  if (!ok)
    throw std::runtime_error(msg);
}

static std::string ReadText(const fs::path &path)
{
  std::ifstream file(path);
  std::ostringstream out;
  out << file.rdbuf();
  return out.str();
}

static void CheckProgramOutput(std::string_view serial, const char *path,
                               std::string_view expected, char checks = '3')
{
  auto start = std::string("UnholyOS: executing ") + path + "\r\n";
  auto finish = std::string("UnholyOS: finished ") + path +
                ", checks 0x000000000000000" + checks + "\r\n";
  auto begin = serial.find(start);
  Check(begin != std::string::npos, "Missing native HolyC source program");
  begin += start.size();
  auto end = serial.find(finish, begin);
  Check(end != std::string::npos,
        "The native HolyC source program or heap cleanup failed");
  Check(serial.substr(begin, end - begin) == expected,
        "Native HolyC source output differed from TempleOS");
}

struct CBootMedia
{
  uint64_t lba, shift, drive, blocks, root, bitmap, kernel_clus, kernel_size,
      kernel_hash;
};

static std::string_view RedSeaName(const Bytes &image, size_t entry);
static size_t RedSeaDirectory(const Bytes &image, uint64_t cluster);

static size_t FindRedSeaEntry(const Bytes &disc, uint64_t directory,
                              std::string_view name)
{
  constexpr size_t blk = 512;
  auto end = RedSeaDirectory(disc, directory);
  for (size_t entry = directory * blk + 128; entry < end; entry += 64) {
    auto found = RedSeaName(disc, entry);
    if (found.empty())
      break;
    if (found == name)
      return entry;
  }
  throw std::runtime_error("Missing RedSea entry " + std::string(name));
}

static void VerifyStartDoc(const Bytes &disc, uint64_t root)
{
  constexpr size_t blk = 512;
  auto entry = FindRedSeaEntry(disc, root, "Doc");
  Check(ReadLE(disc, entry, 2) == 0x810, "Invalid Doc directory attributes");
  auto doc = ReadLE(disc, entry + 40, 8);
  auto file = FindRedSeaEntry(disc, doc, "Start.DD.Z");
  Check(ReadLE(disc, doc * blk + 64 + 40, 8) == root,
        "Doc directory has the wrong parent");
  auto start = ReadLE(disc, file + 40, 8);
  auto size = ReadLE(disc, file + 48, 8);
  Check(ReadLE(disc, file, 2) == 0xC00 && size == 200 &&
            start < disc.size() / blk && size <= disc.size() - start * blk,
        "Invalid Start.DD.Z extent");
  uint64_t hash = 14695981039346656037ULL;
  for (size_t i = 0; i < size; i++)
    hash = (hash ^ disc[start * blk + i]) * 1099511628211ULL;
  Check(hash == 0x0C20EFB983195E6C0ULL, "Start.DD.Z differs from TempleOS");
}

static void VerifyHome(const Bytes &disc, uint64_t root)
{
  constexpr size_t blk = 512;
  auto record = FindRedSeaEntry(disc, root, "Home");
  auto clus = ReadLE(disc, record + 40, 8);
  auto pos = clus * blk;
  auto end = RedSeaDirectory(disc, clus);
  Check(ReadLE(disc, record, 2) == 0x810 &&
            ReadLE(disc, record + 48, 8) == blk && end - pos == blk &&
            RedSeaName(disc, pos) == "Home" &&
            ReadLE(disc, pos + 40, 8) == clus &&
            RedSeaName(disc, pos + 64) == ".." &&
            ReadLE(disc, pos + 64 + 40, 8) == root &&
            RedSeaName(disc, pos + 128).empty(),
        "Invalid empty RedSea Home directory");
}

static CBootMedia VerifyMedia(const fs::path &iso, const fs::path &kernel_path)
{
  constexpr size_t blk = 512, dvd = 4 * blk, primary = 16 * dvd,
                   catalog = 20 * dvd, loader = 21 * dvd, redsea = 88 * blk;
  Bytes disc = FileRead(iso);
  Check(disc.size() % dvd == 0, "CD ends on a partial sector");
  Check(HasBytes(disc, primary,
                 "\x01"
                 "CD001\x01") &&
            HasBytes(disc, primary + 314, "TempleOS RedSea"),
        "Missing RedSea primary descriptor");
  Check(ReadLE(disc, 17 * dvd + 71, 4) == 20, "Invalid El Torito catalog LBA");
  uint16_t sum = 0;
  for (size_t i = 0; i < 32; i += 2)
    sum += ReadLE(disc, catalog + i, 2);
  Check(!sum && HasBytes(disc, catalog + 4, "TempleOS\0"sv) &&
            ReadLE(disc, catalog + 30, 2) == 0xAA55,
        "Invalid El Torito catalog checksum");
  Check(ReadLE(disc, catalog + 32, 8) == 0x0004000000000088 &&
            ReadLE(disc, catalog + 40, 4) == 21,
        "Invalid BIOS boot entry");

  uint64_t drive = ReadLE(disc, redsea + 8, 8);
  uint64_t blocks = ReadLE(disc, redsea + 16, 8);
  uint64_t root = ReadLE(disc, redsea + 24, 8);
  uint64_t bitmap_blocks = ReadLE(disc, redsea + 32, 8);
  Check(disc[redsea + 3] == 0x88 && ReadLE(disc, redsea + 510, 2) == 0xAA55,
        "Invalid RedSea boot block");
  Check(drive == 88 && blocks == disc.size() / blk - drive &&
            bitmap_blocks == CeilDiv(blocks, 4096),
        "Invalid RedSea bounds");
  Check(root == drive + bitmap_blocks + 1 && root < disc.size() / blk,
        "Invalid RedSea root");
  VerifyStartDoc(disc, root);
  VerifyHome(disc, root);

  // TempleOS packs CISOPriDesc and writes the cluster at the record start.
  for (size_t base : {16 * dvd, 18 * dvd}) {
    Check(ReadLE(disc, base + 152, 4) == root,
          "The ISO descriptor root differs from TempleOS");
    uint32_t root_big = 0;
    for (size_t i = 0; i < 4; i++)
      root_big = (root_big << 8) | disc[base + 156 + i];
    Check(root_big == root && ReadLE(disc, base + 160, 4) == 0,
          "The ISO root palindrome is misplaced");
    Check(HasBytes(disc, base + 314, "TempleOS RedSea") &&
              disc[base + 877] == 1 && ReadLE(disc, base + 2044, 4) == 0,
          "The packed ISO descriptor or sector tail differs from TempleOS");
  }

  size_t dir = root * blk + 128;
  Check(ReadLE(disc, dir, 2) == 0x810 &&
            HasBytes(disc, dir + 2, "0000Boot\0"sv),
        "Missing RedSea boot directory");
  uint64_t boot = ReadLE(disc, dir + 40, 8);
  Check(boot < disc.size() / blk, "Invalid boot directory extent");
  size_t entry = boot * blk + 128;
  Check(ReadLE(disc, entry, 2) == 0x800 &&
            HasBytes(disc, entry + 2, "0000Kernel.BIN.C\0"sv),
        "Missing RedSea kernel file");
  uint64_t start = ReadLE(disc, entry + 40, 8);
  uint64_t size = ReadLE(disc, entry + 48, 8);
  Bytes kernel = FileRead(kernel_path);
  Check(start < disc.size() / blk && size <= disc.size() - start * blk &&
            size == kernel.size() && size >= 36,
        "Invalid RedSea kernel extent");
  size_t header = start * blk;
  Check(std::equal(kernel.begin(), kernel.end(), disc.begin() + header),
        "RedSea kernel differs from the built kernel");
  Check(HasBytes(disc, header,
                 "\xEB\x1E\x07\x00"
                 "TOSB"sv) &&
            ReadLE(disc, header + 24, 8) == size && disc[header + 32] == 0xE9,
        "Invalid CBinFile or CKernel header");

  constexpr std::string_view marker = "UNHOLYOS-CD-LBA!";
  std::string_view stage1(reinterpret_cast<const char *>(disc.data() + loader),
                          dvd);
  size_t off = stage1.find(marker);
  Check(off != stage1.npos && off + marker.size() + 8 <= dvd,
        "Missing DVD loader extent");
  off += loader + marker.size();
  uint64_t lba = ReadLE(disc, off, 4), sectors = ReadLE(disc, off + 4, 2),
           shift = ReadLE(disc, off + 6, 2);
  Check(lba == start / 4 && shift == start % 4 &&
            sectors == CeilDiv(size, dvd) + (shift != 0) &&
            lba + sectors <= disc.size() / dvd,
        "Invalid DVD extent");
  uint64_t hash = 14695981039346656037ULL;
  for (uint8_t byte : kernel)
    hash = (hash ^ byte) * 1099511628211ULL;
  return {lba, shift, drive, blocks, root, bitmap_blocks, start, size, hash};
}

class CMonitor
{
public:
  CMonitor(const char *qemu, const fs::path &iso, const fs::path &dir,
           const char *machine, const char *memory, bool secondary_slave,
           const fs::path &disk={}, bool persistent_disk=false)
      : deadline(Clock::now() + 30s)
  {
    std::string image = fs::absolute(iso).string();
    std::string drive = "file=";
    for (char ch : image) {
      drive += ch;
      if (ch == ',')
        drive += ch;
    }
    drive += ",if=ide,index=3,media=cdrom";
    std::string ata="file=";
    if (!disk.empty()) {
      for (char ch:fs::absolute(disk).string()) {
        ata+=ch;
        if (ch==',') ata+=ch;
      }
      ata+=",if=ide,index=0,format=raw,snapshot=";
      ata+=persistent_disk?"off":"on";
    }
    std::string serial = "file:" + (dir / "serial.log").string();
    std::string log = (dir / "qemu.log").string();

    int fd[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd) < 0)
      throw std::runtime_error("Cannot open QEMU monitor socket");
    pid = fork();
    if (pid == 0) {
      close(fd[0]);
      int err = open(log.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
      if (err < 0 || dup2(fd[1], STDIN_FILENO) < 0 ||
          dup2(fd[1], STDOUT_FILENO) < 0 || dup2(err, STDERR_FILENO) < 0)
        _exit(126);
      close(fd[1]);
      close(err);
      if (!disk.empty()) {
        execlp(qemu,qemu,"-machine",machine,"-cpu","qemu64","-m",memory,
               "-boot","d","-cdrom",image.c_str(),"-drive",ata.c_str(),
               "-display","none","-monitor","none","-serial",serial.c_str(),
               "-qmp","stdio","-no-reboot",static_cast<char *>(nullptr));
        _exit(127);
      }
      execlp(qemu, qemu, "-machine", machine, "-cpu", "qemu64", "-m", memory,
             "-boot", "d", secondary_slave ? "-drive" : "-cdrom",
             secondary_slave ? drive.c_str() : image.c_str(), "-display",
             "none", "-monitor", "none", "-serial", serial.c_str(), "-qmp",
             "stdio", "-no-reboot", static_cast<char *>(nullptr));
      _exit(127);
    }

    close(fd[1]);
    if (pid < 0) {
      close(fd[0]);
      throw std::runtime_error("Cannot start QEMU");
    }
    sock = fd[0];
  }

  CMonitor(const CMonitor &) = delete;
  CMonitor &operator=(const CMonitor &) = delete;

  ~CMonitor()
  {
    close(sock);
    if (pid > 0) {
      kill(pid, SIGKILL);
      while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {
      }
    }
  }

  void Connect()
  {
    Check(Receive().find("\"QMP\"") != std::string::npos,
          "Missing QEMU monitor greeting");
    Command(R"({"execute":"qmp_capabilities"})");
  }

  std::string WaitSerial(const fs::path &serial, std::string_view marker)
  {
    while (Clock::now() < deadline) {
      std::string output = ReadText(serial);
      if (output.find(marker) != std::string::npos)
        return output;
      if (output.find(BOOT_MARKER) != std::string::npos)
        throw std::runtime_error("Guest finished before " + std::string(marker));
      if (waitpid(pid, nullptr, WNOHANG) == pid) {
        pid = -1;
        throw std::runtime_error("QEMU stopped before kernel entry");
      }
      std::this_thread::sleep_for(50ms);
    }
    throw std::runtime_error("Timed out waiting for " + std::string(marker));
  }

  std::string Boot(const fs::path &serial)
  {
    return WaitSerial(serial, BOOT_MARKER);
  }

  void ResetDeadline(Clock::duration timeout)
  {
    deadline = Clock::now() + timeout;
  }

  void Quit()
  {
    Command(R"({"execute":"quit"})");
    for (unsigned attempt=0;attempt<200;attempt++) {
      int status=0;
      if (waitpid(pid,&status,WNOHANG)==pid) {
        pid=-1;
        Check(WIFEXITED(status) && WEXITSTATUS(status)==0,"QEMU quit failed");
        return;
      }
      std::this_thread::sleep_for(10ms);
    }
    throw std::runtime_error("QEMU did not exit after quit");
  }

  std::string Hmp(const std::string &command)
  {
    return Command(
        R"({"execute":"human-monitor-command","arguments":{"command-line":")" +
        command + R"("}})");
  }

  std::string Command(std::string request)
  {
    request += '\n';
    size_t sent = 0;
    while (sent < request.size()) {
      ssize_t count = send(sock, request.data() + sent, request.size() - sent,
                           MSG_NOSIGNAL);
      if (count < 0 && errno == EINTR)
        continue;
      Check(count > 0, "Cannot send QEMU monitor command");
      sent += static_cast<size_t>(count);
    }

    for (;;) {
      std::string response = Receive();
      if (response.find("\"error\"") != std::string::npos)
        throw std::runtime_error(response);
      if (response.find("\"return\"") != std::string::npos)
        return response;
    }
  }

private:
  std::string Receive()
  {
    for (;;) {
      size_t end = buffer.find('\n');
      if (end != std::string::npos) {
        std::string line = buffer.substr(0, end);
        buffer.erase(0, end + 1);
        return line;
      }

      auto left =
          std::chrono::ceil<std::chrono::milliseconds>(deadline - Clock::now());
      Check(left > 0ms, "QEMU monitor timed out");
      pollfd input{sock, POLLIN, 0};
      int status = poll(&input, 1, static_cast<int>(left.count()));
      if (status < 0 && errno == EINTR)
        continue;
      Check(status > 0, "QEMU monitor timed out");
      char data[4096];
      ssize_t count = recv(sock, data, sizeof(data), 0);
      if (count < 0 && errno == EINTR)
        continue;
      Check(count > 0, "QEMU closed the monitor connection");
      buffer.append(data, count);
    }
  }

  Clock::time_point deadline;
  std::string buffer;
  pid_t pid;
  int sock;
};

static uint64_t VerifyHandoff(const std::string &output,
                              const CBootMedia &media,
                              const std::string &registers)
{
  const std::regex handoff(
      R"(DVD LBA (0x[0-9a-f]+), source (0x[0-9a-f]+), shift (0x[0-9a-f]+), )"
      R"(E820 entries (0x[0-9a-f]+), physical space (0x[0-9a-f]+), mapped space (0x[0-9a-f]+), )"
      R"(PML4 (0x[0-9a-f]+), heap base (0x[0-9a-f]+), run level (0x[0-9a-f]+))");
  std::smatch match;
  Check(std::regex_search(output, match, handoff), "Missing BIOS DVD handoff");
  std::array<uint64_t, 9> fields;
  for (size_t i = 0; i < fields.size(); i++)
    fields[i] = std::stoull(match[i + 1].str(), nullptr, 16);
  auto [lba, source, shift, entries, physical, mapped, pml4, heap, level] =
      fields;
  Check(lba == media.lba && shift == media.shift && source == 4 &&
            entries > 0 && entries < 48 && level == 0x2FF,
        "Incorrect BIOS DVD handoff");
  Check(mapped == physical + (1ULL << 32) && physical >= (1ULL << 29),
        "Incorrect identity mapped address space");
  uint64_t dirs = CeilDiv(mapped, 1ULL << 30);
  Check(pml4 == 0x102000 + (dirs + CeilDiv(dirs, 512)) * 4096 &&
            heap == pml4 + 4096,
        "Incorrect initial page table layout");
  Check(registers.find("CS64") != std::string::npos &&
            std::regex_search(registers, match,
                              std::regex(R"(CR3=([0-9a-f]+))")) &&
            std::stoull(match[1].str(), nullptr, 16) == pml4,
        "QEMU registers disagree with kernel long mode handoff");
  Check(registers.find("GS =0060 0000000000100200") != std::string::npos,
        "GS does not point to the bootstrap CPU");
  Check(registers.find("FS =0060 0000000000100300") != std::string::npos,
        "FS does not point to Adam");
  return heap;
}

template <size_t Count>
static std::array<uint64_t, Count> ReadPhysical(CMonitor &vm, uint64_t addr,
                                                char width = 'g')
{
  std::ostringstream command;
  command << "xp /" << Count << width << "x 0x" << std::hex << addr;
  std::string reply = vm.Hmp(command.str());
  const std::regex hex(R"(0x([0-9a-f]+))");
  std::array<uint64_t, Count> values;
  auto token = std::sregex_iterator(reply.begin(), reply.end(), hex);
  for (auto &value : values) {
    Check(token != std::sregex_iterator(), "QEMU physical memory read failed");
    value = std::stoull((*token)[1].str(), nullptr, 16);
    ++token;
  }
  Check(token == std::sregex_iterator(), "QEMU physical memory read overflow");
  return values;
}

static void VerifyUncachedAlias(CMonitor &vm, const std::string &output)
{
  constexpr uint64_t four_gib = 1ULL << 32, page = 1ULL << 21, pml2 = 0x102000;
  const std::regex alias_report(
      R"(mapped space (0x[0-9a-f]+).*uncached alias (0x[0-9a-f]+))");
  std::smatch match;
  Check(std::regex_search(output, match, alias_report),
        "Missing uncached alias");
  uint64_t mapped = std::stoull(match[1].str(), nullptr, 16);
  uint64_t alias = std::stoull(match[2].str(), nullptr, 16);
  Check(alias == mapped - four_gib && alias >= four_gib && alias % page == 0,
        "Incorrect uncached alias base");

  constexpr uint64_t ad = 0x60; // Accessed and dirty bits set by the CPU.
  uint64_t entry = pml2 + (alias / page) * sizeof(uint64_t);
  Check((ReadPhysical<1>(vm, entry - sizeof(uint64_t))[0] & ~ad) ==
            ((alias - page) | 0x87),
        "Uncached alias overlaps the identity map");
  for (uint64_t i = 0; i < four_gib / page; i += 64) {
    auto rows = ReadPhysical<64>(vm, entry + i * sizeof(uint64_t));
    for (uint64_t j = 0; j < rows.size(); j++)
      Check((rows[j] & ~ad) == (((i + j) * page) | 0x197),
            "Incorrect uncached page mapping");
  }
}

struct CTestE820
{
  uint64_t base, size, type;
};

static std::vector<CTestE820> ReadE820(CMonitor &vm)
{
  std::vector<CTestE820> entries;
  for (uint64_t i = 0; i < 48; i++) {
    auto words = ReadPhysical<5>(vm, 0x7C44 + i * 20, 'w');
    uint64_t type = words[4] & 255;
    if (!type)
      break;
    entries.push_back(
        {words[0] | (words[1] << 32), words[2] | (words[3] << 32), type});
  }
  return entries;
}

static uint64_t VerifyDeviceRanges(CMonitor &vm, const std::string &output,
                                   const std::vector<CTestE820> &e820)
{
  struct Range
  {
    uint64_t base, end, type;
  };
  std::vector<Range> expected{{0xE0000000, 0xF0000000, 0}};
  for (auto [base, size, type] : e820) {
    auto free =
        std::find_if(expected.begin(), expected.end(), [&](Range range) {
          return !range.type && base >= range.base && base <= range.end &&
                 size <= range.end - base;
        });
    if (free == expected.end())
      continue;

    Range whole = *free;
    free = expected.erase(free);
    if (base != whole.base)
      free = expected.insert(free, {whole.base, base, 0}) + 1;
    free = expected.insert(free, {base, base + size, type}) + 1;
    if (base + size != whole.end)
      expected.insert(free, {base + size, whole.end, 0});
  }

  const std::regex line(R"(device ranges (0x[0-9a-f]+))");
  std::smatch match;
  Check(std::regex_search(output, match, line), "Missing device range list");
  uint64_t head = std::stoull(match[1].str(), nullptr, 16);
  auto links = ReadPhysical<2>(vm, head);
  uint64_t prev = head, next = links[0];
  for (auto range : expected) {
    Check(next && next != head, "Device range list ends early");
    auto node = ReadPhysical<5>(vm, next);
    Check(node[1] == prev && node[2] == range.type && node[3] == range.base &&
              node[4] == range.end - range.base,
          "Invalid 32-bit device range");
    prev = next;
    next = node[0];
  }
  Check(next == head && links[1] == prev, "Device range list does not close");
  return expected.size() * 0x38;
}

static void VerifyKernelGlobals(CMonitor &vm, const std::string &output)
{
  const std::regex globals(
      R"(device ranges (0x[0-9a-f]+), cache line (0x[0-9a-f]+), focus (0x[0-9a-f]+), macro queue (0x[0-9a-f]+), reverse bits (0x[0-9a-f]+), set bits (0x[0-9a-f]+), ext slots (0x[0-9a-f]+), pow10 (0x[0-9a-f]+))");
  std::smatch match;
  Check(std::regex_search(output, match, globals), "Missing kernel globals");
  std::array<uint64_t, 8> values;
  for (size_t i = 0; i < values.size(); i++)
    values[i] = std::stoull(match[i + 1].str(), nullptr, 16);
  auto [ranges, line, focus, macro, reverse, set, ext, pow] = values;
  Check(line == 64 && focus == 0x100300, "Wrong cache line or focus task");
  uint64_t pci = ranges + 0x28;
  Check(ReadPhysical<2>(vm, macro) == std::array<uint64_t, 2>{macro, macro} &&
            ReadPhysical<2>(vm, pci) == std::array<uint64_t, 2>{pci, pci},
        "Invalid macro or PCI queue");
  Check(ReadPhysical<5>(vm, ext) == std::array<uint64_t, 5>{},
        "Kernel extension slots were not cleared");

  auto rev = ReadPhysical<32>(vm, reverse);
  auto counts = ReadPhysical<32>(vm, set);
  for (unsigned i = 0; i < 256; i++) {
    unsigned expected = 0;
    for (unsigned n = 0; n < 8; n++)
      expected = (expected << 1) | ((i >> n) & 1);
    unsigned shift = (i % 8) * 8;
    Check(((rev[i / 8] >> shift) & 255) == expected &&
              ((counts[i / 8] >> shift) & 255) ==
                  static_cast<unsigned>(std::popcount(i)),
          "Invalid bit table");
  }

  // FNV-1a over the pinned guest's 618 binary64 slots, in memory order.
  uint64_t hash = 14695981039346656037ULL;
  auto add = [&](const auto &rows) {
    for (uint64_t value : rows)
      for (unsigned i = 0; i < 8; i++) {
        hash = (hash ^ (value & 255)) * 1099511628211ULL;
        value >>= 8;
      }
  };
  for (unsigned i = 0; i < 576; i += 64)
    add(ReadPhysical<64>(vm, pow + i * 8));
  add(ReadPhysical<42>(vm, pow + 576 * 8));
  Check(hash == 0x751D732DD7097C40,
        "Decimal power table differs from TempleOS");
}

static std::string StopAtIdle(CMonitor &vm)
{
  for (unsigned attempt = 0; attempt < 30; attempt++) {
    std::this_thread::sleep_for(5ms);
    vm.Command(R"({"execute":"stop"})");
    auto registers = vm.Hmp("info registers");
    if (registers.find("HLT=1") != std::string::npos)
      return registers;
    vm.Command(R"({"execute":"cont"})");
  }
  throw std::runtime_error("Kernel did not halt between timer interrupts");
}

static void VerifyInterrupts(CMonitor &vm, const std::string &output,
                             const std::string &registers, bool startup)
{
  const std::regex report(R"(IDT (0x[0-9a-f]+))");
  const std::regex idtr(R"(IDT=\s*([0-9a-f]+)\s+([0-9a-f]+))");
  const std::regex flags(R"(RFL=([0-9a-f]+))");
  std::smatch match;
  Check(std::regex_search(output, match, report), "Missing interrupt table");
  uint64_t base = std::stoull(match[1].str(), nullptr, 16);
  Check(base && std::regex_search(registers, match, idtr) &&
            std::stoull(match[1].str(), nullptr, 16) == base &&
            std::stoull(match[2].str(), nullptr, 16) == 0xFFF,
        "Incorrect interrupt table register");
  Check(std::regex_search(registers, match, flags) &&
            (std::stoull(match[1].str(), nullptr, 16) & 0x200),
        "The idle kernel has maskable interrupts disabled");

  constexpr uint64_t attrs = 0x8E0000400000;
  constexpr uint64_t attrs_mask = 0xFFFFFFFF0000;
  std::array<uint64_t, 256> targets;
  for (unsigned i = 0; i < 256; i += 32) {
    auto gates = ReadPhysical<64>(vm, base + i * 16);
    for (unsigned j = 0; j < 32; j++) {
      uint64_t lo = gates[2 * j], hi = gates[2 * j + 1];
      Check((lo & attrs_mask) == attrs && !(hi >> 32),
            "Invalid ring-0 interrupt gate");
      targets[i + j] = (hi << 32) | ((lo >> 32) & 0xFFFF0000) | (lo & 0xFFFF);
      Check(targets[i + j] != 0, "Interrupt gate has a null target");
    }
  }

  uint64_t stubs = targets[1] - 7, fallback = targets[0x22];
  const std::array<uint64_t, 8> handlers{fallback, targets[0], targets[2],
                                         targets[0x20], targets[0x21], targets[0x2C],
                                         targets[0x30], targets[0x31]};
  auto distinct = handlers;
  std::sort(distinct.begin(), distinct.end());
  Check(std::adjacent_find(distinct.begin(), distinct.end()) == distinct.end(),
        "Dedicated interrupt gates share a handler");
  for (unsigned vector = 0; vector < 256; vector++) {
    bool uses_stub =
        (vector > 0 && vector < 0x20 && vector != 2) || vector == 0x32;
    bool dedicated = vector == 0 || vector == 2 || vector == 0x20 ||
                     vector == 0x21 || vector == 0x2C || vector == 0x30 || vector == 0x31;
    if (uses_stub)
      Check(targets[vector] == stubs + vector * 7, "Incorrect fault gate");
    else if (!dedicated)
      Check(targets[vector] == fallback, "Incorrect default interrupt gate");
  }
  uint64_t fault = 0;
  for (unsigned vector : {0U, 1U, 13U, 0x32U, 255U}) {
    uint64_t addr = stubs + vector * 7;
    uint64_t stub = ReadPhysical<1>(vm, addr)[0];
    Check((stub & 0xFFFFFF) == (0xE9006A | (vector << 8)),
          "Incorrect fault stub encoding");
    uint64_t target = addr + 7 + static_cast<int32_t>(stub >> 24);
    if (vector == 0)
      fault = target;
    Check(target && target == fault, "Incorrect fault stub destination");
  }

  auto pic = vm.Hmp("info pic");
  Check(std::regex_search(
            pic, std::regex(R"(pic0: [^\\]*imr=f8 [^\\]*irq_base=20)")) &&
            std::regex_search(
                pic, std::regex(R"(pic1: [^\\]*imr=ef [^\\]*irq_base=28)")),
        "PIC vector offsets or masks differ from boot configuration");
  const std::regex timer(R"(counters (0x[0-9a-f]+), timer read (0x[0-9a-f]+))");
  Check(std::regex_search(output, match, timer), "Missing PIT timer state");
  auto counters = ReadPhysical<2>(vm, std::stoull(match[1].str(), nullptr, 16));
  uint64_t timer_read = std::stoull(match[2].str(), nullptr, 16);
  uint64_t ticks = counters[0] - 1;
  auto idle_ticks = ReadPhysical<1>(vm, 0x100220)[0];
  auto swaps = ReadPhysical<1>(vm, 0x100250)[0];
  auto adam_swaps = ReadPhysical<1>(vm, 0x1004B8)[0];
  Check(counters[0] >= 3 && counters[1] == ticks * 1193 &&
            timer_read <= counters[1] + 1193 &&
            ReadPhysical<1>(vm, 0x100230)[0] == ticks && idle_ticks <= ticks &&
            swaps >= ticks - idle_ticks && adam_swaps <= swaps,
        "PIT timer and task restoration counters disagree");
  if (startup)
    Check(idle_ticks > 0, "The scheduler did not idle while tasks slept");
  else
    Check(adam_swaps>=3 && swaps>adam_swaps,
          "PIT did not restore Adam and the window manager");
}

static void VerifyClock(CMonitor &vm, const std::string &output,
                        uint32_t host_bridge, bool hpet_enabled)
{
  const std::regex report(
      R"(counters (0x[0-9a-f]+), timer read 0x[0-9a-f]+, )"
      R"(PCI directory (0x[0-9a-f]+), PCI service (0x[0-9a-f]+), )"
      R"(PCI host (0x[0-9a-f]+), HPET read (0x[0-9a-f]+))");
  std::smatch match;
  Check(std::regex_search(output, match, report),
        "Missing PCI and clock state");
  uint64_t counters = std::stoull(match[1].str(), nullptr, 16);
  uint64_t directory = std::stoull(match[2].str(), nullptr, 16);
  uint64_t service = std::stoull(match[3].str(), nullptr, 16);
  uint64_t pci_host = std::stoull(match[4].str(), nullptr, 16);
  uint64_t hpet_read = std::stoull(match[5].str(), nullptr, 16);
  Check(directory >= 0xE0000 && directory <= 0xFFFF0 && !(directory & 3) &&
            service >= 0xE0000 && service < 0x100000 && pci_host == host_bridge,
        "Incorrect BIOS32 PCI service or host bridge ID");
  auto header = ReadPhysical<16>(vm, directory, 'b');
  uint64_t checksum = 0;
  for (uint64_t byte : header)
    checksum += byte;
  Check(header[0] == '_' && header[1] == '3' && header[2] == '2' &&
            header[3] == '_' && header[9] == 1 && (checksum & 255) == 0,
        "Invalid SeaBIOS service directory header");

  auto values = ReadPhysical<6>(vm, counters + 16);
  Check(values[0] == 2676302000 && values[1] == 2676302 &&
            values[2] == 2676302000 &&
            !ReadPhysical<1>(vm, counters + 64, 'b')[0],
        "The first TimeCal changed the default TSC frequency");
  if (hpet_enabled) {
    uint64_t period = ReadPhysical<1>(vm, 0xFED00000)[0] >> 32;
    uint64_t config = ReadPhysical<1>(vm, 0xFED00010)[0];
    uint64_t now = ReadPhysical<1>(vm, 0xFED000F0)[0];
    Check(period > 100000 && period < 1000000000 && (config & 1) &&
              values[3] == 1000000000000000ULL / period &&
              values[4] == 1000000000000ULL / period && values[5] < hpet_read &&
              hpet_read <= now,
          "HPET did not start with the advertised period");
  } else {
    Check(!values[3] && !values[4] && !values[5] && !hpet_read,
          "The clock reported HPET on a machine with HPET disabled");
  }
}

static void VerifyBootDrive(CMonitor &vm, const std::string &output,
                            const CBootMedia &media, bool ide, bool slave)
{
  uint64_t drive = ReadPhysical<1>(vm, 0x100368)[0];
  if (!ide) {
    Check(!drive &&
              output.find("Could not mount the boot DVD") != std::string::npos,
          "Expected the AHCI boot to have no guest IDE drive");
    return;
  }
  const std::regex report(
      R"(RedSea drive (0x[0-9a-f]+), port (0x[0-9a-f]+), unit (0x[0-9a-f]+), )"
      R"(root (0x[0-9a-f]+), kernel cluster (0x[0-9a-f]+), size (0x[0-9a-f]+), )"
      R"(hash (0x[0-9a-f]+))");
  std::smatch match;
  Check(std::regex_search(output, match, report),
        "The guest did not read the kernel file from the boot DVD");
  std::array<uint64_t, 7> fields;
  for (size_t i = 0; i < fields.size(); i++)
    fields[i] = std::stoull(match[i + 1].str(), nullptr, 16);
  Check(drive && fields ==
                     std::array<uint64_t, 7>{
                         drive, 0x170, slave, media.root, media.kernel_clus,
                         media.kernel_size, media.kernel_hash},
        "The guest read the wrong RedSea file or DVD drive");

  auto drv = ReadPhysical<14>(vm, drive);
  Check(drv[1] == 0x0001005456535644 && drv[2] == media.drive &&
            drv[3] == media.blocks && drv[6] == media.drive + 1 &&
            drv[7] == drv[6] && drv[8] == media.root &&
            drv[9] == media.drive + media.bitmap && drv[10] == 1 && drv[13],
        "Invalid mounted RedSea drive record");
  auto bd = ReadPhysical<6>(vm, drv[13]);
  auto geometry = ReadPhysical<3>(vm, drv[13] + 44);
  Check(bd[2] == 0x0000000556534442 && (bd[3] & 7) == 7 &&
            ((bd[3] >> 40) & 255) == slave && (bd[4] & 0xFFFFFFFF) == 0x170 &&
            (bd[5] & 0xFFFFFFFF) == 2048 && geometry[0] == media.drive &&
            geometry[2] == media.drive + media.blocks - 1,
        "Invalid ATAPI block device record");
}

constexpr uint64_t late_used = 0x2828, fault_used = 0x7E8;
constexpr uint64_t drive_pages = 7 + 9, drive_alloced = drive_pages * 512;
constexpr uint64_t drive_used = drive_alloced - 2 * 24 + 40 + 40 + 32 + 24;

static uint64_t VerifyKernelSymbols(CMonitor &vm, uint64_t table_body,
                                    uint64_t owner, uint64_t &graphics,
                                    uint64_t &winmgr_slot)
{
  uint64_t used = 0, count = 0;
  bool kernel = false, hash_del = false, hash_src = false, put_str = false;
  bool colors = false, rainbow = false, os_name = false, file_util = false;
  for (uint64_t i = 0; i < 1024; i += 64) {
    auto buckets = ReadPhysical<64>(vm, table_body + i * 8);
    for (size_t j = 0; j < buckets.size(); j++) {
      for (uint64_t addr = buckets[j]; addr;) {
        Check(++count <= 1024 && addr >= 16, "Bad boot hash chain");
        auto entry = ReadPhysical<5>(vm, addr - 16);
        auto type = static_cast<uint32_t>(entry[4]);
        bool module = type == 0x01004000;
        bool define = type == 4;
        Check((module || define || type == 0x08000001) && entry[0] == owner &&
                  entry[1] == (module ? 64u : define ? 104u : 88u) &&
                  entry[3] >= 16,
              "Invalid boot symbol allocation");
        auto name = ReadPhysical<2>(vm, entry[3] - 16);
        Check(name[0] == owner && name[1] >= 24 && name[1] <= 80 &&
                  !(name[1] & 7),
              "Invalid boot symbol name allocation");
        std::string str;
        bool ended = false;
        for (uint64_t k = 0; k < name[1] - 16 && !ended; k += 8) {
          uint64_t bytes = ReadPhysical<1>(vm, entry[3] + k)[0];
          for (unsigned b = 0; b < 8; b++, bytes >>= 8) {
            if (!(bytes & 255)) {
              ended = true;
              break;
            }
            str.push_back(static_cast<char>(bytes));
          }
        }
        Check(ended && !str.empty() &&
                  ((str.size() + 24) & ~size_t{7}) == name[1],
              "Invalid boot symbol name");
        uint64_t hash = 0;
        for (unsigned char ch : str)
          hash = std::rotl(hash, 1) + ch;
        hash += (hash >> 16) + ((hash >> 15) & 1);
        Check((hash & 1023) == i + j, "Boot symbol in the wrong bucket");
        if (define) {
          bool color_list = str == "ST_COLORS";
          bool rainbow_list = str == "ST_RAINBOW_10";
          bool startup_name = str == "DD_OS_NAME_VERSION";
          bool file_util_list = str == "ST_FILE_UTIL_FLAGS";
          Check(color_list || rainbow_list || startup_name || file_util_list,
                "Unexpected boot definition");
          constexpr char color_data[] =
              "BLACK\0BLUE\0GREEN\0CYAN\0RED\0PURPLE\0BROWN\0LTGRAY\0DKGRAY\0"
              "LTBLUE\0LTGREEN\0LTCYAN\0LTRED\0LTPURPLE\0YELLOW\0WHITE\0";
          constexpr char rainbow_data[] =
              "BLACK\0BROWN\0RED\0LTRED\0YELLOW\0GREEN\0BLUE\0PURPLE\0LTGRAY\0WHITE\0";
          constexpr char os_name_data[] = "UnholyOS\0";
          constexpr char file_util_data[] =
              "r\0d\0i\0a\0c\0R\0p\0m\0x\0s\0D\0F\0T\0$\0S\0A\0J\0G\0Z\0O\0P\0f\0l\0lb\0la\0";
          auto def = ReadPhysical<11>(vm, addr);
          uint64_t entries = color_list ? 16 : rainbow_list ? 10 : file_util_list ? 25 : 1;
          uint64_t data_size = color_list     ? sizeof(color_data)
                               : rainbow_list ? sizeof(rainbow_data)
                               : file_util_list ? sizeof(file_util_data)
                                              : sizeof(os_name_data);
          Check(def[10] == entries && def[3] >= 16 && def[8] >= 16 &&
                    def[9] >= 16,
                "Invalid boot definition");
          const auto link = ReadPhysical<2>(vm, def[3] - 16);
          const auto data = ReadPhysical<2>(vm, def[8] - 16);
          const auto index = ReadPhysical<2>(vm, def[9] - 16);
          Check(link == std::array<uint64_t, 2>{owner, 24} &&
                    data == std::array<uint64_t, 2>{owner,
                                                    (data_size + 23) & ~7ULL} &&
                    index == std::array<uint64_t, 2>{owner, 16 + entries * 8},
                "Invalid boot definition allocation");
          used += link[1] + data[1] + index[1];
          colors |= color_list;
          rainbow |= rainbow_list;
          os_name |= startup_name;
          file_util |= file_util_list;
        }
        kernel |= module && str == "/Kernel/Kernel";
        hash_del |= !module && str == "HashDel";
        hash_src |= !module && str == "HashSrcFileSet";
        put_str |= !module && str == "PUT_STR";
        if (type == 0x08000001 && str == "gr")
          graphics = ReadPhysical<1>(vm, addr + 64)[0];
        if (type == 0x08000001 && str == "sys_winmgr_task")
          winmgr_slot = ReadPhysical<1>(vm, addr + 64)[0];
        used += entry[1] + name[1];
        addr = entry[2];
      }
    }
  }
  Check(kernel && hash_del && hash_src && put_str && colors && rainbow &&
            os_name && file_util && graphics && winmgr_slot,
        "Missing boot symbols");
  return used;
}

using BlockRanges = std::vector<std::pair<uint64_t, uint64_t>>;

struct CTestHeap
{
  BlockRanges ranges;
  uint64_t alloced, used;
};

static CTestHeap VerifyHeap(CMonitor &vm, uint64_t heap, uint64_t pool,
                            uint64_t task)
{
  auto hc = ReadPhysical<12>(vm, heap);
  constexpr uint64_t page = 512;
  const uint64_t head = heap + 0x30;
  Check(hc[0] == pool && hc[1] == 0x56536348 && !hc[2] && hc[5] == task &&
            hc[4] <= hc[3] && hc[10] == heap + 0x48 && hc[11] == hc[10],
        "Invalid task heap controller");
  CTestHeap result{{}, hc[3], hc[4]};
  uint64_t prev = head, bytes = 0;
  bool mergeable = !hc[8], malloc_free = !hc[9];
  for (uint64_t addr = hc[6]; addr != head;) {
    Check(addr && !(addr & (page - 1)) && bytes < result.alloced,
          "Task heap block outside its allocation");
    auto block = ReadPhysical<3>(vm, addr);
    uint64_t size = (block[2] >> 32) * page;
    Check(block[1] == prev && static_cast<uint32_t>(block[2]) == 0x7355424D &&
              size && size <= result.alloced - bytes,
          "Invalid task heap block");
    result.ranges.emplace_back(addr, addr + size);
    bytes += size;
    mergeable |= addr == hc[8];
    malloc_free |= hc[9] >= addr + 24 && hc[9] <= addr + size - 16 && !(hc[9] & 7);
    prev = addr;
    addr = block[0];
  }
  Check(bytes == result.alloced && prev == hc[7] && mergeable && malloc_free,
        "Invalid task heap accounting or links");
  return result;
}

static void VerifyCoverage(BlockRanges blocks, const BlockRanges &ranges)
{
  std::sort(blocks.begin(), blocks.end());
  auto block = blocks.begin();
  for (auto [start, end] : ranges) {
    while (start < end) {
      Check(block != blocks.end() && block->first == start && block->second <= end,
            "Block pool has a gap or overlap");
      start = block++->second;
    }
  }
  Check(block == blocks.end(), "Block pool has pages outside its ranges");
}

static void ReadCachedFreePages(CMonitor &vm, uint64_t pool,
                                BlockRanges &ranges)
{
  constexpr uint64_t page = 512, slots = 256;
  constexpr uint64_t hash = 0x28, hash2 = hash + slots * 8;
  std::set<uint64_t> seen;
  auto read = [&](uint64_t head, size_t slot, bool large) {
    for (auto addr = head; addr;) {
      Check(!(addr & (page - 1)) && seen.insert(addr).second,
            "Invalid cached free page link");
      auto block = ReadPhysical<3>(vm, addr);
      uint64_t pages = block[2] >> 32;
      Check(static_cast<uint32_t>(block[2]) == 0x6E55424D && pages &&
                (large ? pages >= slots &&
                             size_t(std::bit_width(pages - 2) - 1) == slot
                       : pages == slot),
            "Invalid cached free page block");
      ranges.emplace_back(addr, addr + pages * page);
      addr = block[0];
    }
  };
  for (size_t i = 0; i < slots; i += 64) {
    auto bucket = ReadPhysical<64>(vm, pool + hash + 8 * i);
    for (size_t j = 0; j < bucket.size(); j++)
      read(bucket[j], i + j, false);
  }
  auto large = ReadPhysical<55>(vm, pool + hash2);
  for (size_t i = 0; i < large.size(); i++)
    read(large[i], i, true);
}

static uint64_t VerifyGraphicsContexts(CMonitor &vm, uint64_t graphics,
                                       uint64_t owner)
{
  const auto contexts = ReadPhysical<8>(vm, graphics);
  auto z_buf = ReadPhysical<1>(vm, graphics + 72)[0];
  Check(z_buf >= 16 &&
            ReadPhysical<2>(vm, z_buf - 16) ==
                std::array<uint64_t, 2>{owner, 19 * 512 - 24} &&
            ReadPhysical<1>(vm, z_buf)[0] == 0x0001000100010001 &&
            ReadPhysical<1>(vm, z_buf + (2 * 80 + 1) * 2, 'h')[0] == 3 &&
            ReadPhysical<1>(vm, z_buf + (2 * 80 + 41) * 2, 'h')[0] == 2 &&
            ReadPhysical<1>(vm, z_buf + 9592)[0] == 0x0002000200020002,
        "Invalid startup window depth buffer");
  Check(ReadPhysical<1>(vm, graphics + 10480)[0] == 1,
        "Invalid initial graphics zoom");
  constexpr uint64_t flags[] = {0, 0x24000, 0x4000, 0x4000, 0, 0x4000};
  auto bitmap = ReadPhysical<1>(vm, graphics + 88)[0];
  Check(bitmap >= 16 &&
            ReadPhysical<2>(vm, bitmap - 16) ==
                std::array<uint64_t, 2>{owner, 17 * 512 - 24} &&
            ReadPhysical<1>(vm, bitmap)[0] == 14,
        "Invalid startup window coverage bitmap");
  uint64_t used = (19 + 17) * 512 - 48;
  auto text_base = ReadPhysical<1>(vm, graphics + 64)[0];
  auto vga_text_cache = ReadPhysical<1>(vm, graphics + 104)[0];
  Check(text_base >= 16 && vga_text_cache >= 16 &&
            ReadPhysical<2>(vm, text_base - 16) ==
                std::array<uint64_t, 2>{owner, 38 * 512 - 24} &&
            ReadPhysical<2>(vm, vga_text_cache - 16) ==
                std::array<uint64_t, 2>{owner, 19 * 512 - 24} &&
            (ReadPhysical<1>(vm, text_base)[0] & 0xFFFFFFFF) == 0x0F55,
        "Invalid graphics text layer");
  used += (38 + 19) * 512 - 48;
  const auto tables = ReadPhysical<9>(vm, graphics + 10408);
  Check(!tables[0], "Unexpected zoom level zero");
  for (size_t zoom = 1; zoom < tables.size(); zoom++) {
    uint64_t size = 256 * zoom + 16;
    if (size >= 1024)
      size = CeilDiv(size + 24, 512) * 512 - 24;
    Check(tables[zoom] >= 16 &&
              ReadPhysical<2>(vm, tables[zoom] - 16) ==
                  std::array<uint64_t, 2>{owner, size} &&
              (ReadPhysical<1>(vm, tables[zoom])[0] & 0xFFFF) ==
                  (((1ULL << zoom) - 1) << 8),
          "Invalid screen zoom table");
    used += size;
  }
  for (size_t i = 0; i < std::size(flags); ++i) {
    uint64_t dc = contexts[i + 2];
    Check(dc >= 16 && std::count(contexts.begin(), contexts.end(), dc) == 1,
          "Missing or duplicate graphics device context");
    Check(ReadPhysical<4>(vm, dc) ==
              std::array<uint64_t, 4>{0, 0, (640ULL << 32) | 640,
                                      (flags[i] << 32) | 480},
          "Invalid graphics device context geometry");
    auto matrix = ReadPhysical<1>(vm, dc + 200)[0];
    auto body = ReadPhysical<1>(vm, dc + 384)[0];
    Check(matrix >= 16 && body >= 16, "Missing graphics buffer");
    auto context_alloc = ReadPhysical<2>(vm, dc - 16);
    auto matrix_alloc = ReadPhysical<2>(vm, matrix - 16);
    auto body_alloc = ReadPhysical<2>(vm, body - 16);
    Check(context_alloc == std::array<uint64_t, 2>{owner, 424} &&
              matrix_alloc == std::array<uint64_t, 2>{owner, 144} &&
              body_alloc == std::array<uint64_t, 2>{owner, 1026 * 512 - 24},
          "Invalid graphics device context allocations");
    auto first = ReadPhysical<1>(vm, body)[0];
    bool frame = i == 0 || i == 2;
    auto expected = (i == 1             ? UINT64_MAX
                     : i == 3 || i == 4 ? 0x000F0F00000F0F00ULL
                                        : 0);
    Check(ReadPhysical<1>(vm, matrix)[0] == 1ULL << 32 &&
              (frame ? (first & 0xFF) == 0x66 : first == expected) &&
              ReadPhysical<1>(vm, body + 640 * 480 - 8)[0] ==
                  (i == 1 ? UINT64_MAX : 0),
          "Invalid graphics device context initialization");
    used += context_alloc[1] + matrix_alloc[1] + body_alloc[1];
  }
  // Each diameter has four pen contexts with their own matrices and bodies.
  for (uint64_t diameter = 0; diameter < 64; diameter++) {
    const auto stride = (diameter + 7) & ~7ULL;
    uint64_t body = stride * diameter + 16;
    if (body >= 1024)
      body = CeilDiv(body + 24, 512) * 512 - 24;
    used += 4 * (424 + 144 + body);
  }
  return used;
}

static void VerifyPools(CMonitor &vm, uint64_t heap, uint64_t ram_end,
                        const std::string &output, uint64_t range_bytes,
                        uint64_t symbol_used,
                        const std::vector<CTestE820> &e820, uint64_t idle,
                        uint64_t graphics, uint64_t winmgr, uint64_t left,
                        uint64_t right)
{
  constexpr uint64_t adam = 0x100300, adam_hc = 0x101188, code_pool = 0x1007A8;
  constexpr uint64_t code_base = 0x1000000, page = 512;
  auto high_e801 = ReadPhysical<1>(vm, 0x7C42, 'h')[0];
  uint64_t bios = ram_end + (high_e801 << 16), usable = 0;
  for (auto entry : e820)
    if (entry.type == 1)
      usable += entry.size;
  uint64_t total = std::max(bios, usable);
  uint64_t limit = total > 0x100000000  ? 0x80000000
                   : total > 0x80000000 ? total / 4
                                        : total;
  uint64_t pool = 0, data_bytes = 0;
  BlockRanges data_ranges;
  if (limit < total) {
    for (auto entry : e820) {
      if (entry.type != 1)
        continue;
      uint64_t base = std::max(entry.base, limit),
               end = entry.base + entry.size;
      if (base >= end)
        continue;
      data_bytes += end - base;
      if (!pool) {
        pool = base;
        base = (base + 0x9E0 + page - 1) & ~(page - 1);
      }
      data_ranges.emplace_back(base, end);
    }
  }

  const uint64_t expected_data_hc = pool ? limit - 3 * page + 0x28 : adam_hc;
  auto adam_heaps = ReadPhysical<2>(vm, adam + 0x20);
  Check(adam_heaps == std::array<uint64_t, 2>{adam_hc, expected_data_hc},
        "Invalid Adam heap selection");
  const uint64_t srv = adam + 0x3D8;
  const auto waiting = ReadPhysical<2>(vm, srv);
  Check(waiting[0] > 16 && waiting[0] != srv && waiting[0] == waiting[1],
        "Missing the initial mouse position message");
  auto initial = ReadPhysical<14>(vm, waiting[0]);
  Check(initial[0] == srv && initial[1] == srv && initial[2] == srv &&
            initial[3] == 1 && initial[5] == 4 && initial[9] == 320 &&
            initial[10] == 240 && !initial[13],
        "Invalid initial mouse position message");
  auto allocation = ReadPhysical<2>(vm, waiting[0] - 16);
  Check(allocation[0] == expected_data_hc && allocation[1] == 0x80,
        "Invalid mouse message allocation");
  const uint64_t graphics_used =
      VerifyGraphicsContexts(vm, graphics, expected_data_hc);
  // The first terminal lexer loads the shared assembler table into Adam.
  constexpr uint64_t asm_hash_used = 0x10518;
  std::smatch raw_screen;
  Check(std::regex_search(output,raw_screen,
                         std::regex(R"(raw screen (0x[0-9a-f]+))")),
        "Missing raw screen image");
  const uint64_t raw=std::stoull(raw_screen[1].str(),nullptr,16);
  constexpr uint64_t raw_used=76*page-24;
  Check(raw && ReadPhysical<2>(vm,raw-16)==
                   std::array<uint64_t,2>{expected_data_hc,raw_used},
        "Invalid raw screen image allocation");
  const uint64_t boot_used = range_bytes + late_used + symbol_used +
                             drive_used + allocation[1] + graphics_used +
                             asm_hash_used + raw_used;
  auto code = VerifyHeap(vm, adam_hc, code_pool, adam);
  uint64_t code_used = 0x42840 + fault_used + (pool ? 0x5E8 : boot_used);
  uint64_t data_used = boot_used, data_alloced = 0;
  BlockRanges data_used_ranges;
  for (auto task : {idle, winmgr, left, right}) {
    auto heaps = ReadPhysical<2>(vm, task + 0x20);
    auto task_allocation = ReadPhysical<2>(vm, task - 16);
    auto code_hc = ReadPhysical<2>(vm, heaps[0] - 16);
    Check(task_allocation == std::array<uint64_t, 2>{adam_hc, 3 * page - 24} &&
              code_hc[0] == expected_data_hc &&
              (heaps[0] != heaps[1]) == bool(pool),
          "Startup task or heap has the wrong owner");
    auto task_code = VerifyHeap(vm, heaps[0], code_pool, task);
    code_used += task_allocation[1];
    code.alloced += task_code.alloced;
    code.ranges.insert(code.ranges.end(), task_code.ranges.begin(),
                       task_code.ranges.end());
    if (pool) {
      auto data_hc = ReadPhysical<2>(vm, heaps[1] - 16);
      auto task_data = VerifyHeap(vm, heaps[1], pool, task);
      Check(!task_code.alloced && data_hc[0] == expected_data_hc &&
                task_data.alloced > 256 * 1024,
            "Invalid startup task data heap usage");
      data_used += code_hc[1] + data_hc[1];
      data_alloced += task_data.alloced;
      data_used_ranges.insert(data_used_ranges.end(), task_data.ranges.begin(),
                              task_data.ranges.end());
    } else {
      Check(task_code.alloced > 256 * 1024,
            "A startup task stack is missing from the code heap");
      code_used += code_hc[1];
    }
  }
  if (code.used != code_used)
    throw std::runtime_error("Adam code heap used " + std::to_string(code.used) +
                             " bytes, expected " + std::to_string(code_used));
  uint64_t code_bytes = ram_end - heap + limit - code_base;
  Check(ReadPhysical<4>(vm, code_pool) ==
            std::array<uint64_t, 4>{0, code_bytes, code.alloced, code_base},
        "Invalid code block pool");
  auto high_free = ReadPhysical<3>(vm, code_base);
  auto low_free = ReadPhysical<3>(vm, heap);
  Check(high_free[0] == heap && !low_free[0] &&
            static_cast<uint32_t>(high_free[2]) == 0x6E55424D &&
            static_cast<uint32_t>(low_free[2]) == 0x6E55424D,
        "Invalid code pool free list");
  code.ranges.emplace_back(heap, heap + (low_free[2] >> 32) * page);
  code.ranges.emplace_back(code_base, code_base + (high_free[2] >> 32) * page);
  ReadCachedFreePages(vm, code_pool, code.ranges);
  VerifyCoverage(std::move(code.ranges), {{heap, ram_end}, {code_base, limit}});

  if (pool) {
    auto data = VerifyHeap(vm, expected_data_hc, pool, adam);
    Check(data.used == data_used, "Invalid Adam data heap usage");
    data.ranges.insert(data.ranges.end(), data_used_ranges.begin(),
                       data_used_ranges.end());
    uint64_t prev = 0;
    for (auto [base, end] : data_ranges) {
      auto block = ReadPhysical<3>(vm, base);
      uint64_t free_end = base + (block[2] >> 32) * page;
      Check(block[0] == prev && static_cast<uint32_t>(block[2]) == 0x6E55424D &&
                free_end <= end,
            "Invalid data pool free list");
      prev = base;
      data.ranges.emplace_back(base, free_end);
    }
    Check(ReadPhysical<4>(vm, pool) ==
              std::array<uint64_t, 4>{
                  0, data_bytes, data.alloced + data_alloced, prev},
          "Invalid data block pool");
    ReadCachedFreePages(vm, pool, data.ranges);
    VerifyCoverage(std::move(data.ranges), data_ranges);
  }

  const std::regex pools(
      R"(heap limit (0x[0-9a-f]+), data pool (0x[0-9a-f]+))");
  std::smatch match;
  Check(std::regex_search(output, match, pools) &&
            std::stoull(match[1].str(), nullptr, 16) == limit - 1 &&
            std::stoull(match[2].str(), nullptr, 16) == pool,
        "Incorrect heap limit or data pool");
}

static void VerifyFixedArea(CMonitor &vm, uint64_t heap,
                            const std::string &registers,
                            const std::string &output)
{
  constexpr uint64_t fixed = 0x100000, boot_cpu = fixed + 0x200,
                     adam = fixed + 0x300;
  uint64_t e801 = ReadPhysical<1>(vm, 0x7C40, 'h')[0];
  uint64_t ram_end = fixed + e801 * 1024;
  Check(heap < ram_end, "Code pool starts beyond E801 RAM");
  uint64_t top = ram_end - 0x4400;
  const auto e820 = ReadE820(vm);
  uint64_t range_bytes = VerifyDeviceRanges(vm, output, e820);
  uint64_t owner = ReadPhysical<1>(vm, adam + 0x28)[0];
  uint64_t graphics = 0, winmgr_slot = 0;
  uint64_t symbol_used =
      VerifyKernelSymbols(vm, top + 0x28, owner, graphics, winmgr_slot);
  uint64_t winmgr = ReadPhysical<1>(vm, winmgr_slot)[0];
  uint64_t left = ReadPhysical<1>(vm, winmgr + 0x88)[0];
  uint64_t right = ReadPhysical<1>(vm, adam + 0x80)[0];
  Check(winmgr && winmgr != adam && ReadPhysical<1>(vm, winmgr)[0] == winmgr &&
            ReadPhysical<3>(vm, winmgr + 0x78) ==
                std::array<uint64_t, 3>{adam, adam, left} &&
            ReadPhysical<1>(vm, adam + 0x88)[0] == winmgr &&
            ReadPhysical<1>(vm, winmgr + 0x1B8)[0] > 0,
        "Missing window manager task or broken task queue");
  Check(left && right && left != right &&
            ReadPhysical<3>(vm, left + 0x78) ==
                std::array<uint64_t, 3>{adam, winmgr, right} &&
            ReadPhysical<3>(vm, right + 0x78) ==
                std::array<uint64_t, 3>{adam, left, adam},
        "Invalid startup terminal task queue");
  for (auto task : {left, right}) {
    auto docs = ReadPhysical<3>(vm, task + 0x30);
    Check(ReadPhysical<1>(vm, task)[0] == task &&
              (ReadPhysical<1>(vm, task + 0x18, 'w')[0] & 0x218) == 0x218 &&
              docs[0] && docs[0] == docs[1] && docs[2] && docs[2] != docs[0],
          "Missing startup terminal prompt or documents");
  }
  Check(ReadPhysical<1>(vm, fixed, 'h')[0] == 0x37F,
        "Missing initial FPU image");
  auto cpu = ReadPhysical<14>(vm, boot_cpu);
  Check(cpu[0] == boot_cpu && !cpu[1] && cpu[5] == 0x3F847AE147AE147C &&
            cpu[7] == adam && cpu[8] && cpu[8] != adam &&
            cpu[12] == boot_cpu + 0x60 && cpu[13] == cpu[12],
        "Invalid bootstrap CPU record");
  uint64_t idle = cpu[8];
  auto idle_task = ReadPhysical<4>(vm, idle);
  Check(idle_task[0] == idle && static_cast<uint32_t>(idle_task[1]) == 0x536B7354 &&
            static_cast<uint32_t>(idle_task[3]) == 8 &&
            ReadPhysical<3>(vm, idle + 0x78) == std::array<uint64_t, 3>{adam, idle, idle} &&
            ReadPhysical<1>(vm, idle + 0x1A8)[0] == boot_cpu,
        "Invalid idle task record");
  VerifyPools(vm, heap, ram_end, output, range_bytes, symbol_used, e820, idle,
              graphics, winmgr, left, right);

  constexpr uint64_t task_alloced = 0x44800;
  uint64_t used_block = ram_end - task_alloced;
  uint64_t stk = used_block + 0x28;
  Check(ReadPhysical<1>(vm, adam)[0] == adam &&
            ReadPhysical<1>(vm, adam + 8)[0] == 0x0052F3FF536B7354,
        "Invalid Adam task header");
  auto saved = ReadPhysical<2>(vm, adam + 0x120);
  Check((saved[0] & 0x200) && saved[1] > stk + 24 && saved[1] <= top &&
            ReadPhysical<1>(vm, adam + 0x310)[0] == stk &&
            ReadPhysical<3>(vm, stk) ==
                std::array<uint64_t, 3>{0, top - stk - 24, 0},
        "Invalid Adam stack record");
  std::smatch match;
  Check(std::regex_search(registers, match, std::regex(R"(RSP=([0-9a-f]+))")),
        "Missing kernel stack pointer");
  uint64_t rsp = std::stoull(match[1].str(), nullptr, 16);
  Check(rsp > stk + 24 && rsp <= top, "Kernel is not running on Adam's stack");

  uint64_t cur_dir = ram_end - 0x260, fpu = ram_end - 0x210;
  uint64_t hash = ram_end - 0x248;
  Check(ReadPhysical<1>(vm, adam + 0x70)[0] == cur_dir &&
            (ReadPhysical<1>(vm, cur_dir)[0] & 0xFFFFFFFFFFFF) == 0x656D6F482F,
        "Invalid Adam working directory");
  Check(ReadPhysical<1>(vm, adam + 0x1B0)[0] == fpu &&
            ReadPhysical<1>(vm, fpu, 'h')[0] == 0x37F,
        "Invalid Adam FPU image");
  Check(ReadPhysical<1>(vm, adam + 0x3D0)[0] == hash &&
            ReadPhysical<4>(vm, hash) ==
                std::array<uint64_t, 4>{0, 1023, 0, top + 0x28},
        "Invalid Adam hash table");

  auto idle_hash = ReadPhysical<1>(vm, idle + 0x3D0)[0];
  Check(ReadPhysical<1>(vm, idle_hash)[0] == hash,
        "Idle task does not inherit Adam's hash table");
  std::vector<uint64_t> slots;
  for (auto cached = ReadPhysical<1>(vm, owner + 0x98)[0]; cached;) {
    Check(!(cached & 7) &&
              std::find(slots.begin(), slots.end(), cached) == slots.end(),
          "Invalid cached 56-byte heap slot chain");
    auto slot = ReadPhysical<2>(vm, cached);
    Check(slot[1] == 56, "Invalid cached 56-byte heap slot size");
    slots.push_back(cached);
    cached = slot[0];
  }
}

static Bytes ReadVGA(const fs::path &path)
{
  std::ifstream file(path, std::ios::binary);
  std::string magic;
  int width, height, depth;
  Check(bool(file >> magic >> width >> height >> depth) && magic == "P6" &&
            width == 640 && height == 480 && depth == 255 && file.get() == '\n',
        "QEMU did not enter 640x480 VGA mode");
  Bytes rgb(640 * 480 * 3);
  Check(bool(file.read(reinterpret_cast<char *>(rgb.data()), rgb.size())),
        "The VGA screenshot is incomplete");
  return rgb;
}

static void VerifyVGA(const fs::path &path)
{
  const auto rgb = ReadVGA(path);
  Check(std::any_of(rgb.begin(), rgb.end(), [](uint8_t byte) { return byte; }),
        "The VGA screenshot is blank");
}

static void MouseMove(CMonitor &vm, int x, int y)
{
  vm.Command(
      R"({"execute":"input-send-event","arguments":{"events":[{"type":"rel","data":{"axis":"x","value":)" +
      std::to_string(x) + R"(}},{"type":"rel","data":{"axis":"y","value":)" +
      std::to_string(y) + R"(}}]}})");
}

static void MouseButton(CMonitor &vm, const char *name, bool down)
{
  vm.Command(
      std::string(
          R"({"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"button":")") +
      name + R"(","down":)" + (down ? "true" : "false") + R"(}}]}})");
}

static void KeyboardKey(CMonitor &vm, const char *name, bool down)
{
  vm.Command(
      std::string(
          R"({"execute":"input-send-event","arguments":{"events":[{"type":"key","data":{"key":{"type":"qcode","data":")") +
      name + R"("},"down":)" + (down ? "true" : "false") + R"(}}]}})");
}

static void DriveSymmetry(CMonitor &vm, const fs::path &screen)
{
  auto wait_frame = [&](auto ready, const char *error) {
    for (unsigned i = 0; i < 100; i++) {
      vm.Command(R"({"execute":"screendump","arguments":{"filename":")" +
                 screen.string() + R"("}})");
      if (ready(ReadVGA(screen)))
        return;
      std::this_thread::sleep_for(25ms);
    }
    throw std::runtime_error(error);
  };
  auto pixel = [](const Bytes &rgb, int x, int y) {
    auto pos = (y * 640 + x) * 3;
    return uint32_t(rgb[pos]) << 16 | uint32_t(rgb[pos + 1]) << 8 |
           rgb[pos + 2];
  };
  auto button = [&](const char *name, bool down) {
    MouseButton(vm, name, down);
    std::this_thread::sleep_for(125ms);
  };
  enum class Stage
  {
    DefaultAxis,
    NewAxis,
    HorizontalPair,
    SingleStroke
  };
  auto strokes = [&](const Bytes &rgb, Stage stage) {
    for (int y = 150; y <= 286; y++)
      for (int x = 174; x <= 466; x++) {
        auto color = pixel(rgb, x, y);
        if (stage >= Stage::NewAxis && y == 224 && 181 <= x && x <= 284) {
          if (color < 0x800000 || (color & 0xFFFF))
            return false;
          continue;
        }
        if ((stage >= Stage::NewAxis && x == 180 && y == 224) ||
            (stage == Stage::SingleStroke && x == 356 && y == 168)) {
          if (color >= 0x800000 || (color & 0xFFFF) != 0xFFFF)
            return false;
          continue;
        }
        bool ink =
            ((x == 180 || x == 460) && y >= 152 && y <= 224) ||
            (stage >= Stage::HorizontalPair && (y == 168 || y == 280) &&
             x >= 284 && x <= 356) ||
            (stage == Stage::SingleStroke && x == 406 && y >= 208 && y <= 252);
        if (color != (ink ? 0 : 0xFFFFFF))
          return false;
      }
    return true;
  };

  button("left", true);
  MouseMove(vm, 0, 144);
  wait_frame([&](const Bytes &rgb) { return strokes(rgb, Stage::DefaultAxis); },
             "Symmetry.HC did not mirror the stroke across the default axis");
  button("left", false);

  button("right", true);
  MouseMove(vm, 208, 0);
  wait_frame([&](const Bytes &rgb) { return strokes(rgb, Stage::NewAxis); },
             "Symmetry.HC did not show its red axis");
  button("right", false);
  wait_frame([&](const Bytes &rgb) { return strokes(rgb, Stage::NewAxis); },
             "Symmetry.HC did not retain its red axis");

  MouseMove(vm, 0, -112);
  std::this_thread::sleep_for(125ms);
  button("left", true);
  MouseMove(vm, 144, 0);
  wait_frame(
      [&](const Bytes &rgb) { return strokes(rgb, Stage::HorizontalPair); },
      "Symmetry.HC did not mirror the stroke across the new axis");
  button("left", false);

  button("right", true);
  button("right", false);
  MouseMove(vm, 100, 80);
  std::this_thread::sleep_for(125ms);
  button("left", true);
  MouseMove(vm, 0, 88);
  wait_frame(
      [&](const Bytes &rgb) { return strokes(rgb, Stage::SingleStroke); },
      "Symmetry.HC did not disable symmetry after a stationary click");
  button("left", false);

  vm.Hmp("sendkey esc 60");
  wait_frame(
      [&](const Bytes &rgb) {
        for (int y = 150; y <= 286; y++)
          for (int x = 174; x <= 466; x++)
            if (pixel(rgb, x, y) != 0xFFFFFF)
              return false;
        return true;
      },
      "Symmetry.HC left strokes on VGA after Escape");
}

static void DriveDigits(CMonitor &vm, const fs::path &dir,
                        const std::string &boot)
{
  vm.ResetDeadline(30s);
  std::smatch match;
  Check(std::regex_search(boot, match, std::regex(R"(text base (0x[0-9a-f]+))")),
        "Missing boot text layer address");
  auto addr = std::stoull(match[1].str(), nullptr, 16);
  auto path = dir / "digits-text.bin";
  auto key = [&](std::string name) {
    vm.Hmp("sendkey " + name + " 35");
    std::this_thread::sleep_for(65ms);
  };
  auto type_cmd = [&](std::string_view cmd) {
    for (char ch : cmd) {
      if (ch >= 'A' && ch <= 'Z')
        key("shift-" + std::string(1, char(ch + 'a' - 'A')));
      else {
        switch (ch) {
        case '(':
          key("shift-9");
          break;
        case ')':
          key("shift-0");
          break;
        case ':':
          key("shift-semicolon");
          break;
        case ';':
          key("semicolon");
          break;
        case '"':
          key("shift-apostrophe");
          break;
        case '/':
          key("slash");
          break;
        case '.':
          key("dot");
          break;
        case ',':
          key("comma");
          break;
        default:
          key(std::string(1, ch));
          break;
        }
      }
    }
    key("ret");
  };
  Bytes cells;
  auto read = [&] {
    vm.Command(R"({"execute":"pmemsave","arguments":{"val":)" +
               std::to_string(addr) + R"(,"size":19200,"filename":")" +
               path.string() + R"("}})");
    cells = FileRead(path);
    Check(cells.size() == 80 * 60 * 4, "Incomplete Digits text layer");
  };
  auto line = [&](size_t y) {
    std::string result;
    for (size_t x = 1; x < 79; ++x)
      result += char(cells[(y * 80 + x) * 4]);
    return result;
  };
  auto find = [&](std::string_view wanted) {
    for (size_t y = 2; y < 59; ++y) {
      auto x = line(y).find(wanted);
      if (x != std::string::npos)
        return (y * 80 + x + 1) * 4;
    }
    return size_t{0};
  };
  auto wait = [&](std::string_view wanted, std::string_view also = {}) {
    for (unsigned i = 0; i < 160; ++i) {
      read();
      if (find(wanted) && (also.empty() || find(also)))
        return;
      std::this_thread::sleep_for(25ms);
    }
    std::string visible;
    for (size_t y = 2; y < 30; ++y) {
      for (char ch : line(y))
        visible += ch >= 32 && ch < 127 ? ch : ' ';
      visible += '\n';
    }
    throw std::runtime_error("Digits missing " + std::string(wanted) + "\n" +
                             visible);
  };
  auto snapshot = [&](const char *phase) {
    fs::copy_file(path, dir / (std::string("digits-") + phase + ".bin"),
                  fs::copy_options::overwrite_existing);
  };
  constexpr uint8_t rainbow[]{0, 6, 4, 12, 14, 2, 1, 5, 7, 15};
  auto check_digits = [&](size_t start, std::string_view pattern) {
    for (size_t i = 0; i < pattern.size(); ++i) {
      Check(cells[start + i * 4] == uint8_t(pattern[i]),
            "Digits pattern differs");
      Check(cells[start + i * 4 + 1] == (0x80 | rainbow[pattern[i] - '0']),
            "Digits pattern color differs");
    }
  };
  auto check_pattern = [&](std::string_view pattern) {
    auto length = find("Length:" + std::to_string(pattern.size()));
    Check(length != 0 && cells[length + 1] == 0x8F, "Digits length differs");
    check_digits(length + 2 * 80 * 4, pattern);
  };
  auto check_hidden = [&](size_t count) {
    auto length = find("Length:" + std::to_string(count));
    Check(length && !find("Pattern"), "Digits still displays the pattern");
    for (size_t i = 0; i < count; ++i) {
      auto ch = cells[length + 2 * 80 * 4 + i * 4];
      Check(ch < '0' || ch > '9', "Digits did not hide the answer");
    }
  };
  wait("T:/Home>");
  constexpr uint64_t adam = 0x100300, last_task = 0x88;
  const auto winmgr = ReadPhysical<1>(vm, adam + last_task)[0];
  const auto task = ReadPhysical<1>(vm, winmgr + last_task)[0];
  const auto doc = ReadPhysical<1>(vm, task + 0x30)[0];
  auto wait_prompt = [&] {
    for (unsigned i = 0; i < 160; ++i) {
      if ((ReadPhysical<1>(vm, task + 0x18, 'w')[0] & 0x218) == 0x218)
        return;
      std::this_thread::sleep_for(25ms);
    }
    throw std::runtime_error("Digits terminal did not reach its command prompt");
  };
  wait_prompt();
  Check(ReadPhysical<1>(vm, doc + 104)[0] == doc,
        "Digits terminal cursor is not at the command prompt");
  type_cmd("WinMax;");
  wait_prompt();
  constexpr std::array<uint64_t, 4> maximized{1, 78, 2, 58};
  bool resized = false;
  for (unsigned i = 0; i < 160; ++i) {
    if (ReadPhysical<4>(vm, task + 0x48) == maximized) {
      resized = true;
      break;
    }
    std::this_thread::sleep_for(25ms);
  }
  Check(resized, "Digits terminal did not maximize");
  wait_prompt();
  type_cmd("DocClear(,1);Seed(12345);");
  wait("ans=0x00003039=12345", "T:/Home>");
  wait_prompt();
  const auto before = cells[find("T:/Home>") + 1];
  type_cmd(R"(ExeFile("::/Demo/Games/Digits.HC");)");
  wait("PRESS A KEY", "colored based on electrical engineering color codes.");
  snapshot("intro");
  const auto intro = find("This is a memory game.  Try to remember the digits "
                          "and enter them.  They are");
  Check(intro && find("colored based on electrical engineering color codes.") ==
                      intro + 80 * 4,
        "Digits introduction did not wrap at the reference word boundary");
  constexpr const char *names[]{"BLACK", "BROWN", "RED", "LTRED", "YELLOW",
                                "GREEN", "BLUE", "PURPLE", "LTGRAY", "WHITE"};
  for (size_t i = 0; i < std::size(names); ++i) {
    auto legend = std::to_string(i) + ": " + names[i];
    auto pos = find(legend);
    Check(pos == intro + (i + 2) * 80 * 4, "Digits rainbow legend differs");
    for (size_t j = 0; j < legend.size(); ++j)
      Check(cells[pos + 4 * j + 1] == (0x80 | rainbow[i]),
            "Digits legend colors differ");
  }
  key("spc");
  wait("Pattern", "PRESS A KEY");
  check_pattern("4");
  snapshot("pattern1");
  key("spc");
  wait("Guess", "Length:1");
  check_hidden(1);
  snapshot("guess1");
  key("4");
  wait("Pattern", "Length:2");
  check_pattern("40");
  snapshot("pattern2");
  key("spc");
  wait("Guess", "Length:2");
  check_hidden(2);
  snapshot("guess2");
  key("4");
  key("1");
  wait("Score:2", "PRESS A KEY");
  const auto score = find("Score:2");
  check_digits(score - 80 * 4, "41");
  check_digits(score + 80 * 4, "40");
  snapshot("score");
  key("spc");
  wait("Pattern", "Length:1");
  check_pattern("9");
  snapshot("restart");
  key("spc");
  wait("Guess", "Length:1");
  key("esc");
  wait("T:/Home>");
  wait_prompt();
  Check(!find("Guess") && cells[find("T:/Home>") + 1] == before,
        "Digits did not restore the terminal");
  snapshot("exit");
  type_cmd(R"(ExeFile("::/Demo/Games/Digits.HC");)");
  wait("PRESS A KEY");
  key("shift-esc");
  wait("T:/Home>");
  wait_prompt();
  Check(!find("PRESS A KEY") && cells[find("T:/Home>") + 1] == before,
        "Digits did not restore the terminal after Shift-Escape");
  std::cout << "Digits.HC matched seeded patterns, score, restart and Escape\n";
}

static void DriveUserTask(CMonitor &vm, const fs::path &dir,
                          const std::string &boot)
{
  vm.ResetDeadline(15s);
  std::smatch match;
  Check(std::regex_search(boot, match, std::regex(R"(text base (0x[0-9a-f]+))")),
        "Missing boot text layer address");
  auto addr = std::stoull(match[1].str(), nullptr, 16);
  auto path = dir / "terminal-text.bin";
  auto wait_text = [&](std::string_view wanted, bool right_pane = false,
                       size_t count = 1, size_t row = 60,
                       size_t column = std::string_view::npos,
                       unsigned attempts = 60) {
    for (unsigned attempt = 0; attempt < attempts; ++attempt) {
      vm.Command(R"({"execute":"pmemsave","arguments":{"val":)" +
                 std::to_string(addr) + R"(,"size":19200,"filename":")" +
                 path.string() + R"("}})");
      auto data = FileRead(path);
      Check(data.size() == 80 * 60 * 4, "Incomplete terminal text layer");
      size_t seen = 0;
      for (size_t y = 0; y < 60; ++y) {
        if (row != 60 && y != row)
          continue;
        std::string line;
        for (size_t x = right_pane ? 40 : 0; x < (right_pane ? 80 : 40); ++x)
          line += char(data[(y * 80 + x) * 4]);
        auto position = line.find(wanted);
        if (position != std::string::npos &&
            (column == std::string::npos || position == column) &&
            ++seen == count)
          return;
      }
      std::this_thread::sleep_for(20ms);
    }
    throw std::runtime_error(std::string(right_pane ? "Right" : "Left") +
                             " terminal missing " + std::string(wanted));
  };
  auto key = [&](const char *name) {
    vm.Hmp("sendkey " + std::string(name) + " 25");
    std::this_thread::sleep_for(48ms);
  };
  auto keys = [&](std::initializer_list<const char *> line) {
    for (auto *name : line)
      key(name);
  };
  auto type_cmd = [&](std::string_view cmd) {
    for (char ch : cmd) {
      if ('A' <= ch && ch <= 'Z') {
        auto key_name = "shift-" + std::string(1, char(ch + ('a' - 'A')));
        key(key_name.c_str());
      } else {
        switch (ch) {
        case '(':
          key("shift-9");
          break;
        case ')':
          key("shift-0");
          break;
        case '-':
          key("minus");
          break;
        case '<':
          key("shift-comma");
          break;
        case '>':
          key("shift-dot");
          break;
        case '+':
          key("shift-equal");
          break;
        case '{':
          key("shift-bracket_left");
          break;
        case '}':
          key("shift-bracket_right");
          break;
        case '%':
          key("shift-5");
          break;
        case '\\':
          key("backslash");
          break;
        case ' ':
          key("spc");
          break;
        case '_':
          key("shift-minus");
          break;
        case ';':
          key("semicolon");
          break;
        case '=':
          key("equal");
          break;
        case ':':
          key("shift-semicolon");
          break;
        case '"':
          key("shift-apostrophe");
          break;
        case '/':
          key("slash");
          break;
        case '.':
          key("dot");
          break;
        case ',':
          key("comma");
          break;
        case '?':
          key("shift-slash");
          break;
        case '*':
          key("shift-8");
          break;
        default: {
          const char name[]{ch, 0};
          key(name);
          break;
        }
        }
      }
    }
    key("ret");
  };
  auto wait_mem = [&](uint64_t field, uint64_t value, const char *error) {
    for (unsigned i = 0; i < 80; ++i) {
      if (ReadPhysical<1>(vm, field)[0] == value)
        return;
      std::this_thread::sleep_for(20ms);
    }
    throw std::runtime_error(error);
  };

  constexpr auto start_body = "Public Domain Operating System";
  for (bool right_pane : {false, true}) {
    wait_text("UnholyOS", right_pane, 1, 2, 16);
    wait_text(start_body, right_pane, 1, 4, 5);
    wait_text("Help & Index, Quick Start: Cmd line", right_pane, 1, 6, 1);
    wait_text("Directory of T:/Home", right_pane, 1, 8, 1);
    wait_text("DATE_ TIME_ SIZE", right_pane, 1, 9, 1);
    wait_text("0200 .", right_pane, 1, 10);
    wait_text("0000 ..", right_pane, 1, 11);
    wait_text("T:/Home>", right_pane, 1, 12, 1);
  }
  keys({"3", "5", "shift-equal", "8", "backspace", "7", "semicolon", "ret"});
  wait_text("ans=0x0000002A=42");
  keys({"6", "shift-8", "ret", "8", "semicolon", "ret"});
  wait_text("ans=0x00000030=48");
  keys({"3", "8", "semicolon", "left", "left", "delete", "2", "right", "ret"});
  wait_text("ans=0x00000020=32");
  keys({"x", "semicolon", "ret"});
  wait_text("Syntax Error");
  keys({"shift-apostrophe", "shift-2", "equal", "shift-5", "s", "backslash",
        "n", "shift-apostrophe", "comma", "shift-g", "e", "t", "shift-s", "t",
        "r", "shift-9", "shift-0", "semicolon", "ret"});
  keys({"a", "b", "backspace", "c", "ret"});
  wait_text("@=ac");

  constexpr uint64_t adam = 0x100300, win_left = 0x48, last_task = 0x88;
  auto winmgr = ReadPhysical<1>(vm, adam + last_task)[0];
  auto left = ReadPhysical<1>(vm, winmgr + last_task)[0];
  auto right = ReadPhysical<1>(vm, left + last_task)[0];
  auto wait_windows = [&](const std::array<uint64_t, 4> &left_geometry,
                          const std::array<uint64_t, 4> &right_geometry) {
    std::array<uint64_t, 4> actual_left{}, actual_right{};
    for (unsigned i = 0; i < 80; ++i) {
      actual_left = ReadPhysical<4>(vm, left + win_left);
      actual_right = ReadPhysical<4>(vm, right + win_left);
      if (actual_left == left_geometry && actual_right == right_geometry)
        return;
      std::this_thread::sleep_for(20ms);
    }
    auto bounds = [](const std::array<uint64_t, 4> &box) {
      return std::to_string(int64_t(box[0])) + "," +
             std::to_string(int64_t(box[1])) + "," +
             std::to_string(int64_t(box[2])) + "," +
             std::to_string(int64_t(box[3]));
    };
    throw std::runtime_error(
        "Terminal bounds expected " + bounds(left_geometry) + " / " +
        bounds(right_geometry) + ", got " + bounds(actual_left) + " / " +
        bounds(actual_right));
  };
  wait_windows({1, 39, 2, 58}, {41, 78, 2, 58});
  type_cmd("WinMax;");
  wait_mem(left + win_left + 8, 78, "WinMax did not resize the focused task");
  type_cmd("WinTileHorz;");
  wait_windows({1, 78, 2, 29}, {1, 78, 31, 58});
  type_cmd("WinTileVert;");
  wait_windows({1, 39, 2, 58}, {41, 78, 2, 58});

  vm.ResetDeadline(35s);
  constexpr std::array<uint64_t, 4> right_pane_geometry{41, 78, 2, 58};
  type_cmd("WinHorz(4,32);WinVert(5,32);");
  wait_windows({4, 32, 5, 32}, right_pane_geometry);
  wait_text("MENU", false, 1, 4, 4);
  wait_text("[X]", false, 1, 4, 30);
  auto set_pointer = [&](int x, int y, bool right_pane = false) {
    auto marker = x * 1000 + y;
    type_cmd("MsSet(" + std::to_string(x) + "," + std::to_string(y) + ");" +
             std::to_string(marker) + ";");
    std::ostringstream reply;
    reply << "ans=0x" << std::uppercase << std::hex << std::setw(8)
          << std::setfill('0') << marker;
    wait_text(reply.str(), right_pane);
  };
  auto move_pointer = [&](int64_t &mouse_x, int64_t &mouse_y, int64_t x,
                          int64_t y) {
    while (mouse_x != x || mouse_y != y) {
      auto dx = std::clamp(x - mouse_x, int64_t(-80), int64_t(80));
      auto dy = std::clamp(y - mouse_y, int64_t(-80), int64_t(80));
      MouseMove(vm, dx * 2, dy * 2);
      std::this_thread::sleep_for(125ms);
      mouse_x += dx;
      mouse_y += dy;
    }
  };
  auto drag = [&](int dx, int dy, const std::array<uint64_t, 4> &expected) {
    MouseButton(vm, "left", true);
    std::this_thread::sleep_for(125ms);
    MouseMove(vm, dx, dy);
    wait_windows(expected, right_pane_geometry);
    MouseButton(vm, "left", false);
    std::this_thread::sleep_for(125ms);
  };
  set_pointer(163, 35);
  type_cmd("WinFocus(Fs->last_task);");
  wait_mem(winmgr + last_task, right,
           "WinFocus did not raise the right terminal");
  drag(48, 32, {7, 35, 7, 34});
  wait_mem(winmgr + last_task, left,
           "Title drag did not raise the left terminal");
  MouseMove(vm, 16, 16);
  std::this_thread::sleep_for(125ms);
  wait_windows({7, 35, 7, 34}, right_pane_geometry);
  set_pointer(51, 99);
  drag(32, 32, {9, 37, 9, 36});
  // The outer pixels of the corner are clear of the reference scroll thumb.
  set_pointer(311, 303);
  drag(48, 64, {9, 40, 9, 40});
  set_pointer(331, 115);
  drag(-32, 32, {9, 38, 9, 40});
  set_pointer(163, 331);
  drag(16, -48, {9, 38, 9, 37});
  std::cout << "Window borders moved and resized the terminal\n";
  type_cmd("WinTileVert;");
  wait_windows({1, 39, 2, 58}, right_pane_geometry);

  vm.ResetDeadline(15s);
  type_cmd("WinFocus(Fs->last_task);");
  wait_mem(winmgr + last_task, right,
           "WinFocus did not raise the right terminal");
  keys({"7", "shift-8", "8", "semicolon", "ret"});
  wait_text("ans=0x00000038=56", true);
  set_pointer(620, 12, true);
  wait_text("[X]", true, 1, 1, 36);
  MouseButton(vm, "left", true);
  std::this_thread::sleep_for(125ms);
  MouseButton(vm, "left", false);
  wait_mem(winmgr + last_task, left, "Title-bar close did not close the right terminal");
  std::cout << "Title-bar close dismissed the right terminal\n";
  // The window manager restores focus on its next refresh.
  std::this_thread::sleep_for(150ms);
  keys({"7", "shift-8", "9", "semicolon", "ret"});
  wait_text("ans=0x0000003F=63");
  type_cmd("Type(\"::/Doc/Start.DD\");");
  wait_text(start_body, false, 2);
  wait_text("ans=0x00000001=1");
  type_cmd("Dir;");
  wait_text("Directory of T:/Home", false, 2);
  wait_text("ans=0x00000002=2");
  type_cmd("Dir(\"::/Doc/St?rt.DD*\",1);");
  wait_text("Directory of T:/Doc");
  wait_text("__DATE__ __TIME__ SIZE");
  wait_text("00C8");
  wait_text("Start.DD.Z");

  vm.ResetDeadline(14s);
  type_cmd("WinMax;");
  wait_mem(left + win_left + 8, 78,
           "WinMax did not prepare the graphics demos");
  auto screen = dir / "graphics-demo.ppm";
  auto shape_pixels = [&](Bytes &shape) {
    vm.Command(R"({"execute":"screendump","arguments":{"filename":")" +
               screen.string() + R"("}})");
    const auto rgb = ReadVGA(screen);
    size_t count = 0;
    for (size_t i = 120 * 640; i < shape.size(); i++) {
      auto red = rgb[3 * i], green = rgb[3 * i + 1], blue = rgb[3 * i + 2];
      bool primary = (red == 0 && green == 0 && blue >= 128) ||
                     (red == 0 && blue == 0 && green >= 128) ||
                     (green == 0 && blue == 0 && red >= 128);
      shape[i] = primary ? red | green | blue : 0;
      count += primary;
    }
    return count;
  };
  constexpr struct
  {
    const char *name;
    size_t min_pixels, prompts;
  } demos[]{{"3DPoly.HC", 1200, 2}, {"Box.HC", 350, 1}};
  for (const auto &[demo, min_pixels, prompts] : demos) {
    vm.ResetDeadline(14s);
    type_cmd("DocClear(,1);");
    type_cmd(std::string("ExeFile(\"::/Demo/Graphics/") + demo + "\");");
    Bytes shape(640 * 480), first_shape;
    bool moved = false;
    for (unsigned i = 0; i < 100; i++) {
      if (shape_pixels(shape) > min_pixels) {
        if (first_shape.empty())
          first_shape = shape;
        else if (shape != first_shape) {
          moved = true;
          break;
        }
      }
      std::this_thread::sleep_for(26ms);
    }
    if (!moved)
      throw std::runtime_error(std::string(demo) + " did not animate on VGA");
    key("q");
    wait_text("T:/Home>", false, prompts);
    bool cleared = false;
    for (unsigned i = 0; i < 60; i++) {
      if (!shape_pixels(shape)) {
        cleared = true;
        break;
      }
      std::this_thread::sleep_for(26ms);
    }
    if (!cleared)
      throw std::runtime_error(std::string(demo) + " left pixels on VGA");
    std::cout << demo << " animated and returned to the prompt\n";
  }

  vm.ResetDeadline(27s);
  constexpr uint64_t win_inhibit = 0xC;
  const auto old_inhibit = ReadPhysical<1>(vm, left + win_inhibit, 'w');
  type_cmd("DocClear(,1);");
  type_cmd("MsSet(180,152);ms.show=FALSE;");
  type_cmd(R"(ExeFile("::/Demo/Graphics/Symmetry.HC");)");
  wait_mem(left + win_left, 0, "Symmetry.HC did not remove its border");
  wait_mem(left + win_left + 8, 79, "Symmetry.HC did not maximize the window");
  DriveSymmetry(vm, screen);
  wait_text("T:/Home>");
  wait_mem(left + win_left, 1, "Symmetry.HC did not restore the window");
  Check(ReadPhysical<1>(vm, left + win_inhibit, 'w') == old_inhibit,
        "Symmetry.HC did not restore the task's input masks");
  type_cmd("ms.show=TRUE;");
  std::cout
      << "Symmetry.HC drew both reflections, disabled symmetry, and exited\n";

  vm.ResetDeadline(24s);
  type_cmd("DocClear(,1);");
  type_cmd("MsSet(8,0);ms.show=FALSE;");
  type_cmd(R"(ExeFile("::/Demo/PullDownMenu.HC");)");
  wait_text("A pull-down menu will appear", false, 1, 60,
            std::string_view::npos, 200);
  auto wait_menu = [&](int x, int y, std::array<uint8_t, 3> color) {
    for (unsigned i = 0; i < 60; i++) {
      vm.Command(R"({"execute":"screendump","arguments":{"filename":")" +
                 screen.string() + R"("}})");
      const auto rgb = ReadVGA(screen);
      if (std::equal(color.begin(), color.end(),
                     rgb.begin() + (y * 640 + x) * 3))
        return;
      std::this_thread::sleep_for(25ms);
    }
    throw std::runtime_error("PullDownMenu.HC menu highlight differs");
  };
  constexpr std::array<uint8_t, 3> yellow{255, 255, 87};
  wait_menu(38, 3, yellow);
  MouseButton(vm, "left", true);
  std::this_thread::sleep_for(125ms);
  MouseMove(vm, 176, 36);
  wait_menu(126, 18, yellow);
  MouseButton(vm, "left", false);
  wait_text("Option # 1");

  auto nav_key = [&](const char *name, bool down) {
    KeyboardKey(vm, name, down);
    std::this_thread::sleep_for(125ms);
  };
  auto navigate = [&](const char *name) {
    nav_key(name, true);
    nav_key(name, false);
  };
  nav_key("meta_l", true);
  navigate("right");
  navigate("right");
  navigate("down");
  navigate("down");
  navigate("down");
  wait_menu(126, 26, yellow);
  nav_key("meta_l", false);
  wait_text("Option # 2");

  nav_key("meta_l", true);
  navigate("down");
  navigate("down");
  navigate("down");
  wait_menu(38, 26, yellow);
  nav_key("meta_l", false);
  wait_text("Cmd:2 ARG1:1C ARG2:201");
  std::cout
      << "PullDownMenu.HC selected commands by mouse and keyboard and exited\n";

  vm.ResetDeadline(30s);
  type_cmd("DocClear(,1);");
  type_cmd("MsSet(320,240);ms.show=FALSE;");
  type_cmd("Seed(12345);");
  type_cmd(R"(ExeFile("::/Demo/Graphics/CommonAncestor.HC");)");
  wait_text("PRESS A KEY", false, 1, 60, std::string_view::npos, 500);
  Bytes tree(640 * 480), panned(tree.size());
  Check(shape_pixels(tree) > 800, "CommonAncestor.HC did not draw the tree");

  constexpr uint64_t scroll_x = 0x100, scroll_y = 0x108, scroll_z = 0x110;
  vm.ResetDeadline(10s);
  KeyboardKey(vm, "ctrl", true);
  std::this_thread::sleep_for(125ms);
  MouseButton(vm, "left", true);
  std::this_thread::sleep_for(125ms);
  MouseMove(vm, 98, 70);
  wait_mem(left + scroll_x, 48, "Ctrl-left drag did not pan horizontally");
  wait_mem(left + scroll_y, 32, "Ctrl-left drag did not pan vertically");
  auto wait_tree = [&](int dx, int dy) {
    for (unsigned i = 0; i < 60; i++) {
      shape_pixels(panned);
      bool same = true;
      for (int y = 132; y < 320 && same; y++)
        for (int x = 24; x < 300 && same; x++)
          same = tree[y * 640 + x] == panned[(y + dy) * 640 + x + dx];
      if (same)
        return true;
      std::this_thread::sleep_for(20ms);
    }
    return false;
  };
  Check(wait_tree(48, 32), "The tree did not move with the window pan");
  MouseButton(vm, "left", false);
  std::this_thread::sleep_for(125ms);
  MouseMove(vm, 20, 16);
  std::this_thread::sleep_for(125ms);
  Check(ReadPhysical<2>(vm, left + scroll_x) == std::array<uint64_t, 2>{48, 32},
        "The pan changed after releasing the left button");

  MouseButton(vm, "left", true);
  std::this_thread::sleep_for(125ms);
  MouseMove(vm, -114, -80);
  wait_mem(left + scroll_x, uint64_t(-16),
           "Negative horizontal pan was not snapped");
  wait_mem(left + scroll_y, uint64_t(-8),
           "Negative vertical pan was not snapped");
  Check(wait_tree(-16, -8), "The tree did not move to the negative pan");
  MouseButton(vm, "left", false);
  MouseButton(vm, "right", true);
  wait_mem(left + scroll_x, 0, "Ctrl-right did not reset the horizontal pan");
  wait_mem(left + scroll_y, 0, "Ctrl-right did not reset the vertical pan");
  wait_mem(left + scroll_z, 0, "Ctrl-right did not reset the depth pan");
  Check(wait_tree(0, 0), "Ctrl-right did not restore the tree");
  MouseButton(vm, "right", false);
  KeyboardKey(vm, "ctrl", false);
  key("q");
  wait_text("T:/Home>");
  for (unsigned i = 0; i < 60; i++) {
    if (!shape_pixels(tree)) {
      std::cout << "CommonAncestor.HC panned, reset the tree, and returned to "
                   "the prompt\n";
      break;
    }
    std::this_thread::sleep_for(26ms);
  }
  Check(!shape_pixels(tree), "CommonAncestor.HC left the tree on VGA");

  vm.ResetDeadline(40s);
  const auto shading_window = ReadPhysical<4>(vm, left + win_left);
  type_cmd("DocClear(,1);");
  int64_t view_x = 320, view_y = 240;
  set_pointer(view_x, view_y);
  type_cmd(R"(ExeFile("::/Demo/Graphics/Shading.HC");)");
  wait_text("Move mouse to change light source.", false, 1, 60,
            std::string_view::npos, 250);
  constexpr uint64_t ctrl_head = 0x398, scrn_bounds = 0x48;
  const auto head = left + ctrl_head;
  auto find_view = [&]() -> uint64_t {
    for (auto c = ReadPhysical<1>(vm, head)[0]; c && c != head;
         c = ReadPhysical<1>(vm, c)[0])
      if (ReadPhysical<1>(vm, c + 24)[0] == 3)
        return c;
    return 0;
  };
  const auto view = find_view();
  Check(view, "Shading.HC did not create the view-angle control");
  const auto state = ReadPhysical<1>(vm, view + 104)[0];
  wait_mem(state, 4, "Shading.HC changed the initial X angle");
  Check(ReadPhysical<2>(vm, state + 8) == std::array<uint64_t, 2>{14, 12},
        "Shading.HC changed the initial Y or Z angle");
  const auto angles_box = ReadPhysical<4>(vm, view + scrn_bounds);
  Check(angles_box == std::array<uint64_t, 4>{549, 640, 388, 480},
        "Shading.HC placed the control outside its reference bounds");

  bool sphere = false, ticks = false;
  for (unsigned i = 0; i < 100 && !(sphere && ticks); ++i) {
    vm.Command(R"({"execute":"screendump","arguments":{"filename":")" +
               screen.string() + R"("}})");
    const auto rgb = ReadVGA(screen);
    size_t red = 0, blue = 0;
    for (int y = 140; y < 340; ++y)
      for (int x = 170; x < 470; ++x) {
        auto p = 3 * (y * 640 + x);
        const auto r = rgb[p], g = rgb[p + 1], b = rgb[p + 2];
        red += r >= 128 && r > g + 64 && r > b + 64;
        blue += b >= 128 && b > r + 64 && b > g + 64;
      }
    sphere = red > 1000 && blue > 1000;
    constexpr auto tick = 3 * (421 * 640 + 569);
    ticks = rgb[tick] == 0 && rgb[tick + 1] == 0 && rgb[tick + 2] == 0;
    if (!(sphere && ticks))
      std::this_thread::sleep_for(25ms);
  }
  Check(sphere, "Shading.HC did not draw both shaded hemispheres");
  Check(ticks, "Shading.HC did not draw the reference scale ticks");

  move_pointer(view_x, view_y, angles_box[0] + 22, angles_box[2] + 33);
  MouseButton(vm, "left", true);
  wait_mem(state, 12, "The X angle did not snap on click");
  MouseMove(vm, 0, 48);
  wait_mem(state, 36, "The X angle did not track the drag");
  MouseButton(vm, "left", false);
  key("q");
  for (unsigned i = 0; i < 80 && find_view(); ++i)
    std::this_thread::sleep_for(20ms);
  Check(!find_view(), "Shading.HC left its control in the queue");
  Check(ReadPhysical<4>(vm, left + win_left) == shading_window,
        "Shading.HC did not restore the terminal geometry");
  wait_text("T:/Home>");
  std::cout << "Shading.HC drew the sphere and accepted a view-angle drag\n";

  // The scrollbar exercise deliberately moves the document's edit cursor.
  DriveDigits(vm, dir, boot);

  vm.ResetDeadline(30s);
  type_cmd("WinHorz(4,27);WinVert(5,18);");
  constexpr std::array<uint64_t, 4> scroll_window{4, 27, 5, 18};
  set_pointer(160, 104);
  type_cmd(R"HC(U0 Sl(){I64 i;for(i=0;i<60;i++)"%02d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\n",i;PressAKey;})HC");
  type_cmd("DocClear(,1);Sl;");
  try {
    wait_text("PRESS A KEY", false, 1, 60, std::string_view::npos, 300);
  } catch (const std::exception &error) {
    auto cells = FileRead(path);
    std::string visible;
    for (size_t row = 4; row < 20; ++row) {
      for (size_t col = 4; col <= 27; ++col) {
        auto ch = cells[(row * 80 + col) * 4];
        visible += ch >= 32 && ch < 127 ? char(ch) : ' ';
      }
      visible += '\n';
    }
    throw std::runtime_error(std::string(error.what()) + "\n" + visible);
  }

  constexpr uint64_t next_ctrl = 0x398, put_doc = 0x30;
  constexpr uint64_t ctrl_box = 0x48, ctrl_flags = 0x20;
  constexpr uint64_t line_start = 0x80, top_line = 0x84;
  auto horz = ReadPhysical<1>(vm, left + next_ctrl)[0];
  auto vert = ReadPhysical<1>(vm, horz)[0];
  const auto doc = ReadPhysical<1>(vm, left + put_doc)[0];
  auto wait_view = [&](uint64_t offset, uint64_t expected, const char *error) {
    for (unsigned i = 0; i < 80; ++i) {
      if (ReadPhysical<1>(vm, doc + offset, 'w')[0] == expected)
        return;
      std::this_thread::sleep_for(20ms);
    }
    throw std::runtime_error(error);
  };
  wait_view(top_line, 48, "The scrollbar document did not start at its end");
  MouseButton(vm, "wheel-up", true);
  MouseButton(vm, "wheel-up", false);
  wait_view(top_line, 47, "A wheel notch did not scroll one line");

  int64_t mouse_x = 160, mouse_y = 104;
  auto grab_thumb = [&](uint64_t ctrl) {
    auto box = ReadPhysical<4>(vm, ctrl + ctrl_box);
    int64_t x = box[0] + 3, y = box[2] + 3;
    move_pointer(mouse_x, mouse_y, x, y);
    MouseButton(vm, "left", true);
    wait_mem(ctrl + ctrl_flags, 23,
             "The scrollbar thumb did not capture the mouse");
  };
  grab_thumb(vert);
  MouseMove(vm, 0, -100);
  mouse_y -= 50;
  wait_view(top_line, 27, "The vertical thumb did not scroll the document");
  MouseButton(vm, "left", false);
  wait_mem(vert + ctrl_flags, 7, "The vertical thumb did not release");
  Check(ReadPhysical<4>(vm, left + win_left) == scroll_window,
        "The vertical thumb resized the window");

  grab_thumb(horz);
  MouseMove(vm, 96, 0);
  wait_mem(horz + 40, 64, "The horizontal thumb did not move");
  wait_text("CDEFGH", false, 1, 5, 4, 120);
  MouseButton(vm, "left", false);
  wait_mem(horz + ctrl_flags, 7, "The horizontal thumb did not release");
  wait_text("27 ABC", false, 1, 5, 4, 120);
  wait_view(line_start, 0, "The cursor did not restore the viewport");
  Check(ReadPhysical<4>(vm, left + win_left) == scroll_window,
        "The horizontal thumb resized the window");
  key("q");
  std::cout << "Scrollbar thumbs and wheel scrolled the terminal document\n";
}

static fs::path CaptureGuestFile(CMonitor &vm, const fs::path &dir,
                                 const std::string &output, const char *name)
{
  auto prefix = std::string("UnholyOS: guest file /") + name + ", address ";
  size_t address = output.find(prefix);
  Check(address != std::string::npos, "Guest file was not created");
  address += prefix.size();
  size_t size_pos = output.find(", size ", address);
  Check(size_pos != std::string::npos, "Missing guest file extent");
  auto addr = std::stoull(output.substr(address, size_pos - address), nullptr, 16);
  auto size = std::stoull(output.substr(size_pos + 7), nullptr, 16);
  auto image = dir / name;
  vm.Command(R"({"execute":"pmemsave","arguments":{"val":)" +
             std::to_string(addr) + R"(,"size":)" + std::to_string(size) +
             R"(,"filename":")" + image.string() + R"("}})");
  Check(fs::file_size(image) == size, "QEMU saved an incomplete guest file");
  return image;
}

static std::string_view RedSeaName(const Bytes &image, size_t entry)
{
  const char *name = reinterpret_cast<const char *>(image.data() + entry + 2);
  auto *end = std::find(name, name + 38, '\0');
  Check(end != name + 38, "RedSea filename is unterminated");
  return {name, static_cast<size_t>(end - name)};
}

static size_t RedSeaDirectory(const Bytes &image, uint64_t cluster)
{
  Check(cluster < image.size() / 512, "RedSea directory starts outside the image");
  size_t pos = cluster * 512;
  size_t bytes = ReadLE(image, pos + 48, 8);
  Check(bytes >= 512 && bytes % 512 == 0 && bytes <= image.size() - pos,
        "RedSea directory extends outside the image");
  return pos + bytes;
}

static void VerifyCopiedTree(const Bytes &source, uint64_t src_root,
                             const Bytes &copy, uint64_t dst_root)
{
  auto src_end = RedSeaDirectory(source, src_root);
  auto dst_end = RedSeaDirectory(copy, dst_root);
  std::map<std::string_view, size_t> entries;
  for (size_t src = src_root * 512 + 128; src < src_end; src += 64) {
    auto name = RedSeaName(source, src);
    if (name.empty())
      break;
    Check(entries.emplace(name, src).second, "Source ISO has duplicate names");
  }
  for (size_t dst = dst_root * 512 + 128; dst < dst_end; dst += 64) {
    auto name = RedSeaName(copy, dst);
    if (name.empty())
      break;
    auto entry = entries.find(name);
    Check(entry != entries.end(), "Guest ISO added a directory entry");
    size_t src = entry->second;
    entries.erase(entry);
    auto attr = ReadLE(source, src, 2);
    Check(attr == ReadLE(copy, dst, 2), "Guest ISO changed file attributes");
    auto from = ReadLE(source, src + 40, 8), to = ReadLE(copy, dst + 40, 8);
    if (attr & 0x10) {
      VerifyCopiedTree(source, from, copy, to);
    } else {
      Check(ReadLE(source, src + 56, 8) == ReadLE(copy, dst + 56, 8),
            "Guest ISO changed file date");
      auto size = ReadLE(source, src + 48, 8);
      Check(size == ReadLE(copy, dst + 48, 8), "Guest ISO changed file size");
      if (size) {
        Check(from < source.size() / 512 && to < copy.size() / 512 &&
                  size <= source.size() - from * 512 && size <= copy.size() - to * 512,
              "Guest ISO file extends outside the image");
        Check(std::equal(source.begin() + from * 512,
                         source.begin() + from * 512 + size,
                         copy.begin() + to * 512),
              "Guest ISO changed file contents");
      }
    }
  }
  Check(entries.empty(), "Guest ISO lost directory entries");
}

static void VerifyGuestDataISO(const fs::path &path)
{
  Bytes data = FileRead(path);
  constexpr size_t dvd = 2048, redsea = 22 * dvd;
  Check(data.size() > redsea && data.size() % dvd == 0 &&
            HasBytes(data, 16 * dvd + 314, "TempleOS RedSea") &&
            ReadLE(data, 16 * dvd + 80, 4) == data.size() / dvd &&
            std::all_of(data.begin() + 21 * dvd, data.begin() + redsea,
                        [](auto byte) { return byte == 0; }),
        "Invalid guest-built data ISO descriptors or boot sector");
  auto root = ReadLE(data, redsea + 24, 8);
  RedSeaDirectory(data, root);
  auto nested = root * 512 + 128;
  Check(RedSeaName(data, nested) == "Units" &&
            RedSeaName(data, nested + 64).empty(),
        "Guest data ISO did not use the selected subtree");
  auto child = ReadLE(data, nested + 40, 8);
  auto end = RedSeaDirectory(data, child);
  std::string previous;
  size_t files = 0;
  for (size_t entry = child * 512 + 128;
       entry < end && !RedSeaName(data, entry).empty(); entry += 64) {
    auto name = RedSeaName(data, entry);
    Check(previous < name, "Guest ISO did not sort the source subtree");
    previous = name;
    auto size = ReadLE(data, entry + 48, 8);
    auto block = ReadLE(data, entry + 40, 8);
    Check(block < data.size() / 512 && size <= data.size() - block * 512,
          "Guest data ISO file extends outside the image");
    if (name == "Source.HC") {
      Check(size == 619 && data[block * 512] == 0x96,
            "Guest data ISO did not preserve the replaced file");
      for (size_t i = 1; i < size; i++)
        Check(data[block * 512 + i] == static_cast<uint8_t>((i * 43) ^ 0xAC),
              "Guest data ISO changed the source file bytes");
    } else {
      Check(name.size() == 8 && name.starts_with("Item") &&
                name.substr(5) == ".HC" && name[4] >= 'A' && name[4] <= 'J' &&
                size == 1 && data[block * 512] == name[4] - 'A' + 2,
            "Guest data ISO lost a nested directory entry");
    }
    files++;
  }
  Check(files == 11, "Guest data ISO has the wrong file count");
}

static void DriveKeyboard(CMonitor &vm, const fs::path &serial)
{
  auto key = [&](const char *name) {
    Check(vm.Hmp(std::string("sendkey ") + name + " 60").find(
              R"("return": "")") != std::string::npos,
          "QEMU rejected a keyboard event");
    std::this_thread::sleep_for(120ms);
  };

  vm.WaitSerial(serial, "kbd wait key\n");
  StopAtIdle(vm);
  Check(ReadPhysical<1>(vm, 0x100318)[0] & 0x200,
        "GetKey did not block Adam on its message queue");
  vm.Command(R"({"execute":"cont"})");
  key("up");
  vm.WaitSerial(serial, "kbd wait char\n");
  key("shift-a");
  vm.WaitSerial(serial, "<ESC> to Exit\n");
  for (const char *name : {"b", "shift-c", "right",
                           "caps_lock", "d", "caps_lock", "d", "esc"})
    key(name);
}

static void DriveMouse(CMonitor &vm, const fs::path &serial)
{
  auto button = [&](const char *name, bool down) {
    MouseButton(vm, name, down);
  };
  vm.WaitSerial(serial, "mouse ready ");
  MouseMove(vm, 20, -12);
  vm.WaitSerial(serial, "mouse msg 4 ");
  button("left", true);
  vm.WaitSerial(serial, "mouse msg 5 ");
  button("left", false);
  vm.WaitSerial(serial, "mouse msg 6 ");
  button("right", true);
  vm.WaitSerial(serial, "mouse msg 9 ");
  button("right", false);

  vm.WaitSerial(serial, "mouse wheel ready");
  button("wheel-up", true);
  button("wheel-up", false);
  vm.WaitSerial(serial, "mouse wheel back");
  button("wheel-down", true);
  button("wheel-down", false);

  vm.WaitSerial(serial, "mouse double ready");
  for (unsigned i = 0; i < 2; ++i) {
    button("left", true);
    std::this_thread::sleep_for(135ms);
    button("left", false);
    std::this_thread::sleep_for(135ms);
  }
}

static void DrivePalette(CMonitor &vm, const fs::path &serial)
{
  const auto screen = serial.parent_path() / "palette.ppm";
  auto count_pixels = [&](uint8_t blue) {
    vm.Command(R"({"execute":"screendump","arguments":{"filename":")" +
               screen.string() + R"("}})");
    const auto rgb = ReadVGA(screen);
    unsigned pixels = 0;
    for (size_t i = 0; i < rgb.size(); i += 3)
      pixels += rgb[i] == 255 && rgb[i + 1] == 255 && rgb[i + 2] == blue;
    return pixels;
  };
  vm.WaitSerial(serial, "palette yellow ");
  const auto yellow = count_pixels(0);
  Check(yellow > 50, "The VGA palette did not recolor the boot text");
  vm.Command(
      R"({"execute":"human-monitor-command","arguments":{"command-line":"sendkey p 65"}})");
  vm.WaitSerial(serial, "palette restored ");
  Check(count_pixels(255) == yellow,
        "The VGA palette did not restore the boot text");
}

static void DriveLines(CMonitor &vm, const fs::path &serial)
{
  auto output = vm.WaitSerial(serial, "UnholyOS: executing /Lines.HC\r\n");
  std::smatch address;
  Check(std::regex_search(
            output, address,
            std::regex(R"(UnholyOS: raster context (0x[0-9a-f]+))")),
        "Missing the raster bitmap address");
  auto canvas = std::stoull(address[1].str(), nullptr, 16);
  bool drew = false;
  for (unsigned i = 0; i < 80; i++) {
    if ((ReadPhysical<1>(vm, canvas)[0] & 255) != 255) {
      drew = true;
      break;
    }
    std::this_thread::sleep_for(15ms);
  }
  Check(drew, "The original Lines.HC did not draw into the screen context");
  const auto screen = serial.parent_path() / "lines.ppm";
  bool visible = false;
  for (unsigned i = 0; i < 50 && !visible; i++) {
    std::this_thread::sleep_for(80ms);
    vm.Command(R"({"execute":"screendump","arguments":{"filename":")" +
               screen.string() + R"("}})");
    const auto image = ReadVGA(screen);
    for (size_t pixel = 0; pixel < image.size(); pixel += 3)
      if (image[pixel] != image[pixel + 1] || image[pixel + 1] != image[pixel + 2]) {
        visible = true;
        break;
      }
  }
  Check(visible, "Lines.HC did not reach the VGA display");
  vm.Command(
      R"({"execute":"human-monitor-command","arguments":{"command-line":"sendkey x 65"}})");
  auto done = vm.WaitSerial(
      serial, "UnholyOS: finished /Lines.HC, checks 0x0000000000000001");
  CheckProgramOutput(done, "/Lines.HC", "", '1');
  Check(ReadPhysical<1>(vm, canvas)[0] == UINT64_MAX,
        "Lines.HC did not clear its bitmap on exit");
}

struct CCursorPhase
{
  const char *name, *key;
  size_t x, y, zoom;
  std::array<uint8_t, 3> bg, fg;
  size_t box_w, box_h;
};

static void DriveCursor(CMonitor &vm, const fs::path &serial)
{
  constexpr std::array<uint8_t, 3> blue{0, 0, 168}, yellow{255, 255, 87},
      cyan{0, 168, 168}, purple{168, 0, 168}, black{}, white{255, 255, 255};
  constexpr CCursorPhase phases[]{
      {"normal", "n", 160, 128, 1, blue, yellow, 0, 0},
      {"left", "l", 172, 136, 1, blue, cyan, 0, 0},
      {"right", "r", 184, 142, 1, blue, purple, 0, 0},
      {"hidden", "h", 184, 142, 1, blue, blue, 0, 0},
      {"custom", "c", 160, 128, 1, blue, yellow, 5, 4},
      {"grab", "g", 160, 128, 1, blue, yellow, 3, 4},
      {"shortcut", "ctrl-alt-z", 0, 0, 0, {}, {}, 0, 0},
      {"zoom", "z", 320, 240, 2, blue, yellow, 0, 0},
      {"recenter", "m", 324, 240, 2, black, white, 0, 0},
  };

  for (const auto &phase : phases) {
    vm.WaitSerial(serial, std::string("cursor ") + phase.name + "\n");
    if (phase.zoom) {
      auto path =
          serial.parent_path() / (std::string("cursor-") + phase.name + ".ppm");
      vm.Command(R"({"execute":"screendump","arguments":{"filename":")" +
                 path.string() + R"("}})");
      const auto image = ReadVGA(path);
      for (size_t y = 0; y < 10 * phase.zoom; ++y)
        for (size_t x = 0; x < 10 * phase.zoom; ++x) {
          auto px = x / phase.zoom, py = y / phase.zoom;
          const bool cursor = phase.box_w
                                  ? px < phase.box_w && py < phase.box_h
                                  : (px == py && px <= 8) || (!px && py <= 4) ||
                                        (!py && px <= 4);
          const auto &color = cursor ? phase.fg : phase.bg;
          auto offset = ((phase.y + y) * 640 + phase.x + x) * 3;
          if (!std::equal(color.begin(), color.end(), image.begin() + offset))
            throw std::runtime_error(
                std::string("The VGA cursor differs during ") + phase.name);
        }
    }
    vm.Command(
        R"({"execute":"human-monitor-command","arguments":{"command-line":"sendkey )" +
        std::string(phase.key) + R"( 65"}})");
  }
}

constexpr std::string_view cursor_output =
    "cursor normal\n"
    "cursor left\n"
    "cursor right\n"
    "cursor hidden\n"
    "cursor custom\n"
    "cursor grab\n"
    "cursor shortcut\n"
    "cursor zoom state 2 0 8 3FE0000000000000 3FE0000000000000 "
    "3FE0000000000000 80 64 8\n"
    "cursor zoom\n"
    "cursor zoom state 2 168 150 3FE0000000000000 3FE0000000000000 "
    "3FE0000000000000 80 64 8\n"
    "cursor recenter\n"
    "cursor zoom state 3 224 190 3FD5555555555555 3FD5555555555555 "
    "3FD5555555555555 163 132 10\n"
    "cursor zoom state 1 0 0 3FF0000000000000 3FF0000000000000 "
    "3FF0000000000000 -171 -144 -2\n"
    "cursor done\n";

static void DriveGrid(CMonitor &vm, const fs::path &serial)
{
  vm.WaitSerial(serial, "UnholyOS: executing /Grid.HC\r\n");
  auto wait_square = [&](size_t x0, size_t y0, bool moved) {
    auto path = serial.parent_path() / "grid.ppm";
    for (unsigned i = 0; i < 35; i++) {
      vm.Command(R"({"execute":"screendump","arguments":{"filename":")" +
                 path.string() + R"("}})");
      const auto image = ReadVGA(path);
      bool match = true;
      for (size_t y = 0; y < 9; ++y)
        for (size_t x = 0; x < 9; ++x) {
          auto pixel = ((y0 + y) * 640 + x0 + x) * 3;
          uint8_t color = x < 5 && y < 5 ? 255 : 0;
          match &= image[pixel] == color && image[pixel + 1] == color &&
                   image[pixel + 2] == color;
        }
      if (moved)
        match &= image[(160 * 640 + 160) * 3] == 0;
      if (match)
        return;
      std::this_thread::sleep_for(45ms);
    }
    throw std::runtime_error("The Grid.HC cursor did not snap to its new cell");
  };

  wait_square(160, 160, false);
  vm.Command(
      R"({"execute":"input-send-event","arguments":{"events":[{"type":"rel","data":{"axis":"x","value":24}},{"type":"rel","data":{"axis":"y","value":24}}]}})");
  wait_square(170, 170, true);
  vm.Command(
      R"({"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"button":"left","down":true}}]}})");
  auto done = vm.WaitSerial(
      serial, "UnholyOS: Grid restored the cursor and mouse grid");
  vm.Command(
      R"({"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"button":"left","down":false}}]}})");
  CheckProgramOutput(done, "/Grid.HC", "", '1');
}

constexpr std::string_view function_ptr_output =
    "function pointers 42 13 35 29 213 7 9\n"
    "function pointer float 7 24\n"
    "function pointer target 27\n"
    "function pointer rebound 207\n";

constexpr std::string_view variadic_output =
    "variadic direct 9 219 114 0 308\n"
    "variadic raw 73 mutate 213 recurse 10\n"
    "variadic order 206 321\n"
    "variadic cleanup 103000\n";

constexpr std::string_view variadic_ptr_output =
    "variadic pointer 3 211 0 108 211\n"
    "variadic pointer cleanup 30600\n";

constexpr std::string_view file_copy_output =
    "Copying B:/CopySrc.BIN to B:/CopyDst.BIN\n"
    "Copying B:/CopySrc.BIN to B:/CopyDst.BIN.Z\n"
    "Copying B:/CopyDst.BIN.Z to B:/CopyDst.BIN\n"
    "Copying B:/CopySrc.BIN to B:/CopyDst.BIN.C\n"
    "Copying ::/StartOS.HC.Z to B:/CopyArchive.HC.Z\n"
    "Copying B:/CopyEmpty.BIN to B:/CopyEmpty2.BIN\n"
    "File copy probe FF\n";

constexpr std::string_view file_operations_output =
    "Make Directory:B:/Ops\n"
    "Make Directory:B:/Ops/Sub\n"
    "Make Directory:B:/Ops/Sub/Deep\n"
    "Make Directory:B:/Ops/Empty\n"
    "Make Directory:B:/Pick\n"
    "Copying B:/Ops/A.TXT to B:/Pick/A.TXT\n"
    "Copying B:/Pick/A.TXT to B:/Pick/R.TXT\n"
    "Del A.TXT\n"
    "Make Directory:B:/Clone/Empty\n"
    "Make Directory:B:/Clone/Sub\n"
    "Make Directory:B:/Clone/Sub/Deep\n"
    "Copying B:/Ops/Sub/Deep/D.TXT to B:/Clone/Sub/Deep/D.TXT\n"
    "Copying B:/Ops/Sub/E.BIN.Z to B:/Clone/Sub/E.BIN.Z\n"
    "Copying B:/Ops/Sub/C.TXT to B:/Clone/Sub/C.TXT\n"
    "Copying B:/Ops/B.BIN to B:/Clone/B.BIN\n"
    "Copying B:/Ops/A.TXT to B:/Clone/A.TXT\n"
    "tree count 9\n"
    "Make Directory:B:/Txt/Empty\n"
    "Make Directory:B:/Txt/Sub\n"
    "Make Directory:B:/Txt/Sub/Deep\n"
    "Copying B:/Ops/Sub/Deep/D.TXT to B:/Txt/Sub/Deep/D.TXT\n"
    "Copying B:/Ops/Sub/C.TXT to B:/Txt/Sub/C.TXT\n"
    "Copying B:/Ops/A.TXT to B:/Txt/A.TXT\n"
    "masked tree count 7\n"
    "Del D.TXT\n"
    "Del C.TXT\n"
    "Del Deep\n"
    "Del E.BIN.Z\n"
    "Del Empty\n"
    "Del Sub\n"
    "Del B.BIN\n"
    "Del Clone\n"
    "Del D.TXT\n"
    "Del Deep\n"
    "Del C.TXT\n"
    "Del Empty\n"
    "Del Sub\n"
    "Del A.TXT\n"
    "Del Txt\n"
    "Del R.TXT\n"
    "Del Pick\n"
    "Del Reclaim.BIN\n"
    "Del Guard.BIN\n"
    "Del D.TXT\n"
    "Del Deep\n"
    "Del C.TXT\n"
    "Del E.BIN.Z\n"
    "Del Sub\n"
    "Del Empty\n"
    "Del A.TXT\n"
    "Del B.BIN\n"
    "Del Ops\n"
    "File operations probe FFF\n";

constexpr std::string_view class_meta_output =
    "age:44 default:38\n"
    "color: default:4\n"
    "rank:3 default:3 Admiral\n"
    "age:22 default:38 percentile: 54.20\n"
    "rank:2 default:5\n"
    "serial_num: 55555 default:123456\n";

constexpr std::string_view last_class_output =
    "Lastclass names I64i I64i I64i F64\n"
    "Lastclass probe 0F\n";

static void DriveDocFrame(CMonitor &vm, const fs::path &serial)
{
  constexpr std::array<uint8_t, 3> black{}, blue{0, 0, 168}, red{168, 0, 0},
      green{0, 168, 0}, yellow{255, 255, 87}, white{255, 255, 255};

  auto capture = [&](const char *phase) {
    vm.WaitSerial(serial, std::string("doc frame ") + phase + "\n");
    auto path = serial.parent_path() / (std::string("doc-") + phase + ".ppm");
    vm.Command(R"({"execute":"screendump","arguments":{"filename":")" +
               path.string() + R"("}})");
    return ReadVGA(path);
  };
  auto key = [&](const char *name) {
    vm.Command(
        R"({"execute":"human-monitor-command","arguments":{"command-line":"sendkey )" +
        std::string(name) + R"( 65"}})");
  };
  auto cell = [](const Bytes &image, size_t col, size_t row, uint64_t glyph,
                 const auto &fg, const auto &bg) {
    for (size_t y = 0; y < 8; y++)
      for (size_t x = 0; x < 8; x++) {
        const auto &expected = (glyph >> (y * 8 + x)) & 1 ? fg : bg;
        auto pixel = ((row * 8 + y) * 640 + col * 8 + x) * 3;
        if (!std::equal(expected.begin(), expected.end(),
                        image.begin() + pixel))
          throw std::runtime_error("DolDoc VGA cell differs at " +
                                   std::to_string(col) + "," +
                                   std::to_string(row));
      }
  };

  auto lower = capture("lower");
  cell(lower, 30, 24, 0x006666667E66663C, white, blue); // A
  cell(lower, 31, 25, 0, white, blue);
  cell(lower, 38, 25, 0x000606063E06067E, white, blue);  // F after the tab
  cell(lower, 30, 26, 0x00C6C6D6D6FEEEC6, white, red);   // M
  cell(lower, 30, 28, 0xFF3C666666666666, white, blue);  // Underlined U
  cell(lower, 30, 29, 0x007E18181818187E, blue, white);  // Inverted I
  cell(lower, 29, 23, 0x181818F800000000, yellow, blue); // Top-left corner
  cell(lower, 31, 23, 0x006666667E666666, yellow, blue); // Border document H
  cell(lower, 36, 27, 0x003C66663E060C38, white, blue);  // 6
  key("a");

  auto overlap = capture("overlap");
  cell(overlap, 36, 27, 0x001818181818187E, yellow, green); // T
  cell(overlap, 30, 24, 0x006666667E66663C, white, blue);
  key("o");

  auto uncovered = capture("uncovered");
  cell(uncovered, 36, 27, 0x003C66663E060C38, white, blue);
  key("r");

  auto cleared = capture("cleared");
  cell(cleared, 30, 24, 0, white, black);
  cell(cleared, 36, 27, 0, white, black);
  key("c");
}

constexpr std::string_view doc_frame_output = "doc depth 0\n"
                                              "doc frame lower\n"
                                              "doc depth 1\n"
                                              "doc frame overlap\n"
                                              "doc depth 0\n"
                                              "doc frame uncovered\n"
                                              "doc frame cleared\n";

static void DriveDocButton(CMonitor &vm, const fs::path &serial)
{
  auto button = [&](bool down) {
    vm.Command(
        std::string(
            R"({"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"button":"left","down":)") +
        (down ? "true" : "false") + R"(}}]}})");
  };
  auto check_corner = [&](size_t row, uint64_t glyph, const char *phase) {
    auto path =
        serial.parent_path() / (std::string("button-") + phase + ".ppm");
    for (unsigned attempt = 0; attempt < 25; ++attempt) {
      vm.Command(R"({"execute":"screendump","arguments":{"filename":")" +
                 path.string() + R"("}})");
      auto image = ReadVGA(path);
      bool match = true;
      for (size_t y = 0; y < 8; ++y) {
        for (size_t x = 0; x < 8; ++x) {
          size_t pixel = ((row * 8 + y) * 640 + 19 * 8 + x) * 3;
          bool ink = (glyph >> (y * 8 + x)) & 1;
          match &= image[pixel] == (ink ? 255 : 0) &&
                   image[pixel + 1] == (ink ? 255 : 0) &&
                   image[pixel + 2] == (ink ? 255 : 168);
        }
      }
      if (match)
        return;
      std::this_thread::sleep_for(45ms);
    }
    throw std::runtime_error(
        std::string("DolDoc button corner differs during ") + phase);
  };

  constexpr uint64_t single = 0x181818F800000000, solid = 0x6C6C6CEC0CFC0000;
  vm.WaitSerial(serial, "doc button ready\n");
  check_corner(20, single, "ready");
  button(true);
  check_corner(20, solid, "pressed");
  button(false);
  vm.WaitSerial(serial, "doc button exit ready\n");
  check_corner(20, single, "released");
  check_corner(22, single, "exit");
  button(true);
  check_corner(22, solid, "exit-pressed");
  button(false);
}

constexpr std::string_view doc_button_output = "doc button ready\n"
                                               "doc button key 32\n"
                                               "doc button command 1 73 0\n"
                                               "doc button exit ready\n"
                                               "doc button cancel 2 27 0\n";

static void DriveDocCallback(CMonitor &vm, const fs::path &serial)
{
  auto check_tag = [&](uint64_t glyph, const char *phase) {
    auto path =
        serial.parent_path() / (std::string("callback-") + phase + ".ppm");
    for (unsigned attempt = 0; attempt < 25; ++attempt) {
      vm.Command(R"({"execute":"screendump","arguments":{"filename":")" +
                 path.string() + R"("}})");
      auto image = ReadVGA(path);
      bool match = true;
      for (size_t y = 0; y < 8; ++y) {
        for (size_t x = 0; x < 8; ++x) {
          size_t pixel = ((20 * 8 + y) * 640 + 18 * 8 + x) * 3;
          bool ink = (glyph >> (y * 8 + x)) & 1;
          match &= image[pixel] == (ink ? 255 : 0) &&
                   image[pixel + 1] == (ink ? 255 : 0) &&
                   image[pixel + 2] == (ink ? 255 : 168);
        }
      }
      if (match)
        return;
      std::this_thread::sleep_for(45ms);
    }
    throw std::runtime_error(
        std::string("DolDoc callback tag differs during ") + phase);
  };
  auto click = [&](const char *name) {
    for (const char *down : {"true", "false"}) {
      vm.Command(
          std::string(
              R"({"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"button":")") +
          name + R"(","down":)" + down + R"(}}]}})");
      std::this_thread::sleep_for(100ms);
    }
  };
  vm.WaitSerial(serial, "doc callback ready 51\n");
  check_tag(0x007E060606060606, "live"); // L
  click("left");
  vm.WaitSerial(serial, "doc callback right ready\n");
  check_tag(0x006666767E6E6666, "next"); // N
  click("right");
}

constexpr std::string_view doc_callback_output = "doc callback ready 51\n"
                                                 "doc callback left 32 101\n"
                                                 "doc callback right ready\n"
                                                 "doc callback right 10 202\n"
                                                 "doc callback checks 63\n";

static void DriveExceptions(CMonitor &vm, const fs::path &serial)
{
  vm.WaitSerial(serial, "D2\nD1\n1 (y or n)? ");
  vm.Hmp("sendkey n 65");
  vm.WaitSerial(serial, "2 (y or n)? ");
  vm.Hmp("sendkey y 65");
  auto done = vm.WaitSerial(
      serial, "UnholyOS: finished /Exceptions.HC, checks 0x0000000000000001");
  auto start = done.find("UnholyOS: executing /Exceptions.HC\r\n");
  auto inner = done.find("D2 hndlr\nCh:Point2:", start);
  auto outer = done.find("Demo hndlr\nCh:Point2:", start);
  if (inner == std::string::npos || outer == std::string::npos ||
      inner >= outer)
    throw std::runtime_error("Exceptions demo did not reach both handlers");
}

constexpr std::string_view try_output = "try 42 41 73 123 123 5 1\n";

static void CheckTry(const std::string &boot)
{
  CheckProgramOutput(boot, "/TryProbe.HC", try_output, '1');
  CheckProgramOutput(boot, "/TryProbe.BIN.Z", try_output, '1');
  if (boot.find("UnholyOS: try rejected missing catch") == std::string::npos)
    throw std::runtime_error("HolyC try parsed without a catch");
}

static void DriveScreen(CMonitor &vm, const fs::path &serial)
{
  constexpr std::array<std::array<uint8_t, 3>, 16> colors{{
      {0, 0, 0},
      {0, 0, 168},
      {0, 168, 0},
      {0, 168, 168},
      {168, 0, 0},
      {168, 0, 168},
      {168, 87, 0},
      {168, 168, 168},
      {87, 87, 87},
      {87, 87, 255},
      {87, 255, 87},
      {87, 255, 255},
      {255, 87, 87},
      {255, 87, 255},
      {255, 255, 87},
      {255, 255, 255},
  }};
  auto capture = [&](const char *phase) {
    vm.WaitSerial(serial, std::string("screen ") + phase + "\n");
    auto path =
        serial.parent_path() / (std::string("screen-") + phase + ".ppm");
    vm.Command(R"({"execute":"screendump","arguments":{"filename":")" +
               path.string() + R"("}})");
    return ReadVGA(path);
  };
  auto pixel = [&](const Bytes &image, size_t x, size_t y, unsigned color) {
    return std::equal(colors[color].begin(), colors[color].end(),
                      image.begin() + (y * 640 + x) * 3);
  };
  auto key = [&](char ch) {
    vm.Command(
        R"({"execute":"human-monitor-command","arguments":{"command-line":"sendkey )" +
        std::string(1, ch) + R"( 65"}})");
  };

  auto normal = capture("normal");
  for (unsigned i = 0; i < colors.size(); ++i)
    for (size_t x = 321 + i * 11; x < 321 + (i + 1) * 11; ++x)
      Check(pixel(normal, x, 241, i) && pixel(normal, x, 258, i),
            "The VGA color strip differs from TempleOS");
  Check(pixel(normal, 319, 240, 15) && pixel(normal, 497, 259, 15),
        "The VGA first or last bit is wrong");
  constexpr uint64_t boot_glyph = 0x003C666666666666;
  for (size_t y = 0; y < 8; ++y)
    for (size_t x = 0; x < 8; ++x)
      Check(pixel(normal, x, y, (boot_glyph >> (8 * y + x)) & 1 ? 15 : 0),
            "The first boot glyph differs from the TempleOS font");

  key('n');
  auto delta = capture("delta");
  for (size_t x = 376; x < 387; ++x)
    Check(pixel(delta, x, 249, 13) && pixel(delta, x, 250, 5),
          "The changed VGA row did not reach the display");
  Check(pixel(delta, 375, 249, 4) && pixel(delta, 387, 249, 6),
        "A neighboring VGA strip changed");

  key('d');
  auto zoom = capture("zoom");
  for (unsigned i = 0; i < colors.size(); ++i)
    for (size_t x = 2 + i * 22; x < 2 + (i + 1) * 22; ++x)
      Check(pixel(zoom, x, 2, i) && pixel(zoom, x, 3, i),
            "The VGA zoom did not duplicate both axes");
  Check(pixel(zoom, 112, 18, 13) && pixel(zoom, 113, 19, 13) &&
            pixel(zoom, 112, 20, 5),
        "The changed row did not survive VGA zoom");

  key('z');
  DrivePalette(vm, serial);
  auto restored = capture("restored");
  Check(pixel(restored, 467, 250, 0) && pixel(restored, 1, 0, 15),
        "The VGA overlay did not clear back to the text layer");
}

constexpr std::string_view screen_output = "screen normal\n"
                                           "screen delta\n"
                                           "screen zoom\n"
                                           "screen restored\n";

static void DriveFontText(CMonitor &vm,const fs::path &serial)
{
  vm.WaitSerial(serial,"font swap ready\n");
  vm.Hmp("sendkey ctrl-alt-f 65");
  vm.WaitSerial(serial,"font swapped 1 1\n");
  std::this_thread::sleep_for(150ms);
  vm.Hmp("sendkey ctrl-alt-f 65");
  vm.WaitSerial(serial,"font restored 1 1\n");
}

static void DrivePrompts(CMonitor &vm,const fs::path &serial)
{
  vm.ResetDeadline(30s);
  auto retry=[&](std::string_view marker) {
    auto deadline=Clock::now()+5s;
    do {
      auto output=ReadText(serial);
      auto first=output.find(marker);
      if (first!=std::string::npos && output.find(marker,first+marker.size())!=std::string::npos)
        return;
      std::this_thread::sleep_for(50ms);
    } while (Clock::now()<deadline);
    throw std::runtime_error("Prompt did not retry: "+std::string(marker));
  };
  auto type=[&](std::string_view input) {
    auto key=[&](const std::string &name) {
      vm.Hmp("sendkey "+name+" 30");
      std::this_thread::sleep_for(70ms);
    };
    for (char ch:input) {
      switch (ch) {
        case '+': key("shift-equal"); break;
        case '*': key("shift-8"); break;
        case '-': key("minus"); break;
        case '.': key("dot"); break;
        case '/': key("slash"); break;
        default: key(std::string(1,ch)); break;
      }
    }
    key("ret");
  };
  vm.WaitSerial(serial,"probe int(42):");
  type("99");
  retry("probe int(42):");
  type("-3.75");
  vm.WaitSerial(serial,"probe float(");
  type("99");
  retry("probe float(");
  type("-2.5+1");
  vm.WaitSerial(serial,"probe date(");
  type("1/1/25");
  retry("probe date(");
  type("2/29/24");
  vm.WaitSerial(serial,"Enter int expression(123):");
  type("2+3*4");
  vm.WaitSerial(serial,"Enter float expression(");
  type("1.25*4");
  vm.WaitSerial(serial,"Name (John):");
  type("");
  vm.WaitSerial(serial,"Date (");
  type("2/29/24");
  auto output=vm.WaitSerial(serial,"UnholyOS: finished /Demo/Prompt.HC, checks 0x0000000000000001\r\n");
  for (auto expected:{"prompt conversion -3 17 42\n",
                      "prompt dates 02/29/24 02/04/24 02/29/24 01/01/25 12/31/25\n",
                      "probe int result -3\n","probe float result -2\n",
                      "probe date result 02/29/24\n","Int=14\n","Float=5.00000000e0\n","Name:John\n","Date=02/29/24\n"})
    Check(output.find(expected)!=std::string::npos,("Prompt result mismatch: "+std::string(expected)).c_str());
  Check(output.find("UnholyOS: finished /PromptProbe.HC, checks 0x0000000000000001\r\n")!=std::string::npos,
        "Native prompt probe failed");
}

static void CheckFontText(const std::string &output)
{
  CheckProgramOutput(output,"/FontTextProbe.HC",
      "font layout 88 40 48 72 60 80\n"
      "font data 64835F1EE471357B 3A41ADEE39E15EA1 2 D\n"
      "text attr 1 2F42 3043 4144\n"
      "text print 1E42 1E37 4144 0\n"
      "font pixels 64\n"
      "font swap ready\n"
      "font swapped 1 1\n"
      "font restored 1 1\n",'1');
  CheckProgramOutput(output,"/Demo/ExtChars.HC",
                     "Face:\xFF\nf\xFF\xFF=12345\n",'1');
  Check(output.find("UnholyOS: extended character glyph 0x3c42bdc381a5423c")!=
        std::string::npos,"The original ExtChars.HC did not change glyph 255");
}

constexpr std::string_view matrix_output =
    "matrix pi 400921FB54442D18\n"
    "matrix rotation 6812FEF91D413F68 17 -4 -5\n"
    "matrix product DBCE19D7856E362E DBCE19D7856E362E\n"
    "matrix sweep 9E2CB8A0EE1F90BD\n"
    "matrix symmetry 1 0 2130900514 -3729075900 -2 17 6\n"
    "matrix refresh 1 1 1 1 1\n";

constexpr std::string_view arrow_output =
    "arrow flat 1 1 1 0A93575B785BA51A\n"
    "arrow depth 1 1 700 FDC1D53C433F3886 968F230D7FAD8636\n"
    "arrow sprite 25 7 69A4CCBD98FA89F1 1E1F00CEB29E84E5\n"
    "arrow cursor 1 0 DE0EEB3809B0F38D 937BA7BBBD57D70D\n"
    "arrow text 1 4 1 0 2 95 68 10\n"
    "arrow glyphs 89E961B07EC7F577 0F40B14DF3F0EAC5\n"
    "arrow glyphrop 1 1 1 6 9B053996C61C5E16\n";

constexpr std::string_view task_settings_output =
    "settings layout 392 48 104 388\n"
    "settings push 1 1 1 1 1\n"
    "settings border 1 1\n"
    "settings restore 1 1 1 1 1 1\n"
    "settings children 1 1 1\n"
    "settings nested 1 1 1 1 1\n";

constexpr std::string_view win_scroll_output =
    "scroll saved 48 -24 3\n"
    "scroll cleared 0 0 0\n"
    "scroll restored 48 -24 3\n";

constexpr std::string_view scroll_controls_output =
    "ctrl layout 160 32 136 24\n"
    "ctrl box 1 1 0\n"
    "ctrl shape 0 1\n"
    "ctrl callback 8\n"
    "ctrl hidden 0\n";

constexpr std::string_view doc_refresh_output =
    "refresh partial 0 1 0\n"
    "refresh full 3 0 1\n"
    "refresh forced 4 0\n"
    "refresh backwards 5 0 1\n";

constexpr std::string_view view_angles_output =
    "view layout 72 24 48 68\n"
    "view new 1 3 5 91 92\n"
    "view snap 4 47 0 0.523599 6.152286 0.000000\n"
    "view choices 12 36 24\n"
    "view gone 1\n"
    "view math 1 -1 22 16 7 -4 73\n";

constexpr std::string_view menu_output =
    "menu layout 40 80 32 72\n"
    "menu stack 1 1 1\n"
    "menu open Open -2 79 0\n"
    "menu item Item 1 17 123\n"
    "menu base 1 1\n"
    "menu restore 1\n";

constexpr std::string_view pen_output =
    "pen tables 6B9F06DB42815298 29C67144FDAE53A0\n"
    "pen flat 230CA4FC4A9D5ED8 400E0001\n"
    "pen record -2 7 7 16 36\n"
    "pen circles 122 1376 4DD4E698261C23BF\n"
    "pen depth 30D56E30C8ED43BF 93DFD7862B827E46\n"
    "pen collision 129 129 1297A29A79C68C91\n"
    "pen lines 1 1 1 790FA048009D2069 598F028775D2A11A\n"
    "pen steps 4 B689D485B6519259 36DCE5178A4AE735 700\n"
    "pen sprite 77 4 2E999C28B3F2E0F4 2AA8A172D6B8FEB6\n";

constexpr std::string_view depth_spans_output =
    "zs flat 6 6 3FADA006F427EE28\n"
    "zs depth 15 19 0 49125D7AEFFFF671 E85F2E55D1C242AE\n"
    "zs extents 0 -3 -4 22 23 16\n"
    "zs dither 9 8 9 1 2DA8FDEE696B9A4E 9B71953947B40053\n"
    "zs window 15 15 E905D4084BB73416 343107A434F7EF32\n"
    "zs triangle 102 88 40C64284EF01AD55 22E4F7B2E2D793F2\n"
    "zs sweep D385EC40E7041395\n"
    "zs polygon 63 E3124EE47CD8A137 C0C617D516576B08\n"
    "zs mirror 61 D7C1A0C19BF3C47D 68594A1EE5D083E1\n"
    "zs rect 52 0D3B6136E1141B70 C07EE8E956ED11A5\n"
    "zs sprite 0D3B6136E1141B70 C07EE8E956ED11A5\n";

constexpr std::string_view blot3_output =
    "b3 matrix 1 6FB24FB965721A91 9 3 12 9\n"
    "b3 depth EFC05ABCF35F18E6 4 2 12 6\n"
    "b3 mirror 1 13 4 3 220BF8395DA653D5\n"
    "b3 callback 3158DDC09ADC78D5 3 1 12 3 6\n"
    "b3 sprite 8003AC30B2AF186D 123 0 6\n";

constexpr std::string_view raster_output =
    "raster rect 20 0 0 4 3 4 255\n"
    "raster nearest 5 4 -1\n"
    "raster depth 1 2 4\n"
    "raster collision 19 19\n"
    "raster masks 5 2 14 14\n"
    "raster seed 15582A7B EB16 AC5F36197A54EB16\n"
    "raster random 1 1 1 1 C5A40ABCD97CCB75\n"
    "raster blot 1 12 0 10\n"
    "raster brushmask 4 4 1\n"
    "raster opaque 4 4 0\n"
    "raster clip 1 0 0 12 9 -2 -1 14 10\n"
    "raster line 1 11 255 11 255\n"
    "raster window -1 5 -1\n"
    "raster blank 1 2 1\n"
    "raster file 136\n";

constexpr std::string_view dc_output =
    "dc layout 408 32 384 392 10507 10480\n"
    "dc screen 640 480 24000 4000 1\n"
    "dc new 13 16 5 53437644 0 1 0000000100000000\n"
    "dc alias 1 1 0 2000\n"
    "dc depth 7FFFFFFF 7FFFFFFF\n"
    "dc copy 1 15 123 1 1\n"
    "dc transform 12 -4 3 7FFFFFFF\n"
    "dc light 8004000C 37837\n"
    "dc reset 10000 0 15 37837 1\n"
    "dc null 1000 1 1\n"
    "dc raw 240 E52FFF102716BD66\n"
    "dc load 240 -7 8 2 123456789ABC 7\n"
    "dc compressed 191 3BEBDB6BD7DF4848\n"
    "dc expanded 191 7 6 79\n"
    "dc file 191 123456789ABC 7\n";

static void VerifyGuestISO(const char *qemu, CMonitor &vm,
                           const fs::path &dir, const fs::path &source,
                           const fs::path &kernel, const char *memory,
                           const std::string &output)
{
  CheckProgramOutput(output, "/RedSeaISOProbe.HC", "ISO probe 1 1 0 0 0\n", '1');
  auto image = CaptureGuestFile(vm, dir, output, "GuestBuild.ISO.C");
  auto media = VerifyMedia(image, kernel);
  auto original = VerifyMedia(source, kernel);
  VerifyCopiedTree(FileRead(source), original.root, FileRead(image), media.root);
  VerifyGuestDataISO(CaptureGuestFile(vm, dir, output, "GuestData.ISO.C"));
  if (std::string_view(memory) == "512") {
    auto reboot = dir / "reboot";
    fs::create_directory(reboot);
    fs::path serial = reboot / "serial.log";
    CMonitor rebuilt(qemu, image, reboot, "pc", memory, false);
    rebuilt.Connect();
    DriveKeyboard(rebuilt, serial);
    DriveMouse(rebuilt, serial);
    DriveLines(rebuilt, serial);
    DriveCursor(rebuilt, serial);
    DriveGrid(rebuilt, serial);
    DriveDocFrame(rebuilt, serial);
    DriveDocButton(rebuilt, serial);
    DriveDocCallback(rebuilt, serial);
    DriveExceptions(rebuilt, serial);
    DriveScreen(rebuilt, serial);
    DriveFontText(rebuilt,serial);
    DrivePrompts(rebuilt,serial);
    auto boot = rebuilt.Boot(serial);
    CheckProgramOutput(boot, "/DCProbe.HC", dc_output, '1');
    CheckProgramOutput(boot, "/RasterProbe.HC", raster_output, '1');
    CheckProgramOutput(boot, "/Blot3Probe.HC", blot3_output, '1');
    CheckProgramOutput(boot, "/DepthSpansProbe.HC", depth_spans_output, '1');
    CheckProgramOutput(boot, "/MatrixProbe.HC", matrix_output, '1');
    CheckProgramOutput(boot, "/PenProbe.HC", pen_output, '1');
    CheckProgramOutput(boot, "/ArrowProbe.HC", arrow_output, '1');
    CheckProgramOutput(boot, "/TaskSettingsProbe.HC", task_settings_output,
                       '1');
    CheckProgramOutput(boot, "/WinScrollProbe.HC", win_scroll_output, '1');
    CheckProgramOutput(boot, "/ScrollControlsProbe.HC", scroll_controls_output,
                       '1');
    CheckProgramOutput(boot, "/DocRefreshProbe.HC", doc_refresh_output, '1');
    CheckProgramOutput(boot, "/ViewAnglesProbe.HC", view_angles_output, '1');
    CheckProgramOutput(boot, "/MenuProbe.HC", menu_output, '1');
    CheckProgramOutput(boot, "/CursorProbe.HC", cursor_output, '1');
    CheckProgramOutput(boot, "/CursorExcept.HC", "cursor exception 1\n", '1');
    CheckProgramOutput(boot, "/FunctionPtrProbe.HC", function_ptr_output, '1');
    CheckProgramOutput(boot, "/FunctionPtrProbe.BIN.Z", function_ptr_output,
                       '1');
    CheckProgramOutput(boot, "/VariadicProbe.HC", variadic_output, '1');
    CheckProgramOutput(boot, "/VariadicProbe.BIN.Z", variadic_output, '1');
    CheckProgramOutput(boot, "/VariadicPtrProbe.HC", variadic_ptr_output, '1');
    CheckProgramOutput(boot, "/VariadicPtrProbe.BIN.Z", variadic_ptr_output, '1');
    CheckProgramOutput(boot, "/DocFrame.HC", doc_frame_output, '1');
    CheckProgramOutput(boot, "/DocButton.HC", doc_button_output, '1');
    CheckProgramOutput(boot, "/DirectoryProbe.HC", "Directory probe 1FF\n",
                       '1');
    CheckProgramOutput(boot, "/DirContextProbe.HC", "Dir context FF\n", '1');
    CheckProgramOutput(boot, "/FileStreamProbe.HC", "File stream probe 3FF\n", '1');
    CheckProgramOutput(boot, "/FileCopyProbe.HC", file_copy_output, '1');
    CheckProgramOutput(boot, "/FileOperationsProbe.HC", file_operations_output, '1');
    CheckProgramOutput(boot, "/MemberMetaProbe.HC", "Member metadata 7F\n", '1');
    CheckProgramOutput(boot, "/ClassMeta.HC", class_meta_output, '1');
    CheckProgramOutput(boot, "/LastClassProbe.HC", last_class_output, '1');
    CheckProgramOutput(boot, "/LastClassProbe.BIN.Z", last_class_output, '1');
    CheckProgramOutput(boot, "/DateOffsetProbe.HC",
                       "date fraction EE4120D\n"
                       "date local 02/29/24 00:00:00 utc B47ECC0000000\n"
                       "date negative 02/29/24 00:00:00 utc B47ED40000000\n"
                       "Date offset 3FF\n", '1');
    CheckProgramOutput(boot, "/DocBinLinkProbe.HC", "Doc bin links 1F\n", '1');
    CheckProgramOutput(boot, "/SpriteProbe.HC", "sprite bitmap 12546 0 0 4\n",
                       '1');
    CheckProgramOutput(boot, "/DocCallback.HC", doc_callback_output, '1');
    CheckTry(boot);
    if (boot.find("UnholyOS: document callback propagation 0x0000000000000001") ==
        std::string::npos)
      throw std::runtime_error("DolDoc callback exception did not propagate");
    CheckProgramOutput(boot, "/ScreenProbe.HC", screen_output, '1');
    CheckFontText(boot);
    auto registers = StopAtIdle(rebuilt);
    VerifyHandoff(boot, media, registers);
    VerifyBootDrive(rebuilt, boot, media, true, false);
    rebuilt.Command(R"({"execute":"cont"})");
    DriveUserTask(rebuilt, reboot, boot);
    rebuilt.Command(R"({"execute":"quit"})");
    fs::copy_file(image, source.parent_path() / "unholyos-guest-built.iso",
                  fs::copy_options::overwrite_existing);
  }
}

static void TestBoot(const char *qemu, const fs::path &iso,
                     const fs::path &kernel, const char *machine,
                     const char *memory, uint32_t host_bridge,
                     bool hpet_enabled, bool secondary_slave = false,
                     bool startup = false)
{
  auto media = VerifyMedia(iso, kernel);
  char path[] = "/tmp/unholyos-boot-XXXXXX";
  Check(mkdtemp(path), "Cannot create a temporary QEMU directory");
  fs::path dir = path, serial = dir / "serial.log", screen = dir / "screen.ppm";
  try {
    CMonitor vm(qemu, iso, dir, machine, memory, secondary_slave);
    vm.Connect();
    if (startup) {
      DriveKeyboard(vm, serial);
      DriveMouse(vm, serial);
      DriveLines(vm, serial);
      DriveCursor(vm, serial);
      DriveGrid(vm, serial);
      DriveDocFrame(vm, serial);
      DriveDocButton(vm, serial);
      DriveDocCallback(vm, serial);
      DriveExceptions(vm, serial);
      DriveScreen(vm, serial);
      DriveFontText(vm,serial);
      DrivePrompts(vm,serial);
    }
    std::string output = vm.Boot(serial);
    std::string registers = StopAtIdle(vm);
    uint64_t heap = VerifyHandoff(output, media, registers);
    if (!startup)
      VerifyFixedArea(vm, heap, registers, output);
    VerifyUncachedAlias(vm, output);
    VerifyKernelGlobals(vm, output);
    VerifyInterrupts(vm, output, registers, startup);
    VerifyClock(vm, output, host_bridge, hpet_enabled);
    VerifyBootDrive(vm, output, media, host_bridge == 0x12378086,
                    secondary_slave);

    if (startup) {
      CheckProgramOutput(output, "/DCProbe.HC", dc_output, '1');
      CheckProgramOutput(output, "/RasterProbe.HC", raster_output, '1');
      CheckProgramOutput(output, "/Blot3Probe.HC", blot3_output, '1');
      CheckProgramOutput(output, "/DepthSpansProbe.HC", depth_spans_output, '1');
      CheckProgramOutput(output, "/MatrixProbe.HC", matrix_output, '1');
      CheckProgramOutput(output, "/PenProbe.HC", pen_output, '1');
      CheckProgramOutput(output, "/ArrowProbe.HC", arrow_output, '1');
      CheckProgramOutput(output, "/TaskSettingsProbe.HC", task_settings_output,
                         '1');
      CheckProgramOutput(output, "/WinScrollProbe.HC", win_scroll_output, '1');
      CheckProgramOutput(output, "/ScrollControlsProbe.HC", scroll_controls_output,
                         '1');
      CheckProgramOutput(output, "/DocRefreshProbe.HC", doc_refresh_output, '1');
      CheckProgramOutput(output, "/ViewAnglesProbe.HC", view_angles_output, '1');
      CheckProgramOutput(output, "/MenuProbe.HC", menu_output, '1');
      CheckProgramOutput(output, "/CursorProbe.HC", cursor_output, '1');
      CheckProgramOutput(output, "/CursorExcept.HC", "cursor exception 1\n",
                         '1');
      CheckProgramOutput(output, "/FunctionPtrProbe.HC", function_ptr_output,
                         '1');
      CheckProgramOutput(output, "/FunctionPtrProbe.BIN.Z", function_ptr_output,
                         '1');
      CheckProgramOutput(output, "/VariadicProbe.HC", variadic_output, '1');
      CheckProgramOutput(output, "/VariadicProbe.BIN.Z", variadic_output, '1');
      CheckProgramOutput(output, "/VariadicPtrProbe.HC", variadic_ptr_output, '1');
      CheckProgramOutput(output, "/VariadicPtrProbe.BIN.Z", variadic_ptr_output,
                         '1');
      CheckProgramOutput(output, "/DocFrame.HC", doc_frame_output, '1');
      CheckProgramOutput(output, "/DocButton.HC", doc_button_output, '1');
      CheckProgramOutput(output, "/DirectoryProbe.HC", "Directory probe 1FF\n",
                         '1');
      CheckProgramOutput(output, "/DirContextProbe.HC", "Dir context FF\n",
                         '1');
      CheckProgramOutput(output, "/FileStreamProbe.HC", "File stream probe 3FF\n",
                         '1');
      CheckProgramOutput(output, "/FileCopyProbe.HC", file_copy_output, '1');
      CheckProgramOutput(output, "/FileOperationsProbe.HC", file_operations_output, '1');
      CheckProgramOutput(output, "/MemberMetaProbe.HC", "Member metadata 7F\n", '1');
      CheckProgramOutput(output, "/ClassMeta.HC", class_meta_output, '1');
      CheckProgramOutput(output, "/LastClassProbe.HC", last_class_output, '1');
      CheckProgramOutput(output, "/LastClassProbe.BIN.Z", last_class_output, '1');
      CheckProgramOutput(output, "/DateOffsetProbe.HC",
                         "date fraction EE4120D\n"
                         "date local 02/29/24 00:00:00 utc B47ECC0000000\n"
                         "date negative 02/29/24 00:00:00 utc B47ED40000000\n"
                         "Date offset 3FF\n", '1');
      CheckProgramOutput(output, "/DocBinLinkProbe.HC", "Doc bin links 1F\n",
                         '1');
      CheckProgramOutput(output, "/SpriteProbe.HC", "sprite bitmap 12546 0 0 4\n",
                         '1');
      CheckProgramOutput(output, "/DocCallback.HC", doc_callback_output, '1');
      CheckTry(output);
      if (output.find("UnholyOS: document callback propagation 0x0000000000000001") ==
          std::string::npos)
        throw std::runtime_error("DolDoc callback exception did not propagate");
      CheckProgramOutput(output, "/ScreenProbe.HC", screen_output, '1');
      CheckFontText(output);
      CheckProgramOutput(output, "/PaletteProbe.HC",
                         "palette tables 8 AAAA55550000 777777777777 12 BROWN LTRED\n"
                         "palette start 0 AAAA55550000 FFFFFFFFFFFF\n"
                         "palette gray 16 75D675D675D6\n"
                         "palette channels CF3C 5555 1040 0\n"
                         "palette yellow FFFFFFFF0000\n"
                         "palette restored FFFFFFFFFFFF AAAA\n", '1');
      CheckProgramOutput(output, "/MouseProbe.HC",
                         "mouse ready 168 131 80 1 1 4\n"
                         "mouse msg 4 170 154 0 0\n"
                         "mouse msg 5 170 154 1 0\n"
                         "mouse msg 6 170 154 0 0\n"
                         "mouse msg 9 170 154 0 1\n"
                         "mouse msg 10 170 154 0 0\n"
                         "mouse wheel ready\n"
                         "mouse wheel -1\n"
                         "mouse wheel back\n"
                         "mouse irq 1\n"
                         "mouse snap 168 152 171 157\n"
                         "mouse double ready\n"
                         "mouse double 7 160 160\n"
                         "mouse double 8 160 160\n", '1');
      CheckProgramOutput(output, "/KeyboardProbe.HC",
                         "kbd map 21E 41E 33 0 130\n"
                         "kbd empty 0\n"
                         "kbd wait key\n"
                         "kbd key 0 000000C800000048\n"
                         "kbd wait char\n"
                         "kbd char 65 0000021E0000021E\n"
                         "kbd irq 1\n", '1');
      constexpr std::string_view msg_loop =
          "CMD:02 ARG1:00000062 ARG2:0000003000000030\n"
          "CMD:03 ARG1:00000062 ARG2:0000013000000130\n"
          "CMD:02 ARG1:00000000 ARG2:0000022A0000022A\n"
          "CMD:02 ARG1:00000043 ARG2:0000022E0000022E\n"
          "CMD:03 ARG1:00000043 ARG2:0000032E0000032E\n"
          "CMD:03 ARG1:00000000 ARG2:0000012A0000012A\n"
          "CMD:02 ARG1:00000000 ARG2:000000CD0000004D\n"
          "CMD:03 ARG1:00000000 ARG2:000001CD0000014D\n"
          "CMD:02 ARG1:00000000 ARG2:0000003A0000003A\n"
          "CMD:03 ARG1:00000000 ARG2:0000113A0000113A\n"
          "CMD:03 ARG1:00000000 ARG2:0000117A0000117A\n"
          "CMD:03 ARG1:00000000 ARG2:0000117A0000117A\n"
          "CMD:02 ARG1:00000044 ARG2:0000102000001020\n"
          "CMD:03 ARG1:00000044 ARG2:0000112000001120\n"
          "CMD:02 ARG1:00000000 ARG2:0000103A0000103A\n"
          "CMD:03 ARG1:00000000 ARG2:0000013A0000013A\n"
          "CMD:03 ARG1:00000000 ARG2:0000017A0000017A\n"
          "CMD:03 ARG1:00000000 ARG2:0000017A0000017A\n"
          "CMD:02 ARG1:00000064 ARG2:0000002000000020\n"
          "CMD:03 ARG1:00000064 ARG2:0000012000000120\n"
          "CMD:02 ARG1:0000001B ARG2:0000000100000001\n";
      Check(output.find(msg_loop) != std::string::npos &&
                output.find("UnholyOS: finished /MsgLoop.HC, checks "
                            "0x0000000000000001") != std::string::npos,
            "The original message loop did not match TempleOS");
      Check(output.find("StartOS.HC size 0x0000000000000486, hash "
                        "0x68b531597d446b26") != std::string::npos,
            "The guest did not expand StartOS.HC.Z to the expected source");
      Check(output.find("source lexer 0x8676a65fcd40fc3d, "
                        "lexer flags 0xbf6dea4d260e4444") != std::string::npos,
            "The source lexer token stream differed from TempleOS");
      Check(output.find("source directives 0x488c2a2299a5649f "
                        "0x3c00b321bf773ea3, heap 0x0000000000000001, "
                        "symbols 0x0000000000000001") != std::string::npos,
            "The compiler source directive probe did not match TempleOS");
      Check(output.find("DolDoc source 0x5bdc53a30085a3c4, "
                        "text 0x22cc259f6558ab8f, sprite 0xc37e51fd8b8a58ba, "
                        "heap 0x0000000000000001") != std::string::npos,
            "The DolDoc source probe did not match TempleOS");
      Check(output.find("native expressions 0x000000000000002a, "
                        "hash 0x452a7a6d75541fd2, heap 0x0000000000000001, "
                        "rejected 0x0000000000000001") != std::string::npos &&
                output.find("WARNING: Assert Failed /ExpressionProbe.HC,21") !=
                    std::string::npos,
            "The native expression probe did not match TempleOS");
      Check(output.find("native functions 0x000000000000001b, hash "
                        "0x6ebe7d79f6232350, checks "
                        "0x000000000000000f") != std::string::npos,
            "The native HolyC function results or stack ABI differed");
      constexpr std::string_view print_output =
          "123.45679M\n12.345000K\n123456.79K\n12.345000K\n\n"
          "With the ',' flag:\n123,456.8K\n 12.345,0K\n\n"
          "Floating Point:\n1.2345678900000m\n12.345000000000K\n"
          "        1234.57\xe6\n 12345000000.00\xe6\n\n"
          "xxxxxxxxxxxxxxxxxxxxxxxxx\nyyyyyyyyyyyyyyyyyyyyyyyyy\nzzzz\n\n";
      CheckProgramOutput(output, "/Print.HC", print_output, '1');
      CheckProgramOutput(output, "/PrintSourceProbe.HC",
                         "join ab 1 2  -2.50\nparen 7 kept %\nraw B\n"
                         "100% raw\norder 21\n");
      CheckProgramOutput(output, "/StreamProbe.HC",
                         "load 1 0\nstream 87 77 11 46\n", '1');
      Check(output.find("source file result 0x00000000000000a4") !=
                    std::string::npos &&
                output.find("stream checks 0x000000000000000f") !=
                    std::string::npos,
            "The source file result or compile-time stream cleanup failed");
      CheckProgramOutput(output, "/MemoryProbe.HC",
                         "grid 12 -280 2 1100000\nptr 2 2 24 8\n"
                         "local 5492 narrow 255\nbyte -1 187 255 -1\n"
                         "cast 0123456789ABCDF1 -15 CDF1 3FF0000000000000\n"
                         "text ABC B byte -1\n", '1');
      CheckProgramOutput(output, "/SubIntAccess.HC",
                         "q\t\t=FEDCBA9876543210\nq.i16[2]\t=FFFFFFFFFFFFBA98\n"
                         "q.u8[5]\t\t=00000000000000BA\n"
                         "q.i32[1].u8[2]\t=00000000000000DC\n"
                         "q.i32[0].i8[1]\t=0000000000000032\n", '1');
      Check(output.find("memory type checks 0x000000000000000f") !=
                std::string::npos,
            "Typed memory declarations, result types, or heap cleanup failed");
      CheckProgramOutput(output, "/AggregateProbe.HC",
                         "node 9 1 -3 7\nclass 9 1 11 9 127\n"
                         "union 6 1 5 5678 EF B2 A1B2\n"
                         "init 12 1 12 18 one two\narray 24 60 4 hi!\n"
                         "omitted 0 0\nlocal 274 static 33\nstatic 34\n", '1');
      Check(output.find("aggregate checks 0x000000000000000f") !=
                std::string::npos,
            "Class initialization, forward declaration, or cleanup failed");
      CheckProgramOutput(output, "/CompoundProbe.HC",
                         "alias 33\norder 12 store 33\nchain 29\nmath 29 15 1\n"
                         "byte -127\nbyte store -127 shift 60\n"
                         "bits BC\nbits B0\nbits A2\nbits 288\nbits store 88\n"
                         "pointer 13\npointer 33\nfloat 15.00\nfloat 2.25\n"
                         "float 1.12\nmixed 27\nsigned shift -2\n"
                         "signed divide -5\nfloat shift 4\nfloat bits -1.00\n"
                         "narrow runtime 129\nnarrow store -127\n", '1');
      Check(output.find("compound checks 0x0000000000000001") !=
                std::string::npos,
            "Invalid compound assignment published a symbol or leaked memory");
      Check(output.find("Unresolved Reference:LoadProbe") != std::string::npos,
            "The caller import was not deferred");
      Check(output.find("TOSB probe 0x0000000000000028, caller "
                        "0x000000000000001c, relocated 0x0000000000000001, "
                        "aligned 0x0000000000000001, kernel copy "
                        "0x0000000000000001, heap value "
                        "0x0000000000000028") != std::string::npos,
            "The guest did not run both native TOSB fixtures");
      Check(output.find("runtime probe 0x00000000000000ff") !=
                std::string::npos,
            "The native HolyC runtime probe did not pass all checks");
      Check(output.find("file probe 0x000000000000007f") != std::string::npos,
            "The native HolyC file probe did not pass all checks");
      Check(output.find("write probe 0x000000000000007f") != std::string::npos,
            "The native HolyC file-write probe did not pass all checks");
      Check(output.find("exception probe 0x000000000000007f") !=
                    std::string::npos &&
                output.find("Except:Print:0x") != std::string::npos &&
                output.find("Except:Leaf:") == std::string::npos,
            "The native HolyC exception probe did not pass all checks");
      Check(
          output.find("format probe 0x00000000000001ff") != std::string::npos &&
              output.find("FORMAT-PRINT abc:29\nTAB\n") != std::string::npos &&
              output.find("ERROR: FORMAT-ERR 7E\n") != std::string::npos &&
              output.find("WARNING: FORMAT-WARN xy\n") != std::string::npos &&
              output.find(
                  "ERROR: Undefined Define: 'ST_FORMAT_PROBE_MISSING'.\n") !=
                  std::string::npos &&
              output.find("FORMAT-SILENT") == std::string::npos &&
              output.find("SILENT!") == std::string::npos,
          "The native HolyC formatter probe did not pass all checks");
      Check(output.find("hash probe 0x000000000000003f") != std::string::npos &&
                output.find("Unresolved Reference:HashTargetFn") !=
                    std::string::npos &&
                output.find("Unresolved Reference:HashTargetVar") !=
                    std::string::npos &&
                output.find("UnholyOS: typed imports resolved") !=
                    std::string::npos,
            "The native HolyC symbol probe did not pass all checks");
      Check(output.find("document probe 0x000000000000007f") !=
                    std::string::npos &&
                output.find("DOC-LINE\n") != std::string::npos,
            "The native HolyC document probe did not pass all checks");
      Check(output.find("compiler runtime probe 0x000000000000003f") !=
                    std::string::npos &&
                output.find("RAW-LOCATION.HC,0027\n") != std::string::npos,
            "The native HolyC compiler runtime probe did not pass all checks");
      auto isolated = output.find("UnholyOS: AOT symbols isolated");
      Check(output.find("cmp compile 0\n") != std::string::npos &&
                output.find("cmp reject 1\n") != std::string::npos &&
                output.find("cmp print 0\n") != std::string::npos &&
                isolated != std::string::npos &&
                output.find("UnholyOS: AOT source map") != std::string::npos &&
                output.find("$LK,\"CmpProbeSum\",A=\"FL:::/CmpProbe.PRJ.Z,") !=
                    std::string::npos &&
                output.find("cmp module 32 ready\n") != std::string::npos &&
                output.find("cmp module 32 ready\n") > isolated &&
                output.find("UnholyOS: AOT result 0x0000000000000020") !=
                    std::string::npos,
            "Guest Cmp failed to build or load the HolyC module");
      CheckProgramOutput(output, "/CmpPrint.BIN.Z", print_output, '1');
      if (std::string_view(memory) == "512")
        fs::copy_file(CaptureGuestFile(vm, dir, output, "CmpOutput.BIN.Z"),
                      iso.parent_path() / "CmpOutput.BIN.Z",
                      fs::copy_options::overwrite_existing);
      Check(output.find("native link build 0 0\n") != std::string::npos &&
                output.find("Unresolved Reference:NativeLinkAdd") !=
                    std::string::npos &&
                output.find("Unresolved Reference:NativeLinkBias") !=
                    std::string::npos &&
                output.find("native link 34\n") != std::string::npos &&
                output.find("native late build 0\n") != std::string::npos &&
                output.find("Unresolved Reference:NativeLateFn") !=
                    std::string::npos &&
                output.find("Unresolved Reference:NativeLateData") !=
                    std::string::npos &&
                output.find("native late 34\n") != std::string::npos &&
                output.find("UnholyOS: AOT system alias matched") !=
                    std::string::npos &&
                output.find("UnholyOS: extern and import rejection matched") !=
                    std::string::npos,
            "Guest-built imports or extern declarations failed to link");
      CheckProgramOutput(output, "/ImportExtern.HC",
                         "native extern 15 34 49\nnative extern date 1\n", '1');
      if (std::string_view(memory) == "512")
        for (auto *name : {"ImportCaller.BIN.Z", "ImportProvider.BIN.Z"})
          fs::copy_file(CaptureGuestFile(vm, dir, output, name),
                        iso.parent_path() / name,
                        fs::copy_options::overwrite_existing);
      Check(
          output.find("asm build 0 0 0 0 0 0 0\n") != std::string::npos &&
              output.find("UnholyOS: asm rejected unresolved local") !=
                  std::string::npos &&
              output.find("UnholyOS: asm opcode modes and alignment matched") !=
                  std::string::npos,
          "Guest assembly build, label cleanup, or opcode check failed");
      CheckProgramOutput(output, "/DivByHand.HC", "123456/10=12345\n", '1');
      CheckProgramOutput(output, "/AsmHelloWorld.HC",
                         "0A Hello World\n09 Hello World\n08 Hello World\n"
                         "07 Hello World\n06 Hello World\n05 Hello World\n"
                         "04 Hello World\n03 Hello World\n02 Hello World\n"
                         "01 Hello World\n", '1');
      CheckProgramOutput(output, "/MulByHand.HC",
                         "2*7   =0xE\n100*10=0x3E8\n"
                         "0x0123466789ABCDEF*0x1000001=0x000000000001234568ACF13578ABCDEF\n", '1');
      CheckProgramOutput(output, "/PutDec.HC", "122333221\n", '1');
      CheckProgramOutput(
          output, "/AsmAndC3.HC",
          "Meaning of life:42\nMeaning of life:42\nMeaning of life:42\n", '1');
      CheckProgramOutput(output, "/InlineAsmProbe.HC",
                         "inline import\ninline asm 42\ninline counters 1\n",
                         '1');
      CheckProgramOutput(output, "/InlineAsmProbe.BIN.Z",
                         "inline import\ninline asm 42\ninline counters 1\n",
                         '1');
      Check(
          output.find("UnholyOS: goto rejected missing and duplicate labels") !=
              std::string::npos,
          "Compiler accepted an invalid goto label");
      CheckProgramOutput(output, "/GotoProbe.HC", "goto 42 5\n", '1');
      CheckProgramOutput(output, "/GotoProbe.BIN.Z", "goto 42 5\n", '1');
      Check(output.find("UnholyOS: opcode table mode bytes matched") !=
                std::string::npos,
            "Opcode table emitted unexpected mode prefixes");
      std::ostringstream buzz_fizz;
      buzz_fizz << std::hex << std::uppercase << std::setfill('0');
      for (unsigned n = 1; n <= 100; n++) {
        buzz_fizz << std::setw(16) << n << ' ';
        if (n % 6 == 0)
          buzz_fizz << "FIZZ ";
        if (n % 7 == 0)
          buzz_fizz << "BUZZ ";
        buzz_fizz << '\n';
      }
      CheckProgramOutput(output, "/BuzzFizz.HC", buzz_fizz.str(), '1');
      CheckProgramOutput(output, "/OpcodeProbe.HC", "opcode table 42 2 2 42\n",
                         '1');
      CheckProgramOutput(output, "/OpcodeProbe.BIN.Z",
                         "opcode table 42 2 2 42\n", '1');
      for (const char *msg :
           {"UnholyOS: JIT branch values and bytes matched",
            "UnholyOS: AOT branch values and bytes matched",
            "UnholyOS: short and near branch imports matched",
            "UnholyOS: short branches rejected out-of-range targets",
            "Branch out of range at line:"})
        Check(output.find(msg) != std::string::npos,
              "Assembly branch values, bytes, or range checks differed");
      constexpr std::string_view listing =
          "                        00000000 _ASM_LIST_VALUES::\n"
          "B8 0000002A             00000000 \tMOV RAX,42\n"
          "                        00000000   RET\n"
          "C3                      00000005 \n"
          "                        00000005   // Values use both byte and "
          "scalar columns.\n"
          "AB 00                   00000006   DU8 0xAB,0;\n"
          "34 12                   00000008   DU16 0x1234;\n"
          "11223344                0000000A   DU32 0x11223344;\n"
          "1122334455667788        0000000E   DU64 0x1122334455667788;\n"
          "                        0000000E   NOLIST\n"
          "                        00000017 _ASM_LIST_TAIL::\n"
          "90 90 90 90 90 90 90 90 \n"
          "90 90                   00000017   DU8 10 DUP (0x90);\n"
          "EB DD                   00000021   JMP @@done\n"
          "00                      00000023   DU8 0;\n"
          "                        00000024 @@done:\n"
          "                        00000024   RET\n"
          "C3 ";
      CheckProgramOutput(output, "/AsmListingProbe.HC", listing, '1');
      auto cmp_listing = "UnholyOS: executing listing Cmp\r\n" +
                         std::string(listing) + "Errs:0 Warns:0 ";
      Check(output.find(cmp_listing) != std::string::npos &&
                output.find("UnholyOS: listed module returned 42") !=
                    std::string::npos,
            "DolDoc assembly listing or the loaded code differed");
      CheckProgramOutput(
          output, "/AsmListingScope.HC",
          "44 33 22 11             0000FFFC   DU32 0x11223344;\n"
          "1122334455667788        00010000   DU64 0x1122334455667788;\n"
          "                        00010000   NOLIST\n"
          "90 B8 0000002F             00000000     MOV RAX,47\n"
          "                        00000000     NOP\n"
          "90                      00000005     NOLIST\n"
          "inline listing 47\n",
          '1');
      CheckProgramOutput(output, "/SoundProbe.HC",
                         "sound notes 0000000000000000 407B800000000000 "
                         "407EDE22007F791A 4029F4E00A91D08E\n"
                         "sound keys 0 60 60 1 127\n",
                         '1');
      for (const char *msg :
           {"UnholyOS: beeps completed and stopped",
            "UnholyOS: speaker gate, mute and reset matched",
            "UnholyOS: sound event queue and default format matched"})
        Check(output.find(msg) != std::string::npos,
              "The speaker, timing, or sound event recording failed");
      CheckProgramOutput(output, "/FixedAsm.HC", "fixed jit 1 42\n", '1');
      CheckProgramOutput(output, "/FixedImage.BIN", "fixed load 42\n", '1');
      Check(output.find("UnholyOS: invalid ORG left the destination intact") !=
                std::string::npos,
            "Invalid ORG modified its destination or published a symbol");
      Check(output.find(
                "UnholyOS: BINFILE checked missing file and terminator") !=
                std::string::npos,
            "BINFILE missing-file or terminator behavior changed");
      Check(
          output.find(
              "UnholyOS: rejected malformed fixed image before copying") !=
              std::string::npos,
          "Invalid fixed image modified its destination or published a symbol");
      Check(output.find("switch build 0\n") != std::string::npos &&
                output.find("UnholyOS: switch rejected duplicate case") !=
                    std::string::npos,
            "Guest switch build or duplicate case check failed");
      CheckProgramOutput(output, "/NullCase.HC",
                         "Zero\nOne\nTwo\nThree\nTen\nEleven\n", '1');
      CheckProgramOutput(output, "/SubSwitch.HC",
                         "Zero [One] Two [Three] Four [Five] \n", '1');
      for (const char *name : {"/SwitchProbe.HC", "/SwitchProbe.BIN.Z"})
        CheckProgramOutput(output, name, "switch 12 2 -1 99 17 34\n", '1');
      for (const char *name : {"/SchedulerProbe.HC", "/SchedulerProbe.BIN.Z"})
        CheckProgramOutput(output, name, "sched 1234 1 1 1 1\n", '1');
      for (const char *name : {"/JobProbe.HC", "/JobProbe.BIN.Z"})
        CheckProgramOutput(output, name,
                           "jobs masks 2 65 3 30 0 0 0\n"
                           "jobs result 42 47 1 77 22\n"
                           "jobs servant 93 0\n", '1');
      Check(output.find("UnholyOS: dead tasks reaped") != std::string::npos,
            "Terminated tasks remain in the scheduler queues");
      CheckProgramOutput(output, "/DivByHand.BIN.Z", "123456/10=12345\n", '1');
      CheckProgramOutput(output, "/AsmProbe.BIN.Z",
                         "asm import\nasm result 42\n", '1');
      if (std::string_view(memory) == "512") {
        for (auto *name :
             {"AsmProbe.BIN.Z", "DivByHand.BIN.Z", "InlineAsmProbe.BIN.Z",
              "GotoProbe.BIN.Z", "SwitchProbe.BIN.Z", "OpcodeProbe.BIN.Z",
              "FixedImage.BIN", "AsmBranchProbe.BIN.Z",
              "AsmBranchImport.BIN.Z", "AsmListingProbe.BIN.Z",
              "SchedulerProbe.BIN.Z", "JobProbe.BIN.Z", "SoundRecord.BIN.Z"})
          fs::copy_file(CaptureGuestFile(vm, dir, output, name),
                        iso.parent_path() / name,
                        fs::copy_options::overwrite_existing);
        constexpr struct
        {
          const char *name;
          size_t size;
          uint64_t hash;
        } graphics_files[]{
            {"DCProbe.GR.Z", 118, 0x1F9B24E015B471C5},
            {"RasterProbe.GR.Z", 137, 0x707AB6E911CC6482},
        };
        for (const auto &expected : graphics_files) {
          auto path = CaptureGuestFile(vm, dir, output, expected.name);
          const auto archive = FileRead(path);
          uint64_t hash = 0xCBF29CE484222325;
          for (uint8_t byte : archive)
            hash = (hash ^ byte) * 0x100000001B3;
          Check(archive.size() == expected.size && hash == expected.hash,
                "The GR.Z disk file differs from pinned TempleOS");
          fs::copy_file(path, iso.parent_path() / expected.name,
                        fs::copy_options::overwrite_existing);
        }
      }
      VerifyGuestISO(qemu, vm, dir, iso, kernel, memory, output);
      vm.ResetDeadline(5s);
      if (std::string_view(memory) == "4608") {
        std::smatch address;
        Check(std::regex_search(output, address,
                                std::regex(R"(heap address (0x[0-9a-f]+))")) &&
                  std::stoull(address[1].str(), nullptr, 16) >= (1ULL << 32),
              "The module's data allocation did not use RAM above 4 GiB");
      }
    }
    if (!startup && std::string_view(machine) == "pc" &&
        std::string_view(memory) == "512" && !secondary_slave) {
      vm.Command(R"({"execute":"cont"})");
      DriveUserTask(vm, dir, output);
    }
    vm.Command(R"({"execute":"screendump","arguments":{"filename":")" +
               screen.string() + R"("}})");
    VerifyVGA(screen);
    vm.Command(R"({"execute":"quit"})");
    std::cout
        << "QEMU " << machine << " with " << memory << " MiB\n"
        << output
        << "Verified RedSea, BIOS handoff, long mode, VGA, kernel "
           "tables, interrupt gates, PIT, PCI, and the boot DVD.\n"
        << (startup
                ? "Verified StartOS.HC expansion, native HolyC compilation, "
                  "TOSB execution, runtime calls, and guest-built ISO contents.\n"
                : "Verified Adam startup, allocator pools, and device "
                  "memory.\n");
  } catch (...) {
    std::cerr << ReadText(serial) << ReadText(dir / "qemu.log");
    std::error_code ignored;
    fs::remove_all(dir, ignored);
    throw;
  }
  fs::remove_all(dir);
}

int main(int argc, char **argv)
{
  if (argc==5 && (std::string_view(argv[1])=="--fat32-write" ||
                  std::string_view(argv[1])=="--fat32-stream")) {
    bool stream=std::string_view(argv[1])=="--fat32-stream";
    char path[]="/tmp/unholyos-fat32-write-XXXXXX";
    if (!mkdtemp(path)) return 1;
    fs::path dir=path,disk=dir/"owned.img";
    try {
      auto original=FileRead(argv[4]);
      constexpr size_t fat1=2080*512,fat2=2593*512,fat_bytes=513*512;
      Check(original.size()==68699136,"Wrong writable FAT32 geometry");
      fs::copy_file(argv[4],disk);
      auto expected=original;
      for (unsigned pass=0;pass<2;pass++) {
        auto run=dir/(pass?"reboot":"write");
        fs::create_directory(run);
        CMonitor vm(argv[2],argv[3],run,"pc","512",false,disk,true);
        vm.Connect();
        auto output=vm.Boot(run/"serial.log");
        if (stream)
          CheckProgramOutput(output,"/Fat32StreamProbe.HC",
              pass?"FAT32 persisted streams\nFAT32 stream probe 3F\n":
                   "Copying C:/ZIP.BIN.Z to C:/ZIP2.BIN.Z\n"
                   "FAT32 stream mutation 3F\nFAT32 stream probe 3F\n",'1');
        else CheckProgramOutput(output,"/Fat32WriteProbe.HC",
            pass?"FAT32 persisted files\nFAT32 file probe FF\n":
                 "Make Directory:C:/Work\nMake Directory:C:/Work/Grow\n"
                 "Make Directory:C:/Work/Empty\nFAT32 mutation 3F\nFAT32 file probe FF\n",'1');
        vm.Quit();
        auto actual=FileRead(disk);
        Check(actual.size()==original.size(),"FAT32 image size changed");
        Check(std::equal(actual.begin()+fat1,actual.begin()+fat1+fat_bytes,
                         actual.begin()+fat2),"FAT32 mirrors differ");
        Check(std::equal(actual.begin(),actual.begin()+2049*512,original.begin()),
              "FAT32 changed MBR or boot sector");
        if (pass) Check(actual==expected,"Read-only reboot changed FAT32 bytes");
        else {
          Check(actual!=original,"FAT32 mutations did not reach disk");
          expected=std::move(actual);
        }
      }
      Check(FileRead(argv[4])==original,"Original FAT32 fixture was modified");
      std::cout<<(stream?"Verified FAT32 buffered streams, growth, copy and cold reboot\n":
                         "Verified FAT32 file and directory mutations, mirrored FATs and cold reboot\n");
      fs::remove_all(dir);
      return 0;
    } catch (const std::exception &error) {
      std::cerr<<ReadText(dir/"write/serial.log")<<ReadText(dir/"reboot/serial.log")
               <<error.what()<<'\n'<<"Evidence: "<<dir<<'\n';
      return 1;
    }
  }
  if (argc==5 && std::string_view(argv[1])=="--ata-write") {
    char path[]="/tmp/unholyos-ata-XXXXXX";
    if (!mkdtemp(path)) return 1;
    fs::path dir=path,disk=dir/"owned.img";
    try {
      auto original=FileRead(argv[4]);
      constexpr size_t size=68699136,bytes=130*512,start=size-131*512;
      Check(original.size()==size,"Wrong ATA fixture geometry");
      Check(std::all_of(original.begin()+start-512,original.end(),
                       [](uint8_t b){return b==0;}),"ATA fixture scratch area is not empty");
      fs::copy_file(argv[4],disk);
      auto expected=original;
      for (size_t i=0;i<bytes;i++) expected[start+i]=i*37+(i>>8)+90;
      for (unsigned pass=0;pass<2;pass++) {
        auto run=dir/(pass?"reboot":"write");
        fs::create_directory(run);
        CMonitor vm(argv[2],argv[3],run,"pc","512",false,disk,true);
        vm.Connect();
        auto output=vm.Boot(run/"serial.log");
        CheckProgramOutput(output,"/AtaWriteProbe.HC",
            pass?"ATA persisted pattern\nATA write probe 3F\n":
                 "ATA wrote pattern\nATA write probe 3F\n",'1');
        CheckProgramOutput(output,"/AtaWriteRejectProbe.HC","ATA reject probe 1F\n",'1');
        Check(output.find("UnholyOS: ATA partial sectors 0x0000000000000001\r\n")!=std::string::npos,
              "ATA partial sector write or restore failed");
        CheckProgramOutput(output,"/Fat32RejectProbe.HC","FAT32 reject probe 1F\n",'1');
        vm.Quit();
        Check(FileRead(disk)==expected,"ATA disk bytes differed from expected image");
      }
      Check(FileRead(argv[4])==original,"Original ATA fixture was modified");
      std::cout<<"Verified ATA writes, protection, exact disk bytes and cold reboot\n";
      fs::remove_all(dir);
      return 0;
    } catch (const std::exception &error) {
      std::cerr<<ReadText(dir/"write/serial.log")<<ReadText(dir/"reboot/serial.log")
               <<error.what()<<'\n'<<"Evidence: "<<dir<<'\n';
      return 1;
    }
  }
  if (argc==6 && std::string_view(argv[1])=="--fat32") {
    char path[]="/tmp/unholyos-fat32-XXXXXX";
    if (!mkdtemp(path)) return 1;
    fs::path dir=path;
    try {
      bool invalid=std::string_view(argv[5])=="invalid";
      CMonitor vm(argv[2],argv[3],dir,"pc","512",false,argv[4]);
      vm.Connect();
      auto output=vm.Boot(dir/"serial.log");
      if (invalid) {
        Check(output.find("executing /Fat32ReadProbe.HC")==std::string::npos,
              "Malformed FAT32 drive was mounted");
        CheckProgramOutput(output,"/Fat32RejectProbe.HC",
                           "FAT32 invalid geometry rejected\n",'1');
      } else {
        CheckProgramOutput(output,"/Fat32ReadProbe.HC",
            "fat geometry 2 2048 132130 2 2 2080 3102\n"
            "fat metadata FRAG.BIN 5 2501 20 02/29/24 12:34:56\n"
            "fat entry SUB 1024\n"
            "fat entry LongNameExample.txt 333\n"
            "FAT32 read probe FFF\n",'1');
        CheckProgramOutput(output,"/Fat32RejectProbe.HC","FAT32 reject probe 1F\n",'1');
      }
      vm.Command(R"({"execute":"quit"})");
      std::cout<<(invalid?"Verified FAT32 invalid geometry refusal\n":
                            "Verified FAT32 read, chain validation and write refusal\n");
      fs::remove_all(dir);
      return 0;
    } catch (const std::exception &error) {
      std::cerr<<ReadText(dir/"serial.log")<<error.what()<<'\n'<<"Evidence: "<<dir<<'\n';
      return 1;
    }
  }
  if ((argc == 4 || argc == 5) && std::string_view(argv[1]) == "--digits") {
    char path[] = "/tmp/unholyos-digits-XXXXXX";
    if (argc == 4 && !mkdtemp(path))
      return 1;
    fs::path dir = argc == 5 ? fs::absolute(argv[4]) : fs::path(path);
    try {
      fs::create_directories(dir);
      CMonitor vm(argv[2], argv[3], dir, "pc", "512", false);
      vm.Connect();
      auto output = vm.Boot(dir / "serial.log");
      DriveDigits(vm, dir, output);
      vm.Command(R"({"execute":"quit"})");
      if (argc == 4)
        fs::remove_all(dir);
      return 0;
    } catch (const std::exception &error) {
      std::cerr << error.what() << '\n' << "Evidence: " << dir << '\n';
      return 1;
    }
  }
  if (argc != 4 && argc != 5) {
    std::cerr << "Usage: " << argv[0]
              << " qemu image.iso kernel [startup-test.iso]\n"
              << "       " << argv[0]
              << " --digits qemu image.iso [capture-dir]\n";
    return 2;
  }
  try {
    for (const char *memory : {"512", "3072", "4608"})
      TestBoot(argv[1], argv[2], argv[3], "pc", memory, 0x12378086, true);
    TestBoot(argv[1], argv[2], argv[3], "pc,hpet=off", "512", 0x12378086,
             false);
    TestBoot(argv[1], argv[2], argv[3], "pc", "512", 0x12378086, true, true);
    TestBoot(argv[1], argv[2], argv[3], "q35", "512", 0x29C08086, true);
    if (argc == 5)
      for (const char *memory : {"512", "3072", "4608"})
        TestBoot(argv[1], argv[4], argv[3], "pc", memory, 0x12378086, true,
                 false, true);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
