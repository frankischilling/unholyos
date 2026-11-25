#include "../KernelB.hpp"

uint64_t *MemPageTable(uintptr_t addr)
{
  auto *pml2 = reinterpret_cast<uint64_t *>(static_cast<uintptr_t>(MEM_PML2));
  return pml2 + addr / MEM_PAGE_SIZE;
}
