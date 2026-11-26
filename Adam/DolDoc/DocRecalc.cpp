#include "DolDoc.hpp"
#include "../Gr/Gr.hpp"
#include "../Gr/GrExt.hpp"
#include "../Gr/GrGlbls.hpp"

extern "C" void DCRstCpp(CDC *dc);

static bool SameText(const CDocEntryBase &a, const CDocEntryBase &b)
{
  return a.Type() == DOCT_TEXT && b.Type() == DOCT_TEXT && a.tag && b.tag &&
         (a.type & ~0xFF00u) == (b.type & ~0xFF00u) &&
         !(a.de_flags & ~DOCG_PLAIN_ENTRY) && a.de_flags == b.de_flags;
}

static bool JoinText(CDoc &doc, CDocEntryBase &entry, CDocEntryBase &next)
{
  size_t first = StrLen(entry.tag), second = StrLen(next.tag);
  auto *both = reinterpret_cast<char *>(
      MAlloc(first + second + 1, MemTaskHeap(doc.mem_task)));
  if (!both)
    return false;
  MemCpy(both, entry.tag, first);
  MemCpy(both + first, next.tag, second + 1);
  if (doc.cur_entry == &next) {
    doc.cur_entry = &entry;
    doc.cur_col += first;
  }
  Free(entry.tag);
  entry.tag = both;
  DocEntryDel(doc, next);
  return true;
}

static void DocSetColor(CDocSettings &settings, int64_t attr, uint8_t type)
{
  bool background = type == DOCT_BACKGROUND || type == DOCT_DFT_BACKGROUND;
  unsigned shift = background ? 4 : 0;
  unsigned mask = 0xF << shift;
  unsigned bits = attr == DOC_DFT ? settings.dft_text_attr & mask
                                  : (unsigned(attr) & 15) << shift;
  settings.cur_text_attr = (settings.cur_text_attr & ~mask) | bits;
  if (type == DOCT_DFT_FOREGROUND || type == DOCT_DFT_BACKGROUND)
    settings.dft_text_attr = (settings.dft_text_attr & ~mask) | bits;
}

static uint8_t DocColor(const CDocEntryBase &entry, uint8_t text)
{
  uint8_t ink = text & 15;
  if ((entry.de_flags & DOCEF_HAS_BIN) && entry.tag && *entry.tag == '<')
    ink = 10;
  else {
    switch (entry.Type()) {
    case DOCT_LINK:
      ink = 4;
      break;
    case DOCT_MACRO:
      ink = 9;
      break;
    case DOCT_ANCHOR:
      ink = 8;
      break;
    case DOCT_TREE:
    case DOCT_LST:
      ink = 5;
      break;
    case DOCT_HTML_CODE:
      ink = 10;
      break;
    case DOCT_SPRITE:
      if (entry.de_flags & DOCEF_LEFT_EXP)
        break;
      if (entry.de_flags & DOCEF_LINK)
        ink = 4;
      else if (entry.de_flags & DOCEF_LEFT_MACRO)
        ink = 9;
      else if (entry.de_flags & (DOCEF_TREE | DOCEF_LST))
        ink = 5;
      else
        ink = 10;
      break;
    default:
      break;
    }
  }
  return (text & 0xF0) | ink;
}

static void DocScreenCursor(CDoc &doc, const CTask &task)
{
  auto x0 = doc.line_start_col, y0 = doc.top_line_num;
  if (int64_t(x0) + task.win_width - 1 > doc.max_x)
    x0 = doc.max_x - task.win_width + 1;
  if (x0 < doc.min_x)
    x0 = doc.min_x;
  if (doc.x - int64_t(x0) >= task.win_width)
    x0 = doc.x - task.win_width + 1;
  if (doc.x < x0)
    x0 = doc.x;
  if (int64_t(y0) + task.win_height - 1 > doc.max_y)
    y0 = doc.max_y - task.win_height + 1;
  if (y0 < doc.min_y)
    y0 = doc.min_y;
  if (doc.y - int64_t(y0) >= task.win_height)
    y0 = doc.y - task.win_height + 1;
  if (doc.y < y0)
    y0 = doc.y;
  doc.line_start_col = x0;
  doc.top_line_num = y0;

  auto x = doc.x - int64_t(x0) + task.win_left + task.scroll_x / 8;
  auto y = doc.y - int64_t(y0) + task.win_top + task.scroll_y / 8;
  if (!(doc.flags & DOCF_HIDE_CURSOR) && x >= 0 && x < TEXT_COLS && y >= 0 &&
      y < TEXT_ROWS && x <= task.win_right && y <= task.win_bottom) {
    auto &cell = gr.text_base[y * TEXT_COLS + x];
    cell = (cell | ATTRF_BLINK) ^ 0xFF00;
  }
}

static void DocRecalcXY(const CDoc &doc, const CTask &task, uint64_t flags,
                        int64_t len, int64_t margin_right,
                        const CDocSettings &settings, int32_t &x, int32_t &y,
                        int32_t &page_line)
{
  const int64_t x0 = doc.line_start_col, y0 = doc.top_line_num;
  const auto width = task.win_width, height = task.win_height;
  const auto margin_left = settings.left_margin;
  if (flags & DOCEF_LEFT_X)
    x = flags & DOCEF_MARGIN_REL_X ? margin_left : x0;
  else if (flags & DOCEF_RIGHT_X)
    x = flags & DOCEF_MARGIN_REL_X ? margin_right - (len - 1)
                                   : width + x0 - len;
  else if (flags & DOCEF_CENTER_X)
    x = flags & DOCEF_MARGIN_REL_X
            ? ((margin_left + margin_right) >> 1) - (len >> 1)
            : (width + x0 - len) >> 1;

  auto old_y = y;
  if (flags & DOCEF_PAGE_REL_Y) {
    if (flags & DOCEF_TOP_Y)
      y -= page_line;
    else if (flags & DOCEF_BOTTOM_Y)
      y += settings.page_len - page_line;
    else if (flags & DOCEF_CENTER_Y)
      y += (settings.page_len >> 1) - page_line;
  } else if (flags & DOCEF_TOP_Y)
    y = y0;
  else if (flags & DOCEF_BOTTOM_Y)
    y = height - 1 + y0;
  else if (flags & DOCEF_CENTER_Y)
    y = (height >> 1) + y0;

  if (y != old_y && settings.page_len) {
    auto line = (int64_t(page_line) + y - old_y) % settings.page_len;
    page_line = line < 0 ? line + settings.page_len : line;
  }
}

static void DocRefreshTag(CDoc &doc, CDocEntryBase &entry)
{
  auto &full = reinterpret_cast<CDocEntry &>(entry);
  auto *heap = MemTaskHeap(doc.mem_task);
  Free(entry.tag);
  entry.tag = nullptr;
  if (entry.de_flags & DOCEF_TAG_CB) {
    uint64_t result;
    if (full.tag_cb &&
        CallHolyC3Checked(full.tag_cb, reinterpret_cast<uintptr_t>(&doc),
                          reinterpret_cast<uintptr_t>(&entry),
                          reinterpret_cast<uintptr_t>(doc.mem_task), &result))
      entry.tag = reinterpret_cast<char *>(result);
  } else if (full.define_str && doc.win_task) {
    if (auto *value =
            HashFind(full.define_str, doc.win_task->hash_table, HTT_DEFINE_STR))
      entry.tag = StrNew(reinterpret_cast<CHashDefineStr *>(value)->data, heap);
  }
  if (!entry.tag)
    entry.tag = StrNew("", heap);
  if (doc.cur_entry == &entry) {
    int64_t len = entry.tag ? StrLen(entry.tag) : 0;
    if (doc.cur_col >= len)
      doc.cur_col = len ? len - 1 : 0;
  }
}

bool DocRecalc(CDoc &doc, uint64_t recalc_flags)
{
  if (doc.doc_signature != DOC_SIGNATURE_VAL)
    return false;
  const auto mode = recalc_flags & RECALCG_MASK;
  const bool screen = mode == RECALCt_TO_SCRN;
  const bool find_cursor = mode == RECALCt_FIND_CURSOR;
  if (screen && (!gr.text_base || !doc.win_task))
    return false;
  // The refresh task retries a document held by another task next frame.
  if (screen && !DocCanLock(doc))
    return false;
  CDocLock lock(doc);
  auto *task = doc.win_task;
  const bool border_doc = doc.flags & DOCF_BORDER_DOC;
  if (screen) {
    ++doc.updates_cnt;
    if (border_doc)
      doc.line_start_col = doc.top_line_num = 0;
    doc.settings_head.cur_text_attr = doc.settings_head.dft_text_attr =
        border_doc ? task->border_attr : task->text_attr;
  }
  const int64_t x0 = doc.line_start_col, y0 = doc.top_line_num;
  CDC *sprite_dc = nullptr;
  uint32_t format = 0;
  auto &head = doc.head;
  auto *best_entry = &head;
  CDocEntryBase *clear_entry = nullptr;
  uint32_t best_col = 0;
  unsigned __int128 best_d = INT64_MAX;
  CDocSettings settings;
  MemCpy(&settings, &doc.settings_head, sizeof(settings));
  int32_t x = 0, y = 0, page_line = 0;
  int64_t num_entries = 0;
  int64_t right_margin = task ? task->win_width - 1 : 0;
  doc.min_x = doc.max_x = doc.min_y = doc.max_y = 0;
  for (auto *entry = head.next; entry != &head;) {
    if (entry->Type() == DOCT_CURSOR) {
      if (!(doc.flags & DOCF_NO_CURSOR)) {
        doc.cur_entry = entry->next;
        doc.cur_col = doc.cur_entry->min_col;
      }
      auto *next = entry->next;
      DocEntryDel(doc, *entry);
      entry = next;
      continue;
    }
    while (entry->next != &head && SameText(*entry, *entry->next))
      if (!JoinText(doc, *entry, *entry->next))
        break;
    ++num_entries;
    auto type = entry->Type();
    const auto *full = reinterpret_cast<const CDocEntry *>(entry);
    if ((entry->de_flags & (DOCEF_TAG_CB | DOCEF_DEFINE)) &&
        !(entry->de_flags & (DOCEF_LST | DOCEF_SKIP | DOCEF_FILTER_SKIP)))
      DocRefreshTag(doc, *entry);
    uint32_t len = entry->tag ? StrLen(entry->tag) : 0;
    constexpr auto position = DOCEF_LEFT_X | DOCEF_CENTER_X | DOCEF_RIGHT_X |
                              DOCEF_TOP_Y | DOCEF_CENTER_Y | DOCEF_BOTTOM_Y;
    if (task && (entry->de_flags & position))
      DocRecalcXY(doc, *task, entry->de_flags, len, right_margin,
                  settings, x, y, page_line);
    entry->x = x;
    entry->y = y;
    entry->page_line_num = page_line;
    uint32_t color = uint32_t(DocColor(*entry, settings.cur_text_attr)) << 8;
    entry->type = (entry->type & ~0xFF00u) | color;
    MemCpy(&entry->settings, &settings, sizeof(settings));
    entry->settings.final_u32_attr =
        (entry->type & 0xFFFFFF00u) | format |
        ((uint32_t(settings.shifted_x) & 31) << 16) |
        ((uint32_t(settings.shifted_y) & 31) << 21);
    entry->max_col = type == DOCT_TEXT ? len
                     : entry->tag      ? entry->min_col + 1
                                       : 0;
    const bool newline = type == DOCT_NEW_LINE || type == DOCT_SOFT_NEW_LINE;
    auto tab_width = ((x + 8) & ~7) - x;
    if (find_cursor && !(entry->de_flags & DOCEF_NO_CLICK_ON) &&
        ((entry->de_flags & DOCEF_TAG) || newline || type == DOCT_TAB)) {
      int64_t width = type == DOCT_TAB ? tab_width : len;
      int64_t col = int64_t(doc.x) - x;
      if (col < 0)
        col = 0;
      if (col >= width)
        col = width ? width - 1 : 0;
      __int128 dx = int64_t(doc.x) - x - col, dy = int64_t(doc.y) - y;
      unsigned __int128 distance = (dx * dx + dy * dy) * 64;
      if (distance <= best_d) {
        best_entry = entry;
        best_d = distance;
        best_col = type == DOCT_TEXT ? col : entry->min_col;
      }
    }
    if (screen && !(entry->de_flags & DOCEF_DONT_DRAW)) {
      const auto col = x - x0, row = y - y0;
      auto attr = entry->settings.final_u32_attr;
      bool border = (entry->de_flags & DOCEF_BORDER_PLOT) &&
                    !(task->display_flags & DISPLAYF_NO_BORDER);
      if (doc.cur_entry == entry && type != DOCT_TEXT &&
          !(doc.flags & DOCF_DONT_HIGHLIGHT_CURSOR))
        attr ^= 0xFF00;
      if (type == DOCT_SPRITE && full->bin_data && gr.dc2 &&
          !(entry->de_flags & DOCEF_DONT_DRAW) && (SYS_RUN_LEVEL & RLF_VGA)) {
        if (!sprite_dc)
          sprite_dc = DCAliasCpp(gr.dc2, task);
        if (sprite_dc) {
          DCRstCpp(sprite_dc);
          sprite_dc->color.c0.color =
              ((attr >> 12) & 15) ^ (task->text_attr >> 4);
          sprite_dc->bkcolor = sprite_dc->color;
          auto sprite_x =
              col - ((entry->de_flags & DOCEF_FROM_START) ? len : 0);
          Sprite3(*sprite_dc, sprite_x * 8, row * 8, full->bin_data->data,
                  full->bin_data->size);
        }
      }
      if (newline) {
        TextLenAttrCpp(task, col, row, task->win_width - col,
                       uint32_t(settings.cur_text_attr) << 8);
      } else if (type == DOCT_TAB) {
        auto count = tab_width;
        if (border) {
          for (int64_t i = 0; i < count; ++i)
            TextCharCpp(task, true, col + i, row, attr | ' ');
        } else
          TextLenStrCpp(task, col, row, count, attr, "        ");
      } else if (len && (entry->de_flags & DOCEF_TAG)) {
        if (border) {
          for (uint32_t i = 0; i < len; ++i)
            TextCharCpp(task, true, col + i, row,
                        attr | static_cast<uint8_t>(entry->tag[i]));
        } else
          TextLenStrCpp(task, col, row, len, attr, entry->tag);
      }
      if (len && (entry->de_flags & DOCEF_HAS_BORDER))
        TextBorderCpp(task, col, col + len - 1, row, row,
                      static_cast<uint8_t>(attr >> 8),
                      (entry->de_flags & DOCEF_SOLID_BORDER) != 0);
    }
    if (newline) {
      x = settings.left_margin + settings.indent;
      y++;
      if (++page_line >= settings.page_len)
        page_line = 0;
    } else if (type == DOCT_PAGE_BREAK) {
      x = settings.left_margin + settings.indent;
      y += settings.page_len - page_line;
      page_line = 0;
    } else if (type == DOCT_TAB) {
      x = (x + 8) & ~7;
    } else if (type == DOCT_PMT) {
      settings.cur_text_attr = (settings.cur_text_attr & 0xF0) | GREEN;
    } else if (type == DOCT_CLEAR &&
               !(entry->de_flags & (DOCEF_SKIP | DOCEF_FILTER_SKIP))) {
      clear_entry = entry;
    } else if (type == DOCT_PAGE_LEN && full->attr > 0 &&
               full->attr <= UINT16_MAX) {
      settings.page_len = full->attr;
    } else if (type == DOCT_LEFT_MARGIN) {
      settings.left_margin = full->attr;
      x = settings.left_margin + settings.indent;
    } else if (type == DOCT_RIGHT_MARGIN) {
      right_margin = full->attr;
      settings.right_margin = right_margin;
    } else if (type == DOCT_INDENT) {
      settings.indent =
          full->attr + ((entry->de_flags & DOCEF_LEFT_X) ? 0 : settings.indent);
      x = settings.left_margin + settings.indent;
    } else if (type >= DOCT_FOREGROUND && type <= DOCT_DFT_BACKGROUND) {
      DocSetColor(settings, full->attr, type);
    } else if (type >= DOCT_BLINK && type <= DOCT_UNDERLINE) {
      constexpr uint32_t masks[]{ATTRF_BLINK, ATTRF_INVERT, ATTRF_UNDERLINE};
      auto mask = masks[type - DOCT_BLINK];
      format = full->attr ? format | mask : format & ~mask;
    } else if (type == DOCT_CURSOR_MOVEMENT) {
      x += full->attr;
      y += full->right_exp;
    } else if (type == DOCT_SHIFTED_X) {
      settings.shifted_x = full->attr;
    } else if (type == DOCT_SHIFTED_Y) {
      settings.shifted_y = full->attr;
    } else {
      x += len;
    }
    if (x > doc.max_x)
      doc.max_x = x;
    if (x < doc.min_x)
      doc.min_x = x;
    if (y > doc.max_y)
      doc.max_y = y;
    if (y < doc.min_y)
      doc.min_y = y;
    entry = entry->next;
  }
  if (sprite_dc)
    DCDelCpp(sprite_dc);
  if (clear_entry) {
    DocDelToEntry(doc, clear_entry, (clear_entry->de_flags & DOCEF_HOLD) != 0);
    return DocRecalc(doc, recalc_flags);
  }
  if (screen && num_entries - doc.max_entries > 1024 &&
      DocDelToNum(doc, num_entries - doc.max_entries))
    return DocRecalc(doc, recalc_flags);
  head.x = x;
  head.y = y;
  head.page_line_num = page_line;
  MemCpy(&head.settings, &settings, sizeof(settings));
  if (find_cursor) {
    doc.cur_entry = best_entry;
    doc.cur_col = best_col;
    doc.best_d = best_d;
  }
  doc.x = doc.cur_entry->x + doc.cur_col;
  doc.y = doc.cur_entry->y;
  if (screen) {
    doc.line = int64_t(doc.y) + 1;
    doc.col = int64_t(doc.x) + 1;
    if (!border_doc && (recalc_flags & RECALCF_HAS_CURSOR))
      DocScreenCursor(doc, *task);
  }
  return true;
}

extern "C" bool DocRecalcCpp(CDoc *doc, uint64_t recalc_flags)
{
  return doc && DocRecalc(*doc, recalc_flags);
}
