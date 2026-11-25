#include "../Kernel/KernelB.hpp"
#include "CompilerA.hpp"

#include <new>

extern "C" void SysSymImportsResolveCpp(const char *name, int64_t flags);
extern "C" void PrintErrCpp(const char *fmt, int64_t argc, const uint64_t *argv,
                            const uintptr_t *frame);

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
  int reg = -1, index = -1, seg = -1;
  unsigned addr_width = 0, scale = 0;
  uint8_t reg_type = 0;
  bool mem = false, high = false, rex_byte = false;
};

static const CHashOpcode *AsmOpcodeFind(const char *name)
{
  if (Same(name, "IMUL1"))
    name = "IMUL";
  return reinterpret_cast<const CHashOpcode *>(AsmHashFind(name, HTT_OPCODE));
}

static unsigned AsmKeyword(const char *name)
{
  auto *h = reinterpret_cast<const CHashGeneric *>(
      AsmHashFind(name, HTT_ASM_KEYWORD));
  return h ? h->user_data0 : 0;
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
  return AsmOpcodeFind(name) || (!cc.hash_entry && AsmKeyword(name));
}

struct CAsmTableIns
{
  const CInst *inst = nullptr;
  const CAsmArg *rm = nullptr, *imm = nullptr;
  unsigned score = UINT32_MAX, imm_width = 0;
  uint8_t last = 0, group = 0, rex = 0;
  bool operand_prefix = false, addr_prefix = false, relative = false;
};

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
    auto base = aot ? 0 : reinterpret_cast<uintptr_t>(image);
    return !size || EmitAt(image + offset, base);
  }

  bool EmitAt(uint8_t *dest, uintptr_t base)
  {
    if (dest != buf)
      MemCpy(dest, buf, size);
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
    if (org != INVALID_PTR) {
      auto *image = reinterpret_cast<uint8_t *>(org);
      if (size) {
        if (size > UINTPTR_MAX - org || !EmitAt(buf, org))
          return false;
        MemCpy(image, buf, size);
      }
      return Publish(image);
    }
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
    auto *reg =
        reinterpret_cast<const CHashReg *>(AsmHashFind(cc.cur_str, HTT_REG));
    if (!reg)
      return false;
    arg.reg_type = reg->reg_type;
    arg.reg = reg->reg_num;
    if (REGT_R8 <= reg->reg_type && reg->reg_type <= REGT_R64)
      arg.width = 1U << (reg->reg_type - REGT_R8);
    if (arg.reg_type == REGT_R8) {
      arg.high = 4 <= arg.reg && arg.reg < 8;
      if (arg.reg >= 20) {
        arg.rex_byte = true;
        arg.reg &= 7;
      }
    }
    Lex(cc);
    return true;
  }

  unsigned AsmSize() const
  {
    if (cc.token != TK_IDENT)
      return 0;
    auto id = AsmKeyword(cc.cur_str);
    if (AKW_I8 <= id && id <= AKW_I64)
      return 1U << (id - AKW_I8);
    if (AKW_U8 <= id && id <= AKW_U64)
      return 1U << (id - AKW_U8);
    return 0;
  }

  bool AddAddrReg(CAsmArg &arg, const CAsmArg &reg, bool scaled, unsigned scale)
  {
    auto width = reg.width;
    if (width < 2 || (mode == 64 ? width == 2 : width == 8) ||
        (arg.addr_width && arg.addr_width != width))
      return false;
    arg.addr_width = width;
    if (!scaled && arg.reg < 0) {
      arg.reg = reg.reg;
      return true;
    }
    if (arg.index >= 0)
      return false;
    if (reg.reg == 4 && !scaled) {
      arg.index = arg.reg;
      arg.reg = reg.reg;
    } else {
      arg.index = reg.reg;
      arg.scale = scale;
    }
    return true;
  }

  bool PrsAsmAddress(CAsmArg &arg)
  {
    arg.mem = true;
    Lex(cc); // Skip '['.
    if (cc.token == ']')
      return false;
    for (bool first = true;; first = false) {
      bool negate = cc.token == '-';
      if (cc.token == '+' || negate)
        Lex(cc);
      else if (!first)
        return false;
      CAsmArg reg;
      if (ReadReg(reg)) {
        bool scaled = cc.token == '*';
        unsigned scale = 0;
        if (scaled) {
          if (Lex(cc) != TK_I64)
            return false;
          uint64_t value = cc.cur_i64;
          if (value != 1 && value != 2 && value != 4 && value != 8)
            return false;
          scale = __builtin_ctzll(value);
          Lex(cc);
        }
        if (negate || !AddAddrReg(arg, reg, scaled, scale))
          return false;
      } else {
        CAsmNum disp;
        if (!Expression(disp, 5) || !Apply(arg.num, disp, negate ? '-' : '+'))
          return false;
      }
      if (cc.token == ']')
        break;
    }
    return arg.index != 4 && Eat(']');
  }

  bool PrsAsmArg(CAsmArg &arg)
  {
    if ((arg.width = AsmSize()))
      Lex(cc);
    CAsmArg reg;
    if (ReadReg(reg)) {
      if (reg.reg_type != REGT_SEG || cc.token != ':') {
        if (arg.width && arg.width != reg.width)
          return false;
        arg = reg;
        return true;
      }
      arg.seg = reg.reg;
      Lex(cc);
      if (!arg.width && (arg.width = AsmSize()))
        Lex(cc);
    }
    if (cc.token != '[' && !Expression(arg.num))
      return false;
    return cc.token == '[' ? PrsAsmAddress(arg) : arg.seg < 0;
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
        if (cc.token == TK_IDENT && AsmKeyword(cc.cur_str) == AKW_DUP) {
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

  bool PrsBinFile()
  {
    if (cc.token != TK_STR)
      return false;
    auto *path = ExtDftCpp(cc.cur_str, "BIN");
    if (!path)
      return false;
    size_t count = 0;
    auto *data = FileReadCpp(path, &count, nullptr);
    if (!data) {
      const uint64_t name = reinterpret_cast<uintptr_t>(path);
      PrintErrCpp("File not found: \"%s\".\n", 1, &name, nullptr);
    }
    Free(path);
    if (count > INT32_MAX - origin - size)
      failed = true;
    if (data)
      for (size_t i = 0; i < count && !failed; i++)
        Byte(data[i]);
    Free(data);
    return !failed && Eat(TK_STR) && Eat(';');
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
    auto base = rm.reg, index = rm.index;
    if (base < 0) {
      if (index >= 0) {
        Byte(reg | 4);
        Byte((rm.scale << 6) | ((index & 7) << 3) | 5);
        AsmStoreNum(disp, 4);
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

    base &= 7;
    bool known = Constant(disp);
    unsigned mod = known && !disp.addend && base != 5 ? 0
                   : known && I8(disp.addend)         ? 1
                                                      : 2;
    bool sib = base == 4 || index >= 0;
    Byte((mod << 6) | reg | (sib ? 4 : base));
    if (sib)
      Byte((rm.scale << 6) | ((index >= 0 ? index & 7 : 4) << 3) | base);
    if (mod)
      AsmStoreNum(disp, mod == 1 ? 1 : mode == 16 ? 2 : 4);
    return true;
  }

  static bool AsmRM(unsigned type)
  {
    return ARGT_RM8 <= type && type <= ARGT_M64;
  }

  static unsigned AsmImmWidth(unsigned type)
  {
    if (ARGT_REL8 <= type && type <= ARGT_REL32)
      return 1U << (type - ARGT_REL8);
    if (ARGT_IMM8 <= type && type <= ARGT_UIMM64)
      return 1U << ((type - ARGT_IMM8) & 3);
    return 0;
  }

  bool AsmMatchArg(const CAsmArg &arg, unsigned type) const
  {
    bool reg = arg.reg >= 0 && !arg.mem;
    bool gpr = reg && REGT_R8 <= arg.reg_type && arg.reg_type <= REGT_R64;
    if (ARGT_R8 <= type && type <= ARGT_R64)
      return gpr && arg.width == (1U << (type - ARGT_R8));
    if (AsmRM(type)) {
      unsigned width = 1U << ((type - ARGT_RM8) & 3);
      return (arg.mem && (!arg.width || arg.width == width)) ||
             (type <= ARGT_RM64 && gpr && arg.width == width);
    }
    if (ARGT_AL <= type && type <= ARGT_RAX)
      return gpr && !arg.reg && arg.width == (1U << (type - ARGT_AL));
    if (type == ARGT_CL || type == ARGT_DX)
      return gpr && arg.reg == (type == ARGT_CL ? 1 : 2) &&
             arg.width == (type == ARGT_CL ? 1U : 2U);
    if (ARGT_SREG <= type && type <= ARGT_CS) {
      constexpr int segs[]{2, 3, 0, 4, 5, 1};
      return reg && arg.reg_type == REGT_SEG &&
             (type == ARGT_SREG || arg.reg == segs[type - ARGT_SS]);
    }
    if (type == ARGT_ST0 || type == ARGT_STI)
      return reg && arg.reg_type == REGT_FSTK && (type == ARGT_STI || !arg.reg);
    unsigned width = AsmImmWidth(type);
    if (!width || reg || arg.mem || (arg.width && arg.width != width))
      return false;
    if (type <= ARGT_REL32)
      return true;
    if (!Constant(arg.num))
      return width >= 4;
    unsigned shift = 64 - 8 * width;
    uint64_t value = arg.num.addend;
    return type <= ARGT_IMM64
               ? uint64_t(int64_t(value << shift) >> shift) == value
               : !shift || value >> (64 - shift) == 0;
  }

  void AsmTableMatch(CAsmTableIns &best, const CInst &ins, const CAsmArg &a,
                     const CAsmArg &b, unsigned argc) const
  {
    if ((!ins.arg1) != (!argc) || (!ins.arg2) != (argc < 2) ||
        (argc && !AsmMatchArg(a, ins.arg1)) ||
        (argc == 2 && !AsmMatchArg(b, ins.arg2)) ||
        (mode == 64 && (ins.flags & IEF_NOT_IN_64_BIT)) ||
        ins.opcode_modifier == OM_CP)
      return;

    CAsmTableIns form;
    form.inst = &ins;
    form.last = ins.opcode[ins.opcode_cnt - 1];
    form.operand_prefix =
        mode == 16 ? (ins.flags & IEF_OP_SIZE32) : (ins.flags & IEF_OP_SIZE16);
    const bool rm_a = AsmRM(ins.arg1), rm_b = AsmRM(ins.arg2);
    uint8_t rex = ins.flags & IEF_48_REX ? 8 : 0;
    bool rex_byte = a.rex_byte || b.rex_byte;
    if (rm_a || rm_b) {
      if (rm_a && rm_b)
        return;
      form.rm = rm_a ? &a : &b;
      auto &reg = rm_a ? b : a;
      if (ins.slash_val < 8 || ins.slash_val == SV_NONE)
        form.group = ins.slash_val == SV_NONE ? 0 : ins.slash_val;
      else if (ins.slash_val == SV_R_REG && !reg.mem) {
        auto group = reg.reg >= 0 ? reg.reg : form.rm->reg;
        if (group < 0 || (reg.reg < 0 && form.rm->mem))
          return;
        form.group = group;
        if (group >= 8)
          rex |= 4;
      } else
        return;
      if (form.rm->reg >= 8)
        rex |= 1;
      if (form.rm->index >= 8)
        rex |= 2;
      form.addr_prefix = form.rm->mem && mode == 16;
    } else if (ins.flags & IEF_PLUS_OPCODE) {
      if (ins.slash_val == SV_R_REG) {
        auto &reg = ARGT_AL <= ins.arg1 && ins.arg1 <= ARGT_RAX ? b : a;
        if (reg.reg < 0 || reg.mem)
          return;
        form.last |= reg.reg & 7;
        if (reg.reg >= 8)
          rex |= 1;
      } else if (ins.slash_val == SV_I_REG)
        form.last += ins.arg1 == ARGT_STI ? a.reg : b.reg;
      else
        return;
    } else if (ins.slash_val != SV_NONE)
      return;

    if ((form.operand_prefix || form.addr_prefix) &&
        (ins.flags & IEF_DONT_SWITCH_MODES))
      return;
    if (ins.arg1 == ARGT_R64 || ins.arg1 == ARGT_RM64 || ins.arg1 == ARGT_M64 ||
        ins.arg2 == ARGT_R64 || ins.arg2 == ARGT_RM64 || ins.arg2 == ARGT_M64 ||
        AsmImmWidth(ins.arg1) == 8 || AsmImmWidth(ins.arg2) == 8)
      rex |= 8;
    if ((ins.flags & IEF_REX_ONLY_R8_R15) &&
        (ins.arg2 == ARGT_UIMM32 || rex == 8))
      rex &= ~8;
    if (rex == 8 && !rex_byte && (ins.flags & IEF_REX_XOR_LIKE) && !a.mem &&
        !b.mem && a.reg == b.reg)
      rex = 0;
    if (mode != 64) {
      if ((rex & 7) || rex_byte || a.width == 8 || b.width == 8)
        return;
    } else if (rex || rex_byte) {
      if (a.high || b.high)
        return;
      form.rex = 0x40 | rex;
    }
    if ((form.imm_width = AsmImmWidth(ins.arg1))) {
      form.imm = &a;
      form.relative = ins.arg1 <= ARGT_REL32;
      if (AsmImmWidth(ins.arg2))
        return;
    } else if ((form.imm_width = AsmImmWidth(ins.arg2))) {
      form.imm = &b;
      form.relative = ins.arg2 <= ARGT_REL32;
    }
    form.score = ins.opcode_cnt + form.imm_width + bool(form.rm) +
                 bool(form.rex) + form.operand_prefix + form.addr_prefix +
                 bool(ins.flags & IEF_ENDING_ZERO);
    // A branch target can move until the block's labels and imports are fixed
    // up.
    if (form.relative && form.imm_width == 1 && !form.imm->width)
      form.score += 16;
    if (aot && form.imm && form.imm->num.plus && form.imm->num.plus->imported &&
        form.imm_width != (form.relative ? 4U : 8U))
      return;
    if (form.score < best.score)
      best = form;
  }

  bool PrsTableInst(const CHashOpcode &op)
  {
    auto *forms = AsmInstEntries(op);
    unsigned argc = !!forms[0].arg1 + !!forms[0].arg2;
    CAsmArg a, b;
    if ((argc && !PrsAsmArg(a)) || (argc == 2 && (!Eat(',') || !PrsAsmArg(b))))
      return false;
    CAsmTableIns best;
    for (unsigned i = 0; i < op.inst_entry_cnt; i++)
      AsmTableMatch(best, forms[i], a, b, argc);
    if (!best.inst)
      return false;
    const auto &ins = *best.inst;
    auto seg = b.seg >= 0 ? b.seg : a.seg;
    if (seg >= 0) {
      constexpr uint8_t seg_prefixes[]{0x26, 0x2E, 0x36, 0x3E, 0x64, 0x65};
      Byte(seg_prefixes[seg]);
    }
    if (best.operand_prefix)
      Byte(0x66);
    if (best.addr_prefix)
      Byte(0x67);
    if (best.rex)
      Byte(best.rex);
    for (unsigned i = 1; i < ins.opcode_cnt; i++)
      Byte(ins.opcode[i - 1]);
    Byte(best.last);
    bool ending_zero = ins.flags & IEF_ENDING_ZERO;
    if (best.rm && !ModRM(best.group, *best.rm, best.imm_width + ending_zero))
      return false;
    if (best.imm)
      AsmStoreNum(best.imm->num, best.imm_width, best.relative, best.relative);
    if (ending_zero)
      Byte(0);
    return !failed;
  }

  bool PrsAsmInst(const char *name)
  {
    auto *op = AsmOpcodeFind(name);
    return op && PrsTableInst(*op);
  }

  bool Statement(const char *name)
  {
    auto id = AsmKeyword(name);
    if (AKW_DU8 <= id && id <= AKW_DU64)
      return PrsAsmDefine(1U << (id - AKW_DU8));
    if (AKW_USE16 <= id && id <= AKW_USE64) {
      mode = 16U << (id - AKW_USE16);
      return true;
    }
    if (id == AKW_ORG) {
      auto &address = aot ? aot->org : org;
      CAsmNum n;
      if (coc || origin || size || address != INVALID_PTR || !Expression(n) ||
          !Constant(n))
        return false;
      address = n.addend;
      return true;
    }
    if (id == AKW_BINFILE)
      return PrsBinFile();
    if (id == AKW_ALIGN) {
      CAsmNum align, fill;
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
    if (id == AKW_IMPORT) {
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
  uint64_t org = INVALID_PTR;
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
