#include "../Kernel/KernelB.hpp"
#include "CompilerA.hpp"

#include <new>

extern "C" void SysSymImportsResolveCpp(const char *name, int64_t flags);

static bool Same(const char *a, const char *b)
{
  return !StrCmpCpp(reinterpret_cast<const uint8_t *>(a),
                    reinterpret_cast<const uint8_t *>(b));
}

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
};

struct CAsmUnresolvedRef
{
  CAsmUnresolvedRef *next;
  CAsmNum num;
  size_t offset;
  unsigned width;
  bool relative, signed_only;
};

struct CAsmArg
{
  CAsmNum num;
  unsigned width = 0;
  int reg = -1;
  bool mem = false, high = false, rex_byte = false;
};

struct CAsmOp
{
  const char *name;
  uint8_t opcode, ext;
};

static constexpr CAsmOp fixed_ops[]{
    {"NOP", 0x90, 0},          {"RET", 0xC3, 0},      {"LEAVE", 0xC9, 0},
    {"CLD", 0xFC, 0},          {"STD", 0xFD, 0},      {"CLI", 0xFA, 0},
    {"STI", 0xFB, 0},          {"HLT", 0xF4, 0},      {"CLC", 0xF8, 0},
    {"STC", 0xF9, 0},          {"CMC", 0xF5, 0},      {"LODSB", 0xAC, 0},
    {"MOVSB", 0xA4, 0},        {"STOSB", 0xAA, 0},    {"REP_MOVSB", 0xA4, 0xF3},
    {"REP_STOSB", 0xAA, 0xF3}, {"CPUID", 0xA2, 0x0F},
};

static constexpr CAsmOp branch_ops[]{
    {"CALL", 0xE8, 0},   {"JMP", 0xE9, 0},    {"LOOP", 0xE2, 1},
    {"JRCXZ", 0xE3, 1},  {"JO", 0x80, 0x0F},  {"JNO", 0x81, 0x0F},
    {"JB", 0x82, 0x0F},  {"JC", 0x82, 0x0F},  {"JAE", 0x83, 0x0F},
    {"JNC", 0x83, 0x0F}, {"JZ", 0x84, 0x0F},  {"JE", 0x84, 0x0F},
    {"JNZ", 0x85, 0x0F}, {"JNE", 0x85, 0x0F}, {"JBE", 0x86, 0x0F},
    {"JA", 0x87, 0x0F},  {"JS", 0x88, 0x0F},  {"JNS", 0x89, 0x0F},
    {"JL", 0x8C, 0x0F},  {"JGE", 0x8D, 0x0F}, {"JLE", 0x8E, 0x0F},
    {"JG", 0x8F, 0x0F},
};

static constexpr CAsmOp unary_ops[]{
    {"INC", 0xFE, 0},  {"DEC", 0xFE, 1},  {"NOT", 0xF6, 2},   {"NEG", 0xF6, 3},
    {"MUL", 0xF6, 4},  {"IMUL", 0xF6, 5}, {"IMUL1", 0xF6, 5}, {"DIV", 0xF6, 6},
    {"IDIV", 0xF6, 7}, {"ROL1", 0xD0, 0}, {"ROR1", 0xD0, 1},  {"RCL1", 0xD0, 2},
    {"RCR1", 0xD0, 3}, {"SHL1", 0xD0, 4}, {"SHR1", 0xD0, 5},  {"SAR1", 0xD0, 7},
};

static constexpr CAsmOp shift_ops[]{
    {"ROL", 0, 0}, {"ROR", 0, 1}, {"RCL", 0, 2}, {"RCR", 0, 3},
    {"SHL", 0, 4}, {"SHR", 0, 5}, {"SAR", 0, 7},
};

static constexpr CAsmOp binary_ops[]{
    {"ADD", 0, 0}, {"OR", 0, 1},  {"ADC", 0, 2}, {"SBB", 0, 3},
    {"AND", 0, 4}, {"SUB", 0, 5}, {"XOR", 0, 6}, {"CMP", 0, 7},
};

template <size_t N>
static const CAsmOp *AsmOpFind(const CAsmOp (&ops)[N], const char *name)
{
  for (const auto &op : ops)
    if (Same(name, op.name))
      return &op;
  return nullptr;
}

bool IsAsmStatement(const CCmpCtrl &cc)
{
  if (cc.token != TK_IDENT || cc.local_var_entry)
    return false;
  const auto *name = cc.cur_str;
  if (name[0] == '@' && name[1] == '@')
    return true;
  if (cc.hash_entry &&
      (cc.hash_entry->type & (HTT_FUN | HTT_GLBL_VAR | HTT_EXPORT_SYS_SYM)))
    return false;
  if (AsmOpFind(fixed_ops, name) || AsmOpFind(branch_ops, name) ||
      AsmOpFind(unary_ops, name) || AsmOpFind(shift_ops, name) ||
      AsmOpFind(binary_ops, name))
    return true;
  constexpr const char *names[]{
      "RET1",  "INT",   "PUSH",  "POP",   "LEA",   "MOV",    "MOVZX",
      "MOVSX", "TEST",  "IMUL",  "IMUL2", "DU8",   "DU16",   "DU32",
      "DU64",  "USE16", "USE32", "USE64", "ALIGN", "IMPORT",
  };
  for (const char *candidate : names)
    if (Same(name, candidate))
      return true;
  return false;
}

class CAsm
{
public:
  CAsm(CCmpCtrl &cc, CAOT *aot, CCodeCtrl *coc = nullptr)
      : cc(cc), aot(aot), coc(coc), hc(CurrentTask()->data_heap),
        root(coc && coc->asm_first ? coc->asm_first : this),
        origin(aot && !coc ? aot->size : 0)
  {
  }

  ~CAsm()
  {
    while (symbols) {
      auto *sym = symbols;
      symbols = sym->next;
      if (sym->entry)
        HashDel(&sym->entry->sym.hash);
      Free(sym->name);
      Free(sym);
    }
    while (fixups) {
      auto *fix = fixups;
      fixups = fix->next;
      Free(fix);
    }
    Free(buf);
  }

  bool Parse(bool bare)
  {
    if (!bare)
      Lex(cc); // Skip '{'.
    while (!failed && cc.token > TK_EOF && cc.token != '}') {
      if (cc.token == '\n' || (!bare && cc.token == ';')) {
        Lex(cc);
        continue;
      }
      if (bare && !IsAsmStatement(cc))
        break;
      if (cc.token != TK_IDENT)
        return false;
      auto *name = const_cast<char *>(cc.cur_str);
      cc.cur_str = nullptr;
      Lex(cc);
      bool okay = cc.token == ':' || cc.token == TK_DBL_COLON
                      ? Label(name)
                      : Statement(name);
      Free(name);
      if (!okay)
        return false;
    }
    return !failed && (bare ? cc.token != TK_ERROR : cc.token == '}');
  }

  size_t Layout(size_t offset)
  {
    origin = offset + (aot ? aot->size : 0);
    return size;
  }

  void LabelsLayout()
  {
    size_t base = aot ? aot->size : 0;
    for (auto *s = symbols; s; s = s->next)
      if (s->label)
        coc->code[s->label->ic].data = s->block->origin - base + s->offset;
  }

  bool Emit(uint8_t *image, size_t offset)
  {
    if (!size)
      return true;
    auto *dest = image + offset;
    if (dest != buf)
      MemCpy(dest, buf, size);
    auto base = aot ? 0 : reinterpret_cast<uintptr_t>(image);
    for (auto *fix = fixups; fix; fix = fix->next)
      if (!AsmResolve(*fix, dest, base))
        return false;
    return true;
  }

  bool Publish(const uint8_t *image)
  {
    auto base = aot ? 0 : reinterpret_cast<uintptr_t>(image);
    if (aot)
      for (auto *s = symbols; s; s = s->next)
        if (s->entry &&
            !AOTLabel(*aot, s->entry,
                      s->exported ? s->entry->sym.hash.str : nullptr,
                      s->block->origin + s->offset))
          return false;
    for (auto *s = symbols; s; s = s->next) {
      if (!s->entry)
        continue;
      auto &sym = s->entry->sym;
      s->entry->val = base + s->block->origin + s->offset;
      if (!aot)
        sym.hash.type |= HTF_IMM;
      HashAdd(&sym.hash, cc.htc.glbl_hash_table);
      if (!aot)
        SysSymImportsResolveCpp(sym.hash.str, 0);
      s->entry = nullptr;
    }
    return true;
  }

  bool Finish()
  {
    if (aot)
      return Emit(buf, 0) &&
             (!size || AOTStore(*aot, nullptr, nullptr, buf, size)) &&
             Publish(nullptr);
    auto *image = size ? MAlloc(size, CurrentTask()->code_heap) : buf;
    if ((size && !image) || !Emit(image, 0) || !Publish(image)) {
      if (size)
        Free(image);
      return false;
    }
    // The task's labels retain the assembled code.
    bool keep = false;
    for (auto *s = symbols; s; s = s->next)
      keep |= s->block && !s->scope;
    if (size && !keep)
      Free(image);
    return true;
  }

private:
  bool Eat(int64_t t)
  {
    return cc.token == t && Lex(cc) != TK_ERROR;
  }

  bool Ident(const char *text) const
  {
    return cc.token == TK_IDENT && Same(cc.cur_str, text);
  }

  CAsmSymbol *Symbol(const char *name)
  {
    unsigned group = name[0] == '@' && name[1] == '@' ? scope : 0;
    for (auto *s = root->symbols; s; s = s->next)
      if (s->scope == group && Same(s->name, name))
        return s;
    auto *s = reinterpret_cast<CAsmSymbol *>(CAlloc(sizeof(CAsmSymbol), hc));
    if (!s)
      return nullptr;
    s->name = StrNew(name, hc);
    if (!s->name) {
      Free(s);
      return nullptr;
    }
    s->scope = group;
    s->next = root->symbols;
    root->symbols = s;
    return s;
  }

  bool Label(const char *name)
  {
    bool exported = cc.token == TK_DBL_COLON;
    auto *s = Symbol(name);
    if (!s || s->imported ||
        (s->block && (s->block != this || s->offset != size)) ||
        (s->scope && exported))
      return false;
    s->block = this;
    s->exported |= exported;
    s->offset = size;
    if (coc) {
      auto *label = coc->GoToLabelFind(name, true);
      if (!label || label->defined)
        return false;
      label->defined = true;
      s->label = label;
    }
    if (!s->scope && !s->entry && (!coc || exported)) {
      if (HashSingleTableFind(name, cc.htc.glbl_hash_table, HTT_EXPORT_SYS_SYM))
        return false;
      s->entry = reinterpret_cast<CHashExport *>(
          CAlloc(sizeof(CHashExport), hc));
      auto *key = s->entry ? StrNew(name, hc) : nullptr;
      if (!key)
        return false;
      auto &sym = s->entry->sym;
      sym.hash.str = key;
      sym.hash.type = HTT_EXPORT_SYS_SYM | (exported ? HTF_PUBLIC : 0);
      if (exported)
        HashSrcFileSetCpp(&cc, &sym, 0);
    }
    if (exported && !coc)
      ++scope;
    return Eat(cc.token);
  }

  static unsigned Precedence(int64_t token)
  {
    switch (token) {
    case '|': return 1;
    case '^': return 2;
    case '&': return 3;
    case '+': case '-': return 4;
    case '*': case '/': case '%': return 5;
    case TK_SHL: case TK_SHR: return 6;
    default: return 0;
    }
  }

  static bool Add(CAsmNum &n, CAsmSymbol *sym, bool positive)
  {
    if (!sym)
      return true;
    auto *&keep = positive ? n.plus : n.minus;
    auto *&cancel = positive ? n.minus : n.plus;
    if (cancel == sym)
      cancel = nullptr;
    else if (keep)
      return false;
    else
      keep = sym;
    return true;
  }

  static bool Apply(CAsmNum &n, const CAsmNum &rhs, int64_t op)
  {
    auto v = rhs.addend;
    if (op == '+' || op == '-') {
      n.addend += op == '+' ? v : -v;
      return Add(n, rhs.plus, op == '+') && Add(n, rhs.minus, op == '-');
    }
    if (n.plus || n.minus || rhs.plus || rhs.minus)
      return false;
    switch (op) {
    case '*': n.addend *= v; break;
    case '/': if (!v) return false; n.addend /= v; break;
    case '%': if (!v) return false; n.addend %= v; break;
    case '&': n.addend &= v; break;
    case '|': n.addend |= v; break;
    case '^': n.addend ^= v; break;
    case TK_SHL: n.addend <<= (v & 63); break;
    case TK_SHR: n.addend = int64_t(n.addend) >> (v & 63); break;
    default: return false;
    }
    return true;
  }

  bool Expression(CAsmNum &n, unsigned prec = 1, unsigned depth = 0)
  {
    if (depth == 64)
      return false;
    if (cc.token == '+' || cc.token == '-') {
      bool negate = cc.token == '-';
      Lex(cc);
      if (!Expression(n, 7, depth + 1))
        return false;
      if (negate) {
        n.addend = -n.addend;
        auto *s = n.plus;
        n.plus = n.minus;
        n.minus = s;
      }
    } else if (cc.token == '&') {
      if (Lex(cc) != TK_IDENT)
        return false;
      if (auto *member = cc.local_var_entry) {
        if (member->flags & MLF_STATIC)
          return false;
        n.addend = member->offset;
      } else if (!(n.plus = Symbol(cc.cur_str)))
        return false;
      Lex(cc);
    } else if (cc.token == '(') {
      Lex(cc);
      if (!Expression(n, 1, depth + 1) || !Eat(')'))
        return false;
    } else if (cc.token == TK_I64 || cc.token == TK_CHAR_CONST) {
      n.addend = cc.cur_i64;
      Lex(cc);
    } else if (cc.token == TK_IDENT) {
      if (cc.local_var_entry)
        return false;
      auto *entry = cc.hash_entry;
      if (entry && (entry->type & HTT_GLBL_VAR) && !aot)
        return LexExpression(cc, n.addend);
      if (entry && (entry->type & HTT_EXPORT_SYS_SYM) &&
          (entry->type & HTF_IMM) &&
          !(entry->type & (HTF_IMPORT | HTF_UNRESOLVED)))
        n.addend = reinterpret_cast<const CHashExport *>(entry)->val;
      else if (!(n.plus = Symbol(cc.cur_str)))
        return false;
      Lex(cc);
    } else
      return false;
    for (unsigned p; (p = Precedence(cc.token)) >= prec;) {
      auto op = cc.token;
      Lex(cc);
      CAsmNum rhs;
      if (!Expression(rhs, p + 1, depth + 1) || !Apply(n, rhs, op))
        return false;
    }
    return !failed && cc.token != TK_ERROR;
  }

  static bool Constant(const CAsmNum &n)
  {
    return !n.plus && !n.minus;
  }

  bool ReadReg(CAsmArg &arg)
  {
    if (cc.token != TK_IDENT)
      return false;
    static constexpr const char *regs[4][16] = {
      {"AL","CL","DL","BL","AH","CH","DH","BH","R8u8","R9u8","R10u8","R11u8",
       "R12u8","R13u8","R14u8","R15u8"},
      {"AX","CX","DX","BX","SP","BP","SI","DI","R8u16","R9u16","R10u16",
       "R11u16","R12u16","R13u16","R14u16","R15u16"},
      {"EAX","ECX","EDX","EBX","ESP","EBP","ESI","EDI","R8u32","R9u32",
       "R10u32","R11u32","R12u32","R13u32","R14u32","R15u32"},
      {"RAX","RCX","RDX","RBX","RSP","RBP","RSI","RDI","R8","R9","R10","R11",
       "R12","R13","R14","R15"},
    };
    for (unsigned w = 0; w < 4; ++w)
      for (unsigned r = 0; r < 16; ++r)
        if (Ident(regs[w][r])) {
          arg.reg = r;
          arg.width = 1U << w;
          arg.high = !w && 4 <= r && r < 8;
          Lex(cc);
          return true;
        }
    constexpr const char *low[]{"RSPu8","RBPu8","RSIu8","RDIu8"};
    for (unsigned r = 0; r < 4; ++r)
      if (Ident(low[r])) {
        arg.reg = r + 4;
        arg.width = 1;
        arg.rex_byte = true;
        Lex(cc);
        return true;
      }
    return false;
  }

  bool PrsAsmArg(CAsmArg &arg)
  {
    if (ReadReg(arg))
      return true;
    constexpr const char *types[]{"U8","U16","U32","U64"};
    for (unsigned w = 0; w < 4; ++w)
      if (Ident(types[w])) {
        arg.width = 1U << w;
        Lex(cc);
        break;
      }
    if (cc.token != '[' && !Expression(arg.num))
      return false;
    if (cc.token != '[')
      return !arg.width;
    arg.mem = true;
    Lex(cc);
    CAsmArg base;
    CAsmNum disp;
    if (ReadReg(base)) {
      if (base.width != mode / 8)
        return false;
      arg.reg = base.reg;
      if (cc.token != ']' && !Expression(disp))
        return false;
    } else if (!Expression(disp))
      return false;
    return Apply(arg.num, disp, '+') && Eat(']');
  }

  void Byte(uint8_t byte)
  {
    if (failed)
      return;
    if (size == capacity) {
      size_t next = capacity ? 2 * capacity : 256;
      if (next > INT32_MAX - origin) {
        failed = true;
        return;
      }
      auto *ptr = MAlloc(next, hc);
      if (!ptr) {
        failed = true;
        return;
      }
      if (size)
        MemCpy(ptr, buf, size);
      Free(buf);
      buf = ptr;
      capacity = next;
    }
    buf[size++] = byte;
  }

  void Scalar(uint64_t value, unsigned width)
  {
    for (unsigned i = 0; i < width; ++i) {
      Byte(value);
      value >>= 8;
    }
  }

  void AsmStoreNum(const CAsmNum &n, unsigned width, bool relative = false,
                    bool signed_only = false)
  {
    if (!relative && Constant(n)) {
      Scalar(n.addend, width);
      return;
    }
    auto *fix = reinterpret_cast<CAsmUnresolvedRef *>(
        MAlloc(sizeof(CAsmUnresolvedRef), hc));
    if (!fix) {
      failed = true;
      return;
    }
    *fix = {fixups, n, size, width, relative, signed_only};
    fixups = fix;
    Scalar(0, width);
  }

  bool PrsAsmDefine(unsigned width)
  {
    do {
      if (cc.token == TK_STR) {
        for (int64_t i = 0; i < cc.cur_str_len - 1; ++i)
          Byte(cc.cur_str[i]);
        Lex(cc);
      } else {
        CAsmNum n;
        if (!Expression(n))
          return false;
        if (Ident("DUP")) {
          Lex(cc);
          CAsmNum value;
          if (!Constant(n) || n.addend > (INT32_MAX - size - origin) / width ||
              !Eat('(') || !Expression(value) || !Eat(')'))
            return false;
          for (uint64_t i = 0; i < n.addend && !failed; ++i)
            AsmStoreNum(value, width);
        } else
          AsmStoreNum(n, width);
      }
    } while (!failed && cc.token == ',' && Eat(','));
    return !failed && Eat(';');
  }

  bool Prefix(unsigned width, const CAsmArg &r, const CAsmArg &rm)
  {
    if (!width || width > 8 ||
        (mode != 64 && (width == 8 || r.reg >= 8 || rm.reg >= 8 ||
                        r.rex_byte || rm.rex_byte)))
      return false;
    if ((width == 2 && mode != 16) || (width == 4 && mode == 16))
      Byte(0x66);
    if (mode == 64) {
      unsigned rex = (width == 8 ? 8 : 0) | (r.reg >= 8 ? 4 : 0) |
                     (rm.reg >= 8 ? 1 : 0);
      if (rex || r.rex_byte || rm.rex_byte) {
        if (r.high || rm.high)
          return false;
        Byte(0x40 | rex);
      }
    }
    return true;
  }

  static bool I8(uint64_t n)
  {
    return int64_t(n) >= -128 && int64_t(n) <= 127;
  }

  bool ModRM(unsigned reg, const CAsmArg &rm, unsigned tail)
  {
    reg = (reg & 7) << 3;
    if (!rm.mem) {
      Byte(0xC0 | reg | (rm.reg & 7));
      return rm.reg >= 0;
    }
    auto &disp = rm.num;
    if (rm.reg < 0) {
      if (mode == 16) {
        Byte(reg | 6);
        AsmStoreNum(disp, 2);
      } else if (mode == 64 && disp.plus && !disp.minus) {
        Byte(reg | 5);
        auto relative = disp;
        relative.addend -= tail;
        AsmStoreNum(relative, 4, true);
      } else {
        Byte(reg | (mode == 64 ? 4 : 5));
        if (mode == 64)
          Byte(0x25);
        AsmStoreNum(disp, 4);
      }
      return true;
    }
    int base = rm.reg & 7;
    if (mode == 16) {
      switch (base) {
      case 3: base=7; break; // BX
      case 5: base=6; break; // BP
      case 6: base=4; break; // SI
      case 7: base=5; break; // DI
      default: return false;
      }
    }
    bool known = Constant(disp);
    unsigned special = mode == 16 ? 6 : 5;
    unsigned mod = known && !disp.addend && unsigned(base) != special ? 0
                   : known && I8(disp.addend) ? 1 : 2;
    Byte((mod << 6) | reg | base);
    if (mode != 16 && base == 4)
      Byte(0x24);
    if (mod)
      AsmStoreNum(disp, mod == 1 ? 1 : mode == 16 ? 2 : 4);
    return true;
  }

  bool RMInst(uint16_t opcode, unsigned width, const CAsmArg &rm, int ext,
              const CAsmArg &reg = {}, unsigned tail = 0)
  {
    if (rm.reg < 0 && !rm.mem)
      return false;
    if (!Prefix(width, reg, rm))
      return false;
    if (opcode > 0xFF)
      Byte(opcode >> 8);
    Byte(opcode);
    return ModRM(ext, rm, tail);
  }

  bool Binary(const char *name, const CAsmArg &a, const CAsmArg &b)
  {
    unsigned width = a.width ? a.width : b.width;
    if (!width || (a.width && b.width && a.width != b.width) ||
        (a.mem && b.mem))
      return false;
    bool wide = width != 1;
    bool mov = Same(name, "MOV");
    if (!b.mem && b.reg < 0) {
      if (mov && !a.mem) {
        if (a.reg < 0 || !Prefix(width, {}, a))
          return false;
        Byte((wide ? 0xB8 : 0xB0) | (a.reg & 7));
        AsmStoreNum(b.num, width);
        return true;
      }
      unsigned group = 0xFF;
      if (auto *op = AsmOpFind(binary_ops, name))
        group = op->ext;
      bool test = Same(name,"TEST");
      if (group == 0xFF && !mov && !test)
        return false;
      bool small = wide && !mov && !test && Constant(b.num) && I8(b.num.addend);
      unsigned opcode = mov ? 0xC6 + wide : test ? 0xF6 + wide
                             : small ? 0x83 : 0x80 + wide;
      unsigned imm = small ? 1 : width > 4 ? 4 : width;
      if (!RMInst(opcode,width,a,(mov || test) ? 0 : group,{},imm))
        return false;
      AsmStoreNum(b.num,imm);
      return true;
    }

    bool test = Same(name,"TEST");
    unsigned opcode = mov ? 0x88 : test ? 0x84 : 0xFF;
    if (auto *op = AsmOpFind(binary_ops, name))
      opcode = op->ext * 8;
    if (opcode == 0xFF)
      return false;
    if (b.mem)
      return !test && a.reg >= 0 && RMInst(opcode+2+wide,width,b,a.reg,a);
    return RMInst(opcode+wide,width,a,b.reg,b);
  }

  bool PrsAsmInst(const char *name)
  {
    for (auto &op : fixed_ops)
      if (Same(name,op.name)) {
        if (op.ext)
          Byte(op.ext);
        Byte(op.opcode);
        return true;
      }
    CAsmArg a,b;
    if (!PrsAsmArg(a))
      return false;
    for (auto &op : branch_ops)
      if (Same(name,op.name)) {
        if (a.mem || a.reg >= 0) {
          if (op.opcode != 0xE8 && op.opcode != 0xE9)
            return false;
          return RMInst(0xFF,a.width ? a.width : mode/8,a,
                        op.opcode == 0xE8 ? 2 : 4);
        }
        if (op.ext == 0x0F)
          Byte(op.ext);
        Byte(op.opcode);
        AsmStoreNum(a.num, op.ext == 1 ? 1 : mode == 16 ? 2 : 4, true, true);
        return true;
      }
    if (Same(name,"RET1") || Same(name,"INT")) {
      bool ret = Same(name,"RET1");
      if (a.mem || a.reg >= 0 || !Constant(a.num) ||
          a.num.addend > (ret ? 65535U : 255U))
        return false;
      Byte(ret ? 0xC2 : 0xCD);
      Scalar(a.num.addend, ret ? 2 : 1);
      return true;
    }
    if (Same(name,"PUSH") || Same(name,"POP")) {
      bool pop = Same(name,"POP");
      if (a.mem || (mode != 64 && a.reg >= 8))
        return false;
      if (a.reg >= 0) {
        if (a.width != mode / 8 && a.width != 2)
          return false;
        if (a.width == 2 && mode != 16)
          Byte(0x66);
        if (a.reg >= 8)
          Byte(0x41);
        Byte((pop ? 0x58 : 0x50) | (a.reg & 7));
        return true;
      }
      if (pop)
        return false;
      bool small = Constant(a.num) && I8(a.num.addend);
      Byte(small ? 0x6A : 0x68);
      AsmStoreNum(a.num, small ? 1 : mode == 16 ? 2 : 4);
      return true;
    }

    for (auto &op : unary_ops)
      if (Same(name,op.name))
        return RMInst(op.opcode + (a.width != 1),a.width,a,op.ext);
    if (!Eat(',') || !PrsAsmArg(b))
      return false;
    if (Same(name, "IMUL2")) {
      if (a.reg < 0 || a.mem || a.width < 2)
        return false;
      if (b.reg >= 0 || b.mem)
        return (!b.width || b.width == a.width) &&
               RMInst(0x0FAF, a.width, b, a.reg, a);
      bool small = Constant(b.num) && I8(b.num.addend);
      if (!RMInst(small ? 0x6B : 0x69, a.width, a, a.reg, a))
        return false;
      AsmStoreNum(b.num, small ? 1 : a.width == 2 ? 2 : 4);
      return true;
    }
    if (Same(name,"LEA"))
      return a.reg >= 0 && !a.mem && a.width > 1 && b.mem &&
             RMInst(0x8D,a.width,b,a.reg,a);
    if (Same(name,"MOVZX") || Same(name,"MOVSX")) {
      bool sign = Same(name,"MOVSX");
      return a.reg >= 0 && !a.mem && a.width > b.width &&
             (b.width == 1 || b.width == 2) &&
             RMInst((sign ? 0x0FBE : 0x0FB6) + (b.width == 2),a.width,b,a.reg,a);
    }
    for (auto &op : shift_ops)
      if (Same(name,op.name)) {
        bool cl = b.reg == 1 && !b.mem && b.width == 1;
        if ((!cl && (b.reg >= 0 || b.mem || !Constant(b.num) ||
                     b.num.addend > 255)) ||
            !RMInst((cl ? 0xD2 : 0xC0) + (a.width != 1),a.width,a,op.ext,
                    {},cl ? 0 : 1))
          return false;
        if (!cl)
          Byte(b.num.addend);
        return true;
      }
    return Binary(name,a,b);
  }

  bool Statement(const char *name)
  {
    constexpr const char *sizes[]{"DU8","DU16","DU32","DU64"};
    for (unsigned i = 0; i < 4; ++i)
      if (Same(name,sizes[i]))
        return PrsAsmDefine(1U << i);
    if (Same(name,"USE16") || Same(name,"USE32") || Same(name,"USE64")) {
      mode = Same(name,"USE16") ? 16 : Same(name,"USE32") ? 32 : 64;
      return true;
    }
    if (Same(name,"ALIGN")) {
      CAsmNum align,fill;
      if (!Expression(align) || !Constant(align) || !align.addend ||
          (align.addend & (align.addend - 1)) ||
          align.addend > (aot ? 1UL << 20 : 8) ||
          !Eat(',') || !Expression(fill) || !Constant(fill))
        return false;
      if (aot) {
        auto bits = static_cast<uint8_t>(__builtin_ctzll(align.addend));
        if (bits > aot->max_align_bits)
          aot->max_align_bits = bits;
      }
      while (!failed && ((size + origin) & (align.addend - 1)))
        Byte(fill.addend);
      return !failed;
    }
    if (Same(name,"IMPORT")) {
      do {
        if (cc.token != TK_IDENT)
          return false;
        auto *s = Symbol(cc.cur_str);
        if (!s || s->scope || s->block)
          return false;
        s->imported = true;
        Lex(cc);
      } while (cc.token == ',' && Eat(','));
      return Eat(';');
    }
    return PrsAsmInst(name);
  }

  const CHashExport *External(const char *name) const
  {
    auto *entry = HashFind(name,cc.htc.hash_table_lst,HTT_EXPORT_SYS_SYM);
    if (!entry && !aot)
      entry = HashFind(name,CurrentTask()->hash_table,HTT_EXPORT_SYS_SYM);
    return entry && !(entry->type & (HTF_IMPORT | HTF_UNRESOLVED))
               ? reinterpret_cast<const CHashExport *>(entry)
               : nullptr;
  }

  bool AsmResolve(const CAsmUnresolvedRef &fix, uint8_t *dest, uintptr_t base)
  {
    uint64_t value = fix.num.addend;
    int bases = 0;
    auto add = [&](const CAsmSymbol *symbol, int sign) {
      if (!symbol)
        return true;
      if (symbol->block) {
        value += sign * (symbol->block->origin + symbol->offset);
        bases += sign;
        return true;
      }
      if (coc && !symbol->imported) {
        if (auto *label = coc->GoToLabelFind(symbol->name)) {
          size_t offset = coc->code[label->ic].data;
          value += sign * (offset + (aot ? aot->size : 0));
          bases += sign;
          return true;
        }
      }
      auto *external = symbol->scope ? nullptr : External(symbol->name);
      if (!external)
        return false;
      value += sign * external->val;
      if (aot && !(external->sym.hash.type & HTF_IMM))
        bases += sign;
      return true;
    };
    if (aot && fix.num.plus && fix.num.plus->imported) {
      if (fix.num.minus || value ||
          (fix.relative ? fix.width != 4 : fix.width != 8))
        return false;
      return AOTRelAdd(*aot,origin+fix.offset,nullptr,fix.num.plus->name,
                       fix.relative ? IET_REL_I32 : IET_IMM_I64);
    }
    if (!add(fix.num.plus,1) || !add(fix.num.minus,-1))
      return false;
    if (fix.relative) {
      value -= origin + fix.offset + fix.width;
      bases--;
    }
    if (!aot)
      value += bases * base;
    else if (bases) {
      if (bases != 1 || fix.relative || fix.width < 4 ||
          !AOTRelAdd(*aot,origin+fix.offset,nullptr,nullptr,IET_ABS_ADDR))
        return false;
    }
    if (fix.relative || fix.signed_only) {
      unsigned shift = 64 - 8 * fix.width;
      if (uint64_t(int64_t(value << shift) >> shift) != value)
        return false;
    }
    MemCpy(dest + fix.offset, &value, fix.width);
    return true;
  }

  CCmpCtrl &cc;
  CAOT *aot;
  CCodeCtrl *coc;
  CHeapCtrl *hc;
  CAsm *root;
  size_t origin;
  uint8_t *buf = nullptr;
  size_t capacity = 0, size = 0;
  unsigned mode = 64, scope = 1;
  bool failed = false;
  CAsmSymbol *symbols = nullptr;
  CAsmUnresolvedRef *fixups = nullptr;
};

bool PrsAsmBlk(CCmpCtrl &cc, CAOT *aot, CCodeCtrl *coc, bool bare)
{
  if ((!bare && Lex(cc) != '{') || !cc.htc.glbl_hash_table)
    return false;
  CAsm standalone(cc, aot);
  auto *as = &standalone;
  if (coc) {
    auto *mem = MAlloc(sizeof(CAsm), CurrentTask()->data_heap);
    if (!mem)
      return false;
    as = new (mem) CAsm(cc, aot, coc);
  }
  auto old_flags = cc.flags;
  cc.flags |= CCF_ASM_EXPRESSIONS | CCF_KEEP_NEW_LINES;
  bool okay = as->Parse(bare);
  constexpr auto mask = CCF_ASM_EXPRESSIONS | CCF_KEEP_NEW_LINES;
  cc.flags = (cc.flags & ~mask) | (old_flags & mask);
  if (!coc)
    return okay && as->Finish() && Lex(cc) != TK_ERROR;
  if (okay && !bare)
    okay = Lex(cc) != TK_ERROR;
  if (okay) {
    auto index = coc->Add({reinterpret_cast<uintptr_t>(as), NO_IC, NO_IC,
                           IC_ASM, RT_U0, false, false, false});
    if (index != NO_IC) {
      if (!coc->asm_first)
        coc->asm_first = as;
      coc->Append(index);
      return true;
    }
  }
  AsmDel(as);
  return false;
}

size_t AsmLayout(CAsm &as, size_t offset)
{
  return as.Layout(offset);
}

void AsmLabelsLayout(CAsm &as)
{
  as.LabelsLayout();
}

bool AsmEmit(CAsm &as, uint8_t *buf, size_t offset)
{
  return as.Emit(buf, offset);
}

bool AsmPublish(CAsm &as, const uint8_t *buf)
{
  return as.Publish(buf);
}

void AsmDel(CAsm *as)
{
  as->~CAsm();
  Free(as);
}
