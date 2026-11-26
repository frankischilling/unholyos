#include "DolDoc/DolDoc.hpp"
#include "../Compiler/CompilerA.hpp"

template <class... T>
static CDocEntryBase *RepPrint(CDoc &doc, const char *fmt, T... args)
{
  uint64_t values[]{uint64_t(args)..., 0};
  return DocPrintCpp(&doc, fmt, sizeof...(args), values);
}

static void RepString(CDoc &doc, const void *ptr, bool dynamic)
{
  if (dynamic) {
    auto *base = RepPrint(doc, "$DA-TRM-P+RD,LEN=64,A=\"%%40ts\"$\n");
    if (base && base->Type() == DOCT_DATA) {
      auto &entry = *reinterpret_cast<CDocEntry *>(base);
      entry.data = const_cast<uint8_t *>(static_cast<const uint8_t *>(ptr));
      DocDataFmt(doc, entry);
    }
  } else {
    char buf[41];
    auto *str = static_cast<const char *>(ptr);
    size_t len = 0;
    while (len < 40 && ChkPtr(str + len) && str[len]) {
      buf[len] = str[len];
      len++;
    }
    buf[len] = 0;
    RepPrint(doc, "%40ts\n", buf);
  }
}

static uint64_t RepValue(const void *ptr, size_t size, bool sign)
{
  uint64_t value = 0;
  MemCpy(&value, ptr, size);
  if (sign && size < 8) {
    unsigned shift = 64 - size * 8;
    value = int64_t(value << shift) >> shift;
  }
  return value;
}

static void RepScalar(CDoc &doc, void *ptr, uint8_t type, bool dynamic,
                       bool character = false)
{
  const char *fmt, *widget;
  auto size = TypeSize(static_cast<CRawType>(type));
  if (!size || !ChkPtrRange(ptr, size))
    return;
  if (character) {
    fmt = "%c\n";
    widget = "$DA-TRM+RD+UD,RT=U8,A=\"%%c\"$\n";
  } else {
    switch (type) {
    case RT_I8:
      fmt = "%02X ";
      widget = "$DA-TRM+RD+UD,RT=I8,A=\"%%02X\"$ ";
      break;
    case RT_U8:
      fmt = "%02X ";
      widget = "$DA-TRM+RD+UD,RT=U8,A=\"%%02X\"$ ";
      break;
    case RT_I16:
      fmt = "%04X ";
      widget = "$DA-TRM+RD+UD,RT=I16,A=\"%%04X\"$ ";
      break;
    case RT_U16:
      fmt = "%04X ";
      widget = "$DA-TRM+RD+UD,RT=U16,A=\"%%04X\"$ ";
      break;
    case RT_I32:
      fmt = "%08X ";
      widget = "$DA-TRM+RD+UD,RT=I32,A=\"%%08X\"$ ";
      break;
    case RT_U32:
      fmt = "%08X ";
      widget = "$DA-TRM+RD+UD,RT=U32,A=\"%%08X\"$ ";
      break;
    case RT_U64:
      fmt = "%08X ";
      widget = "$DA-TRM+RD+UD,RT=U64,A=\"%%08X\"$ ";
      break;
    case RT_F64:
      fmt = "%16g ";
      widget = "$DA-TRM+RD+UD,RT=F64,A=\"%%16g\"$ ";
      break;
    default:
      fmt = "%016X ";
      widget = "$DA-TRM+RD+UD,A=\"%%016X\"$ ";
    }
  }
  if (dynamic) {
    auto *base = RepPrint(doc, widget);
    if (base && base->Type() == DOCT_DATA) {
      auto &entry = *reinterpret_cast<CDocEntry *>(base);
      entry.data = static_cast<uint8_t *>(ptr);
      DocDataFmt(doc, entry);
    }
  } else
    RepPrint(doc, fmt, RepValue(ptr, size, !character && !(type & 1)));
}

bool ClassRep2(CDoc &doc, void *_d, const char *class_name,
               int64_t depth, int64_t max_depth, bool dynamic,
               uint32_t types, int64_t offset)
{
  if (depth >= max_depth)
    return true;
  auto *tmpc = class_name ? reinterpret_cast<CHashClass *>(
      HashFind(class_name, CurrentTask()->hash_table, types)) : nullptr;
  if (!tmpc || (tmpc->flags & CF_EXTERN)) {
    RepPrint(doc, "Class Not Found.\n");
    return false;
  }
  auto addr = reinterpret_cast<uintptr_t>(_d);
  if (!ChkPtr(_d) || !ChkPtr(reinterpret_cast<void *>(addr + tmpc->size))) {
    RepPrint(doc, "Bad Ptr:%016X\n", addr);
    return false;
  }
  if (tmpc->base_class &&
      !ClassRep2(doc, _d, tmpc->base_class->sym.hash.str, depth, max_depth,
                 dynamic, types, offset))
    return false;
  CDocLock lock(doc);
  RepPrint(doc, "Class:\"%s\"\n", class_name);
  for (auto *ml = tmpc->member_lst_and_root; ml; ml = ml->next) {
    auto *tmpc2 = ml->member_class;
    if (!tmpc2)
      continue;
    auto *ptr = reinterpret_cast<uint8_t *>(addr + ml->offset);
    RepPrint(doc, "%08X ", uintptr_t(ptr) + offset);
    auto stars = tmpc2->ptr_stars_cnt;
    while (tmpc2->fwd_class)
      tmpc2 = tmpc2->fwd_class;
    tmpc2 -= tmpc2->ptr_stars_cnt;
    bool byte = tmpc2->raw_type == RT_I8 || tmpc2->raw_type == RT_U8;
    if (tmpc2->sym.hash.type & HTT_INTERNAL_TYPE) {
      RepPrint(doc, "$GREEN$%-20ts:$FG$", ml->str);
      if (stars == 1 && byte) {
        if (ChkPtrRange(ptr, 8)) {
          auto *value = reinterpret_cast<const void *>(RepValue(ptr, 8, false));
          if (ChkPtr(value))
            RepString(doc, value, dynamic);
        }
      } else if (!stars) {
        auto j = ml->dim.total_cnt < 32 ? ml->dim.total_cnt : 32;
        if (byte) {
          if (j == 1)
            RepScalar(doc, ptr, RT_U8, dynamic, true);
          else
            RepString(doc, ptr, dynamic);
        }
        auto size = TypeSize(static_cast<CRawType>(tmpc2->raw_type));
        for (int64_t i = 0; size && i < j; i++)
          RepScalar(doc, ptr + i * size, tmpc2->raw_type, dynamic);
        if (j < ml->dim.total_cnt)
          RepPrint(doc, "...");
      } else if (ChkPtrRange(ptr, 8))
        RepPrint(doc, "%016X", RepValue(ptr, 8, false));
      RepPrint(doc, "\n");
    } else if (depth < 2) {
      auto value = stars && ChkPtrRange(ptr, 8) ? RepValue(ptr, 8, false) : 0;
      if (stars == 1 && !ChkPtr(reinterpret_cast<void *>(value)))
        RepPrint(doc, "%-20ts:%016X\n", ml->str, value);
      else {
        RepPrint(doc, "$TR,\"%s\"$\n", ml->str);
        RepPrint(doc, "$ID,2$");
        if (!stars)
          ClassRep2(doc, ptr, tmpc2->sym.hash.str, depth + 1, max_depth,
                    dynamic, HTT_CLASS, 0);
        else if (stars == 1)
          ClassRep2(doc, reinterpret_cast<void *>(value), tmpc2->sym.hash.str,
                    depth + 1, max_depth, dynamic, HTT_CLASS, 0);
        RepPrint(doc, "$ID,-2$");
      }
    } else
      RepPrint(doc, "%-20ts\n", ml->str);
  }
  return true;
}

extern "C" uint64_t ClassRep2Cpp(CDoc *doc, void *d, const char *class_name,
                                 int64_t depth, int64_t max_depth, uint64_t dynamic,
                                 uint32_t types, int64_t offset)
{
  return doc && ClassRep2(*doc, d, class_name, depth, max_depth,
                          dynamic, types, offset);
}

static void ClassRepDoc(void *d, const char *class_name, int64_t max_depth,
                        bool fun, int64_t offset, bool dynamic)
{
  bool raw = IsRawCpp();
  auto *doc = raw ? DocNewCpp(nullptr, nullptr) : DocPut();
  if (!doc)
    return;
  if (!raw)
    DocMaxCpp(INT64_MAX);
  ClassRep2(*doc, d, class_name, 0, max_depth, dynamic,
            fun ? HTT_FUN : HTT_CLASS, offset);
  RepPrint(*doc, "\n");
  DocRecalc(*doc);
  if (raw) {
    DocDump(*doc, 100000);
    DocDelCpp(doc);
  }
}

extern "C" void ClassRepCpp(void *d, const char *class_name, int64_t max_depth,
                            uint64_t fun, int64_t offset)
{
  ClassRepDoc(d, class_name, max_depth, fun, offset, false);
}

extern "C" void ClassRepDCpp(void *d, const char *class_name, int64_t max_depth,
                             uint64_t fun, int64_t offset)
{
  ClassRepDoc(d, class_name, max_depth, fun, offset, true);
}
