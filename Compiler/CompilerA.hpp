#pragma once

#include "../Kernel/KernelA.hpp"

enum CLexToken : int64_t
{
  TK_ERROR = -1,
  TK_EOF = 0,
  TK_SUPERSCRIPT,
  TK_SUBSCRIPT,
  TK_NORMALSCRIPT,
  TK_IDENT = 0x100,
  TK_STR,
  TK_I64,
  TK_CHAR_CONST,
  TK_F64,
  TK_PLUS_PLUS,
  TK_MINUS_MINUS,
  TK_DEREFERENCE,
  TK_DBL_COLON,
  TK_SHL,
  TK_SHR,
  TK_EQU_EQU,
  TK_NOT_EQU,
  TK_LESS_EQU,
  TK_GREATER_EQU,
  TK_AND_AND,
  TK_OR_OR,
  TK_XOR_XOR,
  TK_SHL_EQU,
  TK_SHR_EQU,
  TK_MUL_EQU,
  TK_DIV_EQU,
  TK_AND_EQU,
  TK_OR_EQU,
  TK_XOR_EQU,
  TK_ADD_EQU,
  TK_SUB_EQU,
  TK_IF,
  TK_IFDEF,
  TK_IFNDEF,
  TK_IFAOT,
  TK_IFJIT,
  TK_ENDIF,
  TK_ELSE,
  TK_MOD_EQU = 0x122,
  TK_DOT_DOT,
  TK_ELLIPSIS,
  TK_INS_BIN,
  TK_INS_BIN_SIZE,
};

enum CKeyword : int64_t
{
  KW_INCLUDE,
  KW_DEFINE,
  KW_UNION,
  KW_CATCH,
  KW_CLASS,
  KW_TRY,
  KW_IF,
  KW_ELSE,
  KW_FOR,
  KW_WHILE,
  KW_EXTERN,
  KW__EXTERN,
  KW_RETURN,
  KW_SIZEOF,
  KW__INTERN,
  KW_DO,
  KW_ASM,
  KW_GOTO,
  KW_EXE,
  KW_BREAK,
  KW_SWITCH,
  KW_START,
  KW_END,
  KW_CASE,
  KW_DFT,
  KW_PUBLIC,
  KW_OFFSET,
  KW_IMPORT,
  KW__IMPORT,
  KW_IFDEF,
  KW_IFNDEF,
  KW_IFAOT,
  KW_IFJIT,
  KW_ENDIF,
  KW_ASSERT,
  KW_REG,
  KW_NOREG,
  KW_LASTCLASS,
  KW_NO_WARN,
  KW_HELP_INDEX,
  KW_HELP_FILE,
  KW_STATIC,
  KW_LOCK,
  KW_DEFINED,
  KW_INTERRUPT,
  KW_HASERRCODE,
  KW_ARGPOP,
  KW_NOARGPOP,
};

constexpr int64_t CCF_NO_DEFINES = 0x010, CCF_IN_IF = 0x020,
                  CCF_JUST_LOAD = 0x040, CCF_KEEP_NEW_LINES = 0x080,
                  CCF_KEEP_DOT = 0x100, CCF_KEEP_SIGN_NUM = 0x200,
                  CCF_KEEP_AT_SIGN = 0x400, CCF_NO_CHAR_CONST = 0x800,
                  CCF_IN_QUOTES = 0x0000080000, CCF_EXE_BLK = 0x0000100000,
                  CCF_HAS_MISC_DATA = 0x0000200000,
                  CCF_ASM_EXPRESSIONS = 0x0000800000,
                  CCF_USE_LAST_U16 = 0x0200000000,
                  CCF_LAST_WAS_DOT = 0x0400000000,
                  CCF_AOT_COMPILE = 0x0800000000;
constexpr int64_t LFSF_DOC = 1, LFSF_DEFINE = 2;
constexpr int64_t COMPILER_ERROR = 0x72656C69706D6F43; // Compiler
constexpr int64_t OPTF_WARN_UNUSED_VAR = 1 << 0x10,
                  OPTF_WARN_HEADER_MISMATCH = 1 << 0x13;

// The caller owns the root source and filename. The lexer owns nested sources
// and token text. The caller owns the symbol table.
void LexSourceInit(CCmpCtrl &cc, CLexFile &file, const char *src,
                   int64_t flags = 0, const char *filename = nullptr,
                   CHashTable *table = nullptr);
void LexSourceDel(CCmpCtrl &cc);
// An attached include owns the document. The caller owns a root document.
bool LexAttachDoc(CCmpCtrl &cc, CDoc &doc, CLexFile *root = nullptr,
                  CDocEntryBase *entry = nullptr, uint32_t col = 0);
int64_t LexGetChar(CCmpCtrl &cc);
int64_t LexRaw(CCmpCtrl &cc);
int64_t Lex(CCmpCtrl &cc);
char *LexExtStr(CCmpCtrl &cc, size_t &size);
bool LexIncludeStr(CCmpCtrl &cc, char *name, char *src, int64_t flags);
bool CmpAsmHashLoad(CHashTable *table);
CHashTable *CmpAsmHashNew();
CMemberLst *MemberFind(const char *name, CHashClass *cls);
CMemberLst *MemberAdd(CHashClass &owner, const char *name, CHashClass &cls,
                      int64_t offset);
CHashClass *PrsClass(const CCmpCtrl &cc);
CHashClass *PrsStars(CCmpCtrl &cc, CHashClass *cls);
CHashClass *PrsClassNew();
bool PrsArrayDims(CCmpCtrl &cc, CArrayDim &dim, size_t item_size);
bool PrsVarInit(CCmpCtrl &cc, CHashClass &cls, CArrayDim &dim, uint8_t *&data,
                CHeapCtrl *heap);
constexpr unsigned PTR_STARS_NUM = 4;

enum CRawType : uint8_t
{
  RT_U0 = 3,
  RT_I8 = 4,
  RT_U8,
  RT_I16,
  RT_U16,
  RT_I32,
  RT_U32,
  RT_I64 = 10,
  RT_U64 = 11,
  RT_F64 = 14,
  RT_AGGREGATE = 255, // Intermediate code only.
};

inline CRawType ClassRawType(const CHashClass &cls)
{
  if (cls.ptr_stars_cnt || (cls.flags & CF_INTERNAL_TYPE))
    return static_cast<CRawType>(cls.raw_type);
  if (cls.fwd_class)
    return static_cast<CRawType>(cls.fwd_class->raw_type);
  return RT_AGGREGATE;
}

inline bool ClassComplete(const CHashClass &cls)
{
  return cls.size > 0 && !(cls.flags & CF_EXTERN);
}

constexpr bool IsInt(CRawType type)
{
  return RT_I8 <= type && type <= RT_U64;
}

constexpr bool IsUnsigned(CRawType type)
{
  return IsInt(type) && (type & 1);
}

constexpr size_t TypeSize(CRawType type)
{
  return IsInt(type)      ? size_t{1} << ((type - RT_I8) / 2)
         : type == RT_F64 ? sizeof(uint64_t)
                          : 0;
}

enum CICode : uint8_t
{
  IC_END_EXP = 0x02,
  IC_LABEL = 0x04,
  IC_RETURN_VAL = 0x08,
  IC_IMM_I64 = 0x0A,
  IC_IMM_F64,
  IC_STR_CONST,
  IC_RBP = 0x13,
  IC_TO_I64 = 0x1B,
  IC_HOLYC_TYPECAST = 0x1F,
  IC_DEREF = 0x24,
  IC__PP = 0x27,
  IC__MM,
  IC_PP_,
  IC_MM_,
  IC_COM = 0x21,
  IC_NOT,
  IC_UNARY_MINUS,
  IC_SHL = 0x2B,
  IC_SHR,
  IC_POWER = 0x2F,
  IC_MUL,
  IC_DIV,
  IC_MOD,
  IC_AND,
  IC_OR,
  IC_XOR,
  IC_ADD,
  IC_SUB,
  IC_EQU_EQU = 0x3A,
  IC_NOT_EQU,
  IC_LESS,
  IC_GREATER_EQU,
  IC_GREATER,
  IC_LESS_EQU,
  IC_AND_AND = 0x41,
  IC_OR_OR,
  IC_XOR_XOR,
  IC_ASSIGN = 0x44,
  IC_SHL_EQU = 0x47,
  IC_SHR_EQU,
  IC_MUL_EQU,
  IC_DIV_EQU,
  IC_MOD_EQU,
  IC_AND_EQU,
  IC_OR_EQU,
  IC_XOR_EQU,
  IC_ADD_EQU,
  IC_SUB_EQU,
  IC_JMP = 0x51,
  IC_CALL = 0x5C,
  IC_PUSH = 0x62,
  IC_BR_ZERO = 0x85,
  IC_BR_NOT_ZERO,
};

constexpr bool ICIsComparison(CICode code)
{
  return IC_EQU_EQU <= code && code <= IC_LESS_EQU;
}

constexpr bool ICIsCompoundAssign(CICode code)
{
  return IC_SHL_EQU <= code && code <= IC_SUB_EQU;
}

constexpr uint32_t NO_IC = UINT32_MAX;

struct CIntermediateCode
{
  uint64_t data;
  uint32_t left, right;
  CICode code;
  CRawType type;
  bool chain, grouped, runtime;
  uint32_t next = NO_IC;
  CHashClass *cls = nullptr;
  const CArrayDim *dim = nullptr;
};

struct CCodeCtrl
{
  CCodeCtrl() = default;
  CCodeCtrl(const CCodeCtrl &) = delete;
  CCodeCtrl &operator=(const CCodeCtrl &) = delete;
  ~CCodeCtrl();

  uint32_t Add(const CIntermediateCode &entry);
  void Append(uint32_t index);
  void Append(CICode op, uint32_t left = NO_IC, uint32_t right = NO_IC);
  uint32_t LabelNew();
  uint32_t LValue(uint32_t address, CHashClass &cls,
                  const CArrayDim *dim = nullptr);
  uint32_t Count() const
  {
    return used;
  }

  CIntermediateCode *code = nullptr;
  uint32_t first = NO_IC, last = NO_IC;
  bool failed = false, has_misc_data = false;

private:
  uint32_t used = 0, capacity = 0;
};

// The returned code belongs to the current task's executable heap.
// Parsing starts at cc.token and leaves the first token after the expression.
uint8_t *LexExpression2Bin(CCmpCtrl &cc, CRawType &type);
uint8_t *LexStatementExpression2Bin(CCmpCtrl &cc, CRawType &type);
uint8_t *LexTypedExpression2Bin(CCmpCtrl &cc, CRawType type,
                                bool strings = false);
uint32_t PrsExpression(CCmpCtrl &cc, CCodeCtrl &coc);
uint32_t PrsPrint(CCmpCtrl &cc, CCodeCtrl &coc);
bool OptBranch(CCodeCtrl &coc, uint32_t value, CICode branch, uint32_t target,
               unsigned depth = 0);
bool LexExpression(CCmpCtrl &cc, uint64_t &result);
uint8_t *COCCompile(CCodeCtrl &coc, uint32_t root, CRawType type,
                    const CHashFun *fun = nullptr);
bool LexStmt2Bin(CCmpCtrl &cc, uint8_t *&machine, CRawType &type);
bool PrsStreamBlk(CCmpCtrl &cc);
bool CmpCall(CCmpCtrl &cc, const uint8_t *machine, uint64_t &result);
bool ExeCmdLine(CCmpCtrl &cc, uint64_t &result);
struct CExeResult
{
  uint64_t value;
  int64_t exception;
  bool okay;
};

CExeResult ExePutS(const char *src, const char *filename, int64_t flags,
                   const CLexHashTableContext *htc = nullptr);
CExeResult ExeFile(const char *name, int64_t flags = 0);
int64_t ExeAnswer(const CExeResult &result);
extern "C" int64_t ExeFileCpp(const char *name, int64_t flags);
