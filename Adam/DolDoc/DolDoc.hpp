#pragma once

#include "../../Kernel/KernelB.hpp"

constexpr uint64_t DOCG_PLAIN = DOCF_PLAIN_TEXT | DOCF_PLAIN_TEXT_TABS;
constexpr uint64_t DOCG_PLAIN_ENTRY =
    DOCEF_TAG | DOCG_BL_IV_UL | DOCEF_HIGHLIGHT | DOCEF_WORD_WRAP |
    DOCEF_SKIP | DOCEF_FILTER_SKIP;

struct CDocReadContext
{
  const char *name;
  const CDocReadContext *parent;
};

class CDocLock
{
public:
  explicit CDocLock(CDoc &doc);
  ~CDocLock();
  CDocLock(const CDocLock &) = delete;
  CDocLock &operator=(const CDocLock &) = delete;

private:
  CDoc *locked;
};

bool DocCanLock(const CDoc &doc);
bool DocLock(CDoc &doc);
bool DocUnlock(CDoc &doc);
CDoc *DocPut();
extern "C" CDoc *DocPutCpp(CTask *task);
extern "C" CDoc *DocDisplayCpp(CTask *task);
extern "C" CDoc *DocBorderCpp(CTask *task);
extern "C" void DocTermNewCpp();
extern "C" CDoc *DocNewCpp(const char *filename, CTask *task);
extern "C" void DocDelCpp(CDoc *doc);
extern "C" void DocLoadCpp(CDoc *doc, const uint8_t *src, int64_t size);
extern "C" CDoc *DocReadCpp(const char *name, int64_t flags);
CDoc *DocRead(const char *name, int64_t flags, const CDocReadContext *parent,
              CTask *task = nullptr);
extern "C" uint8_t *DocSaveCpp(CDoc *doc, size_t *size);
extern "C" uint64_t DocWriteCpp(CDoc *doc, uint8_t prompt);
extern "C" uint64_t DocTypeCpp(CDoc *doc, const char *name,
                               int64_t trailing_new_lines);
extern "C" uint64_t TypeCpp(const char *name, int64_t trailing_new_lines);
extern "C" char *Doc2PlainTextCpp(CDoc *doc, CDocEntryBase *entry);
CDocEntryBase *DocEntryNewBase(CDoc &doc, uint32_t type, uint64_t flags = 0);
CDocEntryBase *DocEntryCopy(CDoc &doc, const CDocEntryBase &entry);
void DocEntryDel(CDoc &doc, CDocEntryBase &entry);
void DocEntryError(CDocEntry &entry);
bool DocInsEntry(CDoc &doc, CDocEntryBase &entry);
void DocEntryInsert(CDocEntryBase &entry, CDocEntryBase &before);
extern "C" void DocCutCpp(CDoc *doc, CDocEntryBase *start, CDocEntryBase *end);
extern "C" CDoc *DocCopyCpp(CDoc *doc, CDocEntryBase *start, CDocEntryBase *end);
extern "C" uint64_t DocTreeFindCpp(CDoc *doc, const char *path,
                                    CDocEntryBase **tree, CDocEntryBase **start,
                                    CDocEntryBase **end);
extern "C" uint64_t DocTreeMakeCpp(CDoc *doc, const char *path);
extern "C" uint64_t DocTreeWriteCpp(CDoc *doc, const char *path, uint8_t write,
                                     const char *fmt, int64_t argc,
                                     const uint64_t *argv);
extern "C" uint64_t DocTreeAppendCpp(CDoc *doc, const char *path, uint8_t write,
                                      const char *fmt, int64_t argc,
                                      const uint64_t *argv);
extern "C" uint64_t DocTreeExeCpp(CDoc *doc, const char *path);
bool DocRecalc(CDoc &doc, uint64_t recalc_flags = RECALCt_NORMAL);
void DocUpdateTaskDocs(CTask *task);
extern "C" void DocPutKeyCpp(CDoc *doc, int64_t ch, int64_t sc);
extern "C" int64_t DocGetKeyCpp(int64_t *sc);
extern "C" char *DocScanLineCpp(CDoc *doc, CDocEntryBase *entry,
                                int64_t *cur_col, CDocEntryBase **end);
char *DocGetStr2(int64_t flags);
void EdCursorLeft(CDoc &doc, int64_t sc);
void EdCursorRight(CDoc &doc, int64_t sc);
void EdCharDel(CDoc &doc);
bool EdCharIns(CDoc &doc, int64_t ch, int64_t sc);
extern "C" int64_t DocEntryRunCpp(CDoc *doc, CDocEntryBase *entry, uint8_t exited,
                                   int64_t *has_action);
extern "C" void DocBottomCpp(CDoc *doc);
void DocDelToEntry(CDoc &doc, CDocEntryBase *clear_entry, bool clear_holds);
bool DocDelToNum(CDoc &doc, int64_t count);
CDocEntryBase *DocPutS(CDoc &doc, const char *src, size_t size,
                       const CDocReadContext *context = nullptr);
void DocDump(CDoc &doc, int64_t us_delay);
extern "C" CDocEntryBase *DocPrintCpp(CDoc *doc, const char *fmt, int64_t argc,
                                      const uint64_t *argv);
void DocDataFmt(CDoc &doc, CDocEntry &entry, int64_t d = DOCM_CANCEL);
bool DocDataScan(CDoc &doc, CDocEntry &entry);
bool DocFormEntry(const CDocEntryBase &entry);
void DocFormFwd(CDoc &doc, bool giveup = false);
void DocFormBwd(CDoc &doc, bool giveup = false);
bool DocFormKey(CDoc &doc, int64_t ch, int64_t sc);
extern "C" uint64_t DocEdCpp(CDoc *doc, int64_t dof_flags);
extern "C" int64_t PopUpMenuCpp(CDoc *doc, int64_t dof_flags);
extern "C" uint64_t DocFormCpp(uint8_t *d, const char *class_name,
                                int64_t dof_flags, const char *header,
                                const char *footer);
extern "C" void DocFormFwdCpp(CDoc *doc, uint64_t giveup);
extern "C" void DocFormBwdCpp(CDoc *doc, uint64_t giveup);
extern "C" void DocDataFmtCpp(CDoc *doc, CDocEntry *entry, int64_t d);
extern "C" void DocDataScanCpp(CDoc *doc, CDocEntry *entry);
extern "C" int64_t DocMaxCpp(int64_t entries);
CDocEntry *PrsDollarCmd(CDoc &doc, const char *text, const char *end,
                        const CDocReadContext *context);
CDocBin *DocBinCopy(CDoc &doc, const CDocBin &source, const char *tag);
bool DocBinPtrRst(CDoc &doc, CDocEntry &entry, const CDocReadContext *context);
void DocBinsValidate(CDoc &doc);
void DocBinDel(CDoc &doc, CDocBin *bin);
void DocBinFree(CDocBin &bin);

class CDocOut
{
public:
  explicit CDocOut(uint8_t *dst = nullptr) : dst(dst)
  {
  }
  void Put(const void *bytes, size_t size);
  void Put(const char *text);
  void Put(char ch);
  void Number(int64_t value, bool hex = false);
  void Quote(const char *text);
  size_t len = 0;

private:
  uint8_t *dst;
};

void Doc2PlainText(CDocOut &out, const CDoc &doc, const CDocEntryBase &entry);
uint8_t *DocSave(CDoc &doc, size_t *size);
