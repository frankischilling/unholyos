#pragma once

// Uses the same QMP input and physical-memory readers as the boot tests.
static void DriveCharacterGraphics(CMonitor &vm, const fs::path &dir,
                                   const std::string &boot, bool reference,
                                   bool tic = false, bool breaks = false)
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
  auto upload = [&](const fs::path &source) {
    Check(fs::file_size(source) < 4096, "Probe exceeds guest buffer");
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
  };
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
    auto [pixels, table, mouse] = addresses("TT1 ");
    cmd(R"("TT2 %X %X %X\n",&Fs->pix_left,&Fs->pix_top,&ms.lb;)");
    auto [left, top, button] = addresses("TT2 ");
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
        auto hdr = ReadPhysical<4>(vm, tab);
        auto entry = ReadPhysical<1>(vm, hdr[3] + (hash & hdr[1]) * 8)[0];
        for (unsigned i = 0; entry && i < 128; i++) {
          auto h = ReadPhysical<3>(vm, entry);
          if ((h[2] & 8) && h[1] &&
              ReadPhysical<6>(vm, h[1], 'b') ==
                  std::array<uint64_t, 6>{'b','o','a','r','d',0}) {
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
    Check(board_addr && board_addr < 0x20000000, "TicTacToe board was not compiled");
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
