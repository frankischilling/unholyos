#pragma once

#include "../../Kernel/KernelB.hpp"

constexpr uint64_t DOCG_PLAIN = DOCF_PLAIN_TEXT | DOCF_PLAIN_TEXT_TABS;
constexpr uint64_t DOCG_PLAIN_ENTRY =
    DOCEF_TAG | DOCG_BL_IV_UL | DOCEF_HIGHLIGHT | DOCEF_WORD_WRAP |
    DOCEF_SKIP | DOCEF_FILTER_SKIP;

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
CDoc *DocPut();
extern "C" CDoc *DocPutCpp(CTask *task);
extern "C" CDoc *DocDisplayCpp(CTask *task);
extern "C" CDoc *DocBorderCpp(CTask *task);
extern "C" CDoc *DocNewCpp(const char *filename, CTask *task);
extern "C" void DocDelCpp(CDoc *doc);
extern "C" void DocLoadCpp(CDoc *doc, const uint8_t *src, int64_t size);
extern "C" char *Doc2PlainTextCpp(CDoc *doc, CDocEntryBase *entry);
CDocEntryBase *DocEntryNewBase(CDoc &doc, uint32_t type, uint64_t flags = 0);
void DocEntryDel(CDoc &doc, CDocEntryBase &entry);
void DocEntryError(CDocEntry &entry);
bool DocInsEntry(CDoc &doc, CDocEntryBase &entry);
bool DocRecalc(CDoc &doc, uint64_t recalc_flags = RECALCt_NORMAL);
void DocUpdateTaskDocs(CTask *task);
extern "C" void DocPutKeyCpp(CDoc *doc, int64_t ch, int64_t sc);
bool EdCharIns(CDoc &doc, int64_t ch, int64_t sc);
bool DocEntryRun(CDoc &doc, CDocEntryBase &entry);
CDocEntryBase *DocPutS(CDoc &doc, const char *src, size_t size);
CDocEntry *PrsDollarCmd(CDoc &doc, const char *text, const char *end);
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
