#include "DolDoc.hpp"

extern "C" void KDDocPutKeyHC(), KDDocPutSHC();

bool MakeDoc()
{
  return LoadDocDefines() &&
         KeyDevAddCpp(reinterpret_cast<uintptr_t>(KDDocPutKeyHC),
                      reinterpret_cast<uintptr_t>(KDDocPutSHC), 0x80000000, true);
}
