#include "../Kernel/KernelB.hpp"
#include "CompilerA.hpp"

// Based on Terry A. Davis's public-domain Compiler/UAsm.HC.
// Keep the disassembler's views of the assembler dictionary immutable.
struct CUAsmGlbls
{
  const CInst **table_16_32,**table_64;
  size_t table_16_32_entries,table_64_entries;
  uint64_t ins64_arg_mask,signed_arg_mask,mem_arg_mask;
};
static CUAsmGlbls uasm;

static unsigned InstKey(const CInst &ins)
{
  return ins.opcode[0];
}

static const CHashOpcode &InstOwner(const CInst &ins)
{
  return *reinterpret_cast<const CHashOpcode *>(
      reinterpret_cast<const uint8_t *>(&ins)-
      ins.ins_entry_num*sizeof(CInst)-offsetof(CHashOpcode,ins));
}

static void InstSort(const CInst **table,size_t count)
{
  // Stable ordering within each first-opcode group preserves the source
  // order of equivalent immediate forms in a dictionary entry.
  for (size_t i=1;i<count;++i) {
    auto *ins=table[i];
    size_t j=i;
    while (j && InstKey(*table[j-1])>InstKey(*ins)) {
      table[j]=table[j-1];
      --j;
    }
    table[j]=ins;
  }
}

bool UAsmHashLoad(const CHashTable &hash,CHeapCtrl *heap)
{
  if (uasm.table_64)
    return true;
  CUAsmGlbls next;
  MemSet(&next,0,sizeof(next));
  next.ins64_arg_mask=0x0880888880ULL|(1ULL<<ARGT_ST0)|(1ULL<<ARGT_STI);
  next.signed_arg_mask=0xFE;
  next.mem_arg_mask=0xFULL<<ARGT_M8;
  for (int64_t i=0;i<=hash.mask;++i)
    for (auto *h=hash.body[i];h;h=h->next)
      if (h->type==HTT_OPCODE) {
        auto &op=*reinterpret_cast<const CHashOpcode *>(h);
        if (op.oc_flags&OCF_ALIAS) continue;
        auto *ins=AsmInstEntries(op);
        for (unsigned j=0;j<op.inst_entry_cnt;++j) {
          ++next.table_16_32_entries;
          if (!(ins[j].flags&IEF_NOT_IN_64_BIT))
            ++next.table_64_entries;
        }
      }
  if (!next.table_16_32_entries || !next.table_64_entries)
    return false;
  next.table_16_32=reinterpret_cast<const CInst **>(
      MAlloc(next.table_16_32_entries*sizeof(CInst *),heap));
  next.table_64=reinterpret_cast<const CInst **>(
      MAlloc(next.table_64_entries*sizeof(CInst *),heap));
  if (!next.table_16_32 || !next.table_64) {
    Free(next.table_16_32);
    Free(next.table_64);
    return false;
  }
  size_t a=0,b=0;
  for (int64_t i=0;i<=hash.mask;++i)
    for (auto *h=hash.body[i];h;h=h->next)
      if (h->type==HTT_OPCODE) {
        auto &op=*reinterpret_cast<const CHashOpcode *>(h);
        if (op.oc_flags&OCF_ALIAS) continue;
        auto *ins=AsmInstEntries(op);
        for (unsigned j=0;j<op.inst_entry_cnt;++j) {
          next.table_16_32[a++]=&ins[j];
          if (!(ins[j].flags&IEF_NOT_IN_64_BIT))
            next.table_64[b++]=&ins[j];
        }
      }
  InstSort(next.table_16_32,a);
  InstSort(next.table_64,b);
  uasm=next;
  return true;
}

struct CUiOut
{
  char *buf;
  size_t cap,len=0;
  void Ch(char c)
  {
    if (len+1<cap) buf[len++]=c;
  }
  void Text(const char *s)
  {
    while (*s) Ch(*s++);
  }
  void Hex(uint64_t n,unsigned digits=0)
  {
    if (!digits) {
      digits=1;
      for (auto v=n;v>>=4;) ++digits;
    }
    while (digits) Ch("0123456789ABCDEF"[(n>>(4*--digits))&15]);
  }
  void Finish() { if (cap) buf[len]=0; }
};

struct CUiReader
{
  const uint8_t *start;
  size_t cap,pos=0;
  bool okay=true;
  uint64_t Read(unsigned n=1)
  {
    if (!okay || n>8 || n>cap-pos) {
      okay=false;
      return 0;
    }
    uint64_t v=0;
    for (unsigned i=0;i<n;++i)
      v|=uint64_t(start[pos++])<<(i*8);
    return v;
  }
  uint8_t Peek()
  {
    if (pos>=cap) { okay=false; return 0; }
    return start[pos];
  }
};

struct CUiPrefix
{
  unsigned seg_size,op_size,addr_size;
  uint8_t rex=0;
  const char *segment="";
};

static CUiPrefix UiPrefixes(CUiReader &r,unsigned size)
{
  CUiPrefix p{size,size==16?16u:32u,size};
  while (r.okay && r.pos<r.cap) {
    auto byte=r.Peek();
    switch (byte) {
      case 0x2E: p.segment="CS:"; break;
      case 0x36: p.segment="SS:"; break;
      case 0x3E: p.segment="DS:"; break;
      case 0x26: p.segment="ES:"; break;
      case 0x64: p.segment="FS:"; break;
      case 0x65: p.segment="GS:"; break;
      case 0x66: p.op_size=size==16?32:16; break;
      case 0x67: p.addr_size=size==16?32:size==32?16:32; break;
      default:
        if (size!=64 || byte<0x40 || byte>0x4F)
          return p;
        p.rex=byte;
        if (byte&8) p.op_size=64;
        break;
    }
    r.Read();
  }
  return p;
}

static bool UiMem(unsigned a)
{
  return a>=ARGT_RM8 && a<=ARGT_M64;
}

static bool UiMemOnly(unsigned a)
{
  return a>=ARGT_M8 && a<=ARGT_M64;
}

static bool UiReg(unsigned a)
{
  return a>=ARGT_R8 && a<=ARGT_R64;
}

static unsigned UiImmSize(unsigned a)
{
  if (a>=ARGT_REL8 && a<=ARGT_REL32) return 1u<<(a-ARGT_REL8);
  if (a>=ARGT_IMM8 && a<=ARGT_IMM64) return 1u<<(a-ARGT_IMM8);
  if (a>=ARGT_UIMM8 && a<=ARGT_UIMM64) return 1u<<(a-ARGT_UIMM8);
  return 0;
}

static bool UiSupported(unsigned a)
{
  return a==ARGT_NONE || UiImmSize(a) || UiReg(a) || UiMem(a) ||
         (a>=28 && a<=31) || (a>=ARGT_AL && a<=ARGT_DX) ||
         (a>=ARGT_SREG && a<=ARGT_STI);
}

static bool UiNamed(const CInst &ins,const char *name)
{
  const char *s=InstOwner(ins).hash.str;
  while (*s && *s==*name) { ++s; ++name; }
  return *s==*name;
}

static int InstMatch(const CInst &ins,const CUiReader &r,const CUiPrefix &p)
{
  if (!ins.opcode_cnt || ins.opcode_cnt>r.cap-r.pos ||
      !UiSupported(ins.arg1) || !UiSupported(ins.arg2))
    return -1;
  unsigned modrm_pos=r.pos+ins.opcode_cnt;
  for (unsigned i=0;i<ins.opcode_cnt;++i) {
    auto v=r.start[r.pos+i];
    if (i+1==ins.opcode_cnt && (ins.flags&IEF_PLUS_OPCODE))
      v&=0xF8;
    if (v!=ins.opcode[i]) return -1;
  }
  const bool mem=UiMem(ins.arg1)||UiMem(ins.arg2);
  uint8_t modrm=0;
  if (mem) {
    if (modrm_pos>=r.cap) return -1;
    modrm=r.start[modrm_pos];
    if (ins.slash_val<8 && (modrm>>3&7)!=ins.slash_val)
      return -1;
    if ((UiMemOnly(ins.arg1)||UiMemOnly(ins.arg2)) && (modrm>>6)==3)
      return -1;
    if (ins.slash_val==SV_NONE && (ins.opcode[0]==0xC6 || ins.opcode[0]==0xC7)
        && (modrm>>3&7))
      return -1;
  }
  bool floating=ins.arg1==ARGT_ST0 || ins.arg1==ARGT_STI ||
                ins.arg2==ARGT_ST0 || ins.arg2==ARGT_STI ||
                (ins.flags&IEF_STI_LIKE);
  bool wide=ins.arg1==ARGT_R64 || ins.arg2==ARGT_R64 ||
            ins.arg1==ARGT_RM64 || ins.arg2==ARGT_RM64 ||
            ins.arg1==ARGT_RAX || ins.arg2==ARGT_RAX ||
            (ins.flags&IEF_48_REX);
  unsigned op=p.op_size;
  const bool implicit64=p.seg_size==64 && op==32 &&
      (UiNamed(ins,"PUSH") || UiNamed(ins,"POP") ||
       UiNamed(ins,"CALL") || UiNamed(ins,"JMP")) && wide;
  bool short_rex=(ins.flags&IEF_REX_ONLY_R8_R15) && p.rex &&
                 !(p.rex&8) && wide;
  if (!floating) {
    if (p.seg_size!=64 && (wide || (ins.flags&IEF_48_REX))) return -1;
    if ((ins.flags&IEF_OP_SIZE16) && op!=16) return -1;
    if ((ins.flags&IEF_OP_SIZE32) && op==16) return -1;
    if (wide && op!=64 && !implicit64 && !short_rex) return -1;
    if (op==64 && !wide && (ins.flags&IEF_OP_SIZE32) &&
        (ins.size1==32 || ins.size2==32) &&
        !(ins.arg1==ARGT_REL32 || ins.arg2==ARGT_REL32 ||
          ins.arg1==ARGT_IMM32 || ins.arg1==ARGT_UIMM32))
      return -1;
  }
  int score=int(ins.opcode_cnt)*128;
  if (!(ins.flags&IEF_PLUS_OPCODE)) score+=32;
  if (mem && ins.slash_val<8) score+=16;
  if (wide && (op==64 || implicit64 || short_rex)) score+=8;
  if (op==64 && (ins.flags&IEF_REX_ONLY_R8_R15) && !short_rex &&
      !(UiNamed(ins,"PUSH")||UiNamed(ins,"POP")||UiNamed(ins,"CALL")))
    score-=12;
  // The reference unassembler chooses signed immediate forms when
  // the dictionary has otherwise identical signed and unsigned entries.
  if ((ins.arg1>=ARGT_IMM8 && ins.arg1<=ARGT_IMM64) ||
      (ins.arg2>=ARGT_IMM8 && ins.arg2<=ARGT_IMM64)) ++score;
  return score;
}

static const CInst *InstEntryFind(const CUiReader &r,const CUiPrefix &p)
{
  if (!r.okay || r.pos>=r.cap) return nullptr;
  auto **table=p.seg_size==64?uasm.table_64:uasm.table_16_32;
  auto count=p.seg_size==64?uasm.table_64_entries:uasm.table_16_32_entries;
  const CInst *best=nullptr;
  int score=-1;
  const unsigned key=r.start[r.pos];
  unsigned keys[]{key,key&0xF8};
  for (unsigned k=0;k<2;++k) {
    if (k && keys[0]==keys[1]) break;
    size_t lo=0,hi=count;
    while (lo<hi) {
      auto mid=lo+(hi-lo)/2;
      if (InstKey(*table[mid])<keys[k]) lo=mid+1;
      else hi=mid;
    }
    for (;lo<count && InstKey(*table[lo])==keys[k];++lo) {
      auto n=InstMatch(*table[lo],r,p);
      if (n>score) { score=n; best=table[lo]; }
    }
  }
  return best;
}

static void UiRegName(CUiOut &out,unsigned reg,unsigned size,bool rex)
{
  static constexpr const char *names[][8]={
      {"AL","CL","DL","BL","AH","CH","DH","BH"},
      {"AX","CX","DX","BX","SP","BP","SI","DI"},
      {"EAX","ECX","EDX","EBX","ESP","EBP","ESI","EDI"},
      {"RAX","RCX","RDX","RBX","RSP","RBP","RSI","RDI"}};
  static constexpr const char *low_rex[]{"RSPu8","RBPu8","RSIu8","RDIu8"};
  if (reg>=8) {
    out.Ch('R');
    if (reg>=10) out.Ch('1');
    out.Ch('0'+reg%10);
    if (size!=64) out.Text(size==8?"u8":size==16?"u16":"u32");
  } else if (size==8 && rex && reg>=4)
    out.Text(low_rex[reg-4]);
  else
    out.Text(names[size==8?0:size==16?1:size==32?2:3][reg]);
}

static void UiSize(CUiOut &out,unsigned size,bool sign)
{
  if (!size) return;
  out.Ch(sign?'I':'U');
  out.Text(size==8?"8 ":size==16?"16 ":size==32?"32 ":"64 ");
}

static int64_t UiSigned(uint64_t value,unsigned bytes)
{
  if (bytes==1) return int8_t(value);
  if (bytes==2) return int16_t(value);
  if (bytes==4) return int32_t(value);
  return int64_t(value);
}

static void UiMemory(CUiOut &out,CUiReader &r,const CUiPrefix &p,
                     unsigned mod,unsigned rm,unsigned size,unsigned tail)
{
  if (mod==3) {
    UiRegName(out,rm,size,p.rex!=0);
    return;
  }
  out.Text(p.segment);
  unsigned base=rm,index=4,scale=1;
  bool sib=p.addr_size!=16 && (rm&7)==4;
  if (sib) {
    auto s=r.Read();
    base=(s&7)+((p.rex&1)?8:0);
    index=(s>>3&7)+((p.rex&2)?8:0);
    scale=1u<<(s>>6);
  }
  if (p.addr_size==16) {
    static constexpr const char *ea[]{"BX+SI","BX+DI","BP+SI","BP+DI",
                                      "SI","DI","BP","BX"};
    if (!mod && (rm&7)==6) {
      out.Ch('['); out.Hex(r.Read(2)); out.Ch(']');
    } else {
      if (mod) out.Hex(r.Read(mod==1?1:2),mod==1?2:4);
      out.Ch('['); out.Text(ea[rm&7]); out.Ch(']');
    }
    return;
  }
  if (!mod && (base&7)==5) {
    auto disp=r.Read(4);
    if (!sib) {
      if (p.addr_size==64)
        disp=uint64_t(int64_t(int32_t(disp)))+
             reinterpret_cast<uintptr_t>(r.start)+r.pos+tail;
      out.Ch('['); out.Hex(disp); out.Ch(']');
      return;
    }
    out.Hex(disp,8);
    out.Ch('[');
    if (index!=4) {
      UiRegName(out,index,p.addr_size,p.rex!=0);
      out.Ch('*'); out.Ch('0'+scale);
    } else out.Ch('0');
    out.Ch(']');
    return;
  }
  if (mod) out.Hex(r.Read(mod==1?1:4),mod==1?2:8);
  out.Ch('[');
  UiRegName(out,base,p.addr_size,p.rex!=0);
  if (sib && index!=4) {
    out.Ch('+'); UiRegName(out,index,p.addr_size,p.rex!=0);
    out.Ch('*'); out.Ch('0'+scale);
  }
  out.Ch(']');
}

static size_t UiDecode(char *buf,size_t cap,const uint8_t *rip,size_t available,
                       unsigned seg_size,int64_t &jump)
{
  CUiReader r{rip,available<15?available:15};
  CUiOut out{buf,cap};
  jump=-1;
  auto p=UiPrefixes(r,seg_size);
  const auto *ins=InstEntryFind(r,p);
  if (ins) {
    const auto &op=InstOwner(*ins);
    r.pos+=ins->opcode_cnt;
    unsigned plus=0,reg=0,rm=0,mod=0;
    if (ins->flags&IEF_PLUS_OPCODE)
      plus=(rip[r.pos-1]&7)+((p.rex&1)?8:0);
    bool mem=UiMem(ins->arg1)||UiMem(ins->arg2);
    if (mem) {
      auto b=r.Read();
      mod=b>>6; rm=(b&7)+((p.rex&1)?8:0);
      reg=(b>>3&7)+((p.rex&4)?8:0);
    }
    unsigned args[]{ins->arg1,ins->arg2},sizes[]{ins->size1,ins->size2};
    char operands[2][128];
    operands[0][0]=operands[1][0]=0;
    unsigned tail=UiImmSize(args[0])+UiImmSize(args[1])+
                  bool(ins->flags&IEF_ENDING_ZERO);
    // Decode the encoded memory operand before any immediate, irrespective
    // of which display operand owns the ModR/M r/m field.
    int mem_arg=UiMem(args[1])?1:UiMem(args[0])?0:-1;
    if (mem_arg>=0) {
      CUiOut a{operands[mem_arg],sizeof(operands[mem_arg])};
      UiSize(a,sizes[mem_arg],false);
      UiMemory(a,r,p,mod,rm,sizes[mem_arg],tail);
      a.Finish();
    }
    for (unsigned i=0;i<2 && r.okay;++i) {
      unsigned arg=args[i],size=sizes[i];
      if (!arg || int(i)==mem_arg) continue;
      CUiOut a{operands[i],sizeof(operands[i])};
      UiSize(a,size,(uasm.signed_arg_mask>>arg)&1);
      if (auto n=UiImmSize(arg)) {
        auto bits=r.Read(n);
        if (arg>=ARGT_REL8 && arg<=ARGT_REL32) {
          jump=uint64_t(reinterpret_cast<uintptr_t>(rip))+r.pos+UiSigned(bits,n);
          a.Hex(jump);
          a.Ch(' ');
        } else a.Hex(bits,n*2);
      } else if (UiReg(arg))
        UiRegName(a,mem?reg:plus,size,p.rex!=0);
      else if (arg>=ARGT_AL && arg<=ARGT_DX) {
        static constexpr const char *fixed[]{"AL","AX","EAX","RAX","CL","DX"};
        a.Text(fixed[arg-ARGT_AL]);
      } else if (arg==ARGT_SREG) {
        static constexpr const char *seg[]{"ES","CS","SS","DS","FS","GS"};
        if (reg<6) a.Text(seg[reg]); else r.okay=false;
      } else if (arg>=ARGT_SS && arg<=ARGT_ST0) {
        static constexpr const char *fixed[]{"SS","DS","ES","FS","GS","CS","ST0"};
        a.Text(fixed[arg-ARGT_SS]);
      } else if (arg==ARGT_STI) {
        a.Text("ST"); a.Ch('0'+(plus&7));
      } else if (arg>=28 && arg<=31) {
        a.Text(p.segment); a.Ch('['); a.Hex(r.Read(p.addr_size/8)); a.Ch(']');
      } else r.okay=false;
      a.Finish();
    }
    if (ins->flags&IEF_ENDING_ZERO)
      if (r.Read()!=0) r.okay=false;
    if (r.okay) {
      out.Text(op.hash.str);
      if (*operands[0]) { out.Ch('\t'); out.Text(operands[0]); }
      if (*operands[1]) { out.Ch(','); out.Text(operands[1]); }
      out.Ch('\n'); out.Finish();
      return r.pos;
    }
  }
  // A malformed or unsupported form must not invent an instruction or
  // stall U/Un. Preserve the byte and continue at the following address.
  jump=-1;
  if (!available) { out.Finish(); return 0; }
  out.Text("DU8\t"); out.Hex(*rip,2); out.Ch('\n'); out.Finish();
  return 1;
}

static size_t Ui(char *buf,const uint8_t *rip,size_t available,
                 unsigned seg_size,int64_t &jump,bool just_ins)
{
  char instruction[384];
  auto n=UiDecode(instruction,sizeof(instruction),rip,available,seg_size,jump);
  CUiOut out{buf,1024};
  if (just_ins) out.Text(instruction);
  else for (size_t i=0;i<n;i+=6) {
    uint64_t addr=reinterpret_cast<uintptr_t>(rip+i);
    auto *location=StrPrintJoin(nullptr,"%24tp ",1,&addr);
    if (location) {
      out.Text(location);
      Free(location);
    }
    for (unsigned j=0;j<6;++j)
      if (i+j<n) out.Hex(rip[i+j],2);
      else out.Text("  ");
    if (!i) { out.Ch(' '); out.Text(instruction); }
    else out.Ch('\n');
  }
  out.Finish();
  return n;
}

extern "C" void UiCpp(char *buf,const uint8_t **rip,int64_t seg_size,
                      int64_t *jump_dst,int64_t just_ins)
{
  if (!buf || !rip || !*rip) return;
  int64_t jump=-1;
  if (seg_size!=16 && seg_size!=32 && seg_size!=64) seg_size=64;
  auto n=Ui(buf,*rip,15,seg_size,jump,just_ins);
  *rip+=n;
  if (jump_dst) *jump_dst=jump;
}

extern "C" const uint8_t *UCpp(const uint8_t *rip,int64_t cnt,int64_t seg_size)
{
  char buf[1024];
  if (!rip) return rip;
  if (seg_size==16)
    PrintWarnCpp("16-bit unassembly is not well supported.\n",0,nullptr,nullptr);
  while (cnt>0) {
    --cnt;
    UiCpp(buf,&rip,seg_size,nullptr,false);
    PutS(buf);
  }
  return rip;
}

extern "C" int64_t UnCpp(const uint8_t *rip,int64_t cnt,int64_t seg_size)
{
  if (!rip || cnt<=0) return 0;
  if (seg_size==16)
    PrintWarnCpp("16-bit unassembly is not well supported.\n",0,nullptr,nullptr);
  if (seg_size!=16 && seg_size!=32 && seg_size!=64) seg_size=64;
  int64_t i=0,jump=-1;
  char buf[1024];
  while (cnt>0) {
    auto n=Ui(buf,rip,cnt,seg_size,jump,false);
    if (!n) break;
    PutS(buf); rip+=n; cnt-=n; ++i;
  }
  return i;
}
