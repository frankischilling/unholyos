#include "../KernelB.hpp"

extern "C" void DirContextDelCpp(CDirContext *dirc, uint8_t restore)
{
  if (!dirc)
    return;
  if (restore) {
    auto &task = *CurrentTask();
    auto *dv = dirc->old_dv;
    if (dv && dv->bd && (dv->bd->flags & BDF_INIT_IN_PROGRESS)) {
      if (auto *root = StrNew("/", task.data_heap)) {
        Free(dirc->old_dir);
        dirc->old_dir = root;
      }
    }
    Free(task.cur_dir);
    task.cur_dir = dirc->old_dir;
    task.cur_dv = dv;
    dirc->old_dir = nullptr;
  }
  Free(dirc->old_dir);
  Free(dirc->mask);
  Free(dirc);
}

extern "C" CDirContext *DirContextNewCpp(const char *name, uint8_t make_mask,
                                         uint8_t make_dirs, uint8_t no_mask)
{
  auto &task = *CurrentTask();
  auto *heap = task.data_heap;
  auto *text = PathTrim(name, heap);
  auto *dirc =
      reinterpret_cast<CDirContext *>(CAlloc(sizeof(CDirContext), heap));
  if (!text || !dirc) {
    Free(text);
    Free(dirc);
    return nullptr;
  }
  dirc->old_dv = task.cur_dv;
  if (task.cur_dir && !(dirc->old_dir = StrNew(task.cur_dir, heap))) {
    Free(text);
    DirContextDelCpp(dirc, false);
    return nullptr;
  }
  bool valid = true;
  auto *buf = text;
  auto select = [&](uint8_t letter) {
    auto *drive = Let2DrvCpp(letter, false);
    if (!drive || drive->fs_type != FSt_REDSEA)
      return false;
    if (drive == task.cur_dv)
      return true;
    char path[]{static_cast<char>(drive->drv_let), ':', '/', 0};
    return bool(CdCpp(path, false));
  };
  if (*buf && buf[1] == ':') {
    valid = select(*buf);
    buf += 2;
  }
  if (valid && *buf == '~')
    valid = select('~');
  dirc->dv = task.cur_dv;
  valid = valid && dirc->dv && dirc->dv->fs_type == FSt_REDSEA;
  if (valid) {
    if (no_mask) {
      dirc->mask = StrNew("", heap);
    } else {
      char *split = nullptr;
      for (auto *p = buf; *p && *p != ';'; p++)
        if (*p == '/')
          split = p;
      dirc->mask = StrNew(split ? split + 1 : buf, heap);
      if (split == buf)
        split++;
      *(split ? split : buf) = 0;
    }
    valid = dirc->mask && (!*buf || CdCpp(buf, make_dirs));
  }
  if (valid && make_mask &&
      (!*dirc->mask || ((!make_dirs || FileNameChk(dirc->mask)) &&
                        CdCpp(dirc->mask, make_dirs)))) {
    Free(dirc->mask);
    dirc->mask = StrNew("*", heap);
    valid = dirc->mask != nullptr;
  }
  Free(text);
  if (!valid) {
    DirContextDelCpp(dirc, true);
    return nullptr;
  }
  return dirc;
}
