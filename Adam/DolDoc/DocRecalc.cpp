#include "DolDoc.hpp"

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
      if (entry.de_flags & (1ull << 36))
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

void DocRecalc(CDoc &doc)
{
  auto &head = doc.head;
  CDocSettings settings;
  MemCpy(&settings, &doc.settings_head, sizeof(settings));
  int32_t x = 0, y = 0, page_line = 0;
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
    auto type = entry->Type();
    const auto *full = reinterpret_cast<const CDocEntry *>(entry);
    entry->x = x;
    entry->y = y;
    entry->page_line_num = page_line;
    uint32_t color = uint32_t(DocColor(*entry, settings.cur_text_attr)) << 8;
    entry->type = (entry->type & ~0xFF00u) | color;
    MemCpy(&entry->settings, &settings, sizeof(settings));
    entry->settings.final_u32_attr =
        (entry->type & 0xFFFFFF00u) |
        ((uint32_t(settings.shifted_x) & 31) << 16) |
        ((uint32_t(settings.shifted_y) & 31) << 21);
    uint32_t len = entry->tag ? StrLen(entry->tag) : 0;
    entry->max_col = type == DOCT_TEXT ? len
                     : entry->tag      ? entry->min_col + 1
                                       : 0;
    if (type == DOCT_NEW_LINE || type == DOCT_SOFT_NEW_LINE) {
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
    } else if (type == DOCT_PAGE_LEN && full->attr > 0 &&
               full->attr <= UINT16_MAX) {
      settings.page_len = full->attr;
    } else if (type == DOCT_LEFT_MARGIN) {
      settings.left_margin = full->attr;
      x = settings.left_margin + settings.indent;
    } else if (type == DOCT_RIGHT_MARGIN) {
      settings.right_margin = full->attr;
    } else if (type == DOCT_INDENT) {
      settings.indent =
          full->attr + ((entry->de_flags & (1ull << 20)) ? 0 : settings.indent);
      x = settings.left_margin + settings.indent;
    } else if (type >= DOCT_FOREGROUND && type <= DOCT_DFT_BACKGROUND) {
      DocSetColor(settings, full->attr, type);
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
  head.x = x;
  head.y = y;
  head.page_line_num = page_line;
  MemCpy(&head.settings, &settings, sizeof(settings));
  doc.x = doc.cur_entry->x + doc.cur_col;
  doc.y = doc.cur_entry->y;
}
