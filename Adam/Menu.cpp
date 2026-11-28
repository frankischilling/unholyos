#include "Menu.hpp"
#include "../Compiler/CompilerA.hpp"
#include "Gr/GrExt.hpp"

extern "C" int64_t Char2ScanCodeCpp(int64_t ch, int64_t sc_flags);
extern "C" int64_t GrPrintCpp(CDC *dc, int64_t x, int64_t y, const char *fmt,
                              int64_t argc, const uint64_t *argv);
extern "C" [[noreturn]] void ThrowCpp(int64_t tag, uint8_t no_log,
                                      const uintptr_t *frame);

CTask *MenuTask()
{
  auto *task = sys_focus_task;
  while (TaskValidateCpp(task) && !task->cur_menu)
    task = task->parent_task;
  return TaskValidateCpp(task) ? task : nullptr;
}

CMenuEntry *sys_cur_submenu_entry;
static CMenu *drawn_menu;

extern "C" CMenuEntry *MenuSubEntryFindCpp(CMenuEntry *haystack_first,
                                           const char *needle_entry_name)
{
  auto *needle = reinterpret_cast<const uint8_t *>(needle_entry_name);
  for (auto *entry = haystack_first; entry; entry = entry->next)
    if (!StrCmpCpp(reinterpret_cast<const uint8_t *>(entry->name), needle))
      return entry;
  return nullptr;
}

extern "C" CMenuEntry *MenuEntryFindCpp(CMenu *haystack_menu,
                                        const char *needle_full_name)
{
  if (!haystack_menu || !needle_full_name)
    return nullptr;
  // The reference uses the menu header as the root of the entry tree.
  auto *entry = reinterpret_cast<CMenuEntry *>(haystack_menu);
  auto *path = needle_full_name;
  auto *sub = haystack_menu->sub;
  while (*path) {
    char name[sizeof(CMenuEntry::name)];
    size_t length = 0;
    while (path[length] && path[length] != '/')
      length++;
    if (length >= sizeof(name))
      return nullptr;
    MemCpy(name, path, length);
    name[length] = 0;
    entry = MenuSubEntryFindCpp(sub, name);
    if (!entry)
      return nullptr;
    sub = entry->sub;
    path += length;
    if (*path)
      path++;
  }
  return entry;
}

static bool MenuNewSub(CCmpCtrl &cc, CTask *task, CMenuEntry **link,
                       const char *&error)
{
  while (cc.token == TK_IDENT) {
    auto *entry = reinterpret_cast<CMenuEntry *>(
        CAlloc(sizeof(CMenuEntry), task->data_heap));
    if (!entry) {
      error = "Out of memory";
      return false;
    }
    *link = entry;
    link = &entry->next;
    auto length = StrLen(cc.cur_str);
    if (length >= sizeof(entry->name))
      length = sizeof(entry->name) - 1;
    MemCpy(entry->name, cc.cur_str, length);

    if (Lex(cc) == '(') {
      entry->msg_code = -MSG_KEY_DOWN;
      Lex(cc);
      int64_t *args[]{&entry->msg_code, &entry->arg1, &entry->arg2};
      for (size_t i = 0; i < 3 && cc.token != ')'; i++) {
        if (cc.token != ',') {
          auto *machine = LexTypedExpression2Bin(cc, RT_I64, true);
          uint64_t value;
          bool okay = machine && CmpCall(cc, machine, value);
          Free(machine);
          if (!okay) {
            error = "Invalid menu expression";
            return false;
          }
          *args[i] = value;
        }
        if (i == 2 || cc.token != ',')
          break;
        Lex(cc);
      }
      if (cc.token != ')') {
        error = "Missing ')' at ";
        return false;
      }
      if (Lex(cc) != ';') {
        error = "Missing ';' at";
        return false;
      }
      Lex(cc);
    } else if (cc.token == '{') {
      entry->dir = true;
      Lex(cc);
      if (!MenuNewSub(cc, task, &entry->sub, error))
        return false;
      if (cc.token != '}') {
        error = "Missing '}' at ";
        return false;
      }
      Lex(cc);
    } else {
      error = "Expecting '{' at ";
      return false;
    }
  }
  return true;
}

extern "C" void MenuDelCpp(CMenu *m);

static CMenu *MenuNew(const char *st, int64_t flags, CTask *task,
                      const char *&error, uint64_t &exception)
{
  auto *fs = CurrentTask();
  if (!task)
    task = fs;
  if (!CmpAsmHashLoad(fs->hash_table))
    return nullptr;
  auto *m = reinterpret_cast<CMenu *>(CAlloc(sizeof(CMenu), task->data_heap));
  if (!m)
    return nullptr;
  m->task = task;
  m->flags = flags;
  m->attr = BLUE << 4 | YELLOW;

  CCmpCtrl cc;
  CLexFile file;
  LexSourceInit(cc, file, st, 0, nullptr, fs->hash_table);
  QueIns(cc, *fs->last_cc);
  Lex(cc);
  bool okay = MenuNewSub(cc, task, &m->sub, error);
  if (okay && cc.token == TK_ERROR) {
    error = "Invalid menu source";
    okay = false;
  }
  exception = cc.jit_exception;
  QueRem(cc);
  LexSourceDel(cc);
  if (!okay) {
    MenuDelCpp(m);
    return nullptr;
  }
  return m;
}

static void MenuExcept(const char *error, uint64_t exception)
{
  if (exception)
    RethrowHolyC();
  if (error) {
    PutS(error);
    PutS("\n");
    ThrowCpp(COMPILER_ERROR, true, nullptr);
  }
}

extern "C" CMenu *MenuNewCpp(const char *st, int64_t flags, CTask *task)
{
  const char *error = nullptr;
  uint64_t exception = 0;
  auto *m = MenuNew(st, flags, task, error, exception);
  MenuExcept(error, exception);
  return m;
}

extern "C" CMenu *MenuFileCpp(const char *filename, int64_t flags, CTask *task)
{
  uint64_t arg = reinterpret_cast<uintptr_t>(filename);
  auto *st = MStrPrintCpp("#include \"%s\"", 1, &arg);
  if (!st)
    return nullptr;
  const char *error = nullptr;
  uint64_t exception = 0;
  auto *m = MenuNew(st, flags, task, error, exception);
  Free(st);
  MenuExcept(error, exception);
  return m;
}

static void MenuDelSub(CMenuEntry *entry)
{
  for (auto *sub = entry->sub; sub;) {
    auto *next = sub->next;
    MenuDelSub(sub);
    sub = next;
  }
  Free(entry);
}

extern "C" void MenuDelCpp(CMenu *m)
{
  if (!m)
    return;
  if (drawn_menu == m) {
    drawn_menu = nullptr;
    sys_cur_submenu_entry = nullptr;
  }
  for (auto *entry = m->sub; entry;) {
    auto *next = entry->next;
    MenuDelSub(entry);
    entry = next;
  }
  Free(m);
}

int64_t MenuEntryWidth(const CMenuEntry *entry)
{
  auto width = StrLen(entry->name);
  for (auto *sub = entry->sub; sub; sub = sub->next) {
    auto length = StrLen(sub->name);
    if (length > width)
      width = length;
  }
  return width + 1;
}

extern "C" CMenu *MenuPushCpp(const char *st)
{
  auto *m = MenuNewCpp(st, 0, nullptr);
  if (m) {
    auto *task = CurrentTask();
    m->next = task->cur_menu;
    task->cur_menu = m;
  }
  return m;
}

extern "C" CMenu *MenuFilePushCpp(const char *filename)
{
  auto *m = MenuFileCpp(filename, 0, nullptr);
  if (m) {
    auto *task = CurrentTask();
    m->next = task->cur_menu;
    task->cur_menu = m;
  }
  return m;
}

extern "C" void MenuPopCpp()
{
  auto *task = CurrentTask();
  auto *m = task->cur_menu;
  if (m) {
    task->cur_menu = m->next;
    MenuDelCpp(m);
  }
}

static void MenuPrint(CDC *dc, int64_t x, int64_t y, const char *s)
{
  uint64_t arg = reinterpret_cast<uintptr_t>(s);
  GrPrintCpp(dc, x, y, "%s", 1, &arg);
}

void DrawMenu(CDC *dc)
{
  auto *task = MenuTask();
  drawn_menu = task ? task->cur_menu : nullptr;
  sys_cur_submenu_entry = nullptr;
  auto *m = drawn_menu;
  if (!m)
    return;

  auto x1 = ms.pos.x, y1 = ms.pos.y;
  WinMenuPointer(x1, y1);
  auto color = [dc](uint8_t index) { dc->color = {{index, 0}, {0, 0}}; };
  color(m->attr >> 4);
  GrRectCpp(dc, 0, 0, GR_WIDTH, FONT_HEIGHT);
  int64_t x0 = 0;
  for (auto *entry = m->sub; entry; entry = entry->next) {
    auto w = MenuEntryWidth(entry) * FONT_WIDTH;
    color(m->attr & 15);
    if (x0 <= x1 && x1 < x0 + w) {
      if (0 <= y1 && y1 < FONT_HEIGHT) {
        GrRectCpp(dc, x0, 0, w, FONT_HEIGHT);
        auto color = [dc](uint8_t index) { dc->color = {{index, 0}, {0, 0}}; };
        color(m->attr >> 4);
      }
      MenuPrint(dc, x0, 0, entry->name);
      int64_t y0 = FONT_HEIGHT;
      for (auto *sub = entry->sub; sub; sub = sub->next) {
        auto attr = sub->checked ? m->attr ^ 0xFF : m->attr;
        bool selected = y0 <= y1 && y1 < y0 + FONT_HEIGHT;
        color(selected ? attr & 15 : attr >> 4);
        GrRectCpp(dc, x0, y0, w, FONT_HEIGHT);
        color(selected ? attr >> 4 : attr & 15);
        MenuPrint(dc, x0, y0, sub->name);
        if (selected) {
          sys_cur_submenu_entry = sub;
          if (sub->msg_code == MSG_KEY_DOWN || sub->msg_code == -MSG_KEY_DOWN) {
            if (!sub->arg2)
              sub->arg2 = Char2ScanCodeCpp(sub->arg1, 0);
            if (auto *st = ScanCode2KeyNameCpp(sub->arg2)) {
              auto width = (StrLen(st) + 1) * FONT_WIDTH;
              GrRectCpp(dc, x0 + w, y0 - FONT_HEIGHT, width, FONT_HEIGHT * 3);
              color(attr & 15);
              MenuPrint(dc, x0 + w, y0, st);
              Free(st);
            }
          }
        }
        y0 += FONT_HEIGHT;
      }
    } else
      MenuPrint(dc, x0, 0, entry->name);
    x0 += w;
  }
}

void MenuSend(CTask *focus, CMenu *menu)
{
  auto *entry = sys_cur_submenu_entry;
  if (entry && drawn_menu == menu)
    TaskMsgCpp(focus, nullptr, entry->msg_code, entry->arg1, entry->arg2, 0);
}
