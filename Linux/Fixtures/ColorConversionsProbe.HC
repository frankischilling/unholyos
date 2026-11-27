// Checks Terry A. Davis's Kernel/KDefine.HC color conversions at TempleOS
// c26482bb6ad3f80106d28504ec5db3c6a360732c.
_extern _HASH_REM_DEL Bool ColorConversionsRemove(CHash *entry,CHashTable *table,
        I64 instance=1);

asm {
_COLOR_CONVERSIONS_RSP::
  LEA RAX,8[RSP]
  RET
}

_extern _COLOR_CONVERSIONS_RSP U64 ColorConversionsRSP();

class CColorConversionsExport:CHashSrcSym
{
  I64 val;
};

#assert sizeof(CColorROPU16)==2
#assert offset(CColorROPU16.color)==0
#assert offset(CColorROPU16.rop)==1
#assert sizeof(CColorROPU32)==4
#assert offset(CColorROPU32.c0)==0
#assert offset(CColorROPU32.c1)==2
#assert offset(CColorConversionsExport.val)==64
#assert COLOR_INVALID==16

Bool ColorConversionsDirectStack=TRUE,ColorConversionsIndirectStack=TRUE;
U8 *ColorConversionsNames="BLACK\0BLUE\0GREEN\0CYAN\0RED\0PURPLE\0BROWN\0"
    "LTGRAY\0DKGRAY\0LTBLUE\0LTGREEN\0LTCYAN\0LTRED\0LTPURPLE\0YELLOW\0WHITE\0";

Bool ColorConversionsU16(U8 *src,U64 expected)
{
  CColorROPU16 value;
  CColorROPU16 (*call)(U8 *);
  U8 buf[128],original[128];
  U64 before,after;
  I64 i;
  Bool okay=TRUE;
  call=&Str2ColorU16;
  for (i=0;i<2;i++) {
    MemSet(buf,0x5A,sizeof(buf));
    StrCpy(buf,src);
    MemCpy(original,buf,sizeof(buf));
    before=ColorConversionsRSP;
    if (i)
      value=call(buf);
    else
      value=Str2ColorU16(buf);
    after=ColorConversionsRSP;
    if (before!=after) {
      if (i)
        ColorConversionsIndirectStack=FALSE;
      else
        ColorConversionsDirectStack=FALSE;
    }
    if (value!=expected || before!=after || MemCmp(buf,original,sizeof(buf)))
      okay=FALSE;
  }
  return okay;
}

Bool ColorConversionsU32(U8 *src,U64 expected,U8 *tail)
{
  CColorROPU32 value;
  CColorROPU32 (*call)(U8 *);
  U8 buf[128],changed[128];
  U64 before,after;
  I64 i;
  Bool okay=TRUE;
  call=&Str2ColorU32;
  for (i=0;i<2;i++) {
    MemSet(buf,0x5A,sizeof(buf));
    StrCpy(buf,src);
    MemCpy(changed,buf,sizeof(buf));
    MemCpy(changed,tail,StrLen(tail)+1);
    before=ColorConversionsRSP;
    if (i)
      value=call(buf);
    else
      value=Str2ColorU32(buf);
    after=ColorConversionsRSP;
    if (before!=after) {
      if (i)
        ColorConversionsIndirectStack=FALSE;
      else
        ColorConversionsDirectStack=FALSE;
    }
    if (value!=expected || before!=after || MemCmp(buf,changed,sizeof(buf)))
      okay=FALSE;
  }
  return okay;
}

Bool ColorConversionsFormat(CColorROPU32 color,U8 *expected)
{
  U8 *(*call)(U8 *,CColorROPU32);
  U8 buf[128],changed[128],*result;
  U64 before,after;
  I64 i;
  Bool okay=TRUE;
  call=&Color2Str;
  for (i=0;i<2;i++) {
    MemSet(buf,0x5A,sizeof(buf));
    MemSet(changed,0x5A,sizeof(changed));
    StrCpy(changed,expected);
    before=ColorConversionsRSP;
    if (i)
      result=call(buf,color);
    else
      result=Color2Str(buf,color);
    after=ColorConversionsRSP;
    if (before!=after) {
      if (i)
        ColorConversionsIndirectStack=FALSE;
      else
        ColorConversionsDirectStack=FALSE;
    }
    if (result!=buf || before!=after || MemCmp(buf,changed,sizeof(buf)))
      okay=FALSE;
  }
  return okay;
}

U64 ColorConversionsABI()
{
  U8 storage[8];
  CColorROPU16 *a;
  CColorROPU32 *b;
  U64 checks=0;
  MemSet(storage,0x5A,sizeof(storage));
  a=storage+1;
  b=storage+3;
  *a=0x3412;
  *b=0x78563412;
  if (storage[0]==0x5A && storage[7]==0x5A &&
      a->color==0x12 && a->rop==0x34 && *a==0x3412 &&
      b->c0.color==0x12 && b->c0.rop==0x34 &&
      b->c1.color==0x56 && b->c1.rop==0x78 && *b==0x78563412)
    checks|=1;
  if (ColorConversionsU16("hAlF|TWO|RED",0x3004))
    checks|=2;
  if (ColorConversionsU32("7/8",0x40080007,"8"))
    checks|=4;
  if (ColorConversionsFormat(0x70013004,"TWO|HALF|RED/TWO|HALF|BLUE"))
    checks|=8;
  return checks;
}

I64 ColorConversionsNamed()
{
  U8 *name=ColorConversionsNames,lower[32],text[64],*two,*half;
  I64 i,j,flags,count=0;
  U64 expected;
  Bool okay;
  for (i=0;i<16;i++) {
    okay=ColorConversionsU16(name,i) && ColorConversionsFormat(i,name);
    StrCpy(lower,name);
    for (j=0;lower[j];j++)
      if ('A'<=lower[j]<='Z')
        lower[j]+='a'-'A';
    for (flags=0;flags<4;flags++) {
      two=half="";
      expected=i;
      if (flags&1) {
        two="tWo|";
        expected|=0x2000;
      }
      if (flags&2) {
        half="hAlF|";
        expected|=0x1000;
      }
      StrPrint(text,"%s%s%s",two,half,lower);
      if (!ColorConversionsU16(text,expected))
        okay=FALSE;
    }
    if (okay)
      count++;
    name+=StrLen(name)+1;
  }
  return count;
}

I64 ColorConversionsNumeric()
{
  I64 i,count=0;
  U8 text[16],*expected;
  for (i=0;i<=255;i++) {
    StrPrint(text,"%d",i);
    if (i<16)
      expected=DefineSub(i,"ST_COLORS");
    else if (i==255)
      expected="TRANSPARENT";
    else
      expected="INVALID";
    if (ColorConversionsU16(text,i) && ColorConversionsFormat(i,expected))
      count++;
  }
  return count;
}

I64 ColorConversionsEdges()
{
  I64 count=0;
  count+=ColorConversionsU16("",16);
  count+=ColorConversionsU16(" \t\r\n",16);
  count+=ColorConversionsU16("TWO",0x2010);
  count+=ColorConversionsU16("HALF",0x1010);
  count+=ColorConversionsU16("TWO|HALF",0x3010);
  count+=ColorConversionsU16("TWO+HALF+RED",0x3004);
  count+=ColorConversionsU16("half | two | transparent",0x30FF);
  count+=ColorConversionsU16("tWo||+TWO RED",0x2004);
  count+=ColorConversionsU16("RED BLUE",1);
  count+=ColorConversionsU16("1 RED",4);
  count+=ColorConversionsU16("RED 7",7);
  count+=ColorConversionsU16("B",16);
  count+=ColorConversionsU16("BL",16);
  count+=ColorConversionsU16("BLA",0);
  count+=ColorConversionsU16("BRO",6);
  count+=ColorConversionsU16("LT",16);
  count+=ColorConversionsU16("LTB",9);
  count+=ColorConversionsU16("lTg",16);
  count+=ColorConversionsU16("TRAN",16);
  count+=ColorConversionsU16("transparent",255);
  count+=ColorConversionsU16("TW",16);
  count+=ColorConversionsU16("HAL",16);
  count+=ColorConversionsU16("TWO|nope",16);
  count+=ColorConversionsU16("-1",16);
  count+=ColorConversionsU16("256",16);
  count+=ColorConversionsU16("0xFF",255);
  count+=ColorConversionsU16("0b1010",10);
  count+=ColorConversionsU16("0d12",12);
  count+=ColorConversionsU16("00",0);
  count+=ColorConversionsU16("+7",7);
  count+=ColorConversionsU16("0x100",16);
  count+=ColorConversionsU16("0b2",2);
  count+=ColorConversionsU16("0xG",2);
  count+=ColorConversionsU16("0x",0);
  count+=ColorConversionsU16("RED/",4);
  count+=ColorConversionsU16("RED,BLUE",4);
  count+=ColorConversionsU16("RED)BLUE",4);
  count+=ColorConversionsU16("RED}BLUE",4);
  count+=ColorConversionsU16("RED>BLUE",4);
  count+=ColorConversionsU16("1.5",16);
  return count;
}

I64 ColorConversionsPairs()
{
  I64 count=0;
  count+=ColorConversionsU32("RED/BLUE",0x40010004,"BLUE");
  count+=ColorConversionsU32("RED,BLUE",0x40010004,"BLUE");
  count+=ColorConversionsU32("RED)BLUE",0x40010004,"BLUE");
  count+=ColorConversionsU32("RED}BLUE",0x40010004,"BLUE");
  count+=ColorConversionsU32("RED>BLUE",0x40010004,"BLUE");
  count+=ColorConversionsU32("RED/",4,"");
  count+=ColorConversionsU32("RED,",4,"");
  count+=ColorConversionsU32("RED)",4,"");
  count+=ColorConversionsU32("RED}",4,"");
  count+=ColorConversionsU32("RED>",4,"");
  count+=ColorConversionsU32("RED//BLUE",16,"/BLUE");
  count+=ColorConversionsU32("RED/,BLUE",16,",BLUE");
  count+=ColorConversionsU32("/BLUE",16,"BLUE");
  count+=ColorConversionsU32("",16,"");
  count+=ColorConversionsU32("RED",4,"");
  count+=ColorConversionsU32(" RED / hAlF|BLUE)",0x50010004," hAlF|BLUE)");
  count+=ColorConversionsU32("TWO|RED/HALF|BLUE/WHITE",0x50012004,"HALF|BLUE/WHITE");
  count+=ColorConversionsU32("HALF/BLUE",16,"BLUE");
  count+=ColorConversionsU32("RED/TWO",16,"TWO");
  count+=ColorConversionsU32("16/BLUE",16,"BLUE");
  count+=ColorConversionsU32("RED/16",16,"16");
  count+=ColorConversionsU32("17/254",0x40FE0011,"254");
  count+=ColorConversionsU32("TRANSPARENT/255",0x40FF00FF,"255");
  count+=ColorConversionsU32("BLUE/TRANSPARENT",0x40FF0001,"TRANSPARENT");
  count+=ColorConversionsU32("bad/BLUE",16,"BLUE");
  count+=ColorConversionsU32("RED/nope",16,"nope");
  count+=ColorConversionsU32("7/8",0x40080007,"8");
  count+=ColorConversionsU32("7/8,9",0x40080007,"8,9");
  count+=ColorConversionsU32("7/8)",0x40080007,"8)");
  count+=ColorConversionsU32("7//",16,"/");
  count+=ColorConversionsU32(" / ",16," ");
  count+=ColorConversionsU32("0xF/0b10",0x4002000F,"0b10");
  return count;
}

I64 ColorConversionsFormats()
{
  I64 count=0;
  count+=ColorConversionsFormat(0x3004,"TWO|HALF|RED");
  count+=ColorConversionsFormat(0x1004,"HALF|RED");
  count+=ColorConversionsFormat(0x2004,"TWO|RED");
  count+=ColorConversionsFormat(0x0104,"RED");
  count+=ColorConversionsFormat(0xFF04,"TWO|HALF|RED");
  count+=ColorConversionsFormat(0x00010004,"RED");
  count+=ColorConversionsFormat(0x80010004,"RED");
  count+=ColorConversionsFormat(0x40010004,"RED/BLUE");
  count+=ColorConversionsFormat(0x80000004,"RED");
  count+=ColorConversionsFormat(0xC0010004,"RED/BLUE");
  count+=ColorConversionsFormat(0x70013004,"TWO|HALF|RED/TWO|HALF|BLUE");
  count+=ColorConversionsFormat(0x500120FF,"TWO|TRANSPARENT/HALF|BLUE");
  count+=ColorConversionsFormat(0x40FF0010,"INVALID/TRANSPARENT");
  count+=ColorConversionsFormat(0x401000FF,"TRANSPARENT/INVALID");
  count+=ColorConversionsFormat(0x70001000,"HALF|BLACK/TWO|HALF|BLACK");
  count+=ColorConversionsFormat(0x3010,"TWO|HALF|INVALID");
  count+=ColorConversionsFormat(0x12345678,"HALF|INVALID");
  count+=ColorConversionsFormat(0xFFFFFFFF,"TWO|HALF|TRANSPARENT/TWO|HALF|TRANSPARENT");
  return count;
}

U64 ColorConversionsDelimiters()
{
  CColorConversionsExport *entry;
  U8 **slot,*saved;
  U64 checks=0;
  entry=HashFind("str2color_lst",Fs->hash_table,1);
  if (!entry || !entry->val)
    return 0;
  slot=entry->val;
  saved=*slot;
  if (saved && !StrCmp(saved,"/,)}>"))
    checks|=1;
  *slot=";";
  if (ColorConversionsU16("RED;BLUE",4) &&
      ColorConversionsU16("RED/BLUE",16))
    checks|=2;
  if (ColorConversionsU32("7;8",0x40080007,"8") &&
      ColorConversionsU32("7/8",16,""))
    checks|=4;
  *slot=saved;
  if (*slot==saved && HashFind("str2color_lst",Fs->hash_table,1)==entry &&
      ColorConversionsU16("RED/BLUE",4) &&
      ColorConversionsU32("7/8",0x40080007,"8"))
    checks|=8;
  return checks;
}

U64 ColorConversionsShadow()
{
  CHashDefineStr *original,*owned;
  CExcept *first=Fs->next_except,*last=Fs->last_except;
  U8 buf[128];
  U64 checks=0;
  original=HashFind("ST_COLORS",Fs->hash_table,4);
  owned=DefineLstLoad("ST_COLORS","Tone%%\0%d\0");
  if (owned && owned!=original && HashFind("ST_COLORS",Fs->hash_table,4)==owned &&
      HashFind("ST_COLORS",Fs->hash_table,4,2)==original &&
      !HashFind("ST_COLORS",Fs->hash_table,4,3))
    checks|=1;
  if (ColorConversionsFormat(0,"Tone%"))
    checks|=2;
  MemSet(buf,0x5A,sizeof(buf));
  try {
    Color2Str(buf,1);
  } catch {
    if (Fs->except_ch=='StrPrint' && !*buf && buf[127]==0x5A &&
        HashFind("ST_COLORS",Fs->hash_table,4)==owned)
      checks|=4;
    Fs->catch_except=TRUE;
  }
  if (owned && ColorConversionsRemove(owned,Fs->hash_table) &&
      HashFind("ST_COLORS",Fs->hash_table,4)==original &&
      !HashFind("ST_COLORS",Fs->hash_table,4,2) &&
      Fs->next_except==first && Fs->last_except==last &&
      ColorConversionsFormat(4,"RED"))
    checks|=8;
  return checks;
}

U64 ColorConversionsHeap(I64 *retained,I64 *null_delta)
{
  U8 text[8];
  I64 before,after;
  CColorROPU16 a;
  CColorROPU32 b;
  U64 checks=0;
  StrCpy(text,"7/8");
  before=Fs->data_heap->used_u8s;
  b=Str2ColorU32(text);
  after=Fs->data_heap->used_u8s;
  *retained=after-before;
  if (b==0x40080007 && !StrCmp(text,"8"))
    checks|=1;
  if (*retained==24)
    checks|=2;
  before=Fs->data_heap->used_u8s;
  a=Str2ColorU16(NULL);
  b=Str2ColorU32(NULL);
  after=Fs->data_heap->used_u8s;
  *null_delta=after-before;
  if (a==16 && b==16 && !*null_delta)
    checks|=4;
  return checks;
}

U0 ColorConversionsProbe()
{
  U64 abi,delimiters,shadow,heap;
  I64 names,numeric,u16,u32,formatted,retained=0,null_delta=0;
  abi=ColorConversionsABI;
  names=ColorConversionsNamed;
  numeric=ColorConversionsNumeric;
  u16=ColorConversionsEdges;
  u32=ColorConversionsPairs;
  formatted=ColorConversionsFormats;
  delimiters=ColorConversionsDelimiters;
  shadow=ColorConversionsShadow;
  heap=ColorConversionsHeap(&retained,&null_delta);
  if (ColorConversionsDirectStack)
    abi|=16;
  if (ColorConversionsIndirectStack)
    abi|=32;
  DocClear(,TRUE);
  "CC abi %X\n",abi;
  "CC names %d\n",names;
  "CC numeric %d\n",numeric;
  "CC u16 %d\n",u16;
  "CC u32 %d\n",u32;
  "CC format %d\n",formatted;
  "CC delimiters %X\n",delimiters;
  "CC shadow %X\n",shadow;
  "CC heap %d %d %X\n",retained,null_delta,heap;
  "CC DONE\n";
}

ColorConversionsProbe;
