#pragma once

#include "../Kernel/KernelA.hpp"

extern "C" void WhoCpp(const char *fu_flags, CHashTable *h, const char *idx,
                       CDoc *doc);
extern "C" int64_t HashDepthRepCpp(CHashTable *table);
extern "C" void DocHelpIdxCpp(CDoc *doc, const char *idx);
extern "C" void PopUpHelpIndexCpp(const char *idx, CTask *parent);
extern "C" void MapFileLoadCpp(const char *filename);
