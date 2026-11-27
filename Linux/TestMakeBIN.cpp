#include "../Kernel/KernelA.hpp"
#include "BootMedia.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <elf.h>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <set>
#include <sys/wait.h>
#include <unistd.h>

static void Check(bool okay, const std::string &message)
{
  if (!okay)
    throw std::runtime_error(message);
}

template <class T> static T ReadRecord(const Bytes &bytes, size_t offset)
{
  Check(offset <= bytes.size() && sizeof(T) <= bytes.size() - offset,
        "Truncated test input record");
  T record;
  std::memcpy(&record, bytes.data() + offset, sizeof(record));
  return record;
}

template <class T>
static void WriteRecord(Bytes &bytes, size_t offset, const T &record)
{
  Check(offset <= bytes.size() && sizeof(T) <= bytes.size() - offset,
        "Test mutation is outside the input");
  std::memcpy(bytes.data() + offset, &record, sizeof(record));
}

static void FileWrite(const std::filesystem::path &path, const Bytes &bytes)
{
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  Check(file && file.write(reinterpret_cast<const char *>(bytes.data()),
                           static_cast<std::streamsize>(bytes.size())),
        "Cannot write " + path.string());
}

static std::string ReadString(const Bytes &bytes, size_t offset, size_t end)
{
  Check(offset < end && end <= bytes.size(), "Invalid string bounds");
  auto first = bytes.begin() + offset;
  auto last = std::find(first, bytes.begin() + end, uint8_t{0});
  Check(last != bytes.begin() + end, "Unterminated string");
  return std::string(first, last);
}

static int Run(const std::filesystem::path &tool,
               const std::vector<std::string> &arguments,
               const std::filesystem::path &log)
{
  std::vector<std::string> storage{tool.string()};
  storage.insert(storage.end(), arguments.begin(), arguments.end());
  std::vector<char *> argv;
  for (auto &argument : storage)
    argv.push_back(argument.data());
  argv.push_back(nullptr);
  pid_t child = fork();
  Check(child >= 0, "Cannot start MakeBIN");
  if (!child) {
    int fd = open(log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0 || dup2(fd, STDOUT_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0)
      _exit(126);
    close(fd);
    execv(argv[0], argv.data());
    _exit(127);
  }
  int status;
  pid_t result;
  do {
    result = waitpid(child, &status, 0);
  } while (result < 0 && errno == EINTR);
  Check(result == child && WIFEXITED(status),
        "MakeBIN did not exit normally; see " + log.string());
  return WEXITSTATUS(status);
}

static void CheckModule(const Bytes &bytes)
{
  const auto header = ReadRecord<CBinFile>(bytes, 0);
  Check(header.jmp == 0x1EEB && header.reserved == 0 &&
            header.bin_signature == BIN_SIGNATURE_VAL &&
            header.org == INVALID_PTR && header.file_size == bytes.size() &&
            !(header.file_size & 15) && header.module_align_bits >= 6 &&
            header.module_align_bits < 31 &&
            header.patch_table_offset > BIN_HEADER_SIZE &&
            header.patch_table_offset < bytes.size(),
        "Unexpected relocatable TOSB header");
  size_t payload = header.patch_table_offset - BIN_HEADER_SIZE;
  size_t at = header.patch_table_offset, mains = 0, absolute = 0;
  size_t direct_imports = 0, function_pointers = 0, data_pointers = 0;
  std::map<std::string, size_t> exports;
  std::set<size_t> patched;
  std::set<std::string> imports;
  std::string current_import;
  auto Patch = [&](size_t offset, size_t width) {
    Check(offset <= payload && width <= payload - offset,
          "TOSB patch exceeds the module payload");
    for (size_t i = 0; i < width; i++)
      Check(patched.insert(offset + i).second, "TOSB patches overlap");
  };
  while (at < bytes.size() && bytes[at] != IET_END) {
    uint8_t type = bytes[at++];
    uint32_t value = ReadLE(bytes, at, 4);
    at += 4;
    auto name = ReadString(bytes, at, bytes.size());
    at += name.size() + 1;
    if (type == IET_REL_I32 || type == IET_IMM_I64) {
      if (!name.empty())
        current_import = name;
      Check(current_import == "_STRCMP" || current_import == "char_bmp_alpha",
            "Unexpected or missing import name");
      imports.insert(current_import);
      Patch(value, type == IET_REL_I32 ? 4 : 8);
      if (type == IET_REL_I32) {
        Check(current_import == "_STRCMP",
              "Data was imported as a direct call");
        direct_imports++;
      } else if (current_import == "_STRCMP")
        function_pointers++;
      else
        data_pointers++;
      continue;
    }
    current_import.clear();
    if (type == IET_REL32_EXPORT) {
      Check((name == "HostModuleResult" || name == "HostModuleAnchor") &&
                value < payload && exports.emplace(name, value).second,
            "Unexpected or duplicate module export");
    } else if (type == IET_MAIN) {
      Check(name.empty() && value < payload, "Invalid module entry point");
      mains++;
    } else if (type == IET_ABS_ADDR) {
      Check(name.empty() && value > 0 &&
                value <= (bytes.size() - at) / sizeof(uint32_t),
            "Invalid absolute-address table");
      for (uint32_t i = 0; i < value; i++, at += 4) {
        size_t offset = ReadLE(bytes, at, 4);
        Patch(offset, 8);
        Check(ReadLE(bytes, BIN_HEADER_SIZE + offset, 8) < payload,
              "Internal pointer does not address the module payload");
        absolute++;
      }
    } else
      throw std::runtime_error("Unexpected TOSB record type");
  }
  Check(at < bytes.size() && bytes[at] == IET_END && bytes.size() - at <= 16 &&
            std::all_of(
                bytes.begin() + at, bytes.end(),
                [](uint8_t value) { return value == 0; }) &&
            mains == 1 && exports.size() == 2 && imports.size() == 2 &&
            absolute >= 2 && direct_imports && function_pointers &&
            data_pointers >= 2,
        "Module omitted a required entry, export, import, or relocation");
  for (auto [name, width] : {std::pair{"HostModuleResult", size_t{8}},
                             std::pair{"HostModuleAnchor", size_t{40}}}) {
    size_t offset = exports.at(name);
    Check(width <= payload - offset &&
              std::all_of(bytes.begin() + BIN_HEADER_SIZE + offset,
                          bytes.begin() + BIN_HEADER_SIZE + offset + width,
                          [](uint8_t value) { return value == 0; }),
          std::string(name) + " was not initialized to zero");
  }
}

static Elf64_Shdr Section(const Bytes &object, size_t index)
{
  auto header = ReadRecord<Elf64_Ehdr>(object, 0);
  Check(index < header.e_shnum && header.e_shentsize == sizeof(Elf64_Shdr),
        "Invalid fixture section index");
  return ReadRecord<Elf64_Shdr>(object,
                                header.e_shoff + index * sizeof(Elf64_Shdr));
}

struct CRelocation
{
  size_t offset, symbol_offset, symbol_count, string_size, target_size;
};

static CRelocation FindRelocation(const Bytes &object, std::string_view name,
                                  uint32_t type)
{
  auto header = ReadRecord<Elf64_Ehdr>(object, 0);
  for (size_t i = 0; i < header.e_shnum; i++) {
    auto section = Section(object, i);
    if (section.sh_type != SHT_RELA)
      continue;
    auto symbols = Section(object, section.sh_link);
    auto strings = Section(object, symbols.sh_link);
    for (size_t at = section.sh_offset;
         at < section.sh_offset + section.sh_size; at += sizeof(Elf64_Rela)) {
      auto relocation = ReadRecord<Elf64_Rela>(object, at);
      size_t symbol_offset = symbols.sh_offset +
                             ELF64_R_SYM(relocation.r_info) * sizeof(Elf64_Sym);
      auto symbol = ReadRecord<Elf64_Sym>(object, symbol_offset);
      if (symbol.st_shndx == SHN_UNDEF &&
          ELF64_R_TYPE(relocation.r_info) == type &&
          ReadString(object, strings.sh_offset + symbol.st_name,
                     strings.sh_offset + strings.sh_size) == name)
        return {at, symbol_offset, symbols.sh_size / sizeof(Elf64_Sym),
                strings.sh_size, Section(object, section.sh_info).sh_size};
    }
  }
  throw std::runtime_error("Fixture is missing a required ELF relocation");
}

static std::string Replace(std::string text, std::string_view from,
                           std::string_view to)
{
  auto at = text.find(from);
  Check(at != std::string::npos, "Fixture binding to replace was not found");
  text.replace(at, from.size(), to);
  return text;
}

int main(int argc, char **argv)
{
  if (argc != 5) {
    std::cerr
        << "Usage: " << argv[0]
        << " MakeBIN HostModuleProbe.o HostModuleProbe.bindings output-dir\n";
    return 2;
  }
  try {
    auto tool = std::filesystem::absolute(argv[1]);
    auto input = std::filesystem::absolute(argv[2]);
    auto binding_path = std::filesystem::absolute(argv[3]);
    auto directory = std::filesystem::absolute(argv[4]);
    std::filesystem::create_directories(directory);
    auto output = directory / "valid module.BIN";
    auto log = directory / "valid.log";
    Check(Run(tool, {input.string(), binding_path.string(), output.string()},
              log) == 0,
          "Valid module conversion failed; see " + log.string());
    auto module = FileRead(output);
    CheckModule(module);
    Check(Run(tool, {input.string(), binding_path.string(), output.string()},
              directory / "repeat.log") == 0 &&
              FileRead(output) == module,
          "Repeated module conversion changed the bytes");

    const auto object = FileRead(input);
    auto binding_bytes = FileRead(binding_path);
    std::string bindings(binding_bytes.begin(), binding_bytes.end());
    std::erase(bindings, '\r');
    const Bytes retained{'E', 'x', 'i', 's', 't', 'i', 'n', 'g', 0, 0xFF, 0xA5};
    auto rejected_output = directory / "preserved.BIN";
    size_t rejected = 0;
    auto Reject = [&](const std::string &name,
                      const std::filesystem::path &bad_input,
                      const std::filesystem::path &bad_bindings) {
      FileWrite(rejected_output, retained);
      Check(Run(tool,
                {bad_input.string(), bad_bindings.string(),
                 rejected_output.string()},
                directory / (name + ".log")) != 0,
            "MakeBIN accepted " + name);
      Check(FileRead(rejected_output) == retained,
            "MakeBIN replaced existing output after rejecting " + name);
      rejected++;
    };
    auto RejectObject = [&](const std::string &name, const Bytes &bad) {
      auto path = directory / (name + ".o");
      FileWrite(path, bad);
      Reject(name, path, binding_path);
    };
    auto RejectBindings = [&](const std::string &name, const std::string &bad) {
      auto path = directory / (name + ".bindings");
      FileWrite(path, Bytes(bad.begin(), bad.end()));
      Reject(name, input, path);
    };
    auto RejectHeader = [&](const std::string &name, auto change) {
      auto bad = object;
      auto header = ReadRecord<Elf64_Ehdr>(bad, 0);
      change(header);
      WriteRecord(bad, 0, header);
      RejectObject(name, bad);
    };

    RejectObject("empty", {});
    RejectObject(
        "truncated-header",
        Bytes(object.begin(), object.begin() + sizeof(Elf64_Ehdr) - 1));
    auto header = ReadRecord<Elf64_Ehdr>(object, 0);
    size_t section_end = header.e_shoff + header.e_shnum * sizeof(Elf64_Shdr);
    Check(section_end <= object.size(), "Fixture section table is truncated");
    RejectObject("truncated-sections",
                 Bytes(object.begin(), object.begin() + section_end - 1));
    RejectHeader("bad-magic", [](auto &h) { h.e_ident[EI_MAG0] = 0; });
    RejectHeader("wrong-machine", [](auto &h) { h.e_machine = EM_386; });
    RejectHeader("wrong-endian",
                 [](auto &h) { h.e_ident[EI_DATA] = ELFDATA2MSB; });
    RejectHeader("not-relocatable", [](auto &h) { h.e_type = ET_EXEC; });
    RejectHeader("overflow-sections", [](auto &h) { h.e_shoff = UINT64_MAX; });

    size_t allocated = 0;
    for (size_t i = 1; i < header.e_shnum && !allocated; i++) {
      auto section = Section(object, i);
      if (section.sh_type == SHT_PROGBITS && (section.sh_flags & SHF_ALLOC) &&
          section.sh_size)
        allocated = i;
    }
    Check(allocated != 0, "Fixture has no allocated data or code");
    auto RejectSection = [&](const std::string &name, auto change) {
      auto bad = object;
      auto section = Section(bad, allocated);
      change(section);
      WriteRecord(bad, header.e_shoff + allocated * sizeof(Elf64_Shdr),
                  section);
      RejectObject(name, bad);
    };
    RejectSection("overflow-section",
                  [](auto &s) { s.sh_offset = UINT64_MAX; });
    RejectSection("invalid-alignment", [](auto &s) { s.sh_addralign = 3; });
    RejectSection("tls-section", [](auto &s) { s.sh_flags |= SHF_TLS; });
    RejectSection("allocated-note", [](auto &s) { s.sh_type = SHT_NOTE; });
    RejectSection("init-array", [](auto &s) { s.sh_type = SHT_INIT_ARRAY; });

    auto absolute = FindRelocation(object, "_STRCMP", R_X86_64_64);
    auto relative = FindRelocation(object, "_STRCMP", R_X86_64_PC32);
    auto RejectRelocation = [&](const std::string &name,
                                const CRelocation &location, auto change) {
      auto bad = object;
      auto relocation = ReadRecord<Elf64_Rela>(bad, location.offset);
      change(relocation);
      WriteRecord(bad, location.offset, relocation);
      RejectObject(name, bad);
    };
    RejectRelocation("external-addend", absolute,
                     [](auto &r) { r.r_addend = 8; });
    RejectRelocation("external-relative-addend", relative,
                     [](auto &r) { r.r_addend = -3; });
    RejectRelocation("unsupported-relocation", absolute, [](auto &r) {
      r.r_info = ELF64_R_INFO(ELF64_R_SYM(r.r_info), R_X86_64_COPY);
    });
    RejectRelocation("invalid-symbol", absolute, [&](auto &r) {
      r.r_info = ELF64_R_INFO(absolute.symbol_count, ELF64_R_TYPE(r.r_info));
    });
    RejectRelocation("outside-section", absolute,
                     [&](auto &r) { r.r_offset = absolute.target_size; });
    auto bad_symbol = object;
    auto symbol = ReadRecord<Elf64_Sym>(bad_symbol, absolute.symbol_offset);
    symbol.st_name = absolute.string_size;
    WriteRecord(bad_symbol, absolute.symbol_offset, symbol);
    RejectObject("invalid-symbol-name", bad_symbol);

    RejectBindings("missing-entry",
                   Replace(bindings, "entry HostModuleMain\n", ""));
    RejectBindings("duplicate-entry", bindings + "entry HostModuleMain\n");
    RejectBindings("unknown-entry",
                   Replace(bindings, "entry HostModuleMain", "entry Missing"));
    RejectBindings("data-entry", Replace(bindings, "entry HostModuleMain",
                                         "entry HostModuleResult"));
    RejectBindings(
        "missing-import",
        Replace(bindings, "import data char_bmp_alpha char_bmp_alpha\n", ""));
    RejectBindings("unused-import", bindings + "import data Missing Missing\n");
    RejectBindings("native-without-namespace",
                   Replace(bindings, "import holyc _STRCMP _STRCMP",
                           "import native _STRCMP _STRCMP"));
    RejectBindings("unknown-export", bindings + "export Missing Missing\n");
    RejectBindings("duplicate-export",
                   bindings + "export HostModuleResult HostModuleResult\n");
    RejectBindings("unknown-directive", bindings + "unrecognized value\n");
    auto missing_bindings = directory / "missing.bindings";
    Check(!std::filesystem::exists(missing_bindings),
          "Missing-file test path already exists");
    Reject("missing-bindings-file", input, missing_bindings);

    std::cout << "Verified host ELF module conversion, TOSB bounds, "
              << rejected
              << " rejection cases, and preservation of existing output.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "TestMakeBIN: " << error.what() << '\n';
    return 1;
  }
}
