#include "BootMedia.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <poll.h>
#include <regex>
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
           const char *machine, const char *memory, bool secondary_slave)
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

  std::string Boot(const fs::path &serial)
  {
    while (Clock::now() < deadline) {
      std::string output = ReadText(serial);
      if (output.find(BOOT_MARKER) != std::string::npos)
        return output;
      if (waitpid(pid, nullptr, WNOHANG) == pid) {
        pid = -1;
        throw std::runtime_error("QEMU stopped before kernel entry");
      }
      std::this_thread::sleep_for(50ms);
    }
    throw std::runtime_error("Kernel entry timed out");
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
                             const std::string &registers)
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

  uint64_t stubs = targets[1] - 7, fallback = targets[0x21];
  const std::array<uint64_t, 6> handlers{fallback,      targets[0],
                                         targets[2],    targets[0x20],
                                         targets[0x30], targets[0x31]};
  auto distinct = handlers;
  std::sort(distinct.begin(), distinct.end());
  Check(std::adjacent_find(distinct.begin(), distinct.end()) == distinct.end(),
        "Dedicated interrupt gates share a handler");
  for (unsigned vector = 0; vector < 256; vector++) {
    bool uses_stub =
        (vector > 0 && vector < 0x20 && vector != 2) || vector == 0x32;
    bool dedicated = vector == 0 || vector == 2 || vector == 0x20 ||
                     vector == 0x30 || vector == 0x31;
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
            pic, std::regex(R"(pic0: [^\\]*imr=fa [^\\]*irq_base=20)")) &&
            std::regex_search(
                pic, std::regex(R"(pic1: [^\\]*imr=ff [^\\]*irq_base=28)")),
        "PIC vector offsets or masks differ from boot configuration");
  const std::regex timer(R"(counters (0x[0-9a-f]+), timer read (0x[0-9a-f]+))");
  Check(std::regex_search(output, match, timer), "Missing PIT timer state");
  auto counters = ReadPhysical<2>(vm, std::stoull(match[1].str(), nullptr, 16));
  uint64_t timer_read = std::stoull(match[2].str(), nullptr, 16);
  uint64_t ticks = counters[0] - 1;
  Check(counters[0] >= 3 && counters[1] == ticks * 1193 &&
            timer_read <= counters[1] + 1193 &&
            ReadPhysical<1>(vm, 0x100230)[0] == ticks &&
            ReadPhysical<1>(vm, 0x100250)[0] == ticks &&
            ReadPhysical<1>(vm, 0x1004B8)[0] == ticks,
        "PIT interrupts did not advance the timer and restore Adam");
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

constexpr uint64_t late_pages = 19, late_alloced = late_pages * 512;
constexpr uint64_t late_used = 0x2828;
constexpr uint64_t fault_pages = 4, fault_alloced = fault_pages * 512;
constexpr uint64_t fault_used = 0x7E8;
constexpr uint64_t symbol_pages = 32, symbol_alloced = symbol_pages * 512;
constexpr uint64_t drive_pages = 7 + 9, drive_alloced = drive_pages * 512;
constexpr uint64_t drive_used = drive_alloced - 2 * 24 + 40 + 40 + 32 + 24;

static uint64_t VerifyKernelSymbols(CMonitor &vm, uint64_t table_body,
                                    uint64_t owner)
{
  uint64_t used = 0, count = 0;
  bool kernel = false, hash_del = false, hash_src = false, put_str = false;
  for (uint64_t i = 0; i < 1024; i += 64) {
    auto buckets = ReadPhysical<64>(vm, table_body + i * 8);
    for (size_t j = 0; j < buckets.size(); j++) {
      for (uint64_t addr = buckets[j]; addr;) {
        Check(++count <= 1024 && addr >= 16, "Bad boot hash chain");
        auto entry = ReadPhysical<5>(vm, addr - 16);
        auto type = static_cast<uint32_t>(entry[4]);
        bool module = type == 0x01004000;
        Check((module || type == 0x08000001) && entry[0] == owner &&
                  entry[1] == (module ? 64u : 88u) && entry[3] >= 16,
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
        kernel |= module && str == "/Kernel/Kernel";
        hash_del |= !module && str == "HashDel";
        hash_src |= !module && str == "HashSrcFileSet";
        put_str |= !module && str == "PUT_STR";
        used += entry[1] + name[1];
        addr = entry[2];
      }
    }
  }
  Check(kernel && hash_del && hash_src && put_str, "Missing boot symbols");
  return used;
}

static uint64_t VerifyPools(CMonitor &vm, uint64_t heap, uint64_t ram_end,
                            const std::string &output, uint64_t range_bytes,
                            uint64_t symbol_used,
                            const std::vector<CTestE820> &e820)
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
  uint64_t high_pages = (limit - code_base) / page;

  uint64_t pool = 0, data_free = 0, data_bytes = 0, data_end = 0;
  if (limit < total) {
    std::vector<std::pair<uint64_t, uint64_t>> blocks;
    for (auto entry : e820) {
      if (entry.type != 1)
        continue;
      uint64_t base = entry.base, end = base + entry.size;
      base = std::max(base, limit);
      if (base >= end)
        continue;
      uint64_t pages = (end - base) / page;
      uint64_t free_base = base;
      if (!pool) {
        pool = base;
        free_base = (base + 0x9E0 + page - 1) & ~(page - 1);
        pages -= (free_base - base) / page;
        data_bytes += free_base - base;
      }
      blocks.emplace_back(free_base, pages);
      data_bytes += pages * page;
    }

    Check(!blocks.empty(), "Data pool has no free blocks");
    auto [last_base, last_pages] = blocks.back();
    data_end = last_base + last_pages * page;
    for (auto [base, pages] : blocks) {
      auto block = ReadPhysical<3>(vm, base);
      uint64_t free_pages =
          pages - (base == last_base
                       ? 16 + late_pages + symbol_pages + drive_pages
                       : 0);
      Check(block[0] == data_free &&
                block[2] == ((free_pages << 32) | 0x6E55424D),
            "Bad data pool free-list entry");
      data_free = base;
    }
    Check(pool != 0 &&
              ReadPhysical<4>(vm, pool) ==
                  std::array<uint64_t, 4>{0, data_bytes,
                                          0x2000 + late_alloced +
                                              symbol_alloced + drive_alloced,
                                          data_free},
          "Invalid data block pool");
  }

  uint64_t hc_block = pool ? code_base + (high_pages - 3) * page : 0;
  auto block = ReadPhysical<3>(vm, code_base);
  uint64_t remaining = high_pages -
                       (pool ? 3 : late_pages + symbol_pages + drive_pages) -
                       fault_pages;
  Check(block[0] == heap && block[2] == ((remaining << 32) | 0x6E55424D),
        "Invalid high code block");
  Check(ReadPhysical<4>(vm, code_pool) ==
            std::array<uint64_t, 4>{
                0, ram_end - heap + high_pages * page,
                0x44800 +
                    (pool ? 0x600ULL
                          : late_alloced + symbol_alloced + drive_alloced) +
                    fault_alloced,
                code_base},
        "Invalid code block pool");
  if (hc_block) {
    uint64_t data_hc = hc_block + 0x28;
    auto hc = ReadPhysical<12>(vm, data_hc);
    uint64_t allocated = 0x2000 + late_alloced + symbol_alloced + drive_alloced;
    Check(hc[0] == pool && hc[1] == 0x56536348 && !hc[2] &&
              hc[3] == allocated &&
              hc[4] == range_bytes + late_used + symbol_used + drive_used &&
              hc[5] == adam && hc[10] == data_hc + 0x48 && hc[11] == hc[10],
          "Invalid Adam data heap");

    // Registering another symbol can change which small blocks merge.
    uint64_t begin = data_end - allocated, head = data_hc + 0x30, prev = head;
    std::vector<std::pair<uint64_t, uint64_t>> ranges;
    bool mergeable = false;
    for (uint64_t addr = hc[6]; addr != head;) {
      Check(addr >= begin && addr < data_end && !(addr & (page - 1)) &&
                ranges.size() < allocated / page,
            "Adam data heap block outside allocated pages");
      auto blk = ReadPhysical<3>(vm, addr);
      uint64_t bytes = (blk[2] >> 32) * page;
      Check(blk[1] == prev && static_cast<uint32_t>(blk[2]) == 0x7355424D &&
                bytes && bytes <= data_end - addr,
            "Invalid Adam data heap block");
      ranges.emplace_back(addr, addr + bytes);
      mergeable |= addr == hc[8];
      prev = addr;
      addr = blk[0];
    }
    Check(prev == hc[7] && mergeable && hc[9] >= begin + 0x18 &&
              hc[9] <= data_end - 0x10 && !(hc[9] & 7),
          "Invalid Adam data heap links");
    std::sort(ranges.begin(), ranges.end());
    for (auto [lo, hi] : ranges) {
      Check(begin == lo, "Adam data heap blocks do not cover allocated pages");
      begin = hi;
    }
    Check(begin == data_end, "Adam data heap has untracked pages");
  }
  Check(ReadPhysical<2>(vm, adam + 0x20) ==
            std::array<uint64_t, 2>{adam_hc,
                                    hc_block ? hc_block + 0x28 : adam_hc},
        "Adam is using the wrong heap");
  const std::regex pools(
      R"(heap limit (0x[0-9a-f]+), data pool (0x[0-9a-f]+))");
  std::smatch match;
  Check(std::regex_search(output, match, pools) &&
            std::stoull(match[1].str(), nullptr, 16) == limit - 1 &&
            std::stoull(match[2].str(), nullptr, 16) == pool,
        "Incorrect heap limit or data pool");
  return hc_block;
}

static void VerifyFixedArea(CMonitor &vm, uint64_t heap,
                            const std::string &registers,
                            const std::string &output)
{
  constexpr uint64_t fixed = 0x100000, boot_cpu = fixed + 0x200,
                     adam = fixed + 0x300, code_pool = fixed + 0x7A8;
  uint64_t e801 = ReadPhysical<1>(vm, 0x7C40, 'h')[0];
  uint64_t ram_end = fixed + e801 * 1024;
  Check(heap < ram_end, "Code pool starts beyond E801 RAM");
  uint64_t pages = (ram_end - heap) / 512;
  uint64_t top = ram_end - 0x4400;
  const auto e820 = ReadE820(vm);
  uint64_t range_bytes = VerifyDeviceRanges(vm, output, e820);
  uint64_t owner = ReadPhysical<1>(vm, adam + 0x28)[0];
  uint64_t symbol_used = VerifyKernelSymbols(vm, top + 0x28, owner);
  uint64_t hc_block =
      VerifyPools(vm, heap, ram_end, output, range_bytes, symbol_used, e820);
  auto block = ReadPhysical<3>(vm, heap);
  Check(!block[0] && block[2] == (((pages - 0x224) << 32) | 0x6E55424D),
        "Invalid first free block in the code pool");

  Check(ReadPhysical<1>(vm, fixed, 'h')[0] == 0x37F,
        "Missing initial FPU image");
  auto cpu = ReadPhysical<14>(vm, boot_cpu);
  Check(cpu[0] == boot_cpu && !cpu[1] && cpu[5] == 0x3F847AE147AE147C &&
            cpu[7] == adam && cpu[12] == boot_cpu + 0x60 && cpu[13] == cpu[12],
        "Invalid bootstrap CPU record");

  constexpr uint64_t adam_hc = 0x101188, task_alloced = 0x44800;
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

  uint64_t extra =
      hc_block ? 0x600 : late_alloced + symbol_alloced + drive_alloced;
  auto high_block = ReadPhysical<3>(vm, 0x1000000);
  uint64_t code_last = 0x1000000 + (high_block[2] >> 32) * 512;
  uint64_t last_chunk = hc_block ? ram_end - 0x2200 : code_last + drive_alloced;
  auto hc = ReadPhysical<12>(vm, adam_hc);
  Check(hc ==
            std::array<uint64_t, 12>{
                code_pool, 0x56536348, 0, task_alloced + extra + fault_alloced,
                0x42840 +
                    (hc_block
                         ? 0x5E8ULL
                         : range_bytes + late_used + symbol_used + drive_used) +
                    fault_used,
                adam, ram_end - 0x2200, code_last, last_chunk,
                last_chunk + 0x18, adam_hc + 0x48, adam_hc + 0x48},
        "Invalid Adam heap state");
  uint64_t range_heap = hc_block ? hc_block + 0x28 : adam_hc;
  uint64_t cached = ReadPhysical<1>(vm, range_heap + 0x98)[0];
  if (cached)
    Check(!(cached & 7) &&
              ReadPhysical<2>(vm, cached) == std::array<uint64_t, 2>{0, 56},
          "Invalid cached 56-byte heap slot");
}

static void VerifyVGA(const fs::path &path)
{
  std::ifstream file(path, std::ios::binary);
  std::string magic;
  int width, height, depth;
  Check(bool(file >> magic >> width >> height >> depth) && magic == "P6" &&
            width == 640 && height == 480 && depth == 255 && file.get() == '\n',
        "QEMU did not enter 640x480 VGA mode");
  Bytes rgb(640 * 480 * 3);
  Check(bool(file.read(reinterpret_cast<char *>(rgb.data()), rgb.size())) &&
            std::any_of(rgb.begin(), rgb.end(),
                        [](uint8_t byte) { return byte; }),
        "The VGA screenshot is blank or incomplete");
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
    auto boot = rebuilt.Boot(serial);
    auto registers = StopAtIdle(rebuilt);
    VerifyHandoff(boot, media, registers);
    VerifyBootDrive(rebuilt, boot, media, true, false);
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
    std::string output = vm.Boot(serial);
    std::string registers = StopAtIdle(vm);
    uint64_t heap = VerifyHandoff(output, media, registers);
    if (!startup)
      VerifyFixedArea(vm, heap, registers, output);
    VerifyUncachedAlias(vm, output);
    VerifyKernelGlobals(vm, output);
    VerifyInterrupts(vm, output, registers);
    VerifyClock(vm, output, host_bridge, hpet_enabled);
    VerifyBootDrive(vm, output, media, host_bridge == 0x12378086,
                    secondary_slave);
    if (startup) {
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
          output.find("asm build 0 0 0 0\n") != std::string::npos &&
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
      CheckProgramOutput(output, "/DivByHand.BIN.Z", "123456/10=12345\n", '1');
      CheckProgramOutput(output, "/AsmProbe.BIN.Z", "asm import\nasm result 42\n", '1');
      if (std::string_view(memory) == "512")
        for (auto *name : {"AsmProbe.BIN.Z", "DivByHand.BIN.Z",
                           "InlineAsmProbe.BIN.Z", "GotoProbe.BIN.Z"})
          fs::copy_file(CaptureGuestFile(vm, dir, output, name),
                        iso.parent_path() / name,
                        fs::copy_options::overwrite_existing);
      VerifyGuestISO(qemu, vm, dir, iso, kernel, memory, output);
      if (std::string_view(memory) == "4608") {
        std::smatch address;
        Check(std::regex_search(output, address,
                                std::regex(R"(heap address (0x[0-9a-f]+))")) &&
                  std::stoull(address[1].str(), nullptr, 16) >= (1ULL << 32),
              "The module's data allocation did not use RAM above 4 GiB");
      }
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
  if (argc != 4 && argc != 5) {
    std::cerr << "Usage: " << argv[0]
              << " qemu image.iso kernel [startup-test.iso]\n";
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
