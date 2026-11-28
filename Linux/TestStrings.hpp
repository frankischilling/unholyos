#pragma once

struct CTextProbe
{
  const char *name, *source, *module;
  std::string_view first, finish, expected;
  std::span<const std::string_view> exceptions{};
  bool allow_prefix=false;
  const char *reference_file=nullptr;
};

constexpr std::string_view str_scan_exceptions[]{
    "Scan", "Scan", "Scan", "Scan", "Scan", "Scan", "UndefDef", "Scan",
    "Scan", "Scan", "Scan", "Scan", "Scan", "Scan", "Scan",     "Scan"};
constexpr std::string_view doc_scan_exceptions[]{"Scan"};

constexpr std::string_view str_scan_output =
    "scan integers 12 42 11 18\n"
    "scan width xyz 7\n"
    "scan f 3FF4000000000001 C004000000000000 4072BFFFFFFFFFFE "
    "3FE0000000000000 17\n"
    "scan p 3FF4000000000001 3FF4000000000001 3FF4000000000001 "
    "3FF4000000000001 19\n"
    "scan stores 12345678 A5 A5\n"
    "scan lists 0 1 -2 -1 20\n"
    "scan defines 0 1 2 16\n"
    "scan date B47ECC0000000 8\n"
    "scan nul 78 0 52 4\n"
    "scan zero-width precision 401C000000000000 4\n"
    "ERROR: Undefined Define: 'ST_SCAN_PROBE_MISSING'.\n"
    "scan errors FFFF\n"
    "scan indirect 100\n"
    "StrScan probe 7FFF\n";

constexpr std::string_view string_utilities_output =
    "SU trans 2E9A23E338C6D125 99E18322ABC1A045 A9D54056C2DF1575 "
    "50806DBE1A95A848\n"
    "SU allocated BE99495FCBE0B285\n"
    "SU find 0 0 0 0 6 6 19 15\n"
    "SU empty 0 0 0 0 4 4 -1 -1\n"
    "String utilities 7FF\n";

constexpr std::string_view float_scan_output =
    "FS empty 0 0\n"
    "FS space 0 4\n"
    "FS zero 0 1\n"
    "FS minuszero 8000000000000000 2\n"
    "FS minus 8000000000000000 1\n"
    "FS plus 0 0\n"
    "FS minusminus 8000000000000000 1\n"
    "FS integer 405EC00000000000 3\n"
    "FS minusint C05EC00000000000 4\n"
    "FS quarter 3FD0000000000001 3\n"
    "FS dotted 3FF3333333333334 3\n"
    "FS bareexp 0 2\n"
    "FS dot 0 1\n"
    "FS exponent 3FF4000000000001 7\n"
    "FS plusexp 3FF0000000000000 2\n"
    "FS emptyexp 3FF0000000000000 3\n"
    "FS inf 7FF0000000000000 3\n"
    "FS inftail 7FF0000000000000 3\n"
    "FS minusinf FFF0000000000000 4\n"
    "FS upperinf 0 0\n"
    "FS byte 0 0\n"
    "FS lookahead 7FF0000000000000 1\n"
    "FS digitlook 7FF0000000000000 1\n"
    "FS minusbyte 8000000000000000 1\n"
    "FS minuslook FFF0000000000000 2\n"
    "FS nullahead 7FF0000000000000 1\n"
    "FS maxint 43E0000000000000 19\n"
    "FS wrapint BFF0000000000000 20\n"
    "FS minuswrap 3FF0000000000000 21\n"
    "FS wrapfrac 0 21\n"
    "FS largefrac 430999999999999B 17\n"
    "FS largefrac2 430999999999999C 17\n"
    "FS largeexp 430999999999999B 19\n"
    "FS minuslarge C30999999999999B 18\n"
    "FS wrapexpzero 3FF0000000000000 22\n"
    "FS wrapexpone 4023FFFFFFFFFFFF 22\n"
    "FS wrapexpneg 3FB999999999999A 22\n"
    "FS negwrapexp 4023FFFFFFFFFFFF 23\n"
    "FS minexp 0 22\n"
    "FS wrapexpfrac 3FF3333333333334 24\n"
    "FS zeroinf FFF8000000000000 5\n"
    "FS underflow 8000000000000000 7\n"
    "FS nantext 0 0\n"
    "FS signspace 8000000000000000 1\n"
    "FS DONE\n";

constexpr std::string_view date_scan_output =
    "DS boundary B47D400000000 B47EC00000000 B47D100000000 B491D00000000\n"
    "DS space B47D400000000 B47EC00000000 B47D100000000 B491D00000000\n"
    "DS suffix B47F000000000 B47B100000000 B47B100000000 B492000000000\n"
    "DS positive B47D3C0000000 B47EBC0000000 B47D0C0000000 B491CC0000000\n"
    "DS negative B47D440000000 B47EC40000000 B47D140000000 B491D40000000\n"
    "Date scan 3F\n";

constexpr std::string_view string_compare_output =
    "SC empty 0 0 0 1\n"
    "SC left -1 -1 -1 1\n"
    "SC right 1 1 1 1\n"
    "SC case 0 -1 0 1\n"
    "SC order -1 1 -1 1\n"
    "SC punct -1 -1 -1 1\n"
    "SC upperedge -1 -1 -1 1\n"
    "SC loweredge 1 -1 1 1\n"
    "SC late 0 1 0 1\n"
    "SC short -1 0 0 1\n"
    "SC long -1 -1 -1 1\n"
    "SC term -1 -1 -1 1\n"
    "SC negative 0 -1 0 1\n"
    "SC negdiff -1 1 -1 1\n"
    "SC mincount 0 1 0 1\n"
    "SC embedded 0 0 0 1\n"
    "SC overlap -1 -1 -1 1\n"
    "SC high -1 -1 -1 1\n"
    "SC extended 1 1 1 1\n"
    "SC null 0 0\n"
    "SC occ 2 0\n"
    "SC bytes 562AF89B93749399 E94E7B76B0C5E825 562AF89B93749399\n"
    "SC upper ED25B86AE3E78265 0\n"
    "SC DONE\n";

constexpr CTextProbe string_probes[]{
    {"str-scan", "/StrScanProbe.HC", "/StrScanProbe.BIN.Z", "scan integers ",
     "StrScan probe ", str_scan_output, str_scan_exceptions},
    {"doc-scan", "/DocScanProbe.HC", "/DocScanProbe.BIN.Z", "DocScan probe ",
     "DocScan probe ", "DocScan probe 3FFF\n", doc_scan_exceptions},
    {"string-utilities", "/StringUtilitiesProbe.HC",
     "/StringUtilitiesProbe.BIN.Z", "SU trans ", "String utilities ",
     string_utilities_output},
    {"float-scan", "/FloatScanProbe.HC", "/FloatScanProbe.BIN.Z", "FS empty ",
     "FS DONE", float_scan_output},
    {"date-scan", "/DateScanProbe.HC", "/DateScanProbe.BIN.Z", "DS boundary ",
     "Date scan ", date_scan_output},
    {"string-compare", "/StringCompareProbe.HC", "/StringCompareProbe.BIN.Z",
     "SC empty ", "SC DONE", string_compare_output}};

static std::string TextProbeOutput(std::string_view serial,
                                   const CTextProbe &probe, const char *path)
{
  std::string output(ProgramOutput(serial, path, '1'));
  size_t count = 0, pos = 0;
  const std::regex diagnostic(
      R"(Except:([^:\r\n]+):(0x[0-9A-Fa-f]{16}:){8}\r\n)");
  while ((pos = output.find("Except:", pos)) != std::string::npos) {
    auto end = output.find("\r\n", pos);
    Check((!pos || output[pos - 1] == '\n') && end != std::string::npos,
          "Malformed native exception diagnostic");
    auto line = output.substr(pos, end + 2 - pos);
    std::smatch match;
    Check(std::regex_match(line, match, diagnostic),
          "Malformed native exception callers");
    Check(count < probe.exceptions.size() &&
              match[1].str() == probe.exceptions[count],
          "Unexpected native exception tag or order");
    ++count;
    output.erase(pos, line.size());
  }
  Check(count == probe.exceptions.size(),
        "Missing native exception diagnostic");
  if (probe.allow_prefix) {
    auto first=output.find(probe.first);
    Check(first!=std::string::npos && (!first || output[first-1]=='\n'),
          "Missing source probe result line");
    output.erase(0,first);
  }
  return output;
}

static void CheckTextProbes(std::string_view serial,
                            std::span<const CTextProbe> probes)
{
  for (const auto &probe : probes)
    for (const auto *path : {probe.source, probe.module}) {
      auto output = TextProbeOutput(serial, probe, path);
      if (output != probe.expected)
        throw std::runtime_error(std::string("Output differs from TempleOS: ") +
                                 path + "\n" + output);
    }
}

static void RecordTextProbes(std::string_view serial,
                             std::span<const CTextProbe> probes,
                             const fs::path &dir)
{
  for (const auto &probe : probes) {
    for (const auto *path : {probe.source, probe.module}) {
      auto output = TextProbeOutput(serial, probe, path);
      std::ofstream file(
          dir / (std::string(probe.name) +
                 (path == probe.source ? "-source.txt" : "-module.txt")));
      file << output;
      Check(bool(file), "Could not record native text probe output");
      if (path == probe.source)
        std::cout << output;
    }
  }
}
