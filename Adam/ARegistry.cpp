#include "DolDoc/DolDoc.hpp"
#include "../Compiler/CompilerA.hpp"

constexpr char REGISTRY_FILENAME[] = "~/Registry.HC.Z";
CDoc *sys_registry_doc;
int64_t sys_msg_flags[1];
uint64_t registry_version;

static CDoc *RegCache()
{
  auto *doc = __atomic_load_n(&sys_registry_doc, __ATOMIC_ACQUIRE);
  if (doc)
    return doc;

  auto &flags = CurrentTask()->display_flags;
  const auto silent = flags & DISPLAYF_SILENT;
  flags |= DISPLAYF_SILENT;
  auto *loaded = DocRead(REGISTRY_FILENAME, 0, nullptr, adam_task);
  flags = (flags & ~uint64_t(DISPLAYF_SILENT)) | silent;
  if (!loaded)
    return nullptr;
  if (__atomic_compare_exchange_n(&sys_registry_doc, &doc, loaded, false,
                                  __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
    return loaded;
  DocDelCpp(loaded);
  return doc;
}

extern "C" uint64_t RegDftCpp(const char *path, const char *val,
                               uint8_t is_adam_entry)
{
  auto *doc = RegCache();
  if (!doc || !val)
    return false;
  {
    CDocLock lock(*doc);
    if (DocTreeFindCpp(doc, path, nullptr, nullptr, nullptr))
      return true;
    if (!DocTreeMakeCpp(doc, path))
      return false;
    DocPutS(*doc, val, StrLen(val));
  }
  // A registry entry can call the registry again, including from Adam.
  if (is_adam_entry) {
    if (CurrentTask() == adam_task) {
      constexpr int64_t brk = 0x6B61657242;
      auto result = ExePutS(val, nullptr, 0);
      if (result.exception && result.exception != COMPILER_ERROR &&
          result.exception != brk)
        RethrowHolyC();
    } else
      AdamText(val);
  }
  CDocLock lock(*doc);
  if (DrvIsWritableCpp(doc->filename.name[0], false))
    DocWriteCpp(doc, false);
  return false;
}

extern "C" uint64_t RegExeCpp(const char *path)
{
  return DocTreeExeCpp(RegCache(), path);
}

extern "C" uint64_t RegWriteCpp(const char *path, const char *fmt,
                                 int64_t argc, const uint64_t *argv)
{
  return DocTreeWriteCpp(RegCache(), path, true, fmt, argc, argv);
}

extern "C" int64_t RegCntCpp(const char *path)
{
  auto *doc = RegCache();
  if (!doc)
    return 0;
  CDocLock lock(*doc);
  CDocEntryBase *start, *end;
  if (!DocTreeFindCpp(doc, path, nullptr, &start, &end))
    return 0;
  int64_t res = 0;
  end = end->next;
  while (start != end) {
    res++;
    start = start->next;
  }
  return res;
}

extern "C" uint64_t RegAppendCpp(const char *path, const char *fmt,
                                  int64_t argc, const uint64_t *argv)
{
  return DocTreeAppendCpp(RegCache(), path, true, fmt, argc, argv);
}

bool RegInit()
{
  auto *doc = RegCache();
  if (!doc)
    return false;
  char version[64];
  StrPrintJoin(version, "registry_version=%4.3f;\n", 1, &sys_os_version);
  RegDftCpp("Adam/SysMsgFlags", "sys_msg_flags[0]=0;\n", true);
  RegDftCpp("Adam/SysRegVer", version, true);
  auto result = DocTreeExe(doc, "Adam");
  if (result.okay && registry_version != sys_os_version) {
    uint64_t arg = reinterpret_cast<uintptr_t>(version);
    RegWriteCpp("Adam/SysRegVer", "%s", 1, &arg);
    result = DocTreeExe(doc, "Adam");
  }
  return result.okay && registry_version == sys_os_version;
}

extern "C" void AOnceFlushCpp()
{
  RegWriteCpp("Once/Adam", "", 0, nullptr);
}

extern "C" void OnceFlushCpp()
{
  RegWriteCpp("Once/User", "", 0, nullptr);
}

static void OnceAppend(const char *path, uint32_t level, uint8_t drv_let,
                        const char *fmt, int64_t argc, const uint64_t *argv)
{
  auto *doc = RegCache();
  if (!doc)
    return;
  auto *text = StrPrintJoin(nullptr, fmt, argc, argv);
  if (!text)
    return;
  {
    CDocLock lock(*doc);
    const char old_drv_let = doc->filename.name[0];
    if (drv_let)
      doc->filename.name[0] = drv_let;
    if (!(__atomic_load_n(&SYS_RUN_LEVEL, __ATOMIC_RELAXED) & level))
      DocTreeWriteCpp(doc, path, true, "", 0, nullptr);
    const uint64_t arg = reinterpret_cast<uintptr_t>(text);
    DocTreeAppendCpp(doc, path, true, "%s\n", 1, &arg);
    doc->filename.name[0] = old_drv_let;
  }
  Free(text);
}

extern "C" void AOnceCpp(const char *fmt, int64_t argc, const uint64_t *argv)
{
  OnceAppend("Once/Adam", RLF_ONCE_ADAM, 0, fmt, argc, argv);
}

extern "C" void OnceCpp(const char *fmt, int64_t argc, const uint64_t *argv)
{
  OnceAppend("Once/User", RLF_ONCE_USER, 0, fmt, argc, argv);
}

extern "C" void AOnceDrvCpp(uint8_t drv_let, const char *fmt,
                            int64_t argc, const uint64_t *argv)
{
  OnceAppend("Once/Adam", RLF_ONCE_ADAM, drv_let, fmt, argc, argv);
}

extern "C" void OnceDrvCpp(uint8_t drv_let, const char *fmt,
                           int64_t argc, const uint64_t *argv)
{
  OnceAppend("Once/User", RLF_ONCE_USER, drv_let, fmt, argc, argv);
}

extern "C" void OnceExeTryCpp()
{
  RegDftCpp("Once/Adam", "", false);
  if (RegCntCpp("Once/Adam") > 2) {
    AdamText("RegExe(\"Once/Adam\");");
    AOnceFlushCpp();
  }
  __atomic_fetch_or(&SYS_RUN_LEVEL, RLF_ONCE_ADAM, __ATOMIC_RELAXED);

  RegDftCpp("Once/User", "", false);
  if (RegCntCpp("Once/User") > 2) {
    RegExeCpp("Once/User");
    OnceFlushCpp();
  }
  __atomic_fetch_or(&SYS_RUN_LEVEL, RLF_ONCE_USER, __ATOMIC_RELAXED);
}

extern "C" void OnceExeTryHC();

extern "C" void OnceExeCpp()
{
  uint64_t result;
  if (!CallHolyC0Checked(reinterpret_cast<const uint8_t *>(OnceExeTryHC),
                         &result)) {
    AOnceFlushCpp();
    __atomic_fetch_or(&SYS_RUN_LEVEL, RLF_ONCE_ADAM, __ATOMIC_RELAXED);
    OnceFlushCpp();
    __atomic_fetch_or(&SYS_RUN_LEVEL, RLF_ONCE_USER, __ATOMIC_RELAXED);
    // The reference catch clears the queues and leaves the exception pending.
    RethrowHolyC();
  }
}
