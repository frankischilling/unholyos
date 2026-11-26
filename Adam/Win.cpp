#include "../Kernel/KernelB.hpp"
#include "Gr/GrGlbls.hpp"

#include <bit>

CWinMgrGlbls winmgr{0, 0, 0, std::bit_cast<uint64_t>(30000.0 / 1001),
                   0, 0, nullptr, false, false, false};

bool WinMsCursorVisible()
{
  return ms.show && ms_hard.installed;
}

static void DrawMs(CDC *dc)
{
  auto flags = GetRFlags();
  asm volatile("cli" : : : "memory");
  auto x = ms.pos.x, y = ms.pos.y;
  auto color = ms.lb ? LTPURPLE : ms.rb ? LTCYAN : BLACK;
  SetRFlags(flags);
  if (!WinMsCursorVisible() || !(SYS_RUN_LEVEL & RLF_VGA) || !gr.fp_draw_ms)
    return;

  dc->color.c0.rop = ROPB_XOR;
  dc->color.c0.color = color ^ TRANSPARENT;
  auto addr = reinterpret_cast<uintptr_t>(dc);
  if (winmgr.grab_scroll && gr.fp_draw_grab_ms)
    CallHolyC4(gr.fp_draw_grab_ms, addr, x, y, winmgr.grab_scroll_closed);
  else
    CallHolyC3(gr.fp_draw_ms, addr, x, y);
}

extern "C" void WinFinalUpdateCpp(CDC *dc)
{
  DrawMs(dc);
}

static bool ms_input_updated;

void WinMsInput()
{
  if (!ms_hard.installed)
    return;
  ms.has_wheel = ms_hard.has_wheel;
  if (ms_hard.evt) {
    MsUpdate(ms_hard.pos.x, ms_hard.pos.y, ms_hard.pos.z, ms_hard.bttns[0],
             ms_hard.bttns[1]);
    ms_hard.evt = false;
    ms.speed = ms_hard.speed;
    ms.timestamp = ms_hard.timestamp;
    ms_input_updated = true;
  }
}

void WinMsUpdate()
{
  if (!ms_input_updated)
    ms.speed = F64MulBits(ms.speed, std::bit_cast<uint64_t>(0.95));
  ms_input_updated = false;
  if (gr.scrn_zoom != 1) {
    auto x = (uint64_t(ms.pos.x) - gr.sx) * gr.scrn_zoom;
    auto y = (uint64_t(ms.pos.y) - gr.sy) * gr.scrn_zoom;
    if (gr.continuous_scroll || x < 8 || x >= GR_WIDTH - 8 || y < 8 ||
        y >= GR_HEIGHT - 8)
      GrScaleZoomCpp(std::bit_cast<uint64_t>(1.0));
  }
}
