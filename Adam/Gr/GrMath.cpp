#include "../../Kernel/KernelB.hpp"

extern "C" int64_t *Mat4x4EquCpp(int64_t *dst, const int64_t *src)
{
  MemCpy(dst, src, 16 * sizeof(*dst));
  return dst;
}

extern "C" int64_t *Mat4x4NewCpp(const int64_t *src, CTask *mem_task)
{
  auto *dst =
      reinterpret_cast<int64_t *>(MAllocCpp(16 * sizeof(int64_t), mem_task));
  return dst ? Mat4x4EquCpp(dst, src) : nullptr;
}
