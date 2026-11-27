#include "../Kernel/KernelB.hpp"
#include "CompilerA.hpp"

enum CPrecedence : uint8_t
{
  PREC_EXP = 0x10,
  PREC_MUL = 0x14,
  PREC_AND = 0x18,
  PREC_XOR = 0x1C,
  PREC_OR = 0x20,
  PREC_ADD = 0x24,
  PREC_CMP = 0x28,
  PREC_CMP2 = 0x2C,
  PREC_AND_AND = 0x30,
  PREC_XOR_XOR = 0x34,
  PREC_OR_OR = 0x38,
  PREC_ASSIGN = 0x3C,
};

struct CBinaryOp
{
  CICode code;
  uint8_t precedence;
  bool right_assoc;
};

constexpr int64_t CH_BACKTICK = 0x60;

static CBinaryOp BinaryOp(int64_t token)
{
  switch (token) {
  case CH_BACKTICK:
    return {IC_POWER, PREC_EXP, true};
  case TK_SHL:
    return {IC_SHL, PREC_EXP, false};
  case TK_SHR:
    return {IC_SHR, PREC_EXP, false};
  case '*':
    return {IC_MUL, PREC_MUL, false};
  case '/':
    return {IC_DIV, PREC_MUL, false};
  case '%':
    return {IC_MOD, PREC_MUL, false};
  case '&':
    return {IC_AND, PREC_AND, false};
  case '^':
    return {IC_XOR, PREC_XOR, false};
  case '|':
    return {IC_OR, PREC_OR, false};
  case '+':
    return {IC_ADD, PREC_ADD, false};
  case '-':
    return {IC_SUB, PREC_ADD, false};
  case '<':
    return {IC_LESS, PREC_CMP, false};
  case '>':
    return {IC_GREATER, PREC_CMP, false};
  case TK_LESS_EQU:
    return {IC_LESS_EQU, PREC_CMP, false};
  case TK_GREATER_EQU:
    return {IC_GREATER_EQU, PREC_CMP, false};
  case TK_EQU_EQU:
    return {IC_EQU_EQU, PREC_CMP2, false};
  case TK_NOT_EQU:
    return {IC_NOT_EQU, PREC_CMP2, false};
  case TK_AND_AND:
    return {IC_AND_AND, PREC_AND_AND, false};
  case TK_XOR_XOR:
    return {IC_XOR_XOR, PREC_XOR_XOR, false};
  case TK_OR_OR:
    return {IC_OR_OR, PREC_OR_OR, false};
  case '=':
    return {IC_ASSIGN, PREC_ASSIGN, true};
  case TK_SHL_EQU:
    return {IC_SHL_EQU, PREC_ASSIGN, true};
  case TK_SHR_EQU:
    return {IC_SHR_EQU, PREC_ASSIGN, true};
  case TK_MUL_EQU:
    return {IC_MUL_EQU, PREC_ASSIGN, true};
  case TK_DIV_EQU:
    return {IC_DIV_EQU, PREC_ASSIGN, true};
  case TK_MOD_EQU:
    return {IC_MOD_EQU, PREC_ASSIGN, true};
  case TK_AND_EQU:
    return {IC_AND_EQU, PREC_ASSIGN, true};
  case TK_OR_EQU:
    return {IC_OR_EQU, PREC_ASSIGN, true};
  case TK_XOR_EQU:
    return {IC_XOR_EQU, PREC_ASSIGN, true};
  case TK_ADD_EQU:
    return {IC_ADD_EQU, PREC_ASSIGN, true};
  case TK_SUB_EQU:
    return {IC_SUB_EQU, PREC_ASSIGN, true};
  default:
    return {IC_IMM_I64, 0, false};
  }
}

constexpr unsigned PRS_STK_MAX = 127;

class CPrsStk
{
public:
  CPrsStk(CCmpCtrl &cc, CCodeCtrl &coc, const CHashFun *fun, bool strings)
      : cc(cc), coc(coc), fun(fun), strings(strings)
  {
  }

  uint32_t Parse()
  {
    auto root = Expression(PREC_ASSIGN + 1, 0);
    return cc.token == TK_ERROR ? NO_IC : root;
  }

  uint32_t PrsPrint()
  {
    const bool print = cc.token == TK_STR;
    auto *callee = reinterpret_cast<CHashFun *>(
        HashFind(print ? "Print" : "PutChars", cc.htc.hash_table_lst, HTT_FUN));
    if (!callee)
      return NO_IC;
    if (print ? !*cc.cur_str : !cc.cur_i64)
      Lex(cc);
    return PrsFunCall(*callee, 0, print ? CS_PRINT : CS_PUT_CHARS);
  }

private:
  enum CCallSyntax
  {
    CS_NORMAL,
    CS_PRINT,
    CS_PUT_CHARS,
    CS_INDIRECT,
  };

  uint32_t Add(const CIntermediateCode &entry)
  {
    return coc.Add(entry);
  }

  uint32_t Defined()
  {
    unsigned parens = 0;
    while (Lex(cc) == '(')
      ++parens;
    if (cc.token <= TK_EOF)
      return NO_IC;
    bool defined =
        cc.token == TK_IDENT && (cc.hash_entry || cc.local_var_entry);
    Lex(cc);
    while (parens--) {
      if (cc.token != ')')
        return NO_IC;
      Lex(cc);
    }
    return Add(
        {defined, NO_IC, NO_IC, IC_IMM_I64, RT_I64, false, false, false});
  }

  uint32_t PushArg(uint32_t arg, uint32_t args, CRawType type)
  {
    return Add({0, arg, args, IC_PUSH, type, false, false, false});
  }

  uint32_t PrsFunCall(const CHashFun &callee, unsigned depth,
                      CCallSyntax syntax = CS_NORMAL, uint32_t target = NO_IC)
  {
    if (callee.cls.flags & FF_INTERNAL) {
      if (syntax != CS_NORMAL || !callee.return_class || callee.arg_cnt > 2)
        return NO_IC;
      bool paren = Lex(cc) == '(';
      if (paren)
        Lex(cc);
      uint32_t operands[]{NO_IC, NO_IC};
      for (unsigned i = 0; i < callee.arg_cnt; ++i) {
        if (i && (cc.token != ',' || Lex(cc) == TK_EOF))
          return NO_IC;
        operands[i] = Expression(PREC_ASSIGN + 1, depth + 1);
        if (operands[i] == NO_IC || !TypeSize(coc.code[operands[i]].type))
          return NO_IC;
      }
      if (paren) {
        if (cc.token != ')')
          return NO_IC;
        Lex(cc);
      }
      auto opcode = reinterpret_cast<uintptr_t>(callee.exe_addr);
      return Add({0, operands[0], operands[1], static_cast<CICode>(opcode),
                  ClassRawType(*callee.return_class), false, false, true,
                  NO_IC, callee.return_class});
    }
    bool variadic = callee.cls.flags & FF_DOT_DOT_DOT;
    if ((variadic && FunPopsArgs(callee)) ||
        (target == NO_IC && !callee.exe_addr && &callee != fun &&
         !(cc.aot && (cc.flags & CCF_AOT_COMPILE) &&
           (callee.cls.flags & CF_EXTERN))) ||
        !callee.return_class ||
        ClassRawType(*callee.return_class) == RT_AGGREGATE ||
        callee.arg_cnt > UINT16_MAX / 8)
      return NO_IC;
    if (syntax == CS_NORMAL)
      Lex(cc);
    bool paren = cc.token == '(';
    if (paren)
      Lex(cc);
    uint32_t args = NO_IC;
    auto *member = callee.cls.member_lst_and_root;
    uint32_t last_arg = NO_IC;
    for (uint32_t i = 0; i < callee.arg_cnt; ++i) {
      if (!member || !member->member_class || member->dim.next ||
          (member->flags & MLF_DOT_DOT_DOT))
        return NO_IC;
      auto *cls = member->member_class;
      if (!TypeSize(ClassRawType(*cls)))
        return NO_IC;
      auto type = ClassRawType(*cls);
      if (i && paren && cc.token != ')') {
        if (cc.token != ',')
          return NO_IC;
        Lex(cc);
      }
      uint32_t arg;
      if ((member->flags & MLF_DFT_AVAILABLE) &&
          (!paren || cc.token == ',' || cc.token == ')')) {
        auto value = member->dft_val;
        if ((member->flags & MLF_LASTCLASS) && last_arg != NO_IC)
          if (auto *name = LastClass(coc.code[last_arg]))
            value = reinterpret_cast<uintptr_t>(name);
        if (value && (member->flags & (MLF_STR_DFT_AVAILABLE | MLF_LASTCLASS)) &&
            (cc.flags & CCF_AOT_COMPILE)) {
          auto *str = reinterpret_cast<const char *>(value);
          arg = StringConst(StrNew(str, CurrentTask()->data_heap),
                            StrLen(str) + 1, cls);
        } else
          arg = Add({value, NO_IC, NO_IC,
                     type == RT_F64 ? IC_IMM_F64 : IC_IMM_I64, type, false,
                     false, false});
      } else {
        arg = Expression(PREC_ASSIGN + 1, depth + 1);
        last_arg = arg;
      }
      if (arg == NO_IC || !TypeSize(coc.code[arg].type))
        return NO_IC;
      args = PushArg(arg, args, type);
      if (args == NO_IC)
        return NO_IC;
      member = member->next;
    }
    uint32_t slots = callee.arg_cnt;
    if (variadic) {
      uint32_t extra = 0;
      // Arguments are pushed in reverse order, so argc follows the fixed
      // slots in memory and precedes the inline argv.
      auto count =
          Add({0, NO_IC, NO_IC, IC_IMM_I64, RT_I64, false, false, false});
      args = PushArg(count, args, RT_I64);
      if (args == NO_IC)
        return NO_IC;
      auto end = paren ? ')' : ';';
      if (paren || syntax == CS_PRINT) {
        while (cc.token != end) {
          if (slots || extra)
            if (cc.token != ',' || Lex(cc) == end)
              return NO_IC;
          auto arg = Expression(PREC_ASSIGN + 1, depth + 1);
          if (arg == NO_IC || !TypeSize(coc.code[arg].type) ||
              extra == UINT16_MAX / 8)
            return NO_IC;
          args = PushArg(arg, args, coc.code[arg].type);
          if (args == NO_IC)
            return NO_IC;
          ++extra;
        }
      }
      coc.code[count].data = extra;
      slots += extra + 1;
    }
    if (paren) {
      if (cc.token != ')')
        return NO_IC;
      Lex(cc);
    }
    auto op = target == NO_IC ? IC_CALL : IC_CALL_INDIRECT;
    return Add({reinterpret_cast<uintptr_t>(&callee), args, slots, op,
                ClassRawType(*callee.return_class), false, false, true, NO_IC,
                callee.return_class, nullptr, nullptr, target});
  }

  bool IsLValue(uint32_t value) const
  {
    return value != NO_IC && coc.code[value].code == IC_DEREF;
  }

  const char *LastClass(const CIntermediateCode &node)
  {
    auto *cls = node.cls;
    if (cls) {
      while (cls->fwd_class)
        cls = cls->fwd_class;
      cls -= cls->ptr_stars_cnt;
      while (cls->fwd_class)
        cls = cls->fwd_class;
      return cls->sym.hash.str;
    }
    // Arithmetic and literal nodes carry their raw result type.
    const char *name;
    switch (node.type) {
    case RT_I8: name = "Bool"; break;
    case RT_U8: name = "U8"; break;
    case RT_I16: name = "I16i"; break;
    case RT_U16: name = "U16i"; break;
    case RT_I32: name = "I32i"; break;
    case RT_U32: name = "U32i"; break;
    case RT_I64: name = "I64i"; break;
    case RT_U64: name = "U64i"; break;
    case RT_F64: name = "F64"; break;
    default: return nullptr;
    }
    auto *type = HashFind(name, cc.htc.hash_table_lst, HTT_INTERNAL_TYPE);
    return type ? type->str : nullptr;
  }

  uint32_t Int(uint64_t bits)
  {
    return Add({bits, NO_IC, NO_IC, IC_IMM_I64, RT_I64, false, false, false});
  }

  uint32_t StringConst(char *text, size_t size, CHashClass *cls)
  {
    if (!text)
      return NO_IC;
    auto index = size > UINT32_MAX
                     ? NO_IC
                     : Add({reinterpret_cast<uintptr_t>(text),
                            static_cast<uint32_t>(size), 0, IC_STR_CONST,
                            RT_I64, false, false, false, NO_IC, cls});
    if (index == NO_IC)
      Free(text);
    else
      coc.has_misc_data = true;
    return index;
  }

  uint32_t TypeCast(uint32_t value, CHashClass &cls)
  {
    auto type = ClassRawType(cls);
    if (value == NO_IC || (!TypeSize(type) && !IsVoid(type)) ||
        (!TypeSize(coc.code[value].type) && !IsVoid(coc.code[value].type)))
      return NO_IC;
    return Add({0, value, NO_IC, IC_HOLYC_TYPECAST, type, false, false,
                coc.code[value].runtime, NO_IC, &cls});
  }

  const CHashFun *Callable(uint32_t value) const
  {
    const auto &node = coc.code[value];
    return !node.dim && (!node.cls || node.cls->ptr_stars_cnt == 1)
               ? node.fun_ptr
               : nullptr;
  }

  uint32_t Deref(uint32_t value)
  {
    if (value == NO_IC)
      return NO_IC;
    const auto &node = coc.code[value];
    if (auto *signature = Callable(value))
      return Add({0, value, NO_IC, IC_HOLYC_TYPECAST, RT_I64, false, false,
                  true, NO_IC, node.cls, nullptr, signature});
    auto *cls = node.cls;
    if (!cls || !cls->ptr_stars_cnt || !ClassComplete(*(cls - 1)))
      return NO_IC;
    return coc.LValue(value, *(cls - 1), node.dim ? node.dim->next : nullptr,
                      node.fun_ptr);
  }

  uint32_t Index(uint32_t base, unsigned depth)
  {
    const auto node = coc.code[base];
    auto *cls = node.cls;
    if (!cls || !cls->ptr_stars_cnt || !ClassComplete(*(cls - 1)))
      return NO_IC;
    Lex(cc);
    auto index = Expression(PREC_ASSIGN + 1, depth + 1);
    if (index == NO_IC || cc.token != ']' || !TypeSize(coc.code[index].type))
      return NO_IC;
    Lex(cc);
    if (coc.code[index].type == RT_F64)
      index = Add({0, index, NO_IC, IC_TO_I64, RT_I64, false, false, true});
    auto size = (cls - 1)->size * (node.dim ? node.dim->total_cnt : 1);
    auto offset =
        Add({0, index, Int(size), IC_MUL, RT_I64, false, false, true});
    auto address = Add({0, base, offset, IC_ADD, RT_I64, false, false, true});
    return coc.LValue(address, *(cls - 1), node.dim ? node.dim->next : nullptr,
                      node.fun_ptr);
  }

  uint32_t Member(uint32_t value)
  {
    if (cc.token == TK_DEREFERENCE)
      value = Deref(value);
    if (!IsLValue(value) || Lex(cc) != TK_IDENT)
      return NO_IC;
    const auto node = coc.code[value];
    auto *member = MemberFind(cc.cur_str, node.cls);
    if (!member)
      return NO_IC;
    Lex(cc);
    auto address = node.left;
    if (member->offset)
      address = Add({0, address, Int(member->offset), IC_ADD, RT_I64, false,
                     false, true});
    return coc.LValue(address, *member->member_class, member->dim.next,
                      FunPtrBase(member->fun_ptr));
  }

  uint32_t Cast(uint32_t value)
  {
    Lex(cc);
    auto *cls = PrsClass(cc);
    if (!cls)
      return NO_IC;
    Lex(cc);
    cls = PrsStars(cc, cls);
    if (!cls || (!cls->size && !IsVoid(ClassRawType(*cls))) || cc.token != ')')
      return NO_IC;
    Lex(cc);
    if (IsLValue(value) && !coc.code[value].grouped &&
        !IsVoid(ClassRawType(*cls)))
      return coc.LValue(coc.code[value].left, *cls);
    return TypeCast(value, *cls);
  }

  uint32_t SizeOf(bool offset)
  {
    unsigned parens = 0;
    while (Lex(cc) == '(')
      ++parens;
    if (cc.token != TK_IDENT)
      return NO_IC;
    auto *cls = PrsClass(cc);
    int64_t count = 1, member_offset = 0;
    if (cc.local_var_entry && fun) {
      cls = cc.local_var_entry->member_class;
      count = cc.local_var_entry->dim.total_cnt;
    } else if (cc.hash_entry && (cc.hash_entry->type & HTT_GLBL_VAR)) {
      auto *var = reinterpret_cast<CHashGlblVar *>(cc.hash_entry);
      cls = var->var_class;
      count = var->dim.total_cnt;
    }
    if (!cls)
      return NO_IC;
    Lex(cc);
    while (cc.token == '.') {
      if (Lex(cc) != TK_IDENT)
        return NO_IC;
      auto *member = MemberFind(cc.cur_str, cls);
      if (!member)
        return NO_IC;
      cls = member->member_class;
      count = member->dim.total_cnt;
      member_offset += member->offset;
      Lex(cc);
    }
    if (cc.token == '*') {
      cls = PrsStars(cc, cls);
      count = 1;
    }
    while (parens--) {
      if (cc.token != ')')
        return NO_IC;
      Lex(cc);
    }
    return cls ? Int(offset ? member_offset : cls->size * count) : NO_IC;
  }

  uint32_t IncDec(uint32_t value, bool inc, bool prefix)
  {
    if (!IsLValue(value))
      return NO_IC;
    const auto node = coc.code[value];
    if (!TypeSize(node.type))
      return NO_IC;
    auto step = node.cls && node.cls->ptr_stars_cnt ? (node.cls - 1)->size : 1;
    if (!step)
      return NO_IC;
    auto op = prefix ? (inc ? IC__PP : IC__MM) : (inc ? IC_PP_ : IC_MM_);
    return Add({static_cast<uint64_t>(step), value, NO_IC, op, node.type, false,
                false, true, NO_IC, node.cls});
  }

  uint32_t Term(unsigned depth)
  {
    auto value = UnaryTerm(depth);
    while (value != NO_IC && (cc.token == '[' || cc.token == '(' ||
                              cc.token == '.' || cc.token == TK_DEREFERENCE)) {
      if (cc.token == '[')
        value = Index(value, depth);
      else if (cc.token == '(') {
        if (auto *signature = Callable(value))
          value = PrsFunCall(*signature, depth, CS_INDIRECT, value);
        else
          value = Cast(value);
      } else
        value = Member(value);
    }
    if (value != NO_IC &&
        (cc.token == TK_PLUS_PLUS || cc.token == TK_MINUS_MINUS)) {
      auto token = cc.token;
      Lex(cc);
      return IncDec(value, token == TK_PLUS_PLUS, false);
    }
    return value;
  }

  uint32_t UnaryTerm(unsigned depth)
  {
    if (depth >= PRS_STK_MAX)
      return NO_IC;
    int64_t token = cc.token;
    if (token == TK_PLUS_PLUS || token == TK_MINUS_MINUS) {
      Lex(cc);
      return IncDec(Term(depth + 1), token == TK_PLUS_PLUS, true);
    }
    if (token == '&' || token == '*') {
      Lex(cc);
      if (token == '&' && cc.token == TK_IDENT && !cc.local_var_entry &&
          cc.hash_entry && (cc.hash_entry->type & HTT_FUN)) {
        auto *callee = reinterpret_cast<const CHashFun *>(cc.hash_entry);
        if (!(callee->cls.flags & FF_INTERNAL)) {
          if (!callee->exe_addr && callee != fun &&
              !(cc.flags & CCF_AOT_COMPILE))
            return NO_IC;
          Lex(cc);
          return Add({reinterpret_cast<uintptr_t>(callee), NO_IC, NO_IC,
                      IC_ADDR, RT_I64, false, false, true, NO_IC, nullptr,
                      nullptr, callee});
        }
      }
      auto value = Term(depth + 1);
      if (token == '*')
        return Deref(value);
      // Array expressions already carry the address of their first element.
      if (value != NO_IC && coc.code[value].dim)
        return value;
      if (!IsLValue(value))
        return NO_IC;
      const auto node = coc.code[value];
      if (!node.cls || node.cls->ptr_stars_cnt == PTR_STARS_NUM)
        return NO_IC;
      auto address = TypeCast(node.left, *(node.cls + 1));
      if (address != NO_IC)
        coc.code[address].fun_ptr = node.fun_ptr;
      return address;
    }
    if (token == '(') {
      Lex(cc);
      uint32_t value = Expression(PREC_ASSIGN + 1, depth + 1);
      if (value == NO_IC || cc.token != ')')
        return NO_IC;
      coc.code[value].grouped = true;
      Lex(cc);
      return value;
    }
    if (token == '+' || token == '-' || token == '!' || token == '~') {
      Lex(cc);
      uint32_t value = Term(depth + 1);
      if (value == NO_IC || token == '+')
        return value;
      // Unary minus binds after power and before shifts.
      if (token == '-' && cc.token == CH_BACKTICK)
        value = Binary(value, BinaryOp(CH_BACKTICK), depth + 1);
      if (value == NO_IC)
        return NO_IC;
      auto type = coc.code[value].type;
      if (!TypeSize(type))
        return NO_IC;
      if (token == '-' && type == RT_U64)
        type = RT_I64;
      auto op = token == '-' ? IC_UNARY_MINUS : token == '!' ? IC_NOT : IC_COM;
      return Add(
          {0, value, NO_IC, op, type, false, false, coc.code[value].runtime});
    }
    if (token == TK_I64 || token == TK_CHAR_CONST || token == TK_INS_BIN_SIZE) {
      auto bits = static_cast<uint64_t>(cc.cur_i64);
      auto type = cc.cur_i64 < 0 ? RT_U64 : RT_I64;
      Lex(cc);
      return Add({bits, NO_IC, NO_IC, IC_IMM_I64, type, false, false, false});
    }
    if (token == TK_F64) {
      auto bits = cc.cur_f64;
      Lex(cc);
      return Add({bits, NO_IC, NO_IC, IC_IMM_F64, RT_F64, false, false, false});
    }
    if ((token == TK_STR || token == TK_INS_BIN) && strings) {
      size_t size = 0;
      auto *byte = reinterpret_cast<CHashClass *>(
          HashFind("U8", cc.htc.hash_table_lst, HTT_INTERNAL_TYPE));
      if (!byte)
        return NO_IC;
      if (token == TK_INS_BIN) {
        auto *data = const_cast<char *>(cc.cur_str);
        size = cc.cur_str_len;
        cc.cur_str = nullptr;
        cc.cur_str_len = 0;
        Lex(cc);
        return StringConst(data, size, byte + 1);
      }
      auto *text = LexExtStr(cc, size);
      return StringConst(text, size, byte + 1);
    }
    if (token == TK_IDENT && cc.local_var_entry) {
      const auto *member = cc.local_var_entry;
      bool fixed = member->flags & MLF_STATIC;
      if ((fixed && !member->static_data) || (!fixed && !fun))
        return NO_IC;
      Lex(cc);
      auto address = fixed
                         ? Add({reinterpret_cast<uintptr_t>(member->static_data), NO_IC,
                                NO_IC, IC_DATA_ADDR, RT_I64, false, false, true})
                         : Add({static_cast<uint64_t>(member->offset), NO_IC,
                                NO_IC, IC_RBP, RT_I64, false, false, true});
      return coc.LValue(address, *member->member_class, member->dim.next,
                        FunPtrBase(member->fun_ptr));
    }
    if (token == TK_IDENT && cc.hash_entry) {
      auto *hash = cc.hash_entry;
      if (hash->type & HTT_KEYWORD) {
        auto keyword = reinterpret_cast<CHashGeneric *>(hash)->user_data0;
        if (keyword == KW_DEFINED)
          return Defined();
        if (keyword == KW_SIZEOF || keyword == KW_OFFSET)
          return SizeOf(keyword == KW_OFFSET);
      }
      if (hash->type & HTT_EXPORT_SYS_SYM) {
        auto *sym = reinterpret_cast<const CHashExport *>(hash);
        if (hash->type & (HTF_IMPORT | HTF_UNRESOLVED))
          return NO_IC;
        bool reloc = cc.aot && (cc.flags & CCF_AOT_COMPILE) &&
                     !(hash->type & HTF_IMM);
        Lex(cc);
        return Add({reloc ? reinterpret_cast<uintptr_t>(sym) : sym->val,
                    NO_IC, NO_IC, reloc ? IC_ASM_ADDR : IC_IMM_I64,
                    RT_I64, false, false, reloc});
      }
      if (hash->type & HTT_FUN)
        return PrsFunCall(*reinterpret_cast<CHashFun *>(hash), depth);
      if (hash->type & HTT_GLBL_VAR) {
        auto *var = reinterpret_cast<CHashGlblVar *>(hash);
        auto *cls = var->var_class;
        bool aot = cc.aot && (cc.flags & CCF_AOT_COMPILE);
        if (cls && TypeSize(static_cast<CRawType>(cls->raw_type)) &&
            (var->data_addr ||
             (aot ? var->flags & GVF_IMPORT : var->flags & GVF_EXTERN))) {
          Lex(cc);
          auto address = Add({reinterpret_cast<uintptr_t>(var), NO_IC, NO_IC,
                              IC_GLBL_ADDR, RT_I64, false, false, true});
          return coc.LValue(address, *cls, var->dim.next,
                            FunPtrBase(var->fun_ptr));
        }
      }
    }
    return NO_IC;
  }

  uint32_t Binary(uint32_t left, CBinaryOp op, unsigned depth)
  {
    Lex(cc);
    uint32_t right = Expression(op.precedence, depth + 1);
    if (right == NO_IC)
      return NO_IC;
    if (!TypeSize(coc.code[left].type) || !TypeSize(coc.code[right].type))
      return NO_IC;
    if (op.code == IC_ASSIGN || ICIsCompoundAssign(op.code)) {
      if (!IsLValue(left))
        return NO_IC;
      auto *cls = coc.code[left].cls;
      if ((op.code == IC_ADD_EQU || op.code == IC_SUB_EQU) && cls &&
          cls->ptr_stars_cnt && (cls - 1)->size != 1) {
        auto scale = Int((cls - 1)->size);
        auto type = coc.code[right].type == RT_F64 ? RT_F64 : RT_I64;
        right = Add({0, right, scale, IC_MUL, type, false, false, true});
        if (right == NO_IC)
          return NO_IC;
      }
      return Add({0, left, right, op.code, coc.code[left].type, false, false,
                  true, NO_IC, cls});
    }
    bool comparison = ICIsComparison(op.code);
    bool chain = comparison && ICIsComparison(coc.code[left].code) &&
                 !coc.code[left].grouped;
    auto type1 = coc.code[left].type, type2 = coc.code[right].type;
    auto type = type1 > type2 ? type1 : type2;
    auto *cls = coc.code[left].cls;
    if ((op.code == IC_ADD || op.code == IC_SUB) && cls && cls->ptr_stars_cnt &&
        IsInt(type2)) {
      auto step = (cls - 1)->size;
      auto *right_cls = coc.code[right].cls;
      if (right_cls && right_cls->ptr_stars_cnt) {
        if (op.code == IC_SUB && step > 1) {
          auto bytes =
              Add({0, left, right, IC_SUB, RT_I64, false, false, true});
          return Add({0, bytes, Int(step), IC_DIV, RT_I64, false, false, true});
        }
        cls = nullptr;
      } else if (step != 1) {
        right = Add({0, right, Int(step), IC_MUL, RT_I64, false, false, true});
      }
      return Add(
          {0, left, right, op.code, RT_I64, false, false, true, NO_IC, cls});
    }
    if (comparison || op.code == IC_AND_AND || op.code == IC_OR_OR ||
        op.code == IC_XOR_XOR)
      type = RT_I64;
    else if (op.code == IC_POWER)
      type = RT_F64;
    return Add({0, left, right, op.code, type, chain, false,
                coc.code[left].runtime || coc.code[right].runtime ||
                    op.code == IC_POWER});
  }

  uint32_t Expression(unsigned max_prec, unsigned depth)
  {
    auto left = Term(depth);
    while (left != NO_IC) {
      auto op = BinaryOp(cc.token);
      if (!op.precedence || op.precedence > max_prec ||
          (op.precedence == max_prec && !op.right_assoc))
        break;
      left = Binary(left, op, depth);
    }
    if (cc.token == '(' || cc.token == '[' || cc.token == '.' ||
        cc.token == TK_DEREFERENCE || cc.token == TK_PLUS_PLUS ||
        cc.token == TK_MINUS_MINUS)
      return NO_IC;
    return left;
  }

  CCmpCtrl &cc;
  CCodeCtrl &coc;
  const CHashFun *fun;
  bool strings;
};

uint32_t PrsExpression(CCmpCtrl &cc, CCodeCtrl &coc)
{
  CPrsStk ps(cc, coc, cc.htc.fun, true);
  return ps.Parse();
}

uint32_t PrsPrint(CCmpCtrl &cc, CCodeCtrl &coc)
{
  CPrsStk ps(cc, coc, cc.htc.fun, true);
  return ps.PrsPrint();
}

static uint8_t *LexExpressionBin(CCmpCtrl &cc, CRawType &type, bool convert,
                                 bool statement = false, bool strings = false,
                                 CAOT *aot = nullptr, size_t *size = nullptr)
{
  CCodeCtrl coc;
  CPrsStk ps(cc, coc, nullptr, statement || strings);
  auto root = ps.Parse();
  if (root == NO_IC || coc.code[root].type == RT_AGGREGATE)
    return nullptr;
  if ((cc.flags & CCF_AOT_COMPILE) && !aot &&
      (coc.code[root].runtime || coc.has_misc_data))
    return nullptr;
  if (!convert) {
    type = coc.code[root].type;
    // LexStmt2Bin reports narrow values and standalone unsigned calls as I64.
    if (statement && ((IsInt(type) && TypeSize(type) < 8) ||
                      (coc.code[root].code == IC_CALL && type == RT_U64)))
      type = RT_I64;
  }
  auto *machine = COCCompile(coc, root, type, nullptr, aot, size);
  if (machine && coc.has_misc_data)
    cc.flags |= CCF_HAS_MISC_DATA;
  return machine;
}

uint8_t *LexExpression2Bin(CCmpCtrl &cc, CRawType &type)
{
  return LexExpressionBin(cc, type, false);
}

uint8_t *LexStatementExpression2Bin(CCmpCtrl &cc, CRawType &type,
                                    CAOT *aot, size_t *size)
{
  return LexExpressionBin(cc, type, false, true, false, aot, size);
}

uint8_t *LexTypedExpression2Bin(CCmpCtrl &cc, CRawType type, bool strings)
{
  return LexExpressionBin(cc, type, true, false, strings);
}

bool LexExpression(CCmpCtrl &cc, uint64_t &result)
{
  CRawType type;
  auto *machine_code = LexExpression2Bin(cc, type);
  if (!machine_code)
    return false;
  auto okay = CmpCall(cc, machine_code, result);
  Free(machine_code);
  return okay;
}
