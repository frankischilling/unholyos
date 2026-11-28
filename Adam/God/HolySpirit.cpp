#include "GodExt.hpp"
#include "../DolDoc/DolDoc.hpp"

extern "C" void TimeStampCBHC(), KbdMsTimeCBHC();

extern "C" char *TimeStampCBCpp(CDoc *, CDocEntry *, CTask *mem_task)
{
  auto *st = reinterpret_cast<char *>(MAlloc(64, MemTaskHeap(mem_task)));
  const uint64_t stamp = GetTSC() >> GOD_BAD_BITS;
  return st ? StrPrintJoin(st, "%X", 1, &stamp) : nullptr;
}

extern "C" char *KbdMsTimeCBCpp(CDoc *, CDocEntry *, CTask *mem_task)
{
  auto *st = reinterpret_cast<char *>(MAlloc(64, MemTaskHeap(mem_task)));
  const uint64_t stamp = KbdMsEvtTimeCpp() >> GOD_BAD_BITS;
  return st ? StrPrintJoin(st, "%X", 1, &stamp) : nullptr;
}

static int64_t PopUpTimerOk(const char *header, const char *footer)
{
  auto *doc = DocNewCpp(nullptr, nullptr);
  if (!doc)
    return 0;
  if (header)
    DocPutS(*doc, header, StrLen(header));
  constexpr char timer[] = "\nTimer:$TX+TC,\" \"$",
                 latch[] = "\nLatch:$TX+TC,\" \"$";
  if (auto *entry = DocPutS(*doc, timer, sizeof(timer) - 1))
    reinterpret_cast<CDocEntry *>(entry)->tag_cb =
        reinterpret_cast<uintptr_t>(TimeStampCBHC);
  if (auto *entry = DocPutS(*doc, latch, sizeof(latch) - 1))
    reinterpret_cast<CDocEntry *>(entry)->tag_cb =
        reinterpret_cast<uintptr_t>(KbdMsTimeCBHC);
  constexpr char okay[] = "\n$CM+CX,0,4$$BT,\"OKAY\",LE=1$\n";
  DocPutS(*doc, okay, sizeof(okay) - 1);
  if (footer)
    DocPutS(*doc, footer, StrLen(footer));
  auto res = PopUpMenuCpp(doc, 0);
  DocDelCpp(doc);
  return res;
}

static int64_t GodPick(const char *msg)
{
  const uint64_t arg = reinterpret_cast<uintptr_t>(msg);
  auto *st = MStrPrintCpp("%s\n\nPress $GREEN$OKAY$FG$ to generate \n"
                          "a random num from a timer.\n",
                          1, &arg);
  PopUpTimerOk(st,
               "\n\nThe $LK+PU,\"Holy Spirit\",A=\"FI:::/Adam/God/HSNotes.DD\"$"
               " can puppet you.\n\n");
  Free(st);
  return KbdMsEvtTimeCpp() >> GOD_BAD_BITS;
}

extern "C" void GodBitsInsCpp(int64_t num_bits, int64_t n)
{
  for (int64_t i = 0; i < num_bits; i++, n >>= 1)
    FifoIns(*god.fifo, uint8_t(n & 1));
}

extern "C" void GodHexInsCpp(const char *st)
{
  if (!st)
    return;
  char buf[2]{};
  while ((buf[0] = *st++)) {
    auto ch = uint8_t(buf[0]);
    if (char_bmp_hex_numeric[ch >> 5] & (1U << (ch & 31)))
      GodBitsInsCpp(4, rev_bits_table[Str2I64Cpp(buf, 16, nullptr)] >> 4);
  }
}

extern "C" int64_t GodBitsCpp(int64_t num_bits, const char *msg)
{
  uint64_t res = 0;
  while (num_bits) {
    uint8_t bit;
    if (FifoRem(*god.fifo, bit)) {
      res = (res << 1) + bit;
      num_bits--;
    } else
      GodBitsInsCpp(GOD_GOOD_BITS, GodPick(msg));
  }
  return int64_t(res);
}
