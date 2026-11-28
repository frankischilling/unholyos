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

constexpr int64_t CCF_CMD_LINE = 0x001, CCF_PMT = 0x002, CCF_NO_DEFINES = 0x010,
                  CCF_IN_IF = 0x020, CCF_JUST_LOAD = 0x040,
                  CCF_KEEP_NEW_LINES = 0x080, CCF_KEEP_DOT = 0x100,
                  CCF_KEEP_SIGN_NUM = 0x200, CCF_KEEP_AT_SIGN = 0x400,
                  CCF_NO_CHAR_CONST = 0x800, CCF_IN_QUOTES = 0x0000080000,
                  CCF_EXE_BLK = 0x0000100000, CCF_HAS_MISC_DATA = 0x0000200000,
                  CCF_ASM_EXPRESSIONS = 0x0000800000,
                  CCF_USE_LAST_U16 = 0x0200000000,
                  CCF_LAST_WAS_DOT = 0x0400000000,
                  CCF_AOT_COMPILE = 0x0800000000;
constexpr int64_t LFSF_DOC = 1, LFSF_DEFINE = 2;
constexpr int64_t COMPILER_ERROR = 0x72656C69706D6F43; // Compiler
constexpr int64_t OPTF_WARN_UNUSED_VAR = 1 << 0x10,
                  OPTF_WARN_HEADER_MISMATCH = 1 << 0x13;
constexpr int64_t OPTf_EXTERNS_TO_IMPORTS = 0x20, OPTf_NO_REG_VAR = 0x22;
extern "C" uint64_t GetOptionCpp(int64_t num);
char *CmdLinePmt();

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
const char *LexPutLine(const CLexFile &file, const char *start);
void LexPutPos(const CCmpCtrl &cc);
void LexWarn(CCmpCtrl &cc, const char *message = nullptr);
void LexExceptReport(const CCmpCtrl &cc, const char *message);
void UnusedExternWarning(CCmpCtrl &cc, const CHashClass &cls);
int64_t LexRaw(CCmpCtrl &cc);
int64_t Lex(CCmpCtrl &cc);
char *LexExtStr(CCmpCtrl &cc, size_t &size);
bool LexIncludeStr(CCmpCtrl &cc, char *name, char *src, int64_t flags);
enum : uint16_t
{
  IEF_OP_SIZE16 = 0x001,
  IEF_OP_SIZE32 = 0x002,
  IEF_PLUS_OPCODE = 0x004,
  IEF_DONT_SWITCH_MODES = 0x008,
  IEF_DFT = 0x010,
  IEF_NOT_IN_64_BIT = 0x020,
  IEF_48_REX = 0x040,
  IEF_REX_ONLY_R8_R15 = 0x080,
  IEF_REX_XOR_LIKE = 0x100,
  IEF_STI_LIKE = 0x200,
  IEF_ENDING_ZERO = 0x400,
};

enum : uint8_t
{
  SV_R_REG = 8,
  SV_I_REG,
  SV_STI_LIKE,
  SV_NONE,
  OM_NO = 0,
  OM_CB,
  OM_CW,
  OM_CD,
  OM_CP,
  OM_IB,
  OM_IW,
  OM_ID,
  ARGT_NONE = 0,
  ARGT_REL8,
  ARGT_REL16,
  ARGT_REL32,
  ARGT_IMM8,
  ARGT_IMM16,
  ARGT_IMM32,
  ARGT_IMM64,
  ARGT_UIMM8,
  ARGT_UIMM16,
  ARGT_UIMM32,
  ARGT_UIMM64,
  ARGT_R8,
  ARGT_R16,
  ARGT_R32,
  ARGT_R64,
  ARGT_RM8,
  ARGT_RM16,
  ARGT_RM32,
  ARGT_RM64,
  ARGT_M8,
  ARGT_M16,
  ARGT_M32,
  ARGT_M64,
  ARGT_AL = 32,
  ARGT_AX,
  ARGT_EAX,
  ARGT_RAX,
  ARGT_CL,
  ARGT_DX,
  ARGT_SREG = 39,
  ARGT_SS,
  ARGT_DS,
  ARGT_ES,
  ARGT_FS,
  ARGT_GS,
  ARGT_CS,
  ARGT_ST0,
  ARGT_STI,
  OCF_ALIAS = 1,
  AKW_ALIGN = 64,
  AKW_ORG,
  AKW_I8 = 67,
  AKW_I16,
  AKW_I32,
  AKW_I64,
  AKW_U8 = 72,
  AKW_U16,
  AKW_U32,
  AKW_U64,
  AKW_DU8 = 77,
  AKW_DU16,
  AKW_DU32,
  AKW_DU64,
  AKW_DUP,
  AKW_USE16,
  AKW_USE32,
  AKW_USE64,
  AKW_IMPORT,
  AKW_LIST,
  AKW_NOLIST,
  AKW_BINFILE,
};

inline const CInst *AsmInstEntries(const CHashOpcode &op)
{
  return reinterpret_cast<const CInst *>(
      reinterpret_cast<const uint8_t *>(&op) + offsetof(CHashOpcode, ins));
}

bool AsmHashLoad();
bool UAsmHashLoad(const CHashTable &table, CHeapCtrl *heap);
const CHash *AsmHashFind(const char *name, uint32_t mask);
bool CmpAsmHashLoad(CHashTable *table);
CHashTable *CmpAsmHashNew();
CMemberLst *MemberFind(const char *name, CHashClass *cls);
bool MemberLstCmp(const CMemberLst *left, const CMemberLst *right, int64_t count);
extern "C" CMemberLstMeta *MemberMetaFindCpp(const char *name, CMemberLst *member);
extern "C" uint64_t MemberMetaDataCpp(const char *name, CMemberLst *member);
extern "C" CMemberLst *MemberFindCpp(const char *name, CHashClass *cls);
bool PrsMemberMeta(CCmpCtrl &cc, CMemberLst &member);
static_assert(offsetof(CMemberLst, next) == 0 &&
              offsetof(CHashClass, last_in_member_lst) == 104);
CMemberLst *MemberAdd(CHashClass &owner, const char *name, CHashClass &cls,
                      int64_t offset);
CHashClass *PrsClass(const CCmpCtrl &cc);
CHashClass *PrsStars(CCmpCtrl &cc, CHashClass *cls);
CHashClass *PrsClassNew();
static_assert(sizeof(CHashFun) == 168 && offsetof(CHashFun, cls) == 0 &&
              offsetof(CHashFun, return_class) == 128 &&
              offsetof(CHashFun, arg_cnt) == 136 &&
              offsetof(CHashFun, pad) == 140 &&
              offsetof(CHashFun, used_reg_mask) == 144 &&
              offsetof(CHashFun, clobbered_reg_mask) == 148 &&
              offsetof(CHashFun, exe_addr) == 152 &&
              offsetof(CHashFun, ext_lst) == 160);
CHashFun *PrsFunNew();
constexpr int8_t REG_UNDEF = INT8_MIN, REG_NONE = 32, REG_ALLOC = 33;
bool PrsReg(CCmpCtrl &cc, int8_t &reg);
struct CPrsDeclarator
{
  CPrsDeclarator() = default;
  CPrsDeclarator(const CPrsDeclarator &) = delete;
  CPrsDeclarator &operator=(const CPrsDeclarator &) = delete;
  ~CPrsDeclarator();

  char *TakeName();
  CHashFun *TakeFun();
  void Bind(CMemberLst &member);

  CHashClass *cls = nullptr;
  const char *name = nullptr;
  CHashFun *fun_ptr = nullptr;
  int8_t reg = REG_UNDEF;
};

inline CHashFun *FunPtrBase(CHashFun *fun)
{
  return fun ? fun - fun->cls.ptr_stars_cnt : nullptr;
}

CHashFun *PrsFunPtrNew(CHashClass &result, unsigned stars, uint16_t flags);
bool PrsType(CCmpCtrl &cc, CHashClass &base, CPrsDeclarator &decl,
             uint16_t flags = 0, bool local = false, unsigned depth = 0);
bool PrsFunArgs(CCmpCtrl &cc, CHashFun &fun, unsigned depth = 0);
bool PrsArrayDims(CCmpCtrl &cc, CArrayDim &dim, size_t item_size);
bool PrsVarInit(CCmpCtrl &cc, CHashClass &cls, CArrayDim &dim, uint8_t *&data,
                CHeapCtrl *heap);
constexpr unsigned PTR_STARS_NUM = 4;

enum CRawType : uint8_t
{
  RT_I0 = 2,
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

constexpr unsigned INTERNAL_TYPES_NUM = 17;

struct CInternalType
{
  uint8_t type, size, name[8];
};

static_assert(sizeof(CInternalType) == 10 &&
              offsetof(CInternalType, type) == 0 &&
              offsetof(CInternalType, size) == 1 &&
              offsetof(CInternalType, name) == 2);
extern CInternalType internal_types_table[INTERNAL_TYPES_NUM];

inline CRawType ClassRawType(const CHashClass &cls)
{
  auto *type = &cls;
  while (type->fwd_class)
    type = type->fwd_class;
  if (type->ptr_stars_cnt || (type->flags & CF_INTERNAL_TYPE))
    return static_cast<CRawType>(type->raw_type);
  return RT_AGGREGATE;
}

inline bool ClassComplete(const CHashClass &cls)
{
  return cls.size > 0 && !(cls.flags & CF_EXTERN);
}

constexpr bool IsVoid(CRawType type)
{
  return type == RT_I0 || type == RT_U0;
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
  IC_GET_LABEL = 0x12,
  IC_RBP = 0x13,
  IC_DATA_ADDR = 0x14,
  IC_GLBL_ADDR = 0x15,
  IC_ASM_ADDR = 0x16,
  IC_FS = 0x17,
  IC_GS = 0x18,
  IC_TO_I64 = 0x1B,
  IC_TO_F64 = 0x1C,
  IC_TO_BOOL = 0x1D,
  IC_TOUPPER = 0x1E,
  IC_HOLYC_TYPECAST = 0x1F,
  IC_ADDR = 0x20,
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
  IC_SHL_CONST,
  IC_SHR_CONST,
  IC_POWER = 0x2F,
  IC_MUL,
  IC_DIV,
  IC_MOD,
  IC_AND,
  IC_OR,
  IC_XOR,
  IC_ADD,
  IC_SUB,
  IC_ADD_CONST,
  IC_SUB_CONST,
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
  IC_SUB_CALL,
  IC_SWITCH,
  IC_NOBOUND_SWITCH,
  IC_RET = 0x5B,
  IC_CALL = 0x5C,
  IC_CALL_INDIRECT,
  IC_ASM = 0x61,
  IC_PUSH = 0x62,
  IC_IN_U8 = 0x66,
  IC_IN_U16,
  IC_IN_U32,
  IC_OUT_U8,
  IC_OUT_U16,
  IC_OUT_U32,
  IC_GET_RFLAGS = 0x6C,
  IC_RDTSC = 0x76,
  IC_BT = 0x77,
  IC_BTS,
  IC_BTR,
  IC_BTC,
  IC_LBTS,
  IC_LBTR,
  IC_LBTC,
  IC_BSF = 0x7E,
  IC_BSR = 0x7F,
  IC_QUE_INIT = 0x80,
  IC_QUE_INS,
  IC_QUE_INS_REV,
  IC_QUE_REM,
  IC_STRLEN,
  IC_BR_ZERO = 0x85,
  IC_BR_NOT_ZERO,
  IC_SWAP_U8 = 0xA5,
  IC_SWAP_U16,
  IC_SWAP_U32,
  IC_SWAP_I64,
  IC_ABS_I64 = 0xA9,
  IC_SIGN_I64 = 0xAA,
  IC_MIN_I64 = 0xAB,
  IC_MIN_U64 = 0xAC,
  IC_MAX_I64 = 0xAD,
  IC_MAX_U64 = 0xAE,
  IC_MOD_U64 = 0xAF,
  IC_SQR_I64 = 0xB0,
  IC_SQR_U64,
  IC_SQR,
  IC_ABS,
  IC_SQRT = 0xB4,
  IC_SIN,
  IC_COS,
  IC_TAN,
  IC_ATAN,
  IC_SUB_ENTRY = 0xF0,
  IC_CALL_TRY,
  IC_CATCH_ENTRY,
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

struct COptAddress
{
  uint32_t base=NO_IC,index=NO_IC;
  int32_t disp=0;
  uint8_t shift=0;
  bool valid=false;
};

enum class COptRhsKind : uint8_t { Stack, Imm32, Reg, Frame };
struct COptRhs
{
  COptRhsKind kind=COptRhsKind::Stack;
  int32_t disp=0;
  uint8_t reg=0;
};

struct CCodeBytes
{
  uint8_t bytes[16]{};
  uint8_t count=0;
};

enum class CMemOp : uint8_t { Lea, Load, Store };

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
  const CHashFun *fun_ptr = nullptr;
  uint32_t call_target = NO_IC;
  bool cmp_link = false;
  uint8_t local_reg = 0; // Promoted IC_DEREF operand; zero uses memory.
  COptAddress address{};
  COptRhs rhs{};
  bool stack_reg=false; // The left result can remain in R9 during RHS evaluation.
  uint32_t branch_end=0;
  bool short_branch=false;
};

struct COptReg
{
  CMemberLst *m;
  int64_t offset;
  uint8_t reg;
};

struct COptRegPlan
{
  COptReg regs[4]{};
  unsigned count = 0;
};

class CAsm;

struct CCodeLabel
{
  CCodeLabel *next;
  char *name;
  uint32_t ic;
  bool defined;
};

struct CSwitchTable
{
  int64_t lo;
  uint32_t range, dft;
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
  CCodeLabel *GoToLabelFind(const char *name, bool create = false);
  bool GoToLabelsDefined() const;
  uint32_t LValue(uint32_t address, CHashClass &cls,
                  const CArrayDim *dim = nullptr,
                  const CHashFun *fun_ptr = nullptr);
  uint32_t Count() const
  {
    return used;
  }

  CIntermediateCode *code = nullptr;
  CAsm *asm_first = nullptr;
  uint32_t first = NO_IC, last = NO_IC;
  bool failed = false, has_misc_data = false;

private:
  CCodeLabel *labels = nullptr;
  uint32_t used = 0, capacity = 0;
};

struct CAOTBinBlk;
struct CAOTImportExport;

struct CAOT
{
  CAOTBinBlk *first = nullptr, *last = nullptr;
  CAOTImportExport *first_ie = nullptr, *last_ie = nullptr;
  size_t size = 0, warning_cnt = 0;
  uint8_t max_align_bits = 3;
  uint64_t org = INVALID_PTR;
  unsigned lst_col = 0;
};

struct CAsmListing
{
  const CLexFile *lst_last_lfn = nullptr;
  const void *lst_last_source = nullptr;
  size_t lst_last_line = SIZE_MAX, lst_last_rip = 0;
  unsigned lst_col = 0;
  bool lst = false;
};

struct CAsmSymbol
{
  CAsmSymbol *next;
  CAsm *block;
  char *name;
  CHashExport *entry;
  CCodeLabel *label;
  size_t offset;
  unsigned scope;
  bool exported, imported;
};

struct CAsmNum
{
  uint64_t addend = 0;
  CAsmSymbol *plus = nullptr, *minus = nullptr;
  bool c_address = false;
};

struct CAsmUnresolvedRef
{
  CAsmUnresolvedRef *next;
  CAsmNum num;
  size_t offset, line_num;
  unsigned width;
  bool relative;
};

size_t AsmOrigin(const CAsm &as);
const CHashExport *AsmExternal(const CCmpCtrl &cc, const CAOT *aot,
                               const char *name);
bool AsmResolve(CCmpCtrl &cc, CAOT *aot, CCodeCtrl *coc, size_t origin,
                const CAsmUnresolvedRef &fix, uint8_t *dest, uintptr_t base);
bool AsmUnresolvedAdd(CCmpCtrl &cc, CHeapCtrl *hc, CAsmUnresolvedRef *&fixups,
                      const CAsmNum &n, size_t offset, unsigned width,
                      bool relative, unsigned tail);

void AsmLineLst(CCmpCtrl &cc, CAsmListing &listing, size_t rip);
void AsmListCode(CAsmListing &listing, uint64_t value, unsigned width);
bool MapFileWrite(const CHashTable &table, const char *name, uint8_t drv_let);
bool AOTStore(CAOT &aot, const void *symbol, const char *name,
              const uint8_t *body, size_t size, bool main = false);
bool AOTLabel(CAOT &aot, const void *symbol, const char *name, size_t rip);
bool AOTRelAdd(CAOT &aot, size_t rip, const void *symbol, const char *name,
                uint8_t type = IET_IMM_I64);
bool IsAsmStatement(const CCmpCtrl &cc);
bool PrsAsmBlk(CCmpCtrl &cc, CAOT *aot, CCodeCtrl *coc = nullptr,
               bool bare = false);
size_t AsmLayout(CAsm &as, size_t offset);
void AsmLabelsLayout(CAsm &as);
bool AsmEmit(CAsm &as, uint8_t *buf, size_t offset);
bool AsmPublish(CAsm &as, const uint8_t *buf);
void AsmDel(CAsm *as);
void AOTRelResolve(CAOT &aot, const void *before, const void *after);

// The returned code belongs to the current task's executable heap.
// Parsing starts at cc.token and leaves the first token after the expression.
uint8_t *LexExpression2Bin(CCmpCtrl &cc, CRawType &type);
uint8_t *LexStatementExpression2Bin(CCmpCtrl &cc, CRawType &type,
                                    CAOT *aot = nullptr, size_t *size = nullptr);
uint8_t *LexTypedExpression2Bin(CCmpCtrl &cc, CRawType type,
                                bool strings = false);
uint32_t PrsExpression(CCmpCtrl &cc, CCodeCtrl &coc);
uint32_t PrsPrint(CCmpCtrl &cc, CCodeCtrl &coc);
void OptPass012(CIntermediateCode *code, uint32_t count);
void OptPass3(CIntermediateCode *code, uint32_t count, CHashFun &fun,
              COptRegPlan &plan, bool no_reg_var = false);
void OptPass4(CIntermediateCode *code,uint32_t count);
void OptPass5(CIntermediateCode *code,uint32_t count);
void OptPass6(CIntermediateCode *code,uint32_t count,CHashFun *fun,
              bool no_reg_var = false);
bool OptContextTransfer(const CIntermediateCode &node);
bool OptPass789A(CIntermediateCode *code,uint32_t count);
CCodeBytes ICModr1(CMemOp op,CRawType type,uint8_t base,
                    uint8_t index=16,uint8_t shift=0,int32_t disp=0);
CCodeBytes ICBinaryRhs(CICode op,const COptRhs &rhs);
bool OptBranch(CCodeCtrl &coc, uint32_t value, CICode branch, uint32_t target,
               unsigned depth = 0);
bool LexExpression(CCmpCtrl &cc, uint64_t &result);
uint8_t *COCCompile(CCodeCtrl &coc, uint32_t root, CRawType type,
                    CHashFun *fun = nullptr, CAOT *aot = nullptr,
                    size_t *size = nullptr);
bool LexStmt2Bin(CCmpCtrl &cc, uint8_t *&machine, CRawType &type);
bool PrsStreamBlk(CCmpCtrl &cc);
bool CmpCall(CCmpCtrl &cc, const uint8_t *machine, uint64_t &result);
bool ExeCmdLine(CCmpCtrl &cc, uint64_t &result);
struct CExeResult
{
  uint64_t value;
  int64_t exception;
  bool okay, parse_error;
};

CExeResult ExePutS(const char *src, const char *filename, int64_t flags,
                   const CLexHashTableContext *htc = nullptr);
CExeResult ExeFile(const char *name, int64_t flags = 0);
CExeResult ExeDoc(CDoc &doc, int64_t flags = 0);
CExeResult DocTreeExe(CDoc *doc, const char *path);
int64_t ExeAnswer(const CExeResult &result);
extern "C" int64_t ExeFileCpp(const char *name, int64_t flags);

bool CmpLoadDefines();
extern "C" int64_t CInitStatus;
extern "C" void CInitCpp();
