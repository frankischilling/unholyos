#include "../../Kernel/KernelB.hpp"

static constexpr uint64_t F64_SIGN = uint64_t{1} << 63;

static bool SndTimeLess(uint64_t a, uint64_t b)
{
  constexpr uint64_t infinity = 0x7FF0000000000000;
  auto am = a & ~F64_SIGN, bm = b & ~F64_SIGN;
  if (am > infinity || bm > infinity || !(am | bm))
    return false;
  if ((a ^ b) & F64_SIGN)
    return a & F64_SIGN;
  return a & F64_SIGN ? am > bm : am < bm;
}

extern "C" void SndShiftCpp(CSndData *head, uint64_t dt)
{
  if (dt & ~F64_SIGN) {
    auto *tmpsd = head->next;
    while (tmpsd != head) {
      auto *tmp = tmpsd->next;
      tmpsd->tS = F64AddBits(tmpsd->tS, dt ^ F64_SIGN);
      if (SndTimeLess(tmpsd->tS, tmpsd->last->tS)) {
        QueRem(*tmpsd);
        Free(tmpsd);
      }
      tmpsd = tmp;
    }
  }
}

extern "C" int64_t AUWriteCpp(const char *filename, CSndData *head,
                             uint64_t t0_now, uint64_t t0_tS)
{
  size_t cnt = 1;
  for (auto *tmpsd = head->next; tmpsd != head; tmpsd = tmpsd->next)
    cnt++;
  size_t size = cnt * sizeof(CAUData);
  auto *st = ExtDftCpp(filename, "AU");
  auto *base = reinterpret_cast<CAUData *>(
      MAlloc(size, CurrentTask()->data_heap));
  if (!st || !base) {
    Free(st);
    Free(base);
    return 0;
  }
  auto *dst = base;
  auto *tmpsd = head;
  do {
    auto dt = F64AddBits(tmpsd->tS, t0_tS ^ F64_SIGN);
    dst->cdt = t0_now + F64ToI64(F64MulBits(F64FromI64(49710), dt), 0xC00);
    dst->ona = tmpsd->ona;
    dst++;
    tmpsd = tmpsd->next;
  } while (tmpsd != head);
  FileWriteCpp(st, reinterpret_cast<uint8_t *>(base), size, 0, 0);
  Free(base);
  Free(st);
  return size;
}
