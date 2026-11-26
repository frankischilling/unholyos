#include "CtrlsA.hpp"
#include "../Gr/GrExt.hpp"
#include "../Gr/GrGlbls.hpp"

constexpr int64_t FONT_WIDTH = 8, FONT_HEIGHT = 8;

extern "C" void WinDerivedScrollValsUpdateHC(), DrawWinScrollHC();
extern "C" void LeftClickHWinScrollHC(), LeftClickVWinScrollHC();
extern "C" void WheelChangeWinScrollHC();

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
