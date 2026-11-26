#pragma once

// Uses the same QMP input and physical-memory readers as the boot tests.
static void DriveCharacterGraphics(CMonitor &vm, const fs::path &dir,
                                   const std::string &boot, bool reference,
                                   bool music_demo = false)
{
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
  Check(base && base < 0x20000000, "Invalid character graphics text address");
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
  auto addresses = [&](std::string_view marker) {
    const std::regex pattern(std::string(marker) +
                             "([0-9A-Fa-f]+) ([0-9A-Fa-f]+) ([0-9A-Fa-f]+)");
    std::smatch match;
    std::string screen;
    wait([&](const Cells &cells) {
      screen = visible(cells);
      return std::regex_search(screen, match, pattern);
    }, "Missing graphics runtime addresses");
    std::array<uint64_t, 3> result{};
    for (size_t i = 0; i < result.size(); i++) {
      result[i] = std::stoull(match[i + 1].str(), nullptr, 16);
      Check(result[i] && result[i] < 0x20000000, "Invalid graphics runtime address");
    }
    return result;
  };
  wait_text("T:/Home>");
  cmd("WinMax;AutoComplete(0);DocClear(,1);");
  if (music_demo) {
    vm.ResetDeadline(180s);
    cmd("I64 mu1=&scrncast.snd_head,mu2=&scrncast.ona;");
    cmd("I64 mu3=&music.play_note_num;");
    cmd(R"("Sound MU %X %X %X\n",mu1,mu2,mu3;)");
    auto [head, ona, note_num] = addresses("MU ");
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
      Check(node && node < 0x20000000 && visited.insert(node).second &&
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
  if (reference) {
    // Transfer the exact HolyC probe to an allocated guest buffer.
    const auto source = fs::absolute("Linux/Fixtures/MemSetProbe.HC");
    Check(fs::file_size(source) < 4096, "Memory set probe exceeds guest buffer");
    cmd("U8 *cgprobe=CAlloc(4096);");
    cmd(R"("CGPROBE:%X:END\n",cgprobe;)");
    const std::regex pattern("CGPROBE:([0-9A-Fa-f]+):END");
    std::smatch match;
    std::string screen;
    wait([&](const Cells &cells) {
      screen = visible(cells);
      return std::regex_search(screen, match, pattern);
    }, "Missing reference probe buffer address");
    auto addr = std::stoull(match[1].str(), nullptr, 16);
    Check(addr && addr < 0x20000000 - 4096, "Invalid reference probe buffer");
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
    auto uploaded = dir / "reference-probe-source.bin";
    vm.Command(R"({"execute":"pmemsave","arguments":{"val":)" +
               std::to_string(addr) + R"(,"size":)" +
               std::to_string(fs::file_size(source) + 1) +
               R"(,"filename":")" + uploaded.string() + R"("}})");
    auto expected = FileRead(source);
    expected.push_back(0);
    Check(FileRead(uploaded) == expected, "Reference probe transfer changed source bytes");
    cmd("ExePutS(cgprobe);Free(cgprobe);");
    wait_text("Memory set probe 3FF");
    snapshot("reference-memset");
    std::cout << "Reference memory set and cursor flags probe 3FF\n";
  }
  cmd("CDoc *cgdoc=DocPut;");
  cmd(R"("CG1 %X %X %X\n",&text.font,&gr.pan_text_x,&gr.hide_row;)");
  auto [font_slot, pan_slot, hide_slot] = addresses("CG1 ");
  cmd(R"("CG2 %X %X %X\n",&Fs->draw_it,&Fs->animate_task,&cgdoc->flags;)");
  auto [draw_slot, anim_slot, flags_slot] = addresses("CG2 ");
  cmd(R"("CG3 %X %X %X\n",&ms.pos.x,&Fs->win_left,&Fs->scroll_x;)");
  auto [mouse_slot, window_slot, scroll_slot] = addresses("CG3 ");
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
  cmd("Seed(12345);DocClear(,1);");
  cmd(R"(ExeFile("::/Demo/Games/CharDemo.HC");)");
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
    key(button);
    bool subcell = false;
    auto cells = wait([&](const Cells &now) {
      auto pan = ReadPhysical<2>(vm, pan_slot);
      subcell |= pan[0] != 7 || pan[1] != 7;
      return subcell && pan[0] == 7 && pan[1] == 7 &&
             matches_shift(before, now, dx, dy);
    }, "CharDemo did not scroll exactly four cells");
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
