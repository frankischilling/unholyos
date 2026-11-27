class CCompilerDefinesCase
{
  U8 *name,*expected;
  I64 cnt;
  CHashDefineStr *record;
  U8 *data,**index,*sub[56];
};

#assert sizeof(CHashDefineStr)==88
#assert offset(CHashDefineStr.data)==64
#assert offset(CHashDefineStr.sub_idx)==72
#assert offset(CHashDefineStr.cnt)==80

CCompilerDefinesCase CompilerDefinesCases[14];
I64 CompilerDefinesTouch=0;

U0 CompilerDefinesCase(I64 i,U8 *name,U8 *expected,I64 cnt)
{
  CompilerDefinesCases[i].name=name;
  CompilerDefinesCases[i].expected=expected;
  CompilerDefinesCases[i].cnt=cnt;
}

U0 CompilerDefinesCasesInit()
{
  // List bytes from Terry A. Davis's Compiler/CInit.HC::CmpLoadDefines,
  // TempleOS c26482bb6ad3f80106d28504ec5db3c6a360732c.
  CompilerDefinesCase(0,"ST_RAW_TYPES",
        " \0 \0I0\0U0\0I8\0U8\0I16\0U16\0I32\0U32\0I64\0U64\0"
        "F32\0UF32\0F64\0UF64\0",16);
  CompilerDefinesCase(1,"ST_NATURAL_TYPES",
        "I8\0U8\0I16\0U16\0I32\0U32\0I64\0U64\0F64\0String\0",10);
  CompilerDefinesCase(2,"ST_INT_SIZE_TYPES",
        "I8\0U8\0I16\0U16\0I32\0U32\0I64\0U64\0",8);
  CompilerDefinesCase(3,"ST_TY_TYPES","NULL\0STK\0IMM\0REG\0DISP\0SIB\0RIP\0",7);
  CompilerDefinesCase(4,"ST_U8_REGS","AL\0CL\0DL\0BL\0AH\0CH\0DH\0BH\0"
        "R8u8\0R9u8\0R10u8\0R11u8\0R12u8\0R13u8\0R14u8\0R15u8\0",16);
  CompilerDefinesCase(5,"ST_U8_REX_REGS","AL\0CL\0DL\0BL\0RSPu8\0RBPu8\0RSIu8\0RDIu8\0"
        "R8u8\0R9u8\0R10u8\0R11u8\0R12u8\0R13u8\0R14u8\0R15u8\0",16);
  CompilerDefinesCase(6,"ST_U16_REGS","AX\0CX\0DX\0BX\0SP\0BP\0SI\0DI\0"
        "R8u16\0R9u16\0R10u16\0R11u16\0R12u16\0R13u16\0R14u16\0R15u16\0",16);
  CompilerDefinesCase(7,"ST_U32_REGS","EAX\0ECX\0EDX\0EBX\0ESP\0EBP\0ESI\0EDI\0"
        "R8u32\0R9u32\0R10u32\0R11u32\0R12u32\0R13u32\0R14u32\0R15u32\0",16);
  CompilerDefinesCase(8,"ST_U64_REGS","RAX\0RCX\0RDX\0RBX\0RSP\0RBP\0RSI\0RDI\0"
        "R8\0R9\0R10\0R11\0R12\0R13\0R14\0R15\0",16);
  CompilerDefinesCase(9,"ST_MM_REGS","MM0\0MM1\0MM2\0MM3\0MM4\0MM5\0MM6\0MM7\0",8);
  CompilerDefinesCase(10,"ST_XMM_REGS",
        "XMM0\0XMM1\0XMM2\0XMM3\0XMM4\0XMM5\0XMM6\0XMM7\0",8);
  CompilerDefinesCase(11,"ST_ARG_TYPES","NONE\0REL8\0REL16\0REL32\0"
        "IMM8\0IMM16\0IMM32\0IMM64\0UIMM8\0UIMM16\0UIMM32\0UIMM64\0"
        "R8\0R16\0R32\0R64\0RM8\0RM16\0RM32\0RM64\0"
        "M8\0M16\0M32\0M64\0M1632\0M16N32\0M16N16\0M32N32\0"
        "MOFFS8\0MOFFS16\0MOFFS32\0MOFFS64\0"
        "AL\0AX\0EAX\0RAX\0CL\0DX\0 \0SREG\0"
        "SS\0DS\0ES\0FS\0GS\0CS\0ST0\0STI\0"
        "MM\0MM32\0MM64\0XMM\0XMM32\0XMM64\0XMM128\0XMM0\0",56);
  CompilerDefinesCase(12,"ST_SEG_REGS","ES\0CS\0SS\0DS\0FS\0GS\0",6);
  CompilerDefinesCase(13,"ST_FSTK_REGS","ST0\0ST1\0ST2\0ST3\0ST4\0ST5\0ST6\0ST7\0",8);
}

U64 CompilerDefinesInspect(Bool save,I64 *entries,U64 *copied,U64 *stable)
{
  CCompilerDefinesCase *c;
  CHashDefineStr *record;
  U8 *expected,*sub,*origin;
  I64 i,j,size;
  U64 lists=0;
  Bool same;
  for (i=0;i<14;i++) {
    c=&CompilerDefinesCases[i];
    record=HashFind(c->name,Fs->hash_table,4);
    if (!record || record->cnt!=c->cnt || !(record->type&4) ||
        StrCmp(record->str,c->name) || HashFind(c->name,Fs->hash_table,4,2) ||
        !record->data || !record->sub_idx ||
        MSize(record->sub_idx)<c->cnt*sizeof(U8 *) ||
        DefineSub(-1,c->name) || DefineSub(c->cnt,c->name))
      goto next_list;
    origin=record->sub_idx[0];
    if (!origin || origin==record->data)
      goto next_list;
    lists|=1<<i;
    same=record==c->record && record->data==c->data && record->sub_idx==c->index;
    if (save) {
      c->record=record;
      c->data=record->data;
      c->index=record->sub_idx;
    }
    expected=c->expected;
    for (j=0;j<c->cnt;j++) {
      sub=DefineSub(j,c->name);
      if (sub && sub==record->sub_idx[j] &&
          sub==origin+(expected-c->expected) && !StrCmp(sub,expected))
        (*entries)++;
      if (save)
        c->sub[j]=sub;
      else if (sub!=c->sub[j])
        same=FALSE;
      expected+=StrLen(expected)+1;
    }
    size=expected+1-c->expected;
    if (!*expected && MSize(record->data)>=size &&
        !MemCmp(record->data,c->expected,size))
      *copied|=1<<i;
    if (!save && same)
      *stable|=1<<i;
next_list:;
  }
  return lists;
}

U64 CompilerDefinesSelected()
{
  U64 checks=0;
  U8 *text;
  if (DefineMatch("RIP","ST_TY_TYPES")==6 &&
      DefineMatch("XMM0","ST_ARG_TYPES")==55)
    checks|=1;
  if (!StrCmp(DefineSub(0,"ST_RAW_TYPES")," ") &&
      !StrCmp(DefineSub(1,"ST_RAW_TYPES")," ") &&
      !StrCmp(DefineSub(38,"ST_ARG_TYPES")," ") &&
      DefineMatch(" ","ST_RAW_TYPES")==0 && DefineMatch(" ","ST_ARG_TYPES")==38)
    checks|=2;
  if (DefineMatch("AH","ST_U8_REGS")==4 && DefineMatch("BH","ST_U8_REGS")==7)
    checks|=4;
  if (DefineMatch("RSPu8","ST_U8_REX_REGS")==4 &&
      DefineMatch("RDIu8","ST_U8_REX_REGS")==7)
    checks|=8;
  if (DefineMatch("AH","ST_U8_REX_REGS")==-1 &&
      DefineMatch("RSPu8","ST_U8_REGS")==-1)
    checks|=16;
  text=MStrPrint("[%Z][%Z]|%Z/%Z|%Z/%Z|%Z",
      0,"ST_RAW_TYPES",1,"ST_RAW_TYPES",4,"ST_U8_REGS",4,"ST_U8_REX_REGS",
      7,"ST_U8_REGS",7,"ST_U8_REX_REGS",55,"ST_ARG_TYPES");
  if (text && !StrCmp(text,"[ ][ ]|AH/RSPu8|BH/RDIu8|XMM0"))
    checks|=32;
  Free(text);
  if (DefineMatch("String","ST_NATURAL_TYPES")==9 &&
      DefineMatch("U64","ST_INT_SIZE_TYPES")==7)
    checks|=64;
  if (DefineMatch("ST7","ST_FSTK_REGS")==7 && DefineMatch("GS","ST_SEG_REGS")==5)
    checks|=128;
  return checks;
}

U0 CompilerDefinesProbe()
{
  U64 lists0,lists1,bytes0=0,bytes1=0,stable=0,selected=0;
  I64 entries0=0,entries1=0;
  CompilerDefinesCasesInit;
  lists0=CompilerDefinesInspect(TRUE,&entries0,&bytes0,&stable);
  if (lists0==0x3FFF)
    selected=CompilerDefinesSelected;
  ExePutS("CompilerDefinesTouch=6*7;");
  lists1=CompilerDefinesInspect(FALSE,&entries1,&bytes1,&stable);
  DocClear(,TRUE);
  "CD lists %X %X\n",lists0,lists1;
  "CD entries %d %d\n",entries0,entries1;
  "CD bytes %X %X\n",bytes0,bytes1;
  "CD stable %X\n",stable;
  "CD selected %X\n",selected;
  "CD compile %d\n",CompilerDefinesTouch;
  "CD DONE\n";
}

CompilerDefinesProbe;
