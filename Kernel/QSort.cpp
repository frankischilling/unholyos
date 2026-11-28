#include "KernelB.hpp"

struct CSortPartition
{
  int64_t left, right;
};

template <typename Compare, typename Swap>
static CSortPartition QSortSplit(int64_t first, int64_t count,
                                 const Compare &compare, const Swap &swap)
{
  int64_t left = first, right = first + count - 1;
  do {
    while (compare(left) < 0)
      left++;
    while (compare(right) > 0)
      right--;
    if (left <= right)
      swap(left++, right--);
  } while (left <= right);
  return {left, right};
}

template <typename Partition>
static void QSortPartitions(int64_t first, int64_t count,
                            const Partition &partition)
{
  auto [left, right] = partition(first, count);
  int64_t lower = right + 1 - first, upper = first + count - left;
  if (1 < lower && lower < count)
    QSortPartitions(first, lower, partition);
  if (1 < upper && upper < count)
    QSortPartitions(left, upper, partition);
}

template <typename Compare>
static void QSortI64(int64_t *base, int64_t count, const Compare &compare)
{
  if (count <= 1)
    return;
  QSortPartitions(0, count, [&](int64_t first, int64_t length) {
    int64_t pivot = base[first + length / 2];
    return QSortSplit(
        first, length,
        [&](int64_t index) { return compare(base[index], pivot); },
        [&](int64_t left, int64_t right) {
          auto value = base[left];
          base[left] = base[right];
          base[right] = value;
        });
  });
}

extern "C" void QSortI64Cpp(int64_t *base, int64_t count, uintptr_t compare)
{
  QSortI64(base, count,
           [&](int64_t a, int64_t b) { return CallHolyC2(compare, a, b); });
}

void QSortI64Native(int64_t *base, int64_t count,
                    int64_t (*compare)(int64_t, int64_t))
{
  QSortI64(base, count, compare);
}

template <typename Compare>
static void QSort(uint8_t *base, int64_t count, size_t width,
                  const Compare &compare)
{
  if (count <= 1 || !width || width > SIZE_MAX / 2)
    return;
  uint64_t local[2];
  auto *tmp = reinterpret_cast<uint8_t *>(local);
  if (width != sizeof(uint64_t)) {
    tmp = MAlloc(2 * width, MemTaskHeap(nullptr));
    if (!tmp)
      return;
  }
  auto *pivot = tmp + width;
  QSortPartitions(0, count, [&](int64_t first, int64_t length) {
    MemCpy(pivot, base + (first + length / 2) * width, width);
    return QSortSplit(
        first, length,
        [&](int64_t index) {
          return compare(base + index * width, pivot);
        },
        [&](int64_t left, int64_t right) {
          if (left != right) {
            MemCpy(tmp, base + right * width, width);
            MemCpy(base + right * width, base + left * width, width);
            MemCpy(base + left * width, tmp, width);
          }
        });
  });
  if (width != sizeof(uint64_t))
    Free(tmp);
}


void QSortNative(void *base, int64_t count, size_t width,
                 int64_t (*compare)(const void *, const void *))
{
  QSort(static_cast<uint8_t *>(base), count, width, compare);
}

extern "C" void QSortCpp(uint8_t *base, int64_t count, size_t width,
                         uintptr_t compare)
{
  QSort(base, count, width, [=](const void *a, const void *b) {
    return CallHolyC2(compare, reinterpret_cast<uintptr_t>(a),
                      reinterpret_cast<uintptr_t>(b));
  });
}
