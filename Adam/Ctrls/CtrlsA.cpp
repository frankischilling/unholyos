#include "CtrlsA.hpp"
#include "../Gr/GrExt.hpp"
#include "../Gr/GrGlbls.hpp"

constexpr int64_t FONT_WIDTH = 8, FONT_HEIGHT = 8;

extern "C" void WinDerivedScrollValsUpdateHC(), DrawWinScrollHC();
extern "C" void LeftClickHWinScrollHC(), LeftClickVWinScrollHC();
extern "C" void WheelChangeWinScrollHC();
extern "C" void DrawViewAnglesCtrlHC(), UpdateDerivedViewAnglesCtrlHC();
extern "C" void LeftClickViewAnglesHC();

extern "C" CCtrl *CtrlFindUniqueCpp(CTask *task, int64_t type)
{
  auto *head = CtrlHead(task);
  for (auto *c = task->next_ctrl; c != head; c = c->next)
    if (c->type == type)
      return c;
  return nullptr;
}

void CtrlsUpdate(CTask *task)
{
  auto *head = CtrlHead(task);
  for (auto *c = task->next_ctrl; c != head; c = c->next) {
    if (c->update_derived_vals)
      CallHolyC1Checked(c->update_derived_vals, reinterpret_cast<uintptr_t>(c));
    const bool border = c->flags & CTRLF_BORDER;
    const auto x =
        gr.pan_text_x + uint64_t(task->pix_left) - (border ? FONT_WIDTH : 0);
    const auto y =
        gr.pan_text_y + uint64_t(task->pix_top) - (border ? FONT_HEIGHT : 0);
    c->scrn_left = x + c->left;
    c->scrn_right = x + c->right;
    c->scrn_top = y + c->top;
    c->scrn_bottom = y + c->bottom;
  }
}

static bool CtrlInsideRect(const CCtrl *c, int64_t x, int64_t y)
{
  return c->scrn_left <= x && x <= c->scrn_right && c->scrn_top <= y &&
         y <= c->scrn_bottom;
}

extern "C" uint64_t CtrlInsideCpp(CCtrl *c, int64_t x, int64_t y)
{
  if (!(c->flags & CTRLF_SHOW))
    return false;
  if (!c->inside_ctrl)
    return CtrlInsideRect(c, x, y);
  uint64_t inside;
  return CallHolyC3Checked(c->inside_ctrl, reinterpret_cast<uintptr_t>(c), x, y,
                           &inside) &&
         uint8_t(inside);
}

bool DrawCtrls(CTask *task)
{
  auto *head = CtrlHead(task);
  if (task->next_ctrl == head)
    return true;
  auto *dc = DCAliasCpp(gr.dc2, task);
  if (!dc)
    return true;
  auto expand = [task](uint64_t border) {
    auto flags = GetRFlags();
    asm volatile("cli" : : : "memory");
    task->win_left -= border;
    task->win_right += border;
    task->win_top -= border;
    task->win_bottom += border;
    WinDerivedValsUpdate(task);
    SetRFlags(flags);
  };
  bool drawn = true;
  for (auto *c = task->next_ctrl; c != head; c = c->next) {
    if (!(c->flags & CTRLF_SHOW) || !c->draw_it)
      continue;
    bool border = c->flags & CTRLF_BORDER;
    if (border && (task->display_flags & DISPLAYF_NO_BORDER))
      continue;
    if (border)
      expand(1);
    uint64_t result;
    drawn = CallHolyC2Checked(c->draw_it, reinterpret_cast<uintptr_t>(dc),
                              reinterpret_cast<uintptr_t>(c), &result);
    if (border)
      expand(uint64_t(-1));
    if (!drawn)
      break;
  }
  DCDelCpp(dc);
  return drawn;
}

constexpr int64_t WIN_SCROLL_SIZE = 8, WIN_SCROLL_BORDER_BONUS = 4;

extern "C" void DrawWinScrollCpp(CDC *dc, CCtrl *c)
{
  auto *s = static_cast<CWinScroll *>(c->state);
  uint8_t outer = s->color & 0xF, inner = s->color >> 4;
  if (c->flags & CTRLF_CLICKED) {
    auto color = outer;
    outer = inner;
    inner = color;
  }
  auto width = c->right - c->left + 1, height = c->bottom - c->top + 1;
  dc->color = {{outer, 0}, {0, 0}};
  GrRectCpp(dc, c->left, c->top, width, height);
  dc->color = {{inner, 0}, {0, 0}};
  GrRectCpp(dc, c->left + 2, c->top + 2, width - 4, height - 4);
}

extern "C" void WinDerivedScrollValsUpdateCpp(CCtrl *c)
{
  auto *s = static_cast<CWinScroll *>(c->state);
  auto *task = c->win_task;
  if (s->max < s->min)
    s->max = s->min;
  if (s->pos < s->min)
    s->pos = s->min;
  if (s->pos > s->max)
    s->pos = s->max;
  auto fg = task->border_attr & 0xF;
  s->color = (fg ^ 0xF) | (fg << 4);
  int64_t range = uint64_t(s->max) - s->min;
  if (!range)
    range = 1;
  switch (c->type) {
  case CTRLT_WIN_HSCROLL:
    c->left = gr.pan_text_x + FONT_WIDTH - WIN_SCROLL_BORDER_BONUS +
              int64_t((uint64_t(s->pos) - s->min) *
                      (uint64_t(task->pix_width) + 2 * WIN_SCROLL_BORDER_BONUS -
                       WIN_SCROLL_SIZE)) /
                  range;
    c->top = gr.pan_text_y + FONT_HEIGHT + (FONT_WIDTH - WIN_SCROLL_SIZE) / 2 +
             uint64_t(task->pix_height);
    break;
  case CTRLT_WIN_VSCROLL:
    c->left = gr.pan_text_x + FONT_WIDTH + (FONT_WIDTH - WIN_SCROLL_SIZE) / 2 +
              uint64_t(task->pix_width);
    c->top = gr.pan_text_y + FONT_HEIGHT - WIN_SCROLL_BORDER_BONUS +
             int64_t((uint64_t(s->pos) - s->min) *
                     (uint64_t(task->pix_height) + 2 * WIN_SCROLL_BORDER_BONUS -
                      WIN_SCROLL_SIZE)) /
                 range;
    break;
  default:
    return;
  }
  c->right = uint64_t(c->left) + WIN_SCROLL_SIZE - 1;
  c->bottom = uint64_t(c->top) + WIN_SCROLL_SIZE - 1;
}

static void LeftClickWinScroll(CCtrl *c, int64_t pos, int64_t range, bool down)
{
  auto *s = static_cast<CWinScroll *>(c->state);
  s->flags |= WSSF_SET_TO_POS;
  if (range > 0)
    s->pos = uint64_t(int64_t((uint64_t(pos) -
                               (FONT_WIDTH - WIN_SCROLL_BORDER_BONUS)) *
                                  (uint64_t(s->max) - s->min + 1) +
                              range / 2) /
                      range) +
             s->min;
  c->flags = down ? c->flags | CTRLF_CLICKED : c->flags & ~CTRLF_CLICKED;
  if (c->update_derived_vals)
    CallHolyC1Checked(c->update_derived_vals, reinterpret_cast<uintptr_t>(c));
}

extern "C" void LeftClickHWinScrollCpp(CCtrl *c, int64_t x, int64_t, bool down)
{
  LeftClickWinScroll(c, x,
                     c->win_task->pix_width + 2 * WIN_SCROLL_BORDER_BONUS -
                         WIN_SCROLL_SIZE,
                     down);
}

extern "C" void LeftClickVWinScrollCpp(CCtrl *c, int64_t, int64_t y, bool down)
{
  LeftClickWinScroll(c, y,
                     c->win_task->pix_height + 2 * WIN_SCROLL_BORDER_BONUS -
                         WIN_SCROLL_SIZE,
                     down);
}

extern "C" void WheelChangeWinScrollCpp(CCtrl *c, int64_t delta)
{
  auto *s = static_cast<CWinScroll *>(c->state);
  s->flags |= WSSF_SET_TO_POS;
  s->pos = uint64_t(s->pos) + delta;
  if (c->update_derived_vals)
    CallHolyC1Checked(c->update_derived_vals, reinterpret_cast<uintptr_t>(c));
}

extern "C" void WinScrollsInitCpp(CTask *task)
{
  constexpr int64_t types[]{CTRLT_WIN_HSCROLL, CTRLT_WIN_VSCROLL};
  for (auto type : types) {
    if (CtrlFindUniqueCpp(task, type))
      continue;
    auto *c = reinterpret_cast<CCtrl *>(CAlloc(sizeof(CCtrl), task->data_heap));
    if (!c)
      break;
    bool vert = type == CTRLT_WIN_VSCROLL;
    c->win_task = task;
    c->flags = CTRLF_SHOW | CTRLF_BORDER | CTRLF_CAPTURE_LEFT_MS;
    c->type = type;
    c->state = vert ? &task->vert_scroll : &task->horz_scroll;
    c->update_derived_vals =
        reinterpret_cast<uintptr_t>(WinDerivedScrollValsUpdateHC);
    c->draw_it = reinterpret_cast<uintptr_t>(DrawWinScrollHC);
    c->left_click = reinterpret_cast<uintptr_t>(vert ? LeftClickVWinScrollHC
                                                     : LeftClickHWinScrollHC);
    if (vert)
      c->wheel_chg = reinterpret_cast<uintptr_t>(WheelChangeWinScrollHC);
    QueIns(*c, *task->last_ctrl);
  }
  CtrlsUpdate(task);
}

constexpr int64_t VIEWANGLES_SPACING = 22, VIEWANGLES_RANGE = 48,
                  VIEWANGLES_BORDER = 2, VIEWANGLES_SNAP = 2;

extern "C" void DrawViewAnglesCtrlCpp(CDC *dc, CCtrl *c)
{
  auto *s = static_cast<CViewAngles *>(c->state);
  const int64_t width = VIEWANGLES_SPACING * 4 + 3;
  const int64_t height = VIEWANGLES_SPACING * 2 + VIEWANGLES_RANGE;
  dc->color = s->cbd;
  GrRectCpp(dc, c->left, c->top, width, height);
  dc->color = s->cbg;
  GrRectCpp(dc, c->left + VIEWANGLES_BORDER, c->top + VIEWANGLES_BORDER,
            width - 2 * VIEWANGLES_BORDER, height - 2 * VIEWANGLES_BORDER);
  dc->color = s->cfg;
  const int64_t axis_top = c->top + VIEWANGLES_SPACING;
  const int64_t axis_bottom = axis_top + VIEWANGLES_RANGE - 1;
  for (int64_t axis = 0; axis < 3; ++axis) {
    int64_t x = c->left + VIEWANGLES_SPACING + axis * (VIEWANGLES_SPACING + 1);
    GrLineCpp(dc, x, axis_top, x, axis_bottom, 1, 0);
  }
  for (int64_t i = 1; i <= VIEWANGLES_RANGE; i += 2 * VIEWANGLES_SNAP) {
    int64_t j = 2 - (i / 3 & 1), y = c->bottom - VIEWANGLES_SPACING - i;
    for (int64_t axis = 0; axis < 3; ++axis) {
      int64_t x =
          c->left + VIEWANGLES_SPACING + axis * (VIEWANGLES_SPACING + 1);
      GrLineCpp(dc, x - j, y, x + j, y, 1, 0);
    }
  }

  const CColorROPU32 colors[]{s->cx, s->cy, s->cz};
  const int64_t positions[]{s->sx, s->sy, s->sz};
  const char *labels[]{"X", "Y", "Z"};
  for (int64_t axis = 0; axis < 3; ++axis) {
    int64_t x = c->left + VIEWANGLES_SPACING + axis * (VIEWANGLES_SPACING + 1);
    dc->color = colors[axis];
    GrPrintCpp(dc, x - FONT_WIDTH / 2, axis_top - (1 + FONT_HEIGHT),
               labels[axis], 0, nullptr);
    int64_t pos = positions[axis];
    uint64_t degrees = pos * 360 / VIEWANGLES_RANGE;
    GrPrintCpp(dc, x - 3 * FONT_WIDTH / 2, axis_bottom + 4, "%3d", 1, &degrees);
    int64_t y = pos > VIEWANGLES_RANGE / 2 ? pos - VIEWANGLES_RANGE / 2
                                           : pos + VIEWANGLES_RANGE / 2;
    y = axis_bottom - y;
    GrRectCpp(dc, x - 3, y - 2, 7, 5);
    dc->color.c0.color ^= 8;
    GrRectCpp(dc, x - 2, y - 1, 5, 3);
  }
}

extern "C" void UpdateDerivedViewAnglesCtrlCpp(CCtrl *c)
{
  auto *s = static_cast<CViewAngles *>(c->state);
  c->left = c->win_task->pix_width - (VIEWANGLES_SPACING * 4 + 3);
  c->right = c->left + VIEWANGLES_SPACING * 4 + 3;
  c->top =
      c->win_task->pix_height - (VIEWANGLES_SPACING * 2 + VIEWANGLES_RANGE);
  c->bottom = c->top + VIEWANGLES_SPACING * 2 + VIEWANGLES_RANGE;
  auto snap = [](int64_t pos) {
    pos -= pos % VIEWANGLES_SNAP;
    return pos < 0 ? 0 : pos >= VIEWANGLES_RANGE ? VIEWANGLES_RANGE - 1 : pos;
  };
  s->sx = snap(s->sx);
  s->sy = snap(s->sy);
  s->sz = snap(s->sz);
  auto angle = [](int64_t pos) {
    constexpr uint64_t two_pi = 0x401921FB54442D18;
    return F64DivBits(F64MulBits(F64FromI64(pos), two_pi),
                      F64FromI64(VIEWANGLES_RANGE));
  };
  s->ax = angle(s->sx);
  s->ay = angle(s->sy);
  s->az = angle(s->sz);
}

extern "C" void LeftClickViewAnglesCpp(CCtrl *c, int64_t x, int64_t y, bool)
{
  auto *s = static_cast<CViewAngles *>(c->state);
  int64_t i = VIEWANGLES_RANGE - 1 - (y - (c->top + VIEWANGLES_SPACING));
  if (i >= VIEWANGLES_RANGE / 2)
    i -= VIEWANGLES_RANGE / 2;
  else
    i += VIEWANGLES_RANGE / 2;
  if (x < c->left + (c->right - c->left) / 3)
    s->sx = i;
  else if (x < c->left + 2 * (c->right - c->left) / 3)
    s->sy = i;
  else
    s->sz = i;
  if (c->update_derived_vals)
    CallHolyC1Checked(c->update_derived_vals, reinterpret_cast<uintptr_t>(c));
}

extern "C" CCtrl *ViewAnglesNewCpp(CTask *task)
{
  if (!task)
    task = CurrentTask();
  if (auto *c = CtrlFindUniqueCpp(task, CTRLT_VIEWING_ANGLES))
    return c;
  auto *s = reinterpret_cast<CViewAngles *>(
      CAlloc(sizeof(CViewAngles), task->data_heap));
  if (!s)
    return nullptr;
  auto *c = reinterpret_cast<CCtrl *>(
      CAlloc(sizeof(CCtrl), CurrentTask()->data_heap));
  if (!c) {
    Free(s);
    return nullptr;
  }
  s->cbd.c0.color = BLUE;
  s->cbg.c0.color = LTBLUE;
  s->cfg.c0.color = BLACK;
  s->cx.c0.color = LTGREEN;
  s->cy.c0.color = GREEN;
  s->cz.c0.color = LTGREEN;
  c->win_task = task;
  c->flags = CTRLF_SHOW | CTRLF_CAPTURE_LEFT_MS;
  c->type = CTRLT_VIEWING_ANGLES;
  c->state = s;
  c->draw_it = reinterpret_cast<uintptr_t>(DrawViewAnglesCtrlHC);
  c->left_click = reinterpret_cast<uintptr_t>(LeftClickViewAnglesHC);
  c->update_derived_vals =
      reinterpret_cast<uintptr_t>(UpdateDerivedViewAnglesCtrlHC);
  auto flags = GetRFlags();
  asm volatile("cli" : : : "memory");
  QueIns(*c, *task->last_ctrl);
  WinDerivedValsUpdate(task);
  CtrlsUpdate(task);
  if (task->display_flags & DISPLAYF_SHOW)
    __atomic_fetch_or(&sys_semas[SEMA_UPDATE_WIN_Z_BUF].flags, 1,
                      __ATOMIC_RELAXED);
  SetRFlags(flags);
  return c;
}

extern "C" void ViewAnglesDelCpp(CTask *task)
{
  if (!task)
    task = CurrentTask();
  auto flags = GetRFlags();
  asm volatile("cli" : : : "memory");
  auto *c = CtrlFindUniqueCpp(task, CTRLT_VIEWING_ANGLES);
  if (c)
    QueRem(*c);
  SetRFlags(flags);
  if (c) {
    Free(c->state);
    Free(c);
  }
}
