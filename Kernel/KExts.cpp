#include "KExts.hpp"

extern "C" {
const CCompilerImports *compiler_imports;
}

static CCompilerImports compiler_imports_storage;
static uint64_t compiler_imports_lock;

bool CompilerImportsPublish(const CCompilerImports &imports)
{
  CMemLock lock(compiler_imports_lock);
  if (__atomic_load_n(&compiler_imports, __ATOMIC_ACQUIRE))
    return false;
  compiler_imports_storage = imports;
  __atomic_store_n(&compiler_imports, &compiler_imports_storage,
                   __ATOMIC_RELEASE);
  return true;
}
