#pragma once

struct CDrv;
struct CLoadedModule;

// Guest boot checks and serial reports consumed by TestBoot.cpp.
void BootPreCompilerProbe();
void BootCompilerImageReport(const CLoadedModule &module);
void BootOpcodeResourceReport(const CDrv &dvd);
void BootKernelCfgReport();
void BootProbe();
