#include "DolDoc.hpp"
#include "../../Compiler/CompilerA.hpp"

static bool DataAddress(CDocEntry &entry, uint8_t *&ptr, size_t &size)
{
  auto type = static_cast<CRawType>(entry.raw_type);
  size = TypeSize(type);
  if (!size)
    return false;
  if ((entry.base.de_flags & DOCEF_DEREF_DATA) &&
      !(entry.base.de_flags & DOCEF_REMALLOC_DATA)) {
    ptr = entry.data;
    return ChkPtrRange(ptr, size);
  }
  ptr = reinterpret_cast<uint8_t *>(&entry.data);
  return true;
}

static uint64_t DataValue(const uint8_t *ptr, uint8_t type, size_t size)
{
  uint64_t value = 0;
  MemCpy(&value, ptr, size);
  if (size < 8 && !(type & 1)) {
    unsigned shift = 64 - 8 * size;
    value = int64_t(value << shift) >> shift;
  }
  return value;
}

static bool DataFormatIs(const CDocEntry &entry, const char *fmt)
{
  return entry.aux_str &&
         !StrCmpCpp(reinterpret_cast<const uint8_t *>(entry.aux_str),
                    reinterpret_cast<const uint8_t *>(fmt));
}

void DocDataFmt(CDoc &doc, CDocEntry &entry, int64_t d)
{
  auto &base = entry.base;
  if (base.Type() != DOCT_DATA || !(base.de_flags & DOCEF_AUX_STR) ||
      !entry.aux_str || (base.de_flags & DOCEF_REMALLOC_DATA))
    return;
  CDocLock lock(doc);
  uint8_t *ptr;
  size_t size;
  if (d == DOCM_CANCEL) {
    if (!DataAddress(entry, ptr, size))
      return;
    d = DataValue(ptr, entry.raw_type, size);
  }
  // Class reports show at most 40 bytes of a live string.
  char str[41];
  if (DataFormatIs(entry, "%40ts")) {
    auto *src = reinterpret_cast<const char *>(d);
    size_t n = 0;
    while (n < sizeof(str) - 1 && ChkPtr(src + n) && src[n]) {
      str[n] = src[n];
      n++;
    }
    str[n] = 0;
    d = reinterpret_cast<int64_t>(str);
  }
  uint64_t args[]{uint64_t(d), reinterpret_cast<uintptr_t>(entry.my_fmt_data)};
  auto *text = StrPrintJoin(nullptr, entry.aux_str, 2, args);
  if (!text)
    return;
  size_t len = StrLen(text);
  if (base.de_flags & DOCEF_HAS_TERMINATOR)
    len++;
  auto capacity = len + 1;
  if (entry.len >= 0 && size_t(entry.len) + base.min_col + 2 > capacity)
    capacity = size_t(entry.len) + base.min_col + 2;
  if (!base.tag || MSize(base.tag) < capacity) {
    auto *tag = reinterpret_cast<char *>(MAlloc(capacity, MemTaskHeap(doc.mem_task)));
    if (!tag) {
      Free(text);
      return;
    }
    Free(base.tag);
    base.tag = tag;
  }
  MemCpy(base.tag, text, StrLen(text) + 1);
  Free(text);
  if (base.de_flags & DOCEF_HAS_TERMINATOR) {
    base.tag[len - 1] = '_';
    base.tag[len] = 0;
  }
  base.max_col = len;
  if (doc.cur_entry == &base && doc.cur_col > int64_t(len))
    doc.cur_col = len;
}

// The bound scalar formats used by ClassRepD, including raw F64 bits.
bool DocDataScan(CDoc &doc, CDocEntry &entry)
{
  auto &base = entry.base;
  if (base.Type() != DOCT_DATA || !(base.de_flags & DOCEF_AUX_STR) ||
      !base.tag || (base.de_flags & DOCEF_REMALLOC_DATA))
    return false;
  CDocLock lock(doc);
  uint8_t *ptr;
  size_t size;
  if (!DataAddress(entry, ptr, size))
    return false;
  char text[17];
  size_t width = 0;
  if (DataFormatIs(entry, "%02X"))
    width = 2;
  else if (DataFormatIs(entry, "%04X"))
    width = 4;
  else if (DataFormatIs(entry, "%08X"))
    width = 8;
  else if (DataFormatIs(entry, "%016X") || DataFormatIs(entry, "%16g"))
    width = 16;
  if (width) {
    size_t len = StrLen(base.tag);
    if (len > width)
      len = width;
    MemCpy(text, base.tag, len);
    text[len] = 0;
  }
  uint64_t value;
  if (DataFormatIs(entry, "%c"))
    value = uint8_t(*base.tag);
  else if (DataFormatIs(entry, "%02X") || DataFormatIs(entry, "%04X") ||
           DataFormatIs(entry, "%08X") || DataFormatIs(entry, "%016X"))
    value = Str2I64Cpp(text, 16, nullptr);
  else if (DataFormatIs(entry, "%16g"))
    value = Str2F64Cpp(text, nullptr);
  else
    return false;
  MemCpy(ptr, &value, size);
  return true;
}

extern "C" void DocDataFmtCpp(CDoc *doc, CDocEntry *entry, int64_t d)
{
  if (doc && entry && doc->doc_signature == DOC_SIGNATURE_VAL)
    DocDataFmt(*doc, *entry, d);
}

extern "C" void DocDataScanCpp(CDoc *doc, CDocEntry *entry)
{
  if (doc && entry && doc->doc_signature == DOC_SIGNATURE_VAL)
    DocDataScan(*doc, *entry);
}
