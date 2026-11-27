#include "../Kernel/KernelA.hpp"
#include "BootMedia.hpp"

#include <algorithm>
#include <bit>
#include <cerrno>
#include <cstring>
#include <elf.h>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <system_error>
#include <unistd.h>

namespace fs = std::filesystem;

// Kernel/KLoad.HC loads relocatable images into the code heap below 2 GiB.
// IET_ABS_ADDR adjusts a low dword; every internal pointer remains in that
// complete allocation. Loading these images at a caller-supplied origin is
// outside this producer's contract.
constexpr uint64_t CODE_LIMIT = uint64_t{1} << 31;
constexpr std::string_view NATIVE_PREFIX = "__unholyos_sysv.";
static_assert(std::endian::native == std::endian::little);
static_assert(sizeof(Elf64_Ehdr) == 64 && sizeof(Elf64_Shdr) == 64 &&
              sizeof(Elf64_Sym) == 24 && sizeof(Elf64_Rela) == 24);

struct CElfSection
{
  Elf64_Shdr header;
  std::string name;
  uint64_t offset = 0;
  bool allocated = false;
};

struct CElfSymbol
{
  Elf64_Sym header;
  std::string name;
};

struct CElfFile
{
  Bytes bytes;
  std::vector<CElfSection> sections;
  std::vector<CElfSymbol> symbols;
  size_t symtab = 0;
};

enum class CImportKind
{
  Native,
  HolyC,
  Data
};

struct CBinding
{
  std::string symbol_name, guest_name;
  size_t line = 0, symbol = SIZE_MAX;
  CImportKind kind = CImportKind::Data;
  bool used = false;
};

struct CBindings
{
  CBinding entry;
  std::vector<CBinding> exports, imports;
  std::map<size_t, size_t> import_symbols;
};

struct CRelocation
{
  size_t section, symbol;
  uint64_t section_offset;
  uint32_t offset, type;
  int64_t addend;
  unsigned width;
};

struct CBinWrite
{
  uint32_t offset;
  unsigned width;
  uint64_t value;
};

struct CImportPatch
{
  uint32_t offset;
  uint8_t type;
};

struct CBinImage
{
  uint64_t payload_size = 0, alignment = 16;
  std::map<size_t, uint32_t> got;
  std::vector<CBinWrite> writes;
  std::vector<uint32_t> absolute;
  std::vector<std::vector<CImportPatch>> imports;
};

[[noreturn]] static void Fail(const std::string &message)
{
  throw std::runtime_error(message);
}

static std::string Hex(uint64_t value)
{
  std::ostringstream out;
  out << "0x" << std::hex << value;
  return out.str();
}

static void CheckRange(uint64_t offset, uint64_t size, uint64_t limit,
                       const std::string &where)
{
  if (offset > limit || size > limit - offset)
    Fail(where + ": extent is out of bounds");
}

static uint64_t AddSize(uint64_t value, uint64_t amount,
                        const std::string &where)
{
  if (value >= CODE_LIMIT || amount >= CODE_LIMIT - value)
    Fail(where + ": complete module allocation must remain below 2 GiB");
  return value + amount;
}

static uint64_t Align(uint64_t value, uint64_t alignment,
                      const std::string &where)
{
  if (!alignment || (alignment & (alignment - 1)) || alignment >= CODE_LIMIT)
    Fail(where + ": unsupported alignment " + Hex(alignment));
  return AddSize(value, alignment - 1, where) & ~(alignment - 1);
}

static Bytes ReadFile(const fs::path &path)
{
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  auto length = file.tellg();
  if (length < 0)
    Fail("Cannot open " + path.string());
  if (static_cast<uint64_t>(length) >= CODE_LIMIT)
    Fail(path.string() + ": input is too large");
  Bytes bytes(static_cast<size_t>(length));
  file.seekg(0);
  if (!bytes.empty() &&
      !file.read(reinterpret_cast<char *>(bytes.data()), length))
    Fail("Cannot read " + path.string());
  return bytes;
}

template <class T>
static T ReadRecord(const Bytes &bytes, uint64_t offset,
                    const std::string &where)
{
  CheckRange(offset, sizeof(T), bytes.size(), where);
  T record;
  std::memcpy(&record, bytes.data() + offset, sizeof(record));
  return record;
}

static std::string ReadString(const Bytes &bytes, const Elf64_Shdr &table,
                              uint64_t offset, const std::string &where)
{
  if (offset >= table.sh_size)
    Fail(where + ": string offset is out of bounds");
  auto begin = bytes.begin() + table.sh_offset + offset;
  auto end = bytes.begin() + table.sh_offset + table.sh_size;
  auto nul = std::find(begin, end, uint8_t{0});
  if (nul == end)
    Fail(where + ": unterminated string");
  return {begin, nul};
}

static std::string SectionName(const CElfSection &section, size_t index)
{
  return "section " + std::to_string(index) + " (" + section.name + ")";
}

static CElfFile ReadElf(const fs::path &path)
{
  CElfFile elf;
  elf.bytes = ReadFile(path);
  const auto header = ReadRecord<Elf64_Ehdr>(elf.bytes, 0, "ELF header");
  if (std::memcmp(header.e_ident, ELFMAG, SELFMAG) ||
      header.e_ident[EI_CLASS] != ELFCLASS64 ||
      header.e_ident[EI_DATA] != ELFDATA2LSB ||
      header.e_ident[EI_VERSION] != EV_CURRENT ||
      header.e_version != EV_CURRENT || header.e_type != ET_REL ||
      header.e_machine != EM_X86_64)
    Fail("Expected an x86-64 little-endian ELF64 ET_REL object");
  if (header.e_ehsize != sizeof(header) || header.e_phoff || header.e_phnum ||
      header.e_phentsize || header.e_entry || header.e_flags)
    Fail("Unsupported ELF header layout or execution metadata");
  if (header.e_shentsize != sizeof(Elf64_Shdr) || !header.e_shnum ||
      header.e_shnum >= SHN_LORESERVE || header.e_shstrndx == SHN_UNDEF ||
      header.e_shstrndx >= header.e_shnum || header.e_shoff < sizeof(header))
    Fail("Unsupported ELF section table or extended section numbering");
  CheckRange(header.e_shoff, uint64_t{header.e_shnum} * sizeof(Elf64_Shdr),
             elf.bytes.size(), "ELF section table");
  elf.sections.reserve(header.e_shnum);
  for (size_t i = 0; i < header.e_shnum; i++) {
    auto sh = ReadRecord<Elf64_Shdr>(elf.bytes,
                                     header.e_shoff + i * sizeof(Elf64_Shdr),
                                     "ELF section table");
    if (sh.sh_type != SHT_NOBITS)
      CheckRange(sh.sh_offset, sh.sh_size, elf.bytes.size(),
                 "ELF section " + std::to_string(i));
    if (sh.sh_addralign && (sh.sh_addralign & (sh.sh_addralign - 1)))
      Fail("ELF section " + std::to_string(i) +
           ": alignment is not a power of two");
    elf.sections.push_back({sh, {}, 0, bool(sh.sh_flags & SHF_ALLOC)});
  }
  const Elf64_Shdr zero{};
  if (std::memcmp(&elf.sections[0].header, &zero, sizeof(zero)))
    Fail("ELF null section is not empty");
  const auto &names = elf.sections[header.e_shstrndx].header;
  if (names.sh_type != SHT_STRTAB || !names.sh_size)
    Fail("Invalid ELF section-name string table");

  size_t symtabs = 0;
  for (size_t i = 1; i < elf.sections.size(); i++) {
    auto &section = elf.sections[i];
    const auto &sh = section.header;
    section.name = ReadString(elf.bytes, names, sh.sh_name, "ELF section name");
    const auto where = SectionName(section, i);
    if (sh.sh_flags & SHF_TLS)
      Fail(where + ": TLS is unsupported");
    if (sh.sh_type == SHT_INIT_ARRAY || sh.sh_type == SHT_FINI_ARRAY ||
        sh.sh_type == SHT_PREINIT_ARRAY)
      Fail(where + ": constructor and destructor arrays are unsupported");
    if (sh.sh_type == SHT_GROUP || (sh.sh_flags & SHF_GROUP))
      Fail(where +
           ": combine objects with ld -r --force-group-allocation first");
    if (sh.sh_type == SHT_DYNSYM || sh.sh_type == SHT_SYMTAB_SHNDX)
      Fail(where + ": unsupported symbol-table semantics");
    if (section.allocated) {
      constexpr uint64_t flags =
          SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR | SHF_MERGE | SHF_STRINGS;
      if ((sh.sh_flags & ~flags) || sh.sh_addr ||
          (sh.sh_type != SHT_PROGBITS && sh.sh_type != SHT_NOBITS))
        Fail(where + ": unsupported allocated section semantics");
      if (section.name == ".init" || section.name == ".fini" ||
          section.name.starts_with(".ctors") ||
          section.name.starts_with(".dtors") ||
          section.name.starts_with(".eh_frame") ||
          section.name.starts_with(".gcc_except_table"))
        Fail(where + ": implicit initialization or unwinding is unsupported");
      if ((sh.sh_flags & SHF_STRINGS) && !(sh.sh_flags & SHF_MERGE))
        Fail(where + ": string section is missing SHF_MERGE");
      if ((sh.sh_flags & SHF_MERGE) &&
          (!sh.sh_entsize || sh.sh_size % sh.sh_entsize))
        Fail(where + ": invalid merge element size");
      if (sh.sh_type == SHT_NOBITS &&
          (sh.sh_flags & (SHF_EXECINSTR | SHF_MERGE)))
        Fail(where + ": unsupported zero-filled section flags");
    }
    if (sh.sh_type == SHT_SYMTAB) {
      elf.symtab = i;
      symtabs++;
    }
  }
  if (symtabs != 1)
    Fail("Expected exactly one ELF static symbol table");
  const auto &symtab = elf.sections[elf.symtab].header;
  if (symtab.sh_entsize != sizeof(Elf64_Sym) || !symtab.sh_size ||
      symtab.sh_size % sizeof(Elf64_Sym) ||
      symtab.sh_link >= elf.sections.size())
    Fail("Invalid ELF symbol table extent or string-table link");
  const auto &strings = elf.sections[symtab.sh_link].header;
  if (strings.sh_type != SHT_STRTAB || !strings.sh_size)
    Fail("Invalid ELF symbol-name string table");
  const size_t count = symtab.sh_size / sizeof(Elf64_Sym);
  if (!symtab.sh_info || symtab.sh_info > count)
    Fail("Invalid ELF local-symbol boundary");
  elf.symbols.reserve(count);
  for (size_t i = 0; i < count; i++) {
    const auto symbol = ReadRecord<Elf64_Sym>(
        elf.bytes, symtab.sh_offset + i * sizeof(Elf64_Sym),
        "ELF symbol table");
    const auto name =
        ReadString(elf.bytes, strings, symbol.st_name, "ELF symbol name");
    const auto where = "symbol " + std::to_string(i) + " (" + name + ")";
    const unsigned type = ELF64_ST_TYPE(symbol.st_info);
    const unsigned binding = ELF64_ST_BIND(symbol.st_info);
    if (type == STT_TLS)
      Fail(where + ": TLS is unsupported");
    if (type == STT_GNU_IFUNC)
      Fail(where + ": IFUNC is unsupported");
    if (type != STT_NOTYPE && type != STT_OBJECT && type != STT_FUNC &&
        type != STT_SECTION && type != STT_FILE)
      Fail(where + ": unsupported ELF symbol type");
    if ((binding != STB_LOCAL && binding != STB_GLOBAL &&
         binding != STB_WEAK) ||
        (symbol.st_other & ~3U) ||
        (i < symtab.sh_info) != (binding == STB_LOCAL))
      Fail(where + ": unsupported symbol binding or visibility");
    if (symbol.st_shndx == SHN_UNDEF) {
      if (symbol.st_value || symbol.st_size)
        Fail(where + ": undefined symbol has a value or extent");
    } else if (symbol.st_shndx == SHN_ABS) {
      // File names and assembler constants may be absolute metadata. A
      // relocation or manifest binding to them is rejected separately.
    } else if (symbol.st_shndx >= elf.sections.size()) {
      Fail(where + ": unsupported or invalid symbol section index");
    } else {
      CheckRange(symbol.st_value, symbol.st_size,
                 elf.sections[symbol.st_shndx].header.sh_size, where);
    }
    elf.symbols.push_back({symbol, name});
  }
  const Elf64_Sym null_symbol{};
  if (std::memcmp(&elf.symbols[0].header, &null_symbol, sizeof(null_symbol)))
    Fail("ELF null symbol is not empty");
  return elf;
}

static std::string BindingName(const CBinding &binding)
{
  return "bindings line " + std::to_string(binding.line) + " (" +
         binding.symbol_name + ")";
}

static CBindings ReadBindings(const fs::path &path)
{
  const auto bytes = ReadFile(path);
  std::istringstream input(std::string(bytes.begin(), bytes.end()));
  CBindings bindings;
  std::set<std::string> imports, exports, guests;
  std::string line;
  size_t line_num = 0;
  while (std::getline(input, line)) {
    line_num++;
    if (line.find('\0') != line.npos)
      Fail("bindings line " + std::to_string(line_num) + ": embedded NUL");
    if (const auto comment = line.find('#'); comment != line.npos)
      line.resize(comment);
    std::istringstream fields(line);
    std::vector<std::string> words;
    for (std::string word; fields >> word;)
      words.push_back(std::move(word));
    if (words.empty())
      continue;
    const auto where = "bindings line " + std::to_string(line_num);
    for (const auto &word : words)
      for (unsigned char ch : word)
        if (ch < 32 || ch == 127)
          Fail(where + ": control byte in a name");
    if (words[0] == "entry") {
      if (words.size() != 2 || bindings.entry.line)
        Fail(where + ": expected exactly one entry <ELF symbol>");
      bindings.entry = {words[1], {}, line_num};
    } else if (words[0] == "export") {
      if (words.size() != 3)
        Fail(where + ": expected export <ELF symbol> <guest name>");
      if (!exports.insert(words[1]).second || !guests.insert(words[2]).second)
        Fail(where + ": duplicate or conflicting export binding");
      bindings.exports.push_back({words[1], words[2], line_num});
    } else if (words[0] == "import") {
      if (words.size() != 4)
        Fail(where +
             ": expected import <native|holyc|data> <ELF symbol> <guest name>");
      CImportKind kind;
      if (words[1] == "native")
        kind = CImportKind::Native;
      else if (words[1] == "holyc")
        kind = CImportKind::HolyC;
      else if (words[1] == "data")
        kind = CImportKind::Data;
      else
        Fail(where + ": unknown import ABI " + words[1]);
      if (kind == CImportKind::Native &&
          (!words[3].starts_with(NATIVE_PREFIX) ||
           words[3].size() == NATIVE_PREFIX.size()))
        Fail(where + ": native imports require the __unholyos_sysv. namespace");
      if (kind == CImportKind::HolyC && words[3].starts_with(NATIVE_PREFIX))
        Fail(where + ": HolyC import conflicts with the native namespace");
      if (!imports.insert(words[2]).second || !guests.insert(words[3]).second)
        Fail(where + ": duplicate or conflicting import binding");
      bindings.imports.push_back(
          {words[2], words[3], line_num, SIZE_MAX, kind});
    } else
      Fail(where + ": unknown directive " + words[0]);
  }
  if (!bindings.entry.line)
    Fail("Bindings require exactly one entry <ELF symbol>");
  for (const auto &name : imports)
    if (exports.contains(name) || name == bindings.entry.symbol_name)
      Fail("Conflicting definition and import binding for " + name);
  return bindings;
}

static size_t FindSymbol(const CElfFile &elf, const CBinding &binding)
{
  size_t found = SIZE_MAX;
  for (size_t i = 1; i < elf.symbols.size(); i++) {
    if (elf.symbols[i].name != binding.symbol_name)
      continue;
    if (found != SIZE_MAX)
      Fail(BindingName(binding) + ": ambiguous ELF symbol name");
    found = i;
  }
  if (found == SIZE_MAX)
    Fail(BindingName(binding) + ": ELF symbol not found");
  return found;
}

static uint64_t SymbolOffset(const CElfFile &elf, size_t index,
                             const std::string &where)
{
  const auto &symbol = elf.symbols[index];
  const auto &s = symbol.header;
  if (!s.st_shndx || s.st_shndx >= elf.sections.size() ||
      !elf.sections[s.st_shndx].allocated)
    Fail(where + ": symbol " + symbol.name +
         " is not in the allocated payload");
  if (ELF64_ST_TYPE(s.st_info) == STT_FILE)
    Fail(where + ": file symbol is not an address");
  return elf.sections[s.st_shndx].offset + s.st_value;
}

static void BindSymbols(const CElfFile &elf, CBindings &bindings)
{
  auto bind_export = [&elf](CBinding &binding, bool entry) {
    binding.symbol = FindSymbol(elf, binding);
    const auto &s = elf.symbols[binding.symbol].header;
    const auto where = BindingName(binding);
    SymbolOffset(elf, binding.symbol, where);
    const auto &section = elf.sections[s.st_shndx];
    if (s.st_value >= section.header.sh_size ||
        (ELF64_ST_TYPE(s.st_info) != STT_NOTYPE &&
         ELF64_ST_TYPE(s.st_info) != STT_FUNC &&
         ELF64_ST_TYPE(s.st_info) != STT_OBJECT))
      Fail(where + ": invalid entry or export target");
    if (entry && (!(section.header.sh_flags & SHF_EXECINSTR) ||
                  ELF64_ST_TYPE(s.st_info) == STT_OBJECT))
      Fail(where + ": entry must name executable code");
  };
  bind_export(bindings.entry, true);
  for (auto &binding : bindings.exports)
    bind_export(binding, false);
  for (size_t i = 0; i < bindings.imports.size(); i++) {
    auto &binding = bindings.imports[i];
    binding.symbol = FindSymbol(elf, binding);
    const auto &s = elf.symbols[binding.symbol].header;
    const unsigned type = ELF64_ST_TYPE(s.st_info);
    const auto where = BindingName(binding);
    if (s.st_shndx != SHN_UNDEF ||
        (type != STT_NOTYPE && type != STT_OBJECT && type != STT_FUNC))
      Fail(where + ": import must name an undefined function or data symbol");
    if ((binding.kind == CImportKind::Data && type == STT_FUNC) ||
        (binding.kind != CImportKind::Data && type == STT_OBJECT))
      Fail(where + ": import ABI conflicts with the ELF symbol type");
    bindings.import_symbols.emplace(binding.symbol, i);
  }
}

static bool IsGot(uint32_t type)
{
  return type == R_X86_64_GOTPCREL || type == R_X86_64_GOTPCRELX ||
         type == R_X86_64_REX_GOTPCRELX;
}

static std::string RelocationName(uint32_t type)
{
  switch (type) {
  case R_X86_64_64:
    return "R_X86_64_64";
  case R_X86_64_PC32:
    return "R_X86_64_PC32";
  case R_X86_64_PLT32:
    return "R_X86_64_PLT32";
  case R_X86_64_GOTPCREL:
    return "R_X86_64_GOTPCREL";
  case R_X86_64_GOTPCRELX:
    return "R_X86_64_GOTPCRELX";
  case R_X86_64_REX_GOTPCRELX:
    return "R_X86_64_REX_GOTPCRELX";
  default:
    return "ELF relocation type " + std::to_string(type);
  }
}

static std::string RelocationWhere(const CElfFile &elf, const CRelocation &rel)
{
  return SectionName(elf.sections[rel.section], rel.section) + "+" +
         Hex(rel.section_offset) + ": " + RelocationName(rel.type) + " " +
         elf.symbols[rel.symbol].name + " addend " + std::to_string(rel.addend);
}

static std::vector<CRelocation> ReadRelocations(const CElfFile &elf,
                                                CBindings &bindings)
{
  std::vector<CRelocation> result;
  for (size_t i = 1; i < elf.sections.size(); i++) {
    const auto &section = elf.sections[i];
    const auto &sh = section.header;
    if (sh.sh_type == SHT_REL)
      Fail(SectionName(section, i) +
           ": REL relocations are unsupported; expected RELA");
    if (sh.sh_type != SHT_RELA)
      continue;
    const auto where = SectionName(section, i);
    if (sh.sh_link != elf.symtab || !sh.sh_info ||
        sh.sh_info >= elf.sections.size() ||
        sh.sh_entsize != sizeof(Elf64_Rela) || sh.sh_size % sizeof(Elf64_Rela))
      Fail(where + ": invalid RELA table link, target, or extent");
    const auto &target = elf.sections[sh.sh_info];
    for (uint64_t p = 0; p < sh.sh_size; p += sizeof(Elf64_Rela)) {
      const auto rel =
          ReadRecord<Elf64_Rela>(elf.bytes, sh.sh_offset + p, where);
      const size_t symbol = ELF64_R_SYM(rel.r_info);
      const uint32_t type = ELF64_R_TYPE(rel.r_info);
      if (symbol >= elf.symbols.size() || rel.r_offset > target.header.sh_size)
        Fail(where + ": relocation symbol or target is out of bounds");
      if (!target.allocated || type == R_X86_64_NONE)
        continue;
      CRelocation record{sh.sh_info,
                         symbol,
                         rel.r_offset,
                         static_cast<uint32_t>(target.offset + rel.r_offset),
                         type,
                         rel.r_addend,
                         type == R_X86_64_64 ? 8U : 4U};
      const auto location = RelocationWhere(elf, record);
      if (type != R_X86_64_64 && type != R_X86_64_PC32 &&
          type != R_X86_64_PLT32 && !IsGot(type))
        Fail(location + ": unsupported relocation");
      CheckRange(rel.r_offset, record.width, target.header.sh_size, location);
      if (!symbol)
        Fail(location + ": relocation references the null symbol");
      const auto &sym = elf.symbols[symbol];
      if (sym.header.st_shndx == SHN_UNDEF) {
        const auto found = bindings.import_symbols.find(symbol);
        if (found == bindings.import_symbols.end())
          Fail(location + ": missing explicit import binding");
        auto &binding = bindings.imports[found->second];
        binding.used = true;
        if (type == R_X86_64_64 && rel.r_addend != 0)
          Fail(location +
               ": unsupported external absolute addend (expected zero)");
        if ((type == R_X86_64_PC32 || type == R_X86_64_PLT32) &&
            rel.r_addend != -4)
          Fail(location +
               ": unsupported external relative addend (expected -4)");
        if (type == R_X86_64_PLT32 && binding.kind == CImportKind::Data)
          Fail(location + ": PLT call conflicts with a data import binding");
      } else
        SymbolOffset(elf, symbol, location);
      result.push_back(record);
    }
  }
  std::sort(result.begin(), result.end(),
            [](const auto &a, const auto &b) { return a.offset < b.offset; });
  for (size_t i = 1; i < result.size(); i++)
    if (uint64_t{result[i - 1].offset} + result[i - 1].width > result[i].offset)
      Fail(RelocationWhere(elf, result[i]) + ": overlapping relocation fields");
  for (const auto &binding : bindings.imports)
    if (!binding.used)
      Fail(BindingName(binding) + ": unused import binding");
  return result;
}

static uint32_t RelativeValue(uint64_t target, uint64_t place, int64_t addend,
                              const std::string &where)
{
  const int64_t difference =
      static_cast<int64_t>(target) - static_cast<int64_t>(place);
  if (addend < int64_t{INT32_MIN} - difference ||
      addend > int64_t{INT32_MAX} - difference)
    Fail(where + ": signed 32-bit relative relocation overflow");
  return static_cast<uint32_t>(difference + addend);
}

static uint64_t AbsoluteValue(uint64_t target, int64_t addend, uint64_t size,
                              const std::string &where)
{
  if (addend < -static_cast<int64_t>(target) ||
      addend >= static_cast<int64_t>(size) - static_cast<int64_t>(target))
    Fail(where + ": internal absolute target is outside the payload");
  return static_cast<uint64_t>(static_cast<int64_t>(target) + addend);
}

static CBinImage MakeLayout(CElfFile &elf, CBindings &bindings)
{
  CBinImage image;
  for (size_t i = 1; i < elf.sections.size(); i++) {
    auto &section = elf.sections[i];
    if (!section.allocated)
      continue;
    const auto where = SectionName(section, i);
    const uint64_t alignment =
        std::max<uint64_t>(1, section.header.sh_addralign);
    image.alignment = std::max(image.alignment, alignment);
    section.offset = Align(image.payload_size, alignment, where);
    image.payload_size = AddSize(section.offset, section.header.sh_size, where);
  }
  BindSymbols(elf, bindings);
  const auto relocations = ReadRelocations(elf, bindings);
  image.imports.resize(bindings.imports.size());
  for (const auto &rel : relocations)
    if (IsGot(rel.type) && !image.got.contains(rel.symbol)) {
      image.payload_size = Align(image.payload_size, 8, "GOT");
      const auto offset = static_cast<uint32_t>(image.payload_size);
      image.payload_size = AddSize(image.payload_size, 8, "GOT");
      image.got.emplace(rel.symbol, offset);
    }
  for (const auto &[symbol, offset] : image.got) {
    if (elf.symbols[symbol].header.st_shndx == SHN_UNDEF) {
      image.imports.at(bindings.import_symbols.at(symbol))
          .push_back({offset, IET_IMM_I64});
    } else {
      const auto value = AbsoluteValue(SymbolOffset(elf, symbol, "GOT"), 0,
                                       image.payload_size, "GOT");
      image.writes.push_back({offset, 8, value});
      image.absolute.push_back(offset);
    }
  }
  for (const auto &rel : relocations) {
    const auto where = RelocationWhere(elf, rel);
    uint64_t value = 0;
    if (IsGot(rel.type)) {
      value = RelativeValue(image.got.at(rel.symbol), rel.offset, rel.addend,
                            where);
    } else if (elf.symbols[rel.symbol].header.st_shndx == SHN_UNDEF) {
      const uint8_t type = rel.type == R_X86_64_64 ? IET_IMM_I64 : IET_REL_I32;
      image.imports.at(bindings.import_symbols.at(rel.symbol))
          .push_back({rel.offset, type});
    } else {
      const auto target = SymbolOffset(elf, rel.symbol, where);
      if (rel.type == R_X86_64_64) {
        value = AbsoluteValue(target, rel.addend, image.payload_size, where);
        image.absolute.push_back(rel.offset);
      } else
        value = RelativeValue(target, rel.offset, rel.addend, where);
    }
    image.writes.push_back({rel.offset, rel.width, value});
  }
  std::sort(image.absolute.begin(), image.absolute.end());
  for (auto &imports : image.imports)
    std::sort(imports.begin(), imports.end(),
              [](const auto &a, const auto &b) { return a.offset < b.offset; });
  return image;
}

static void WriteLE(Bytes &bytes, size_t offset, uint64_t value, size_t width)
{
  CheckRange(offset, width, bytes.size(), "BIN output");
  for (size_t i = 0; i < width; i++) {
    bytes[offset + i] = static_cast<uint8_t>(value);
    value >>= 8;
  }
}

static size_t PatchRecord(Bytes &bytes, size_t offset, uint8_t type,
                          uint32_t value, std::string_view name)
{
  CheckRange(offset, 6 + name.size(), bytes.size(), "BIN patch record");
  bytes[offset] = type;
  WriteLE(bytes, offset + 1, value, 4);
  std::copy(name.begin(), name.end(), bytes.begin() + offset + 5);
  bytes[offset + 5 + name.size()] = 0;
  return offset + 6 + name.size();
}

static Bytes MakeBin(const CElfFile &elf, const CBindings &bindings,
                     const CBinImage &image)
{
  uint64_t patch_size = 7; // IET_MAIN followed by IET_END.
  for (const auto &binding : bindings.exports)
    patch_size =
        AddSize(patch_size, 6 + binding.guest_name.size(), "BIN exports");
  if (!image.absolute.empty())
    patch_size = AddSize(patch_size, 6 + uint64_t{image.absolute.size()} * 4,
                         "BIN absolute patches");
  for (size_t i = 0; i < image.imports.size(); i++)
    patch_size = AddSize(patch_size,
                         uint64_t{image.imports[i].size()} * 6 +
                             bindings.imports[i].guest_name.size(),
                         "BIN imports");
  const uint64_t patch_offset =
      AddSize(BIN_HEADER_SIZE, image.payload_size, "BIN payload");
  const uint64_t file_size =
      Align(AddSize(patch_offset, patch_size, "BIN patches"), 16, "BIN file");
  const uint64_t misalignment =
      (0 - uint64_t{BIN_HEADER_SIZE}) & (image.alignment - 1);
  auto allocation =
      AddSize(file_size, image.alignment - 1, "BIN code-heap allocation");
  allocation = AddSize(allocation, sizeof(int64_t), "BIN code-heap allocation");
  AddSize(allocation, misalignment, "BIN code-heap allocation");

  Bytes bytes(file_size);
  WriteLE(bytes, 0, 0x1EEB, 2);
  bytes[2] = static_cast<uint8_t>(std::countr_zero(image.alignment));
  WriteLE(bytes, 4, BIN_SIGNATURE_VAL, 4);
  WriteLE(bytes, 8, INVALID_PTR, 8);
  WriteLE(bytes, 16, patch_offset, 8);
  WriteLE(bytes, 24, file_size, 8);
  for (const auto &section : elf.sections)
    if (section.allocated && section.header.sh_type == SHT_PROGBITS)
      std::copy_n(elf.bytes.begin() + section.header.sh_offset,
                  section.header.sh_size,
                  bytes.begin() + BIN_HEADER_SIZE + section.offset);
  for (const auto &write : image.writes)
    WriteLE(bytes, BIN_HEADER_SIZE + write.offset, write.value, write.width);
  size_t p = patch_offset;
  for (const auto &binding : bindings.exports)
    p = PatchRecord(bytes, p, IET_REL32_EXPORT,
                    static_cast<uint32_t>(SymbolOffset(elf, binding.symbol,
                                                       BindingName(binding))),
                    binding.guest_name);
  if (!image.absolute.empty()) {
    p = PatchRecord(bytes, p, IET_ABS_ADDR,
                    static_cast<uint32_t>(image.absolute.size()), {});
    for (const auto offset : image.absolute) {
      WriteLE(bytes, p, offset, 4);
      p += 4;
    }
  }
  for (size_t i = 0; i < image.imports.size(); i++) {
    bool first = true;
    for (const auto &patch : image.imports[i]) {
      p = PatchRecord(bytes, p, patch.type, patch.offset,
                      first ? std::string_view(bindings.imports[i].guest_name)
                            : std::string_view{});
      first = false;
    }
  }
  p = PatchRecord(bytes, p, IET_MAIN,
                  static_cast<uint32_t>(
                      SymbolOffset(elf, bindings.entry.symbol, "BIN entry")),
                  {});
  bytes.at(p++) = IET_END;
  if (p != patch_offset + patch_size)
    Fail("BIN patch extent did not match its planned size");
  return bytes;
}

static void WriteImage(const fs::path &output, const Bytes &bytes)
{
  std::string name = output.string() + ".XXXXXX";
  int fd = mkstemp(name.data());
  if (fd < 0)
    throw std::system_error(errno, std::generic_category(), name);
  try {
    size_t written = 0;
    while (written < bytes.size()) {
      ssize_t count = write(fd, bytes.data() + written, bytes.size() - written);
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

int main(int argc, char **argv)
{
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " input.o bindings.txt output.BIN\n";
    return 2;
  }
  try {
    auto elf = ReadElf(argv[1]);
    auto bindings = ReadBindings(argv[2]);
    const auto image = MakeLayout(elf, bindings);
    const auto bytes = MakeBin(elf, bindings, image);
    WriteImage(argv[3], bytes);
  } catch (const std::exception &error) {
    std::cerr << "MakeBIN: " << argv[1] << " (bindings " << argv[2]
              << "): " << error.what() << '\n';
    return 1;
  }
}
