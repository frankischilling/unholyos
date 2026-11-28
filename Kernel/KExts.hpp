#pragma once

#include "../Compiler/CompilerA.hpp"
#include "KernelB.hpp"

// Native C++ entries for the compiler imports in Kernel/Kernel.PRJ.
struct CCompilerImports
{
  decltype(&::AsmHashLoad) AsmHashLoad;
  decltype(&::LexStmt2Bin) LexStmt2Bin;
  decltype(&::ExePutS) ExePutS;
  decltype(&::ClassMemberLstDel) ClassMemberLstDel;
  decltype(&::CmpAsmHashLoad) CmpAsmHashLoad;
  decltype(&::LexSourceInit) LexSourceInit;
  decltype(&::LexSourceDel) LexSourceDel;
  decltype(&::Lex) Lex;
  decltype(&::LexTypedExpression2Bin) LexTypedExpression2Bin;
  decltype(&::MemberMetaDataCpp) MemberMetaDataCpp;
  decltype(&::AsmHashFind) AsmHashFind;
  decltype(&::CmpAsmHashNew) CmpAsmHashNew;
  decltype(&::LexAttachDoc) LexAttachDoc;
  decltype(&::LexRaw) LexRaw;
  decltype(&::LexExpression2Bin) LexExpression2Bin;
  decltype(&::ExeFile) ExeFile;
  decltype(&::LexExceptReport) LexExceptReport;
};

static_assert(sizeof(CCompilerImports) == 136);
static_assert(offsetof(CCompilerImports, AsmHashLoad) == 0);
static_assert(offsetof(CCompilerImports, LexStmt2Bin) == 8);
static_assert(offsetof(CCompilerImports, ExePutS) == 16);
static_assert(offsetof(CCompilerImports, ClassMemberLstDel) == 24);
static_assert(offsetof(CCompilerImports, CmpAsmHashLoad) == 32);
static_assert(offsetof(CCompilerImports, LexSourceInit) == 40);
static_assert(offsetof(CCompilerImports, LexSourceDel) == 48);
static_assert(offsetof(CCompilerImports, Lex) == 56);
static_assert(offsetof(CCompilerImports, LexTypedExpression2Bin) == 64);
static_assert(offsetof(CCompilerImports, MemberMetaDataCpp) == 72);
static_assert(offsetof(CCompilerImports, AsmHashFind) == 80);
static_assert(offsetof(CCompilerImports, CmpAsmHashNew) == 88);
static_assert(offsetof(CCompilerImports, LexAttachDoc) == 96);
static_assert(offsetof(CCompilerImports, LexRaw) == 104);
static_assert(offsetof(CCompilerImports, LexExpression2Bin) == 112);
static_assert(offsetof(CCompilerImports, ExeFile) == 120);
static_assert(offsetof(CCompilerImports, LexExceptReport) == 128);
static_assert(sizeof(CExeResult) == 24 && offsetof(CExeResult, value) == 0 &&
              offsetof(CExeResult, exception) == 8 &&
              offsetof(CExeResult, okay) == 16);

extern "C" const CCompilerImports *compiler_imports;
bool CompilerImportsPublish(const CCompilerImports &imports);
