#include "DolDoc/DolDoc.hpp"

CInsReg ins_reg;

bool InsRegInit()
{
  return DefineLstLoadCpp("ST_HOST_OSES", "Other\0Native\0Linux\0Windows\0",
                          nullptr) &&
         DefineLstLoadCpp("ST_HOST_VMS",
                          "Other\0Native\0VMware\0QEMU\0VirtualBox\0", nullptr);
}

constexpr int64_t RR_UNREGISTER = -1, RR_REREGISTER = 1;

extern "C" uint64_t InsRegCpp(uint8_t drv_let, int64_t action)
{
  CInsReg r;
  if (action == RR_UNREGISTER)
    MemSet(&r, 0, sizeof(CInsReg));
  else {
    if (action != RR_REREGISTER && ins_reg.registered)
      return false;
    MemCpy(&r, &ins_reg, sizeof(CInsReg));
    r.registered = true;
    if (!PopUpFormCpp(reinterpret_cast<uint8_t *>(&r), "CInsReg", DOF_SIZE_MIN,
                      nullptr, nullptr))
      return false;
  }

  auto *doc = RegCache();
  if (!doc)
    return false;
  constexpr char path[] = "Adam/InsReg";
  const uint64_t initials = reinterpret_cast<uintptr_t>(r.initials);
  const uint64_t args[]{
      r.registered, uint64_t(r.host_os),     uint64_t(r.host_vm),    initials,
      initials,     uint64_t(r.machine_num), uint64_t(r.machine_num)};
  {
    CDocLock lock(*doc);
    const char old_drv_let = doc->filename.name[0];
    if (drv_let)
      doc->filename.name[0] = drv_let;
    DocTreeWriteCpp(
        doc, path, true,
        "#help_index \"Registry/Install Registration\"\n"
        "MemSet(&ins_reg,0,sizeof(CInsReg));\n"
        "ins_reg.registered=%d;\n"
        "ins_reg.host_os=%d;\n"
        "ins_reg.host_vm=%d;\n"
        "\n#define INS_REG_PERSONAL_INITIALS \"%s\"\n"
        "StrCpy(ins_reg.initials,\"%s\");\n"
        "HashPublic(\"INS_REG_PERSONAL_INITIALS\",HTT_DEFINE_STR);\n"
        "\n#define INS_REG_MACHINE_NUM %d\n"
        "ins_reg.machine_num=%d;\n"
        "HashPublic(\"INS_REG_MACHINE_NUM\",HTT_DEFINE_STR);\n"
        "\n\"InsReg:%%s:%%d\\n\","
        "INS_REG_PERSONAL_INITIALS,INS_REG_MACHINE_NUM;\n"
        "\n#help_index \"\"\n",
        7, args);
    doc->filename.name[0] = old_drv_let;
  }
  DocTreeExeCpp(doc, path);
  return true;
}

extern "C" uint64_t InsReregCpp(uint8_t drv_let)
{
  return InsRegCpp(drv_let, RR_REREGISTER);
}

extern "C" uint64_t InsUnregCpp(uint8_t drv_let)
{
  return InsRegCpp(drv_let, RR_UNREGISTER);
}
