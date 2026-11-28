#pragma once

constexpr std::string_view flood_fill_output =
    "fill region 48 A1B582AFB5972DE5\n"
    "fill boundary 48 8421AE126C7CED25\n"
    "fill dither 128 B16124DE9090DEA5\n"
    "fill xor 6 BC485F29989517E5\n"
    "fill depth 4 041C8166341270CB 18B09C2E6758C549\n"
    "fill symmetry 6 101D6F5E76191AC5 1CA312FDBF5F72ED\n"
    "fill transform 12 DA390E6207FC8CB5 796135481584641D\n"
    "flood fill 1FF\n";

constexpr std::string_view music_precision_output =
    "MP initial 3FF0000000000000 4004000000000000 3FECCCCCCCCCCCCD\n"
    "MP reset 3FF0000000000000 4004000000000000 3FECCCCCCCCCCCCD\n"
    "MP literal 3FD0000000000001 4004000000000000 3FECCCCCCCCCCCCD\n"
    "MP w 4010000000000000\n"
    "MP h 4000000000000000\n"
    "MP q 3FF0000000000000\n"
    "MP e 3FE0000000000000\n"
    "MP s 3FD0000000000001\n"
    "MP wt 4005555555555555\n"
    "MP ht 3FF5555555555555\n"
    "MP qt 3FE5555555555555\n"
    "MP et 3FD5555555555555\n"
    "MP st 3FC5555555555557\n"
    "MP w. 4018000000000000\n"
    "MP h. 4008000000000000\n"
    "MP q. 3FF8000000000000\n"
    "MP e. 3FE8000000000000\n"
    "MP s. 3FD8000000000002\n"
    "MP wt. 4010000000000000\n"
    "MP ht. 4000000000000000\n"
    "MP qt. 3FF0000000000000\n"
    "MP et. 3FE0000000000000\n"
    "MP st. 3FD0000000000001\n"
    "MP w.t 4010000000000000\n"
    "MP h.t 4000000000000000\n"
    "MP q.t 3FF0000000000000\n"
    "MP e.t 3FE0000000000000\n"
    "MP s.t 3FD0000000000001\n"
    "MP qtt 3FDC71C71C71C71C\n"
    "MP stt 3FBC71C71C71C71F\n"
    "MP q.. 4002000000000000\n"
    "MP s.. 3FE2000000000002\n"
    "MP DONE\n";

constexpr std::string_view math_helpers_output =
    "round -21 -20 -30 -20\n"
    "round -20 -20 -20 -20\n"
    "round -19 -10 -20 -20\n"
    "round -10 -10 -10 -10\n"
    "round -9 0 -10 -10\n"
    "round -1 0 -10 -10\n"
    "round 0 0 0 0\n"
    "round 1 0 0 10\n"
    "round 9 0 0 10\n"
    "round 10 10 10 10\n"
    "round 11 10 10 20\n"
    "round 19 10 10 20\n"
    "round 20 20 20 20\n"
    "math integer 903D605CF130C49A 65978B3FBB23DE21\n"
    "math floating 023263F76EC7DF9A\n"
    "math clamp C000000000000000 4008000000000000 3FF4000000000001\n"
    "math reversed 4020000000000000 4008000000000000 4020000000000000\n"
    "math zero 8000000000000000 8000000000000000 8000000000000000\n"
    "math finite C016000000000000 4002000000000001 C008000000000000\n"
    "math nan1 3FF0000000000000 7FF8000000001234 7FF8000000001234\n"
    "math nan2 3FF0000000000000 4008000000000000 3FF0000000000000\n"
    "math infinity 4008000000000000 C008000000000000 4008000000000000\n"
    "math powers 0000000000000000 4058FFFFFFFFFFFE 7FF0000000000000\n"
    "math indirect -10 20\n"
    "Math helpers done\n";

constexpr std::string_view sprite_flood_output =
    "sprite flood stream 95BB364C84E33F56\n"
    "sprite flood single 026661834AEEFF05 66901F20C6B79EE1\n"
    "sprite flood F\n";

constexpr CTextProbe math_probes[]{{"math-helpers", "/MathHelpersProbe.HC",
                                    "/MathHelpersProbe.BIN.Z", "round -21 ",
                                    "Math helpers done", math_helpers_output}};
constexpr CTextProbe music_precision_probes[]{
    {"music-precision", "/MusicPrecision.HC", "/MusicPrecision.BIN.Z",
     "MP initial ", "MP DONE", music_precision_output}};
constexpr CTextProbe training_input_probes[]{
    {"training-input", "/InputTrainingProbe.HC", "",
     "Training result ", "Training result FF", "Training result FF\n", {}, true}};
constexpr CTextProbe file_editor_probes[]{
    {"file-editor", "/FileEditorProbe.HC", "",
     "File editor ", "File editor 1FF", "File editor 1FF\n", {}, true}};
constexpr CTextProbe key_map_probes[]{
    {"key-map", "/KeyMapProbe.HC", "", "Key map probe ", "Key map probe 7F",
     "Key map probe 7F\n", {}, true}};
constexpr CTextProbe dictionary_probes[]{
    {"dictionary", "/DictionaryProbe.HC", "", "Dictionary probe ",
     "Dictionary probe 3F", "Dictionary probe 3F\n", {}, true}};

struct CModuleFile
{
  fs::path source;
  const char *guest_name, *evidence_name;
};

enum class CStockStartup { None, Decline, Tour };
enum class CDesktopTest {
  None, Progress, DictionaryInstall, DictionaryGenerator, DictionaryGeneratorHolyC,
  MenuDiagnostic, CompilerDiagnostic, CompilerCmpDiagnostic,
  ExternImportOption, ExternImportOptionJIT
};

// Uses the same QMP input and physical-memory readers as the boot tests.
static void DriveCharacterGraphics(
    CMonitor &vm, const fs::path &dir, const std::string &boot, bool reference,
    bool tic = false, bool breaks = false, bool screen_capture = false,
    bool spline = false, bool flood_fill = false, bool blot = false,
    bool sprite_flood = false, bool music_demo = false,
    std::span<const CTextProbe> text_probes = {},
    std::span<const CModuleFile> module_files = {},
    uint64_t memory_limit = 0x20000000,
    std::string_view startup_output = {},
    CStockStartup stock = CStockStartup::None,
    CDesktopTest desktop = CDesktopTest::None)
{
  const bool reference_dictionary = reference && !text_probes.empty() &&
      std::string_view(text_probes.front().name) == "dictionary";
  const std::string prompt = reference_dictionary ? "A:/Home>" : "T:/Home>";
  auto in_memory = [memory_limit](uint64_t addr, uint64_t size) {
    return addr && size <= memory_limit && addr <= memory_limit - size;
  };
  vm.ResetDeadline(150s);
  auto key = [&](std::string name) {
    vm.Hmp("sendkey " + name + " 35");
    std::this_thread::sleep_for(65ms);
  };
  auto type = [&](std::string_view text) {
    for (char ch : text) {
      if (ch >= 'A' && ch <= 'Z')
        key("shift-" + std::string(1, char(ch + 'a' - 'A')));
      else {
        switch (ch) {
        case '(': key("shift-9"); break;
        case ')': key("shift-0"); break;
        case ':': key("shift-semicolon"); break;
        case ';': key("semicolon"); break;
        case '"': key("shift-apostrophe"); break;
        case '\'': key("apostrophe"); break;
        case '\\': key("backslash"); break;
        case '%': key("shift-5"); break;
        case '<': key("shift-comma"); break;
        case '>': key("shift-dot"); break;
        case '[': key("bracket_left"); break;
        case ']': key("bracket_right"); break;
        case '{': key("shift-bracket_left"); break;
        case '}': key("shift-bracket_right"); break;
        case '/': key("slash"); break;
        case '.': key("dot"); break;
        case ',': key("comma"); break;
        case '=': key("equal"); break;
        case '+': key("shift-equal"); break;
        case '*': key("shift-8"); break;
        case '&': key("shift-7"); break;
        case ' ': key("spc"); break;
        case '-': key("minus"); break;
        case '_': key("shift-minus"); break;
        case '~': key("shift-grave_accent"); break;
        default: key(std::string(1, ch)); break;
        }
      }
    }
  };
  auto cmd = [&](std::string_view text) { type(text); key("ret"); };
  uint64_t base = 0;
  if (reference) {
    std::this_thread::sleep_for(12s);
    key("n");
    std::this_thread::sleep_for(2s);
    key("n");
    std::this_thread::sleep_for(2s);
    cmd("I64 cgtext=gr.text_base;");
    cmd("I64 cgi;for(cgi=0;cgi<8;cgi++)OutU8(0xE9,cgtext.u8[cgi]);");
    auto debug = dir / "debugcon.bin";
    for (unsigned i = 0; i < 100; i++) {
      if (fs::exists(debug) && fs::file_size(debug) >= 8) {
        const auto bytes = FileRead(debug);
        base = ReadLE(bytes, bytes.size() - 8, 8);
        break;
      }
      std::this_thread::sleep_for(50ms);
    }
  } else {
    std::smatch match;
    Check(std::regex_search(boot, match, std::regex(R"(text base (0x[0-9a-f]+))")),
          "Missing text base address");
    base = std::stoull(match[1].str(), nullptr, 16);
  }
  Check(in_memory(base, 19200), "Invalid character graphics text buffer");
  auto path = dir / "character-text.bin";
  using Cells = std::array<uint32_t, 80 * 60>;
  auto capture = [&] {
    vm.Command(R"({"execute":"pmemsave","arguments":{"val":)" +
               std::to_string(base) + R"(,"size":19200,"filename":")" +
               path.string() + R"("}})");
    const auto bytes = FileRead(path);
    Check(bytes.size() == 19200, "Incomplete character graphics text layer");
    Cells cells{};
    for (size_t i = 0; i < cells.size(); i++)
      cells[i] = ReadLE(bytes, i * 4, 4);
    return cells;
  };
  auto visible = [](const Cells &cells) {
    std::string result;
    for (size_t i = 0; i < cells.size(); i++) {
      unsigned char c = cells[i];
      result += c >= 32 && c < 127 ? char(c) : ' ';
      if (i % 80 == 79)
        result += '\n';
    }
    return result;
  };
  auto wait = [&](auto predicate, const char *message, unsigned attempts = 180) {
    Cells cells{};
    for (unsigned i = 0; i < attempts; i++) {
      cells = capture();
      if (predicate(cells))
        return cells;
      std::this_thread::sleep_for(30ms);
    }
    throw std::runtime_error(std::string(message) + "\n" + visible(cells));
  };
  auto wait_text = [&](std::string_view wanted) {
    return wait([&](const Cells &cells) {
      return visible(cells).find(wanted) != std::string::npos;
    }, "Missing character graphics terminal output");
  };
  auto snapshot = [&](const char *name) {
    fs::copy_file(path, dir / (std::string(name) + ".bin"),
                  fs::copy_options::overwrite_existing);
    vm.Command(R"({"execute":"screendump","arguments":{"filename":")" +
               (dir / (std::string(name) + ".ppm")).string() + R"("}})");
  };
  auto training_input = [&] {
    const auto training = visible(wait_text("Training string"));
    Check(training.find("Training output 27") != std::string::npos,
          "InPrint did not format the training message");
    key("x");
    wait_text("Press the <f> key.");
    key("spc");
    key("f");
    key("g");
    Check(visible(wait_text("Training key")).find("fg") != std::string::npos,
          "InGetStr did not echo the requested characters");
    key("x");
    wait_text("Press the <F1> key");
    key("spc");
    key("f1");
    wait_text("Training choice");
    key("x");
    wait_text("Press <a>, <b> or <c>.");
    key("spc");
    key("b");
    wait_text("Training until key");
    key("right");
    wait_text("Training until char");
    key("d");
    wait_text("Training view");
    key("esc");
    wait_text("Training result FF");
  };
  auto file_editor = [&] {
    wait_text("EditorSave");
    key("end");
    key("q");
    wait_text("EditorSaveq");
    key("esc");
    wait_text("Editor canceled write test");
    wait_text("EditorSaveq");
    key("end");
    key("w");
    wait_text("EditorSaveqw");
    key("shift-esc");
    wait_text("Open editor help");
    key("spc");
    wait_text("Keyboard Ctrls");
    key("shift-esc");
    wait_text("Open editor help");
    key("shift-esc");
    wait_text("Not Text File");
    key("shift-esc");
    wait_text("File editor 1FF");
  };
  auto dictionary_keys = [&] {
    type("Albatros");
    wait_text("'0' Albatross");
    key("ctrl-shift-0");
    wait_text("A web-footed bird");
    snapshot("dictionary-definition");
    key("shift-esc");
    wait_text("'0' Albatross");
    key("ctrl-0");
    wait_text(prompt + "Albatross");
    snapshot("dictionary-completion");
    for (size_t i = 0; i < std::string_view("Albatross").size(); ++i)
      key("backspace");
    wait_text("AutoComplete is in StandBy");
    cmd(R"(Ed("DN:Albatross,1");0;)");
    wait_text("A web-footed bird");
    snapshot("dictionary-link");
    key("shift-esc");
  };
  auto addresses = [&](std::string_view marker,
                       std::array<uint64_t, 3> sizes = {8, 8, 8},
                       unsigned attempts = 180) {
    const std::regex pattern(std::string(marker) +
                             "([0-9A-Fa-f]+) ([0-9A-Fa-f]+) ([0-9A-Fa-f]+)");
    std::smatch match;
    std::string screen;
    wait([&](const Cells &cells) {
      screen = visible(cells);
      return std::regex_search(screen, match, pattern);
    }, "Missing graphics runtime addresses", attempts);
    std::array<uint64_t, 3> result{};
    for (size_t i = 0; i < result.size(); i++) {
      result[i] = std::stoull(match[i + 1].str(), nullptr, 16);
      Check(in_memory(result[i], sizes[i]), "Invalid graphics runtime address");
    }
    return result;
  };
  auto pane_links = [&] {
    cmd(R"("ACM %X %X %X\n",&ms.pos.x,&ms.lb,&ms.rb;)");
    const auto mouse = addresses("ACM ", {16, 1, 1});
    auto click = [&](std::string_view label) {
      const auto screen = visible(wait_text(label));
      const auto index = screen.find(label);
      const int64_t x = (index % 81) * 8 + 4, y = (index / 81) * 8 + 4;
      bool reached = false;
      for (unsigned i = 0; i < 90; i++) {
        const auto coords = ReadPhysical<2>(vm, mouse[0]);
        auto dx = x - int64_t(coords[0]), dy = y - int64_t(coords[1]);
        if (std::abs(dx) <= 1 && std::abs(dy) <= 1) {
          reached = true;
          break;
        }
        MouseMove(vm, std::clamp(dx, int64_t(-40), int64_t(40)) * 2,
                  std::clamp(dy, int64_t(-40), int64_t(40)) * 2);
        std::this_thread::sleep_for(55ms);
      }
      Check(reached, "Mouse did not reach the DolDoc entry");
      std::this_thread::sleep_for(100ms);
      for (bool down : {true, false}) {
        MouseButton(vm, "left", down);
        unsigned i = 0;
        while (bool(ReadPhysical<1>(vm, mouse[1], 'b')[0]) != down && ++i < 100)
          std::this_thread::sleep_for(25ms);
        Check(i < 100, "Mouse button state did not change");
        std::this_thread::sleep_for(down ? 180ms : 400ms);
      }
    };
    wait_text("AutoComplete is in StandBy");
    click("System Keys Quick Guide");
    wait_text("Keyboard Ctrls");
    snapshot("pane-help");
    vm.ResetDeadline(120s);
    click("Key Map");
    wait(
        [&](const Cells &cells) {
          return visible(cells).find("KeyMap Completed.") != std::string::npos;
        },
        "The Key Map macro did not finish", 2900);
    snapshot("pane-key-map");
    key("shift-esc");
    wait(
        [&](const Cells &cells) {
          const auto screen = visible(cells);
          return screen.find("Keyboard Ctrls") != std::string::npos &&
                 screen.find("KeyMap Completed.") == std::string::npos;
        },
        "Closing Key Map did not return to HelpIndex");
    key("shift-esc");
    wait(
        [&](const Cells &cells) {
          const auto screen = visible(cells);
          return screen.find("AutoComplete is in StandBy") !=
                     std::string::npos &&
                 screen.find("Keyboard Ctrls") == std::string::npos;
        },
        "Closing HelpIndex did not return to StandBy");
    std::cout
        << "The StandBy link opened HelpIndex and its Key Map macro ran\n";
  };
  wait_text("T:/Home>");
  if (stock != CStockStartup::None) {
    const auto prompt = visible(wait_text("Install onto hard drive (y or n)?"));
    Check(prompt.find("the live CD without installing.") != std::string::npos,
          "The live DVD option is missing");
    snapshot("install-prompt");
    key("n");
    wait_text("Take Tour(y or n)?");
    snapshot("take-tour");
    if (stock == CStockStartup::Decline) {
      key("n");
      const auto screen = visible(wait_text("Take Tour(y or n)? NO"));
      Check(screen.find("Install onto hard drive (y or n)? NO") != std::string::npos,
            "The install prompt did not accept No");
      wait_text("T:/Home>");
      snapshot("live-prompt");
      std::cout << "Declined installation and tour on the stock Once script\n";
      key("f1");
      wait_text("Keyboard Ctrls");
      snapshot("help-index");
      key("shift-f1");
      wait_text("Owner/President:");
      snapshot("about");
      key("shift-esc");
      wait_text("Keyboard Ctrls");
      key("shift-esc");
      wait_text("T:/Home>");
      cmd("WinMax;DocClear(,1);Seed(12345);TipOfDay;");
      wait_text("* Use <CTRL-ALT-f> to toggle between Std Font and Cyrillic Font.");
      snapshot("tip-of-day");
      std::cout << "Displayed the seeded tip from the original Tips.DD\n";
      cmd(R"(ExeFile2("::/InputTrainingProbe.HC");0;)");
      training_input();
      snapshot("training-input");
      std::cout << "Training result FF\n";
      cmd(R"(ExeFile2("::/FileEditorProbe.HC");0;)");
      file_editor();
      snapshot("file-editor");
      std::cout << "File editor 1FF\n";
    } else {
      key("y");
      const auto menu = visible(wait_text("Part 12: Test Suite"));
      Check(menu.find("TempleOS Tours") != std::string::npos &&
                menu.find("Part  1: The help system") != std::string::npos &&
                menu.find("Done") != std::string::npos,
            "The original tour menu is incomplete");
      snapshot("tour-menu");
      key("spc");
      wait_text("for the main help index.");
      key("x");
      wait_text("Press the <F1> key");
      key("spc");
      key("f1");
      wait_text("Keyboard Ctrls");
      wait_text("to abort and exit.");
      snapshot("tour-help-index");
      key("shift-esc");
      constexpr const char *mods[]{"ctrl", "ctrl-shift", "alt", "alt-shift",
                                    "ctrl-alt", "ctrl-alt-shift"};
      constexpr const char *names[]{"CTRL", "CTRL SHIFT", "ALT", "ALT SHIFT",
                                     "CTRL ALT", "CTRL ALT SHIFT"};
      for (size_t i = 0; i < std::size(mods); i++) {
        wait_text(std::string("hold the <") + names[i] + ">");
        if (i < 4) {
          vm.Hmp(std::string("sendkey ") + mods[i] + " 1400");
          std::this_thread::sleep_for(1575ms);
          continue;
        }
        KeyboardKey(vm, "ctrl", true);
        KeyboardKey(vm, "alt", true);
        if (i == 5)
          KeyboardKey(vm, "shift", true);
        const auto held_since = std::chrono::steady_clock::now();
        const auto description = i == 4 ? "CTRL ALT z         Cmd /Zoom In"
                                        : "CTRL ALT SHIFT z         Cmd /Zoom Out";
        wait([&](const Cells &cells) {
          const auto screen = visible(cells);
          return screen.find(description) != std::string::npos &&
                 (i == 5 || screen.find("Cmd /Toggle Aux Font") != std::string::npos);
        }, "Missing held Ctrl-Alt command descriptions");
        snapshot(i == 4 ? "autocomplete-ctrl-alt" : "autocomplete-ctrl-alt-shift");
        std::this_thread::sleep_until(held_since + 2s);
        if (i == 5)
          KeyboardKey(vm, "shift", false);
        KeyboardKey(vm, "alt", false);
        KeyboardKey(vm, "ctrl", false);
        if (i == 4)
          wait_text("AutoComplete is in StandBy");
      }
      wait_text("TempleOS Tours");
      snapshot("first-tour-complete");
      key("shift-esc");
      wait([&](const Cells &cells) {
        const auto screen = visible(cells);
        return screen.find("TempleOS Tours") == std::string::npos &&
               screen.find("Directory of T:/Home") != std::string::npos &&
               screen.find("T:/Home>") != std::string::npos;
      }, "The stock tour did not return to the home prompt");
      snapshot("tour-exit");
      cmd("AutoComplete(0);0;");
      wait([&](const Cells &cells) {
        return visible(cells).find("AutoComplete is in StandBy") == std::string::npos;
      }, "AutoComplete task stayed visible after shutdown");
      cmd("AutoComplete(1);0;");
      wait_text("AutoComplete is in StandBy");
      snapshot("autocomplete-restarted");
      std::cout << "AutoComplete displayed Ctrl-Alt descriptions and restarted\n";
      std::cout << "Completed the first tour chapter and returned to the prompt\n";
      cmd(R"(ACInit("::/ACIndex.TXT*");)");
      wait_text("ACIndex.TXT.Z");
      type("AcIndex");
      wait([&](const Cells &cells) {
        const auto screen = visible(cells);
        return screen.find("Word:AcIndex") != std::string::npos &&
               screen.find("F01 AcIndexGamma") != std::string::npos &&
               screen.find("F02 AcIndexBeta") != std::string::npos &&
               screen.find("F03 AcIndexAlpha") != std::string::npos &&
               screen.find("acIndexWrong") == std::string::npos;
      }, "Missing frequency-ordered completion for the active prefix");
      snapshot("autocomplete-words");
      key("ctrl-f1");
      wait_text("T:/Home>AcIndexGamma");
      key("shift-esc");
      wait_text("AutoComplete is in StandBy");
      std::cout << "AutoComplete ranked and inserted a completion from compressed text\n";
      cmd("WinMax;");
      cmd(R"(ExeFile2("::/DictionaryProbe.HC");0;)");
      wait_text("Dictionary probe 3F");
      dictionary_keys();
      std::cout << "Dictionary probe 3F and keyboard actions passed\n";
      pane_links();
    }
    return;
  }
  if (!startup_output.empty()) {
    auto screen = visible(wait_text(startup_output));
    Check(screen.find(startup_output) == screen.rfind(startup_output),
          "Startup command ran in multiple terminals");
    snapshot("once-startup");
    std::cout << startup_output << '\n';
  }
  cmd("WinMax;AutoComplete(0);DocClear(,1);");
  const bool compiler_diagnostic = desktop == CDesktopTest::CompilerDiagnostic ||
                                   desktop == CDesktopTest::CompilerCmpDiagnostic;
  const bool extern_import = desktop == CDesktopTest::ExternImportOption ||
                             desktop == CDesktopTest::ExternImportOptionJIT;
  if (desktop == CDesktopTest::MenuDiagnostic || compiler_diagnostic ||
      desktop == CDesktopTest::ExternImportOptionJIT)
    cmd("I64 dsz;U8 *dout;");
  auto wait_compiler_exception = [&](std::string_view dir_prompt) {
    return wait([&](const Cells &cells) {
      const auto screen = visible(cells);
      const auto exception = screen.find("Except:Compiler");
      return exception != std::string::npos &&
             screen.find(dir_prompt, exception) != std::string::npos;
    }, "The terminal did not return after its Compiler exception");
  };
  auto save_diagnostic = [&](const char *filename) {
    cmd("dout=DocSave(DocPut,&dsz);");
    cmd(R"("DGP %X %X %X\n",dout,&dsz,&dout;)");
    const auto fields = addresses("DGP ", {1, 8, 8});
    const auto size = ReadPhysical<1>(vm, fields[1])[0];
    Check(size && in_memory(fields[0], size), "Invalid diagnostic document size");
    const auto output = dir / filename;
    vm.Command(R"({"execute":"pmemsave","arguments":{"val":)" +
               std::to_string(fields[0]) + R"(,"size":)" + std::to_string(size) +
               R"(,"filename":")" + output.string() + R"("}})");
    cmd("Free(dout);");
    return ReadText(output);
  };
  if (compiler_diagnostic || (extern_import && !reference)) {
    cmd("CBlkDev *db=BlkDevNextFreeSlot('A',BDT_RAM);");
    cmd("db->max_blk=131071;");
    cmd(R"("DB %X\n",BlkDevAdd(db,I64_MIN,FALSE,TRUE);)");
    wait_text("DB 1");
    cmd(R"(Cd("A:/");)");
    wait_text("A:/>");
  }
  if (desktop == CDesktopTest::CompilerCmpDiagnostic) {
    cmd(R"(U8 *ci="public I64i CmpStaleExport;\nCmpStaleExport=UnboundCmpName;\n";)");
    cmd(R"(FileWrite("Partial.HC",ci,StrLen(ci));)");
    cmd(R"("DI %X\n",Cmp("Partial.HC",,"Bad.BIN");)");
    wait_text("DI 2");
    const auto rejected = save_diagnostic("compiler-partial.bin");
    Check(rejected.find(R"(Invalid lval at "UnboundCmpName")") != std::string::npos &&
              rejected.find("FL:A:/Partial.HC,2") != std::string::npos &&
              rejected.find("Errs:2 Warns:0 Code:0 Size:0") != std::string::npos,
          "Failed partial Cmp reported the wrong error, source, or output size");
    std::cout << "HolyC Cmp reported the late error without publishing output\n";
    return;
  }
  if (desktop == CDesktopTest::CompilerDiagnostic) {
    cmd("NoSuchParserName; // undefined statement");
    wait_compiler_exception("A:/>");
    const auto prompt_doc = save_diagnostic("compiler-prompt.bin");
    Check(prompt_doc.find(R"(Undefined identifier at ";")") != std::string::npos,
          "Missing undefined identifier diagnostic or lookahead token");
    cmd("DocClear(,1);");
    cmd(R"(U8 *bad="I64 Broke(){return 4+; } // bad expression\n";)");
    cmd(R"("DW %X\n",FileWrite("Bad.HC",bad,StrLen(bad));)");
    wait_text("DW ");
    cmd(R"("DF %X\n",ExeFile("Bad.HC");)");
    wait_text("DF 0");
    cmd(R"(ExeFile2("Bad.HC");)");
    wait_compiler_exception("A:/>");
    cmd(R"(U8 *bo="4+; // bad AOT expression\n";)");
    cmd(R"(FileWrite("BadAot.PRJ",bo,StrLen(bo));)");
    cmd(R"("DA %X\n",Cmp("BadAot.PRJ",,"Bad.BIN");)");
    wait_text("DA 2");
    cmd(R"("DR %X %X\n",FileFind("Bad.BIN")==0,6*7;)");
    wait_text("DR 1 2A");
    const auto doc = save_diagnostic("compiler-file.bin");
    size_t next = 0;
    auto report = [&](std::string_view diagnostic, std::string_view link,
                      std::string_view line) {
      const auto message = doc.find(diagnostic, next);
      const auto file = doc.find(link, message);
      const auto source = doc.find(line, file);
      Check(message != std::string::npos && file != std::string::npos &&
                source != std::string::npos && file < doc.find('\n', message) &&
                source < doc.find('\n', file),
            "Missing compiler diagnostic token, link, or source line");
      next = source + 1;
    };
    constexpr std::string_view missing = R"(Missing expression at ";")";
    constexpr std::string_view parse = R"(Compiler Parse Error at ";")";
    constexpr std::string_view jit = "FL:A:/Bad.HC,1",
                               jit_line = "I64 Broke(){return 4+; } // bad expression";
    for (unsigned i = 0; i < 2; i++) {
      report(missing, jit, jit_line);
      report(parse, jit, jit_line);
    }
    constexpr std::string_view aot = "FL:A:/BadAot.PRJ,1",
                               aot_line = "4+; // bad AOT expression";
    report(missing, aot, aot_line);
    report(parse, aot, aot_line);
    report(R"(";" $LK,)", aot, aot_line);
    std::cout << "HolyC diagnostics matched the prompt, ExeFile, ExeFile2 and Cmp paths\n";
    return;
  }
  if (desktop == CDesktopTest::MenuDiagnostic) {
    cmd("CBlkDev *mdb=BlkDevNextFreeSlot('A',BDT_RAM);");
    cmd("mdb->max_blk=131071;");
    cmd(R"("MB %X\n",BlkDevAdd(mdb,I64_MIN,FALSE,TRUE);)");
    wait_text("MB 1");
    cmd(R"(Cd("A:/");)");
    wait_text("A:/>");
    cmd(R"(U8 *bad="Root {\nLeaf(1)\n} // End of menu\n";)");
    cmd(R"("MW %X\n",FileWrite("BadMenu.HC",bad,StrLen(bad));)");
    wait_text("MW ");
    cmd(R"(CMenu *oldm=MenuPush("Root {Leaf(1);}");)");
    cmd(R"hc(if(oldm)"MS %X\n",1;)hc");
    wait_text("MS 1");
    cmd(R"(MenuFilePush("BadMenu.HC");)");
    wait_compiler_exception("A:/>");
    cmd(R"("Menu intact %X\n",Fs->cur_menu==oldm;)");
    const auto terminal = visible(wait_text("Menu intact 1"));
    std::ofstream(dir / "menu-diagnostic-terminal.txt") << terminal;
    snapshot("menu-diagnostic");
    Check(terminal.find(R"(Missing ';' at"}")") != std::string::npos &&
              terminal.find("Except:Compiler") != std::string::npos,
          "Missing menu parser token or exception");
    const auto doc = save_diagnostic("menu-diagnostic-document.bin");
    const auto link = doc.find("FL:A:/BadMenu.HC,3");
    Check(link != std::string::npos &&
              doc.find("} // End of menu", link) != std::string::npos,
          "Missing menu parser source link or source line");
    cmd("MenuPop;");
    std::cout << "Malformed menu reported its source and preserved the menu stack\n";
    return;
  }
  if (desktop == CDesktopTest::DictionaryGenerator ||
      desktop == CDesktopTest::DictionaryGeneratorHolyC) {
    vm.ResetDeadline(1200s);
    const bool holyc = reference || desktop == CDesktopTest::DictionaryGeneratorHolyC;
    cmd("CBlkDev *genbd=BlkDevNextFreeSlot('A',BDT_RAM);");
    auto wait_number = [&](std::string_view prefix, unsigned attempts) {
      const std::regex number("(^|\n) {0,4}" + std::string(prefix) +
                              " [1-9A-Fa-f][0-9A-Fa-f]*\\b");
      return wait([&](const Cells &cells) {
        return std::regex_search(visible(cells), number);
      }, "Missing dictionary generator transfer result", attempts);
    };
    cmd("genbd->max_blk=262143;");
    cmd(R"("GB %X\n",BlkDevAdd(genbd,I64_MIN,FALSE,TRUE);)");
    try {
      wait_number("GB", 1200);
      cmd(R"(Cd("A:/");)");
      wait_text("A:/>");
    } catch (...) {
      snapshot("ram-disk-terminal");
      throw;
    }
    if (reference) {
      cmd(R"("GM %X\n",ATAMount('C',BDT_ATA,0x1F0,0x3F4,0);)");
      wait_number("GM", 1200);
    }
    cmd(R"(DirMk("Adam");DirMk("Adam/AutoComplete");)");
    cmd(R"(Cd("Adam/AutoComplete");)");
    wait_text("A:/Adam/AutoComplete>");
    cmd("I64 gn;U8 *gp;");
    cmd(R"(gp=FileRead("C:/DICTIONARY.DD",&gn);"GR %X\n",gn;)");
    wait_number("GR", 6000);
    cmd(R"("GW %X\n",FileWrite("DICTIONARY.DD",gp,gn);)");
    wait_number("GW", 6000);
    cmd("Free(gp);");
    if (holyc)
      cmd(R"(Copy("C:/ACDictGen.HC","ACDictGen.HC");)");
    cmd(holyc ? R"(ExeFile2("ACDictGen.HC");"DGG return %X\n",1;)"
              : R"(ACDictGen;"DGG return %X\n",1;)");
    const auto done = wait([&](const Cells &cells) {
      const auto screen = visible(cells);
      return screen.find("DGG return 1") != std::string::npos ||
             screen.find("Except:Compiler") != std::string::npos;
    }, "The dictionary generator did not finish", 28000);
    const auto terminal = visible(done);
    std::ofstream(dir / "generator-terminal.txt") << terminal;
    if (terminal.find("Word File Size:") == std::string::npos ||
        terminal.find("Word Count") == std::string::npos)
      throw std::runtime_error(
          "The dictionary generator returned without writing the files\n" + terminal);
    snapshot("dictionary-generator");
    struct CGeneratedFile {
      const char *name, *evidence, *marker;
    };
    constexpr CGeneratedFile files[]{
        {"DICTIONARY2.DD", "DICTIONARY2.DD", "GPP "},
        {"ACDefs.DATA", "ACDefs.DATA", "GDF "},
        {"ACWords.DATA.Z", "ACWords.DATA", "GWD "}};
    for (const auto &file : files) {
      cmd(std::string(R"(gp=FileRead(")") + file.name + R"(",&gn);")" +
          file.marker + R"(read %X\n",gn;)");
      wait_number(std::string(file.marker) + "read", 6000);
      cmd(std::string("\"") + file.marker + R"(%X %X %X\n",gp,&gn,&gp;)");
      const auto fields = addresses(file.marker, {1, 8, 8}, 6000);
      const auto size = ReadPhysical<1>(vm, fields[1])[0];
      Check(size && in_memory(fields[0], size), "Invalid generated dictionary file size");
      const auto output = dir / file.evidence;
      vm.Command(R"({"execute":"pmemsave","arguments":{"val":)" +
                 std::to_string(fields[0]) + R"(,"size":)" +
                 std::to_string(size) + R"(,"filename":")" + output.string() + R"("}})");
      Check(fs::file_size(output) == size, "Incomplete generated dictionary file capture");
      cmd("Free(gp);");
      std::cout << file.evidence << " has " << size << " bytes\n";
    }
    return;
  }
  if (desktop == CDesktopTest::DictionaryInstall) {
    const auto expected = FileRead("Adam/AutoComplete/ACDefs.DATA");
    const auto blocks = (expected.size() + 511) / 512;
    vm.ResetDeadline(420s);
    if (reference)
      cmd("ATAMount('C',BDT_ATA,0x1F0,0x3F4,0);");
    cmd("Let2Drv('C',0)->drv_let;");
    wait_text("=67");
    cmd("Let2Drv('C',0)->bd->flags&=~BDF_READ_ONLY;");
    cmd(R"(Cd("C:/");)");
    wait_text("C:/>");
    if (reference)
      cmd(R"(blkdev.home_dir=AStrNew("C:/");)");
    cmd(R"(U8 *dcz="/Adam/AutoComplete/ACDefs.DATA.Z";)");
    cmd(R"hc(if(FileFind(dcz))"DIC staged %X\n",1;)hc");
    wait_text("DIC staged 1");
    cmd(R"("ACP %X %X %X\n",&progress1,&progress1_max,progress1_desc;)");
    const auto progress = addresses("ACP ", {8, 8, 48});
    Check(progress[1] == progress[0] + 8,
          "The dictionary progress fields are not adjacent");
    constexpr char desc[] = "Uncompressing Dictionary";
    bool named = false, rendered = false;
    uint64_t written = 0, observed_max = 0;
    type(R"(ACInit;"DIC ready %X\n",acd.num_words;)");
    vm.Hmp("sendkey ret 35");
    const std::regex ready_count(R"((^|\n) {0,4}DIC ready [0-9a-fA-F]+\b)");
    const auto ready = wait(
        [&](const Cells &cells) {
          const auto counters = ReadPhysical<2>(vm, progress[0]);
          const auto val = counters[0], max = counters[1];
          observed_max = std::max(observed_max, max);
          if (max == blocks) {
            written = std::max(written, val);
            if (!named) {
              const auto label =
                  ReadPhysical<sizeof(desc)>(vm, progress[2], 'b');
              named = std::equal(label.begin(), label.end(), desc);
            }
            if (named && val >= blocks / 10 && !rendered) {
              snapshot("dictionary-install-progress");
              const auto rgb = ReadVGA(dir / "dictionary-install-progress.ppm");
              const auto *pixel = &rgb[(172 * 640 + 105) * 3];
              rendered = pixel[1] > 200 && pixel[0] < 150 && pixel[2] < 150;
            }
          }
          return std::regex_search(visible(cells), ready_count);
        },
        "The writable dictionary did not finish initialization", 10000);
    if (visible(ready).find("DIC ready 1743D") == std::string::npos)
      throw std::runtime_error(
          "The installed dictionary did not load 95,293 words\n" +
          visible(ready));
    if (!named || !written || !rendered)
      throw std::runtime_error(
          "Did not observe the dictionary write progress (maximum " +
          std::to_string(observed_max) + ", value " + std::to_string(written) +
          ")");
    wait(
        [&](const Cells &) {
          return !ReadPhysical<1>(vm, progress[0])[0] &&
                 !ReadPhysical<1>(vm, progress[1])[0] &&
                 !ReadPhysical<1>(vm, progress[2], 'b')[0];
        },
        "The dictionary progress bar did not clear");
    cmd("AutoComplete(0);WinMax;");
    cmd(R"hc(if(FileFind(dcz)==0)"DIC removed %X\n",1;)hc");
    wait_text("DIC removed 1");
    cmd(R"(I64 dsz;U8 *dict=FileRead("/Adam/AutoComplete/ACDefs.DATA",&dsz);"DIB %X %X %X\n",dict,&dsz,&progress1;)");
    const auto data = addresses("DIB ", {expected.size(), 8, 8}, 2200);
    Check(ReadPhysical<1>(vm, data[1])[0] == expected.size(),
          "The expanded definitions size differs");
    auto installed = dir / "installed-definitions.bin";
    vm.Command(R"({"execute":"pmemsave","arguments":{"val":)" +
               std::to_string(data[0]) + R"(,"size":)" +
               std::to_string(expected.size()) + R"(,"filename":")" +
               installed.string() + R"("}})");
    Check(FileRead(installed) == expected,
          "The expanded definitions bytes differ");
    std::cout
        << "Dictionary progress reached " << written << " of " << blocks
        << " blocks and cleared. Expanded definitions and word count matched\n";
    return;
  }
  if (desktop == CDesktopTest::Progress) {
    cmd(R"("PGM %X %X %X\n",&progress1,&sys_progresses[0].val,&progress1_max;)");
    const auto progress = addresses("PGM ");
    Check(progress[0] == progress[1] && progress[2] - progress[0] == 8,
          "Progress fields do not alias the CProgress record");
    cmd(R"(ExeFile2("::/Demo/Progress.HC");)");
    const char *phases[]{
        "With description.", "With description and elapsed time.",
        "With description, elapsed time and registry total time.",
        "No description."};
    const auto render = dir / "progress-current.ppm";
    for (size_t i = 0; i < std::size(phases); i++) {
      wait_text(phases[i]);
      Check(ReadPhysical<2>(vm, progress[0]) == std::array<uint64_t, 2>{},
            "The earlier progress bar did not reset");
      key("spc");
      bool filled = false;
      for (unsigned attempt = 0; attempt < 60; attempt++) {
        vm.Command(R"({"execute":"screendump","arguments":{"filename":")" +
                   render.string() + R"("}})");
        const auto rgb = ReadVGA(render);
        auto green = [&](int x) {
          const auto *pixel = &rgb[(172 * 640 + x) * 3];
          return pixel[1] > 200 && pixel[0] < 150 && pixel[2] < 150;
        };
        if (green(105) && !green(520)) {
          filled = true;
          break;
        }
        std::this_thread::sleep_for(45ms);
      }
      Check(filled, "The Progress demo bar did not partially fill");
      fs::copy_file(render,
                    dir / ("progress-" + std::to_string(i + 1) + ".ppm"),
                    fs::copy_options::overwrite_existing);
      wait(
          [&](const Cells &) {
            return ReadPhysical<2>(vm, progress[0]) ==
                   std::array<uint64_t, 2>{};
          },
          "The Progress demo did not clear the bar", 330);
    }
    wait_text("Watch the progress on the wallpaper.");
    vm.ResetDeadline(300s);
    key("spc");
    uint64_t val = 0;
    wait(
        [&](const Cells &) {
          val = ReadPhysical<1>(vm, progress[0])[0];
          return val > 0 && val < 12000000;
        },
        "The Progress demo assembly loop did not update the shared counter");
    const auto first = val;
    for (unsigned i = 0; val && i < 1800; i++) {
      std::this_thread::sleep_for(150ms);
      val = ReadPhysical<1>(vm, progress[0])[0];
    }
    if (val)
      throw std::runtime_error(
          "The Progress demo assembly loop did not clear the counter (" +
          std::to_string(first) + " to " + std::to_string(val) + ")");
    std::cout << "Progress.HC drew and reset its four display phases and ran its assembly loop\n";
    return;
  }
  if (reference_dictionary) {
    cmd("CBlkDev *dictbd=BlkDevNextFreeSlot('A',BDT_RAM);");
    cmd("dictbd->max_blk=131071;BlkDevAdd(dictbd,I64_MIN,FALSE,TRUE);");
    cmd(R"(Cd("A:/");DirMk("Adam");DirMk("Adam/AutoComplete");DirMk("Home");)");
    cmd(R"(Copy("::/Adam/AutoComplete/ACDefs.DATA.Z","/Adam/AutoComplete/ACDefs.DATA");)");
    cmd(R"(Cd("/Home");)");
    wait([&](const Cells &cells) {
      return visible(cells).find(prompt) != std::string::npos;
    }, "The reference dictionary did not finish copying to the RAM drive", 2500);
    cmd(R"(blkdev.home_dir=AStrNew("A:/Home");)");
  }
  if (music_demo) {
    vm.ResetDeadline(180s);
    cmd("I64 mu1=&scrncast.snd_head,mu2=&scrncast.ona;");
    cmd("I64 mu3=&music.play_note_num;");
    cmd(R"("Sound MU %X %X %X\n",mu1,mu2,mu3;)");
    auto [head, ona, note_num] = addresses("MU ", {24, 1, 8});
    cmd("ScrnCast(1,1);DocClear(,1);");
    cmd(R"(ExeFile("::/Demo/Snd/OhGreat.HC");)");
    wait([&](const Cells &cells) {
      return visible(cells).find("is a god of love.") != std::string::npos;
    }, "OhGreat did not play its first line", 600);
    snapshot("music-playing");
    // The original checks ScanChar between complete verses, not each note.
    key("x");
    wait([&](const Cells &cells) {
      auto screen = visible(cells);
      return std::regex_search(screen, std::regex(":/Home> *\\n")) &&
             screen.find("world isn't always nice.") != std::string::npos;
    }, "OhGreat did not finish the song after a real key", 2400);
    Check(ReadPhysical<1>(vm, ona, 'b')[0] == 0,
          "OhGreat left the speaker playing");
    Check(ReadPhysical<1>(vm, note_num)[0] == 4,
          "OhGreat did not finish its last phrase");
    auto node = ReadPhysical<1>(vm, head)[0];
    auto previous = head;
    auto last_time = ReadPhysical<1>(vm, head + 16)[0];
    Bytes notes;
    std::set<uint64_t> visited;
    while (node != head) {
      Check(in_memory(node, 25) && visited.insert(node).second &&
                notes.size() < 512, "Invalid recorded song queue");
      auto entry = ReadPhysical<3>(vm, node);
      Check(entry[1] == previous && entry[2] >= last_time,
            "Broken song queue or timestamps");
      notes.push_back(ReadPhysical<1>(vm, node + 24, 'b')[0]);
      previous = node;
      last_time = entry[2];
      node = entry[0];
    }
    Check(ReadPhysical<1>(vm, head + 8)[0] == previous && !notes.empty(),
          "OhGreat did not record a complete sound queue");
    // One complete verse captured from the pinned TempleOS guest.
    constexpr uint8_t tones[]{
      79, 77, 75, 74, 75, 77, 79, 70, 68, 67, 68, 70,
      79, 77, 75, 74, 75, 77, 79, 70, 68, 67, 68, 70,
      74, 75, 77, 75, 77, 75, 74, 70, 68, 67, 68, 70,
      79, 77, 75, 74, 75, 77, 79, 70, 68, 67, 68, 70,
      79, 77, 75, 74, 75, 77, 79, 70, 68, 67, 68, 70,
      79, 77, 75, 74, 75, 77, 79, 70, 68, 67, 68, 70,
      74, 75, 77, 75, 77, 75, 74, 70, 68, 67, 68, 70,
    };
    Bytes expected{0, 0};
    for (auto tone : tones) {
      expected.push_back(tone);
      expected.push_back(0);
    }
    expected.push_back(0);
    Check(notes == expected, "OhGreat note events differ from the reference");
    std::cout << "OhGreat events " << notes.size() << ':';
    for (auto note : notes) std::cout << ' ' << unsigned(note);
    std::cout << '\n';
    std::ofstream out(dir / "music-notes.bin", std::ios::binary);
    out.write(reinterpret_cast<const char *>(notes.data()), notes.size());
    out.close();
    snapshot("music-exit");
    cmd("ScrnCast(0,1);");
    cmd(R"("Done Music exit %d\n",7*9;)");
    wait_text("Music exit 63");
    return;
  }
  if (blot) {
    cmd(R"("BL %X %X %X\n",gr.dc->body,&Fs->pix_left,&Fs->pix_top;)");
    auto [pixels, left, top] = addresses("BL ", {640 * 480, 8, 8});
    cmd("DocClear(,1);DCFill;MsSet(620,460);");
    cmd(R"(ExeFile("::/Demo/Graphics/Blot.HC");)");
    wait_text("PRESS A KEY");
    auto bitmap = [&](std::string name) {
      auto file = dir / (name + ".raw");
      vm.Command(R"({"execute":"pmemsave","arguments":{"val":)" +
                 std::to_string(pixels) + R"(,"size":307200,"filename":")" +
                 file.string() + R"("}})");
      auto bytes = FileRead(file);
      Check(bytes.size() == 640 * 480, "Incomplete blot drawing");
      return bytes;
    };
    auto drawn = bitmap("blot-drawn");
    std::array<uint64_t, 256> colors{};
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (auto pixel : drawn) {
      colors[pixel]++;
      hash = (hash ^ pixel) * 0x100000001b3ULL;
    }
    std::cout << "Blot " << std::hex << hash << std::dec << " origin "
              << ReadPhysical<1>(vm, left)[0] << ' ' << ReadPhysical<1>(vm, top)[0]
              << " colors " << colors[4] << ' ' << colors[1] << ' ' << colors[14]
              << ' ' << colors[255] << '\n';
    Check(colors[4] && colors[1] && colors[14], "Blot did not draw all three colors");
    Check(hash == 0xed339cb81a835d9dULL && colors[4] == 17188 &&
              colors[1] == 3556 && colors[14] == 1600 && colors[255] == 284856,
          "Blot drawing differs from the pinned reference");
    snapshot("blot-drawn");
    key("spc");
    wait([&](const Cells &) {
      auto data = bitmap("blot-cleared");
      return std::all_of(data.begin(), data.end(), [](uint8_t c) { return c == 255; });
    }, "Blot did not clear graphics on exit");
    wait_text("T:/Home>");
    cmd("3*21;");
    wait_text("=63");
    snapshot("blot-exit");
    return;
  }
  if (screen_capture) {
    cmd("CBlkDev *scbd=BlkDevNextFreeSlot('B',BDT_RAM);");
    cmd("scbd->max_blk=4095;BlkDevAdd(scbd,,0,1);HomeSet(\"B:/\");");
    cmd("Let2Drv('B',0)->drv_let;");
    wait_text("=66");
    cmd("CTask *sct=gr.dc->win_task;");
    cmd(R"("SC %X %X %X\n",gr.dc->body,&sct->pix_left,&sct->pix_top;)");
    auto [pixels, left, top] = addresses("SC ", {640 * 480, 8, 8});
    cmd("DocClear(,1);DCFill;MsSet(480,360);");
    cmd("U8 *scbody=gr.dc->body;");
    cmd("I64 sci;for(sci=0;sci<8;sci++)MemSet(scbody+156484+sci*640,4,16);");
    auto bitmap = [&] {
      auto file = dir / "capture-bitmap.bin";
      vm.Command(R"({"execute":"pmemsave","arguments":{"val":)" +
                 std::to_string(pixels) + R"(,"size":307200,"filename":")" +
                 file.string() + R"("}})");
      auto data = FileRead(file);
      Check(data.size() == 640 * 480, "Incomplete capture overlay");
      return data;
    };
    cmd(R"(ExeFile("::/Demo/Graphics/ScrnCapture.HC");)");
    wait_text("Capture scrn...");
    wait_text("PRESS A KEY");
    snapshot("capture-prompt");
    key("spc");
    wait_text("View captured scrn...");
    snapshot("capture-view-prompt");
    key("spc");
    const int x = 324 + int(ReadPhysical<1>(vm, left)[0]);
    const int y = 244 + int(ReadPhysical<1>(vm, top)[0]);
    Check(x >= 0 && x + 16 <= 640 && y >= 0 && y + 8 <= 480,
          "Screen capture rectangle outside window");
    wait([&](const Cells &) {
      const auto data = bitmap();
      if (std::count_if(data.begin(), data.end(), [](uint8_t c) {return c < 16;}) < 10000)
        return false;
      for (int j = 0; j < 8; j++)
        for (int i = 0; i < 16; i++)
          if (data[(y+j)*640+x+i] != 4)
            return false;
      return true;
    }, "Original capture did not restore the image and test rectangle");
    snapshot("capture-replay");
    key("spc");
    wait([&](const Cells &) {
      const auto data = bitmap();
      return std::all_of(data.begin(), data.end(), [](uint8_t c) {return c == 255;});
    }, "Screen capture did not clear the overlay");
    wait_text("T:/Home>");
    cmd(R"(CDC *sd=GRRead("B:/DemoScrnShot");)");
    cmd(R"("SAVED %d %d %d\n",sd->width,sd->height,sd->body[156484];)");
    wait_text("SAVED 640 480 4");
    cmd("DCDel(sd);");
    snapshot("capture-cleanup");
    std::cout << "Original ScrnCapture.HC saved, replayed and cleared a GR image\n";
  }
  bool probe_declared = false;
  auto upload = [&](const fs::path &source, bool clear = false,
                    bool execute = true) {
    auto expected = FileRead(source);
    Check(!expected.empty() && expected.size() <= 64 * 1024,
          "Reference transfer must contain at most 64 KiB");
    const size_t allocation = (expected.size() + 1 + 4095) & ~size_t(4095);
    cmd(std::string(probe_declared ? "cgprobe" : "U8 *cgprobe") + "=CAlloc(" +
        std::to_string(allocation) + ");");
    probe_declared = true;
    cmd(R"("CGPROBE:%X:END\n",cgprobe;)");
    const std::regex pattern("CGPROBE:([0-9A-Fa-f]+):END");
    std::smatch match;
    std::string screen;
    wait([&](const Cells &cells) {
      screen = visible(cells);
      return std::regex_search(screen, match, pattern);
    }, "Missing reference probe buffer address");
    auto addr = std::stoull(match[1].str(), nullptr, 16);
    Check(in_memory(addr, allocation), "Invalid reference probe buffer");
    char payload[] = "/tmp/unholyos-character-probe-XXXXXX";
    int fd = mkstemp(payload);
    Check(fd >= 0, "Cannot create reference probe transfer file");
    close(fd);
    fs::copy_file(source, payload, fs::copy_options::overwrite_existing);
    std::ostringstream restore;
    restore << "restore " << payload << " binary 0x" << std::hex << addr;
    auto child = fork();
    Check(child >= 0, "Cannot start GDB");
    if (child == 0) {
      execlp("gdb", "gdb", "-q", "-nx", "-batch", "-ex",
             "target remote 127.0.0.1:45362", "-ex", restore.str().c_str(),
             "-ex", "detach", static_cast<char *>(nullptr));
      _exit(127);
    }
    int status;
    pid_t done;
    do
      done = waitpid(child, &status, 0);
    while (done < 0 && errno == EINTR);
    fs::remove(payload);
    Check(done == child && WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "GDB could not restore the reference probe");
    auto uploaded = dir / (source.stem().string() + "-uploaded.bin");
    vm.Command(R"({"execute":"pmemsave","arguments":{"val":)" +
               std::to_string(addr) + R"(,"size":)" +
               std::to_string(expected.size() + 1) + R"(,"filename":")" +
               uploaded.string() + R"("}})");
    expected.push_back(0);
    Check(FileRead(uploaded) == expected,
          "Reference transfer changed payload bytes");
    if (execute)
      cmd(clear ? "DocClear(,1);ExePutS(cgprobe);Free(cgprobe);"
                : "ExePutS(cgprobe);Free(cgprobe);");
  };
  if (extern_import) {
    const bool jit = desktop == CDesktopTest::ExternImportOptionJIT;
    const auto *name = jit ? "ExternImportOptionInvalid.HC" : "ExternImportOption.PRJ";
    const std::string source = jit ? "ETO.HC" : "ETO.PRJ";
    if (reference) {
      cmd(R"(Cd("B:/");)");
      wait_text("B:/>");
      const auto project = fs::absolute(fs::path("Linux/Fixtures") / name);
      upload(project, false, false);
      cmd("FileWrite(\"" + source + "\",cgprobe," +
          std::to_string(fs::file_size(project)) + ");");
    } else
      cmd(std::string("Copy(\"::/") + name + "\",\"" + source + "\");");
    if (jit) {
      cmd(R"(ExeFile2("ETO.HC");)");
      wait_compiler_exception(reference ? "B:/>" : "A:/>");
      const auto doc = save_diagnostic("extern-import-jit.bin");
      const auto *link = reference ? "FL:B:/ETO.HC,4" : "FL:A:/ETO.HC,4";
      Check(doc.find(R"(import not needed at ";")") != std::string::npos &&
                doc.find(link) != std::string::npos,
            "JIT extern conversion reported a different error");
      std::cout << "JIT rejected the converted import at the declaration\n";
      return;
    }
    cmd(R"("EC %X\n",Cmp("ETO.PRJ",,"ETO.BIN");)");
    wait_text("EC 0");
    cmd("public I64 ETOProvider(I64 n){return n*2;}");
    cmd("public I64 ETOData=11,_ETOData=13;");
    cmd(R"hc(if(Load("ETO.BIN"))"EL %X\n",1;)hc");
    wait_text("EL 1");
    cmd("_extern _ETORun I64 ETORun(I64 value);");
    cmd(R"("EV %X\n",ETORun(2);)");
    wait_text("EV 2A");
    std::cout << "Cmp and Load matched extern conversion and local forward declarations\n";
    return;
  }
  if (!module_files.empty()) {
    Check(reference && !text_probes.empty(),
          "A module transfer needs a reference text probe");
    vm.ResetDeadline(300s);
    cmd("CBlkDev *hmdrive=Let2BlkDev('B',FALSE);");
    cmd("I64 hmcluster=0,hmsize=0;U8 *hmfile,*hmpath;Bool hmready;");
    for (const auto &module : module_files) {
      vm.ResetDeadline(300s);
      const auto expected = FileRead(module.source);
      cmd("DocClear(,1);");
      upload(module.source, false, false);
      cmd(std::string("hmpath=\"B:/") + module.guest_name + "\";");
      cmd("hmready=hmdrive && hmdrive->type==BDT_RAM;");
      cmd("hmready=hmready && FileFind(hmpath)==0;");
      cmd(R"("HMREADY:%d:END\n",hmready;)");
      wait_text("HMREADY:1:END");
      cmd("hmcluster=FileWrite(hmpath,cgprobe," +
          std::to_string(expected.size()) + ");");
      cmd("Free(cgprobe);");
      cmd("hmsize=0;hmfile=FileRead(hmpath,&hmsize);");
      cmd(R"("HMFILE:%X:%X:%X:END\n",hmfile,hmsize,hmcluster;)");
      const std::regex pattern(
          "HMFILE:([0-9A-Fa-f]+):([0-9A-Fa-f]+):([0-9A-Fa-f]+):END");
      std::smatch match;
      std::string screen;
      wait(
          [&](const Cells &cells) {
            screen = visible(cells);
            return std::regex_search(screen, match, pattern);
          },
          "Missing reference module file readback");
      const auto addr = std::stoull(match[1].str(), nullptr, 16);
      const auto size = std::stoull(match[2].str(), nullptr, 16);
      const auto cluster = std::stoull(match[3].str(), nullptr, 16);
      Check(size == expected.size() && cluster && cluster != UINT64_MAX &&
                in_memory(addr, size),
            "Invalid reference module file readback");
      const auto readback =
          dir / (std::string(module.evidence_name) + "-file-readback.bin");
      vm.Command(R"({"execute":"pmemsave","arguments":{"val":)" +
                 std::to_string(addr) + R"(,"size":)" + std::to_string(size) +
                 R"(,"filename":")" + readback.string() + R"("}})");
      Check(FileRead(readback) == expected,
            "Reference module file bytes differ");
      cmd("Free(hmfile);");
      std::cout << "Verified " << size
                << " module bytes after upload and RAM-file readback\n";
    }
  }
  if (!text_probes.empty()) {
    std::string failures;
    for (const auto &probe : text_probes) {
      vm.ResetDeadline(150s);
      cmd("DocClear(,1);");
      if (reference && probe.reference_file)
        cmd(std::string("DocClear(,1);ExeFile2(\"") + probe.reference_file + "\");");
      else if (reference)
        upload(fs::absolute(std::string("Linux/Fixtures") + probe.source), true);
      else
        cmd(std::string("ExeFile(\"::") + probe.source + "\");0;");
      if (std::string_view(probe.name) == "popup-menus") {
        wait_text("Menu Beta");
        snapshot("popup-menu");
        key("tab");
        key("spc");
        wait_text("Cancel menu");
        key("shift-esc");
        wait_text("Popup Value:");
        key("esc");
        wait_text("One-time probe");
        snapshot("popup-checkbox");
        key("spc");
        key("tab");
        key("spc");
        wait_text("One-time cancel");
        key("shift-esc");
        wait_text("Registry Notice");
        key("spc");
        key("tab");
        key("spc");
      }
      if (std::string_view(probe.name) == "training-input")
        training_input();
      if (std::string_view(probe.name) == "file-editor")
        file_editor();
      if (std::string_view(probe.name) == "key-map") {
        wait_text("Press the key map probe chord");
        key("ctrl-alt-q");
        wait([&](const Cells &cells) {
          return visible(cells).find("KeyMap Completed.") != std::string::npos;
        }, "KeyMap did not complete its key description scan", 2700);
      }
      const auto cells = wait(
          [&](const Cells &cells) {
            const auto screen = visible(cells);
            const auto finish = screen.find(probe.finish);
            return finish != std::string::npos &&
                   screen.find(prompt, finish) != std::string::npos;
          },
          "Missing completed reference text probe");
      const auto screen = visible(cells);
      std::ofstream screen_file(
          dir / (std::string(probe.name) + "-reference-screen.txt"));
      screen_file << screen;
      screen_file.close();
      Check(bool(screen_file), "Could not record reference text screen");
      snapshot(probe.name);
      const auto first = screen.find(probe.first);
      Check(first != std::string::npos,
            "Missing first reference text probe line");
      if (std::string_view(probe.name) == "assert-warning") {
        Check(first / 81 > 2 && first % 81 == 1,
              "Reference assert warning did not precede the program body");
        const auto diagnostic =
            std::string_view(screen).substr(2 * 81 + 1, first - (2 * 81 + 1));
        const auto warning = diagnostic.find("WARNING:");
        const auto assertion = diagnostic.find("Assert Failed ");
        Check(warning != std::string_view::npos && assertion > warning &&
                  assertion != std::string_view::npos &&
                  diagnostic.find("WARNING:", warning + 1) ==
                      std::string_view::npos &&
                  diagnostic.find("Assert Failed ", assertion + 1) ==
                      std::string_view::npos &&
                  diagnostic.find("ERROR:") == std::string_view::npos,
              "Reference failed assertion did not produce exactly one warning");
      } else if (!probe.allow_prefix)
        Check(
            first / 81 == 2 && first % 81 == 1,
            "Reference text probe did not begin at the cleared window origin");
      else
        Check(first % 81 == 1,"Reference probe result did not begin at a line");
      const auto finish = screen.find(probe.finish, first);
      Check(finish != std::string::npos,
            "Missing last reference text probe line");
      auto terminal_line = [&](size_t row) {
        auto line = std::string_view(screen).substr(row * 81 + first % 81,
                                                    80 - first % 81);
        const auto end = line.find_last_not_of(' ');
        return line.substr(0, end == std::string_view::npos ? 0 : end + 1);
      };
      std::string output;
      for (size_t row = first / 81; row <= finish / 81; row++) {
        output.append(terminal_line(row));
        output += '\n';
      }
      const auto prompt_pos = screen.find(prompt, finish);
      Check(prompt_pos != std::string::npos && prompt_pos % 81 == first % 81 &&
                terminal_line(prompt_pos / 81) == prompt,
            "Reference probe did not return to an empty command prompt");
      bool timed = false;
      const bool answer = !reference || std::string_view(probe.name) == "startup-exec";
      const std::regex timing(answer ? R"([0-9]+\.[0-9]{6}s ans=0x00000000=0)"
                                     : R"([0-9]+\.[0-9]{6}s)");
      for (size_t row = finish / 81 + 1; row < prompt_pos / 81; row++) {
        const auto line = terminal_line(row);
        if (line.empty())
          continue;
        Check(!timed && std::regex_match(line.begin(), line.end(), timing),
              "Unexpected reference output after the text probe");
        timed = true;
      }
      std::ofstream file(dir / (std::string(probe.name) + "-reference.txt"));
      file << output;
      file.close();
      Check(bool(file), "Could not record reference text probe output");
      std::cout << output;
      if (output != probe.expected)
        failures +=
            std::string("Reference output differs: ") + probe.source + "\n";
      if (std::string_view(probe.name) == "dictionary" && output == probe.expected) {
        cmd("AutoComplete(1);");
        dictionary_keys();
        std::cout << "Dictionary keyboard actions passed\n";
        pane_links();
      }
    }
    if (!failures.empty())
      throw std::runtime_error(failures);
    if (std::string_view(text_probes.front().name) == "popup-menus") {
      cmd(R"(ExeFile("::/Demo/DolDoc/MenuBttn.HC");0;)");
      wait_text("Menu Demo");
      key("tab");
      key("spc");
      wait_text("100 Hz");
      snapshot("menu-button-demo");
      key("tab");
      key("spc");
      wait([&](const Cells &cells) {
        auto screen = visible(cells);
        return screen.find("100 Hz") == std::string::npos &&
               screen.find("Menu Demo") != std::string::npos;
      }, "MenuBttn.HC did not return from the frequency popup");
      key("end");
      key("spc");
      wait_text("T:/Home>");
      std::cout << "MenuBttn.HC opened the frequency menu and returned to the prompt\n";
    }
    if (std::string_view(text_probes.front().name) == "ins-reg") {
      cmd("InsReg;0;");
      wait_text("Install Registration");
      wait_text("Host OS:");
      wait_text("Host VM:");
      wait_text("Your Initials:");
      wait_text("This Machine Num (Choose Any):");
      snapshot("ins-reg-form");
      key("spc");
      wait_text("Windows");
      key("down");
      key("down");
      key("esc");
      wait_text("[Linux]");
      key("tab");
      key("spc");
      wait_text("VirtualBox");
      key("down");
      key("down");
      key("down");
      key("esc");
      wait_text("[QEMU]");
      key("tab");
      type("ABC");
      key("tab");
      type("4");
      wait_text("This Machine Num (Choose Any):4");
      snapshot("ins-reg-filled");
      key("esc");
      wait_text("InsReg:ABC:4");
      cmd(R"("Reg fields %d %d\n",ins_reg.host_os,ins_reg.host_vm;0;)");
      wait_text("Reg fields 2 3");
      snapshot("ins-reg-saved");
      std::cout << "Install registration saved Linux, QEMU, ABC and 4\n";
    }
    return;
  }
  if (screen_capture) {
    if (reference) {
      upload(fs::absolute("Linux/Fixtures/CaptureProbe.HC"));
      wait_text("capture probe 7F");
      snapshot("capture-probe");
      std::cout << "Capture probe 7F: exact success and missing-file returns\n";
    }
    return;
  }
  if (flood_fill || sprite_flood) {
    upload(fs::absolute(sprite_flood ? "Linux/Fixtures/SpriteFloodProbe.HC"
                                    : "Linux/Fixtures/FloodFillProbe.HC"));
    auto cells = wait_text(sprite_flood ? "sprite flood F" : "flood fill 1FF");
    const auto screen = visible(cells);
    auto lines = sprite_flood ? sprite_flood_output : flood_fill_output;
    while (!lines.empty()) {
      auto end = lines.find('\n');
      Check(screen.find(lines.substr(0, end)) != std::string::npos,
            "Flood-fill reference output mismatch");
      lines.remove_prefix(end + 1);
    }
    snapshot(sprite_flood ? "sprite-flood" : "flood-fill");
    std::cout << (sprite_flood ? sprite_flood_output : flood_fill_output);
    return;
  }
  if (spline) {
    cmd(R"("SP1 %X %X %X\n",gr.dc->body,&ms.pos.x,&winmgr.updates;)");
    auto [pixels, mouse, updates] = addresses("SP1 ", {640 * 480, 16, 8});
    cmd(R"("SP2 %X %X %X\n",&Fs->pix_left,&Fs->pix_top,&Fs->win_inhibit;)");
    auto [left, top, inhibit] = addresses("SP2 ", {8, 8, 4});
    cmd(R"("SP3 %X %X %X\n",&ms.lb,&ms.rb,&Fs->scroll_x;)");
    auto [lb, rb, scroll] = addresses("SP3 ", {1, 1, 24});
    auto old_inhibit = ReadPhysical<1>(vm, inhibit, 'w')[0];
    auto old_scroll = ReadPhysical<3>(vm, scroll);
    cmd("DocClear(,1);MsSet(32,32);");
    cmd(R"(ExeFile("::/Demo/Graphics/BSpline.HC");)");
    wait_text("Sel ctrl points with left mouse.");
    auto x0 = int(ReadPhysical<1>(vm, left)[0]);
    auto y0 = int(ReadPhysical<1>(vm, top)[0]);
    Check(x0 >= 0 && x0 + 410 < 640 && y0 >= 0 && y0 + 330 < 480,
          "BSpline points outside the window");
    auto frames = [&](uint64_t n) {
      auto start = ReadPhysical<1>(vm, updates)[0];
      for (unsigned i = 0; i < 300; i++) {
        if (ReadPhysical<1>(vm, updates)[0] - start >= n)
          return;
        std::this_thread::sleep_for(30ms);
      }
      throw std::runtime_error("Spline window did not refresh");
    };
    auto move = [&](int x, int y) {
      for (unsigned i = 0; i < 50; i++) {
        auto pos = ReadPhysical<2>(vm, mouse);
        auto dx = x - int64_t(pos[0]), dy = y - int64_t(pos[1]);
        if (!dx && !dy)
          return;
        MouseMove(vm, std::clamp(dx, int64_t(-40), int64_t(40)) * 2,
                  std::clamp(dy, int64_t(-40), int64_t(40)) * 2);
        std::this_thread::sleep_for(60ms);
      }
      throw std::runtime_error("Spline mouse did not reach target");
    };
    auto bitmap = [&](std::string name) {
      auto file = dir / (name + ".raw");
      vm.Command(R"({"execute":"pmemsave","arguments":{"val":)" +
                 std::to_string(pixels) + R"(,"size":307200,"filename":")" +
                 file.string() + R"("}})");
      auto bytes = FileRead(file);
      Check(bytes.size() == 640 * 480, "Incomplete spline drawing");
      return bytes;
    };
    auto fingerprint = [](const auto &bytes) {
      uint64_t hash = 0xCBF29CE484222325;
      for (auto pixel : bytes)
        hash = (hash ^ pixel) * 0x100000001B3;
      return hash;
    };
    auto click = [&](const char *name, uint64_t addr, int x, int y) {
      move(x0 + x, y0 + y);
      frames(4);
      for (bool down : {true, false}) {
        MouseButton(vm, name, down);
        unsigned tries = 0;
        while (bool(ReadPhysical<1>(vm, addr, 'b')[0]) != down && tries++ < 100)
          std::this_thread::sleep_for(30ms);
        Check(tries < 100, "Spline mouse button was not consumed");
        frames(4);
      }
    };
    // These complete drawing buffers were captured on the pinned guest.
    constexpr uint64_t initial = 0x24CDA718D8961325;
    constexpr uint64_t point_hashes[] = {
        0xF989890895D2A096, 0xDCF117DDEBC20395, 0x924113F1B3D82546,
        0xE7FD6E2559370945, 0x6324DC5288E4889E};
    auto before = bitmap("spline-initial");
    Check(fingerprint(before) == initial, "BSpline initial drawing differs");
    unsigned i = 0;
    for (auto [x,y] : {std::pair{130,130}, std::pair{360,100},
                       std::pair{400,290}, std::pair{200,320},
                       std::pair{110,230}}) {
      click("left", lb, x, y);
      auto after = bitmap("spline-point" + std::to_string(++i));
      Check(std::count(after.begin(), after.end(), uint8_t(2)) >
                std::count(before.begin(), before.end(), uint8_t(2)),
            "BSpline did not mark a clicked control point");
      Check(fingerprint(after) == point_hashes[i - 1],
            "BSpline control points differ from the pinned guest");
      before = std::move(after);
    }
    click("right", rb, 300, 380);
    wait_text("PRESS A KEY");
    frames(4);
    auto drawn = bitmap("spline-drawn");
    Check(std::count(drawn.begin(), drawn.end(), uint8_t(4)) > 100 &&
              std::count(drawn.begin(), drawn.end(), uint8_t(1)) > 100,
          "BSpline did not draw both closed curves");
    auto hash = fingerprint(drawn);
    std::cout << "BSpline drawing " << std::hex << hash << std::dec << '\n';
    Check(hash == 0xFE26C3D4972FD4F0,
          "BSpline closed curves differ from the pinned guest");
    snapshot("spline-drawn");
    key("ret");
    wait_text("T:/Home>");
    cmd(R"("Done Spline exit %d\n",7*9;)");
    wait_text("Spline exit 63");
    auto restored_inhibit = ReadPhysical<1>(vm, inhibit, 'w')[0];
    auto restored_scroll = ReadPhysical<3>(vm, scroll);
    std::cout << "BSpline settings " << std::hex << old_inhibit << ' '
              << restored_inhibit << std::dec << " scroll";
    for (auto v : old_scroll) std::cout << ' ' << int64_t(v);
    for (auto v : restored_scroll) std::cout << ' ' << int64_t(v);
    std::cout << '\n';
    Check(restored_inhibit == old_inhibit && restored_scroll == old_scroll,
          "BSpline did not restore task settings");
    auto cleared = bitmap("spline-cleared");
    Check(fingerprint(cleared) == initial &&
              std::all_of(cleared.begin(), cleared.end(),
                          [](uint8_t p) { return p == 0xFF; }),
          "BSpline did not clear the screen");
    snapshot("spline-exit");
    return;
  }
  if (breaks) {
    cmd(R"("BR %X %X %X\n",&Fs->task_flags,&Fs->except_ch,&Fs->last_except;)");
    auto [flags, exception, last] = addresses("BR ");
    auto old_last = ReadPhysical<1>(vm, last)[0];
    if (reference)
      upload(fs::absolute("Linux/Fixtures/BreakProbe.HC"));
    else
      cmd(R"(ExeFile("::/BreakProbe.HC");)");
    wait_text("Break probe locked");
    Check((ReadPhysical<1>(vm, flags)[0] & 0x1C00) == 0x400,
          "BreakLock did not lock the task");
    snapshot("break-locked");
    key("ctrl-alt-c");
    wait_text("Break probe shift");
    Check((ReadPhysical<1>(vm, flags)[0] & 0x1C00) == 0x1000 &&
              ReadPhysical<1>(vm, exception)[0] == 0x6B61657242,
          "Pending break was not caught and cleared");
    snapshot("break-shift");
    key("ctrl-alt-c");
    wait_text("Break probe 7F");
    wait([&](const Cells &) {
      return !(ReadPhysical<1>(vm, flags)[0] & 0x1C00) &&
             ReadPhysical<1>(vm, last)[0] == old_last;
    }, "Break probe leaked task flags or exception frames");
    snapshot("break-complete");
    std::cout << "Break probe 7F: direct, locked, pending and Shift-Esc delivery\n";
    return;
  }
  if (tic) {
    cmd(R"("TT1 %X %X %X\n",gr.dc->body,Fs->hash_table,&ms.pos.x;)");
    auto [pixels, table, mouse] = addresses("TT1 ", {640 * 480, 32, 16});
    cmd(R"("TT2 %X %X %X\n",&Fs->pix_left,&Fs->pix_top,&ms.lb;)");
    auto [left, top, button] = addresses("TT2 ", {8, 8, 1});
    cmd(R"("TT3 %X %X %X\n",&winmgr.updates,&Fs->task_flags,&Fs->except_ch;)");
    auto [updates, flags, exception] = addresses("TT3 ");
    cmd("DocClear(,1);MsSet(32,32);");
    cmd(R"(ExeFile("::/Demo/Games/TicTacToe.HC");)");
    wait_text("CTRL-ALT-c to exit.");
    const auto x0 = int(ReadPhysical<1>(vm, left)[0]);
    const auto y0 = int(ReadPhysical<1>(vm, top)[0]);
    Check(x0 >= 0 && x0 + 403 < 640 && y0 >= 0 && y0 + 403 < 480,
          "TicTacToe board outside the graphics buffer");
    // Resolve the original program's global array from the guest hash table.
    uint64_t hash = 0;
    for (char c : std::string_view("board"))
      hash = (hash << 1) + (hash >> 63) + uint8_t(c);
    hash += (hash >> 16) + ((hash >> 15) & 1);
    uint64_t board_addr = 0;
    for (unsigned tries = 0; tries < 20 && !board_addr; tries++) {
      for (uint64_t tab = table; tab && !board_addr;) {
        Check(in_memory(tab, 32), "Invalid TicTacToe hash table");
        auto hdr = ReadPhysical<4>(vm, tab);
        auto slot = hash & hdr[1];
        Check(in_memory(hdr[3], 8) && slot <= (memory_limit - hdr[3] - 8) / 8,
              "Invalid TicTacToe hash bucket");
        auto entry = ReadPhysical<1>(vm, hdr[3] + slot * 8)[0];
        for (unsigned i = 0; entry && i < 128; i++) {
          Check(in_memory(entry, 24), "Invalid TicTacToe hash entry");
          auto h = ReadPhysical<3>(vm, entry);
          if ((h[2] & 8) && h[1])
            Check(in_memory(h[1], 6), "Invalid TicTacToe global name");
          if ((h[2] & 8) && h[1] &&
              ReadPhysical<6>(vm, h[1], 'b') ==
                  std::array<uint64_t, 6>{'b','o','a','r','d',0}) {
            Check(in_memory(entry, 128), "Invalid TicTacToe global record");
            board_addr = ReadPhysical<1>(vm, entry + 120)[0];
            break;
          }
          entry = h[0];
        }
        tab = hdr[0];
      }
      if (!board_addr)
        std::this_thread::sleep_for(100ms);
    }
    Check(in_memory(board_addr, 9), "TicTacToe board was not compiled");
    using Board = std::array<uint64_t, 9>;
    Board expected{};
    auto board = [&] { return ReadPhysical<9>(vm, board_addr, 'b'); };
    auto wait_board = [&] {
      for (unsigned i = 0; i < 120; i++) {
        if (board() == expected)
          return;
        std::this_thread::sleep_for(35ms);
      }
      std::ostringstream message;
      message << "TicTacToe board state differs: expected ";
      for (auto v : expected) message << v;
      message << ", got ";
      for (auto v : board()) message << v;
      throw std::runtime_error(message.str());
    };
    auto frames = [&](uint64_t count) {
      auto start = ReadPhysical<1>(vm, updates)[0];
      for (unsigned i = 0; i < 500; i++) {
        if (ReadPhysical<1>(vm, updates)[0] - start >= count)
          return;
        std::this_thread::sleep_for(30ms);
      }
      throw std::runtime_error("TicTacToe window manager did not refresh");
    };
    auto move = [&](int x, int y) {
      for (unsigned i = 0; i < 50; i++) {
        auto pos = ReadPhysical<2>(vm, mouse);
        auto dx = x - int64_t(pos[0]), dy = y - int64_t(pos[1]);
        if (!dx && !dy)
          return;
        MouseMove(vm, std::clamp(dx, int64_t(-40), int64_t(40)) * 2,
                  std::clamp(dy, int64_t(-40), int64_t(40)) * 2);
        std::this_thread::sleep_for(60ms);
      }
      throw std::runtime_error("TicTacToe mouse did not reach target");
    };
    auto click = [&](int x, int y) {
      move(x0 + x, y0 + y);
      frames(4);
      for (bool down : {true, false}) {
        MouseButton(vm, "left", down);
        unsigned tries = 0;
        while (ReadPhysical<1>(vm, button, 'b')[0] != down && ++tries < 100)
          std::this_thread::sleep_for(25ms);
        Check(tries < 100, "TicTacToe mouse button did not change");
        frames(5);
      }
    };
    auto image_path = dir / "tic-bitmap.bin";
    auto image_hash = [&] {
      vm.Command(R"({"execute":"pmemsave","arguments":{"val":)" +
                 std::to_string(pixels) + R"(,"size":307200,"filename":")" +
                 image_path.string() + R"("}})");
      auto bitmap = FileRead(image_path);
      Check(bitmap.size() == 640 * 480, "Incomplete TicTacToe graphics buffer");
      uint64_t value = 0xCBF29CE484222325;
      for (int y = 97; y <= 403; y++)
        for (int x = 97; x <= 403; x++)
          value = (value ^ bitmap[(y0 + y) * 640 + x0 + x]) * 0x100000001B3;
      return value;
    };
    // The settled board region in each phase is measured on the pinned guest.
    constexpr std::pair<std::string_view, uint64_t> oracle[] = {
      {"tic-empty",0x9611FC41100952A6}, {"tic-x0",0x55A64FE52AD3D56F},
      {"tic-occupied",0x55A64FE52AD3D56F}, {"tic-outside",0x55A64FE52AD3D56F},
      {"tic-o3",0x4AB62BD7737049FD}, {"tic-x1",0xB3036FCE078BDB44},
      {"tic-o4",0xF362F90B89421FF6}, {"tic-x-win",0x9611FC41100952A6},
      {"tic-next-x0",0x55A64FE52AD3D56F}, {"tic-next-o3",0x4AB62BD7737049FD},
      {"tic-next-x1",0xB3036FCE078BDB44}, {"tic-next-o4",0xF362F90B89421FF6},
      {"tic-next-x8",0x6C21CB8B7740621F}, {"tic-o-win",0x9611FC41100952A6}};
    size_t phase = 0;
    auto draw = [&](const char *name) {
      move(x0 + 32, y0 + 32);
      // TakeTurn beeps for 500 ms, then sleeps another 200 ms before
      // drawing the next frame. Do not compare a stale cursor preview.
      frames(35);
      auto value = image_hash();
      for (unsigned i = 0; i < 30; i++) {
        std::this_thread::sleep_for(40ms);
        auto next = image_hash();
        if (next == value) {
          std::cout << "TicTacToe " << name << ' ' << std::hex << value
                    << std::dec << '\n';
          fs::copy_file(image_path, dir / (std::string(name) + ".raw"),
                        fs::copy_options::overwrite_existing);
          snapshot(name);
          Check(phase < std::size(oracle) && oracle[phase].first == name &&
                    oracle[phase].second == value,
                "TicTacToe board pixels differ from the pinned guest");
          ++phase;
          return value;
        }
        value = next;
      }
      throw std::runtime_error("TicTacToe graphics did not settle");
    };
    wait_board();
    const auto empty = draw("tic-empty");
    uint64_t prior = empty;
    auto play = [&](unsigned cell, unsigned player, const char *name,
                    bool reset = false) {
      click(150 + int(cell % 3) * 100, 150 + int(cell / 3) * 100);
      expected[cell] = player;
      wait_board();
      if (reset) {
        expected = {};
        wait_board();
      }
      auto value = draw(name);
      Check(reset ? value == empty : value != prior,
            "TicTacToe did not redraw after the move");
      prior = value;
    };
    play(0, 1, "tic-x0");
    click(150, 150);
    wait_board();
    Check(draw("tic-occupied") == prior, "TicTacToe accepted an occupied cell");
    click(50, 50);
    wait_board();
    Check(draw("tic-outside") == prior, "TicTacToe accepted an outside click");
    play(3, 2, "tic-o3");
    play(1, 1, "tic-x1");
    play(4, 2, "tic-o4");
    play(2, 1, "tic-x-win", true);
    play(0, 1, "tic-next-x0");
    play(3, 2, "tic-next-o3");
    play(1, 1, "tic-next-x1");
    play(4, 2, "tic-next-o4");
    play(8, 1, "tic-next-x8");
    play(5, 2, "tic-o-win", true);
    key("ctrl-alt-c");
    wait_text("T:/Home>");
    Check(ReadPhysical<1>(vm, exception)[0] == 0x6B61657242,
          "TicTacToe exit did not catch Break");
    Check(!(ReadPhysical<1>(vm, flags)[0] & 0xC00),
          "TicTacToe exit left a pending or locked break");
    auto cleared = image_hash();
    Check(cleared != empty, "TicTacToe did not clear graphics on exit");
    auto bitmap = FileRead(image_path);
    for (int y = 97; y <= 403; y++)
      for (int x = 97; x <= 403; x++)
        Check(bitmap[(y0 + y) * 640 + x0 + x] == 0xFF,
              "TicTacToe left graphics in the board region");
    Check(phase == std::size(oracle), "Missing TicTacToe board checkpoint");
    snapshot("tic-exit");
    cmd(R"("TicTacToe exit %d\n",7*9;)");
    wait_text("TicTacToe exit 63");
    std::cout << "TicTacToe.HC rejected invalid moves, reset both wins and exited on Ctrl-Alt-C\n";
    return;
  }
  if (reference) {
    // Transfer the exact HolyC probe to an allocated guest buffer.
    upload(fs::absolute("Linux/Fixtures/MemSetProbe.HC"));
    wait_text("Memory set probe 3FF");
    snapshot("reference-memset");
    std::cout << "Reference memory set and cursor flags probe 3FF\n";
  }
  cmd("CDoc *cgdoc=DocPut;");
  cmd(R"("CG1 %X %X %X\n",&text.font,&gr.pan_text_x,&gr.hide_row;)");
  auto [font_slot, pan_slot, hide_slot] = addresses("CG1 ", {8, 16, 2});
  cmd(R"("CG2 %X %X %X\n",&Fs->draw_it,&Fs->animate_task,&cgdoc->flags;)");
  auto [draw_slot, anim_slot, flags_slot] = addresses("CG2 ");
  cmd(R"("CG3 %X %X %X\n",&ms.pos.x,&Fs->win_left,&Fs->scroll_x;)");
  auto [mouse_slot, window_slot, scroll_slot] = addresses("CG3 ", {16, 32, 24});
  auto old_font = ReadPhysical<1>(vm, font_slot)[0];
  auto old_draw = ReadPhysical<1>(vm, draw_slot)[0];
  auto old_anim = ReadPhysical<1>(vm, anim_slot)[0];
  auto old_flags = ReadPhysical<1>(vm, flags_slot)[0] & 0x3800;
  auto old_window = ReadPhysical<4>(vm, window_slot);
  auto old_scroll = ReadPhysical<3>(vm, scroll_slot);
  auto restored = [&] {
    return ReadPhysical<1>(vm, font_slot)[0] == old_font &&
           ReadPhysical<1>(vm, draw_slot)[0] == old_draw &&
           ReadPhysical<1>(vm, anim_slot)[0] == old_anim &&
           (ReadPhysical<1>(vm, flags_slot)[0] & 0x3800) == old_flags &&
           ReadPhysical<4>(vm, window_slot) == old_window &&
           ReadPhysical<3>(vm, scroll_slot) == old_scroll &&
           ReadPhysical<2>(vm, pan_slot) == std::array<uint64_t, 2>{0, 0} &&
           ReadPhysical<2>(vm, hide_slot, 'b') == std::array<uint64_t, 2>{0, 0};
  };
  auto wait_restored = [&] {
    for (unsigned i = 0; i < 100; i++) {
      if (restored())
        return;
      std::this_thread::sleep_for(35ms);
    }
    throw std::runtime_error("Character graphics did not restore task and font state");
  };
  constexpr uint32_t water = 0x195E, land = 0xE020, tree = 0xE22A;
  auto map_visible = [&](const Cells &cells) {
    for (size_t y = 2; y < 58; y++)
      for (size_t x = 2; x < 78; x++)
        if (auto c = cells[y * 80 + x]; c != water && c != land && c != tree)
          return false;
    return std::count(cells.begin(), cells.end(), water) > 500 &&
           std::count(cells.begin(), cells.end(), land) > 200 &&
           std::count(cells.begin(), cells.end(), tree) > 200;
  };
  auto terrain_hash = [](const Cells &cells) {
    uint64_t hash = 0xCBF29CE484222325;
    for (size_t y = 8; y < 53; y++)
      for (size_t x = 8; x < 73; x++)
        for (unsigned shift = 0; shift < 32; shift += 8)
          hash = (hash ^ uint8_t(cells[y * 80 + x] >> shift)) * 0x100000001B3;
    return hash;
  };
  // Load identical test-only source on both guests. Long keyboard-entered
  // definitions and address commands can leave the terminal panned or still
  // editing when the host expects the observer's setup output.
  cmd("DocClear(,1);");
  if (reference)
    upload(fs::absolute("Linux/Fixtures/CharPanObserver.HC"),true);
  else
    cmd(R"(ExeFile("::/CharPanObserver.HC");)");
  // Column zero belongs to the window border (the reference spells its
  // title vertically there). Match the complete marker inside the window.
  const std::regex observer_marker(R"((?:^|\n). *CGPAN:([0-9A-Fa-f]+):END *\n)");
  std::smatch observer_match;
  std::string observer_screen;
  wait([&](const Cells &cells) {
    observer_screen=visible(cells);
    return std::regex_search(observer_screen,observer_match,observer_marker) &&
           observer_screen.find("T:/Home>",observer_match.position()+observer_match.length())!=std::string::npos;
  },"CharDemo pan observer setup did not finish");
  const auto pan_watch=std::stoull(observer_match[1].str(),nullptr,16);
  Check(in_memory(pan_watch,32),"Invalid CharDemo pan observer address");
  const auto initial_observer = ReadPhysical<4>(vm, pan_watch);
  Check(initial_observer==std::array<uint64_t,4>{},
        "CharDemo pan observer was not initialized");
  // Start the observer and enter the demo in one guest operation.
  cmd("CGRun;");
  wait([&](const Cells &) {
    const auto observed = ReadPhysical<4>(vm, pan_watch);
    return !observed[2] && observed[3] > initial_observer[3];
  },
       "CharDemo pan observer did not start");
  auto initial = wait(map_visible, "CharDemo did not draw its terrain");
  Check(terrain_hash(initial) == 0xB980E584B8EAAC9C,
        "CharDemo initial terrain differs from the pinned seed");
  snapshot("char-initial");
  constexpr std::array<uint64_t, 4> waves{
      0x0011AA440011AA44, 0x0022558800225588,
      0x0044AA110044AA11, 0x0088552200885522};
  uint64_t font = 0;
  for (unsigned i = 0; i < 100; i++) {
    font = ReadPhysical<1>(vm, font_slot)[0];
    if (font)
      Check(in_memory(font, ('^' + 1) * 8), "Invalid animated font buffer");
    if (font && font != old_font &&
        ReadPhysical<1>(vm, font + '*' * 8)[0] == 0x18187E7E3C3C18) {
      auto wave = ReadPhysical<1>(vm, font + '^' * 8)[0];
      if (std::find(waves.begin(), waves.end(), wave) != waves.end())
        break;
    }
    font = 0;
    std::this_thread::sleep_for(25ms);
  }
  Check(font != 0, "CharDemo did not install its animated font");
  Check(ReadPhysical<1>(vm, font + '*' * 8)[0] == 0x18187E7E3C3C18,
        "CharDemo tree glyph differs");
  std::set<uint64_t> observed;
  for (unsigned i = 0; i < 40 && observed.size() < waves.size(); i++) {
    auto wave = ReadPhysical<1>(vm, font + '^' * 8)[0];
    Check(std::find(waves.begin(), waves.end(), wave) != waves.end(),
          "Unexpected animated water glyph");
    observed.insert(wave);
    std::this_thread::sleep_for(45ms);
  }
  Check(observed.size() == waves.size(), "CharDemo did not animate all four water glyphs");
  Check(ReadPhysical<2>(vm, hide_slot, 'b') == std::array<uint64_t, 2>{1, 1},
        "CharDemo did not hide the last text row and column");
  auto matches_shift = [](const Cells &before, const Cells &after, int dx, int dy) {
    // Exclude the screen header and the panned last row/column.
    for (int y = 8; y < 53; y++)
      for (int x = 8; x < 73; x++)
        if (before[(y + dy) * 80 + x + dx] != after[y * 80 + x])
          return false;
    return true;
  };
  auto scroll = [&](const Cells &before, const char *button, int dx, int dy,
                    const char *name) {
    const size_t axis = dx ? 0 : 1;
    const auto start = ReadPhysical<4>(vm, pan_watch);
    key(button);
    std::array<uint64_t, 2> pan{};
    std::array<uint64_t, 4> observed{};
    bool shifted = false;
    Cells cells;
    try {
      cells = wait([&](const Cells &now) {
        pan = ReadPhysical<2>(vm, pan_slot);
        observed = ReadPhysical<4>(vm, pan_watch);
        shifted = matches_shift(before, now, dx, dy);
        return observed[axis] > start[axis] && pan[0] == 7 && pan[1] == 7 && shifted;
      }, "CharDemo did not scroll exactly four cells");
    } catch (...) {
      std::cerr << name << " pan " << pan[0] << ' ' << pan[1]
                << " observer " << start[0] << ' ' << start[1] << ' ' << start[3]
                << " -> " << observed[0] << ' ' << observed[1] << ' ' << observed[3]
                << " shifted " << shifted << '\n';
      throw;
    }
    snapshot(name);
    return cells;
  };
  auto right = scroll(initial, "right", 4, 0, "char-right");
  auto down = scroll(right, "down", 0, 4, "char-down");
  auto left = scroll(down, "left", -4, 0, "char-left");
  auto up = scroll(left, "up", 0, -4, "char-up");
  Check(terrain_hash(right) == 0x60215F375DAD5EB8 &&
            terrain_hash(down) == 0xE000320614958604 &&
            terrain_hash(left) == 0x25FF83C4E708EFD8 &&
            terrain_hash(up) == 0xB980E584B8EAAC9C,
        "CharDemo scrolled terrain differs from the pinned seed");
  Check(matches_shift(initial, up, 0, 0), "CharDemo did not return to its first viewport");
  key("ret");
  auto restart = wait([&](const Cells &cells) {
    return map_visible(cells) && !matches_shift(initial, cells, 0, 0);
  }, "CharDemo did not regenerate its map");
  Check(map_visible(restart), "CharDemo restart has no terrain");
  Check(terrain_hash(restart) == 0xA38CC20CF8367694,
        "CharDemo restarted terrain differs from the pinned seed");
  snapshot("char-restart");
  key("esc");
  wait_text("T:/Home>");
  wait_restored();
  snapshot("char-exit");
  wait([&](const Cells &) {
    return ReadPhysical<4>(vm, pan_watch)[2] == 2;
  }, "CharDemo pan observer did not stop");
  wait([&](const Cells &cells) {
    auto screen=visible(cells);
    auto done=screen.find("CGWATCH 2");
    return done!=std::string::npos && screen.find("T:/Home>",done)!=std::string::npos;
  },"CharDemo pan observer did not exit");
  cmd("CGDone;");
  wait([&](const Cells &cells) {
    auto screen=visible(cells);
    auto done=screen.find("CGDONE");
    return done!=std::string::npos && screen.find("T:/Home>",done)!=std::string::npos;
  },"CharDemo pan observer cleanup did not finish");
  std::cout << "CharDemo.HC animated all four glyphs, scrolled four ways, restarted and restored state\n";

  auto move_to = [&](int x, int y) {
    for (unsigned i = 0; i < 40; i++) {
      auto pos = ReadPhysical<2>(vm, mouse_slot);
      auto dx = x - int64_t(pos[0]), dy = y - int64_t(pos[1]);
      if (!dx && !dy)
        return;
      MouseMove(vm, std::clamp(dx, int64_t(-40), int64_t(40)) * 2,
                std::clamp(dy, int64_t(-40), int64_t(40)) * 2);
      std::this_thread::sleep_for(80ms);
    }
    throw std::runtime_error("Could not move the maze mouse to its target");
  };
  cmd("DocClear(,1);MsSet(240,416);");
  cmd(R"(ExeFile("::/Demo/Games/Maze.HC");)");
  wait_text("Draw a maze with left bttn.");
  wait_text("Solve maze starting at right click.");
  snapshot("maze-initial");
  constexpr uint32_t wall = 0x20000F20, start = 0xF2A, dot = 0xF2E;
  move_to(240, 416);
  MouseButton(vm, "left", true);
  wait([&](const Cells &cells) {
    return std::count(cells.begin(), cells.end(), wall) >= 1;
  }, "Maze left button did not draw a wall");
  for (int x = 248; x <= 288; x += 8) {
    move_to(x, 416);
    std::this_thread::sleep_for(100ms);
  }
  MouseButton(vm, "left", false);
  auto walls = wait([&](const Cells &cells) {
    return std::count(cells.begin(), cells.end(), wall) == 7;
  }, "Maze mouse drag did not draw a wall");
  for (size_t x = 30; x <= 36; x++)
    Check(walls[52 * 80 + x] == wall, "Maze wall coordinate differs");
  snapshot("maze-wall");
  move_to(264, 392);
  MouseButton(vm, "right", true);
  auto started = wait([&](const Cells &cells) {
    return std::count(cells.begin(), cells.end(), start) == 1;
  }, "Maze right click did not start solving");
  MouseButton(vm, "right", false);
  auto solved = wait([&](const Cells &cells) {
    return cells[53 * 80 + 37] == dot;
  }, "Maze did not follow a path around the wall");
  for (size_t i = 0; i < walls.size(); i++)
    if (walls[i] == wall)
      Check(solved[i] == wall, "Maze solution overwrote a wall");
  auto start_at = std::find(started.begin(), started.end(), start) - started.begin();
  Check(start_at == 49 * 80 + 33 && solved[start_at] == start,
        "Maze starting point differs");
  constexpr std::array<size_t, 8> path_cells{
      50 * 80 + 33, 51 * 80 + 33, 51 * 80 + 34, 51 * 80 + 35,
      51 * 80 + 36, 51 * 80 + 37, 52 * 80 + 37, 53 * 80 + 37};
  for (auto i : path_cells)
    Check(solved[i] == dot, "Maze solution path differs");
  snapshot("maze-solved");
  // Wait for the solver to reach the bottom before giving its GetChar a key.
  wait([&](const Cells &cells) {
    return std::count(cells.begin() + 57 * 80, cells.end(), dot) > 0;
  }, "Maze solver did not reach the lower boundary");
  std::this_thread::sleep_for(600ms);
  key("ret");
  wait([&](const Cells &cells) {
    return std::count(cells.begin(), cells.end(), wall) == 0 &&
           std::count(cells.begin(), cells.end(), start) == 0 &&
           std::count(cells.begin() + 3 * 80, cells.end(), dot) == 0 &&
           visible(cells).find("Draw a maze with left bttn.") != std::string::npos;
  }, "Maze did not clear its board after solving");
  snapshot("maze-reset");
  key("esc");
  wait_text("T:/Home>");
  wait_restored();
  snapshot("maze-exit");
  std::cout << "Maze.HC drew a wall, solved around it, cleared and restored state\n";
}
