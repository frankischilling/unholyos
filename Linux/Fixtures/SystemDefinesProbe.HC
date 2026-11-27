class CSystemDefinesCase
{
  U8 *name,*expected;
  I64 cnt;
};

#assert sizeof(CHashDefineStr)==88
#assert offset(CHashDefineStr.data)==64
#assert offset(CHashDefineStr.sub_idx)==72
#assert offset(CHashDefineStr.cnt)==80

CSystemDefinesCase SystemDefinesCases[10];

U0 SystemDefinesCase(I64 i,U8 *name,U8 *expected,I64 cnt)
{
  SystemDefinesCases[i].name=name;
  SystemDefinesCases[i].expected=expected;
  SystemDefinesCases[i].cnt=cnt;
}

U0 SystemDefinesCasesInit()
{
  // List bytes from Terry A. Davis's Kernel/KDefine.HC::SysDefinesLoad,
  // TempleOS c26482bb6ad3f80106d28504ec5db3c6a360732c.
  SystemDefinesCase(0,"ST_OFF_ON","Off\0On\0",2);
  SystemDefinesCase(1,"ST_HTT_TYPES","ExportSysSym\0ImportSysSym\0DefineStr\0GlbVar\0"
        "Class\0IntType\0Funct\0Word\0DictWord\0KeyWord\0AsmKeyWord\0OpCode\0"
        "Reg\0File\0Module\0HelpFile\0Frame Ptr\0 \0 \0 \0 \0 \0 \0Private\0"
        "Public\0Export\0Import\0Imm\0Goto\0Res\0Unres\0Local\0",32);
  SystemDefinesCase(2,"ST_DAYS_OF_WEEK","Sunday\0Monday\0Tuesday\0Wednesday\0"
        "Thursday\0Friday\0Saturday\0",7);
  SystemDefinesCase(3,"ST_MONTHS","January\0February\0March\0April\0May\0"
        "June\0July\0August\0September\0October\0November\0December\0",12);
  SystemDefinesCase(4,"ST_FILE_ATTRS","R\0H\0S\0V\0D\0A\0 \0 \0X\0T\0Z\0C\0F\0",13);
  SystemDefinesCase(5,"ST_FILE_UTIL_FLAGS","r\0d\0i\0a\0c\0R\0p\0m\0x\0s\0"
        "D\0F\0T\0$$\0S\0A\0J\0G\0Z\0O\0P\0f\0l\0lb\0la\0",25);
  SystemDefinesCase(6,"ST_BLKDEV_TYPES",
        "NULL\0RAM\0ATA\0FILE_READ\0FILE_WRITE\0ATAPI\0",6);
  SystemDefinesCase(7,"ST_DRV_TYPES",
        "NULL\0REDSEA\0FAT32\0ISO9660\0NTFS\0UNKNOWN\0",6);
  SystemDefinesCase(8,"ST_COLORS","BLACK\0BLUE\0GREEN\0CYAN\0"
        "RED\0PURPLE\0BROWN\0LTGRAY\0DKGRAY\0LTBLUE\0LTGREEN\0"
        "LTCYAN\0LTRED\0LTPURPLE\0YELLOW\0WHITE\0",16);
  SystemDefinesCase(9,"ST_INT_NAMES","Divide Error\0SingleStep\0NMI\0Breakpoint\0"
        "Overflow\0BOUND Range Exceeded\0Invalid Opcode\0No Math Coprocessor\0"
        "Double Fault\0Coprocessor Segment Fault\0Invalid TASK\0"
        "Segment Not Present\0Stk Segment Fault\0General Protection\0"
        "Page Fault\0 \0Math Fault\0Alignment Check\0Machine Check\0"
        "SIMD Exception\0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0"
        " \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0MP Crash\0Wake\0Dbg\0",51);
}

U64 SystemDefinesInspect(I64 *entries,I64 *bytes,U64 *copied,U64 *indexed)
{
  CSystemDefinesCase *c;
  CHashDefineStr *record;
  U8 *expected,*sub,*origin;
  I64 i,j,size,matches;
  U64 lists=0;
  for (i=0;i<10;i++) {
    c=&SystemDefinesCases[i];
    record=HashFind(c->name,Fs->hash_table,4);
    if (!record || record->type!=4 || record->cnt!=c->cnt ||
        StrCmp(record->str,c->name) || HashFind(c->name,Fs->hash_table,4,2) ||
        !record->data || !record->sub_idx ||
        MSize(record->sub_idx)<c->cnt*sizeof(U8 *) ||
        Define(c->name)!=record->data || DefineCnt(c->name)!=c->cnt ||
        DefineSub(-1,c->name) || DefineSub(c->cnt,c->name))
      goto next_list;
    lists|=1<<i;
    origin=record->sub_idx[0];
    expected=c->expected;
    matches=0;
    for (j=0;j<c->cnt;j++) {
      sub=DefineSub(j,c->name);
      if (origin && sub && sub==record->sub_idx[j] &&
          sub==origin+(expected-c->expected) && !StrCmp(sub,expected)) {
        (*entries)++;
        matches++;
      }
      expected+=StrLen(expected)+1;
    }
    size=expected+1-c->expected;
    if (!*expected && MSize(record->data)>=size &&
        !MemCmp(record->data,c->expected,size)) {
      *copied|=1<<i;
      *bytes+=size;
    }
    if (origin && origin!=record->data && matches==c->cnt &&
        !*expected && !MemCmp(origin,c->expected,size))
      *indexed|=1<<i;
next_list:;
  }
  return lists;
}

U64 SystemDefinesConsumers(U8 **date_text)
{
  U64 checks=0;
  U8 *text,*dollar;
  CDate date;
  CDateStruct ds;
  if (DefineCnt("ST_OFF_ON")==2 && DefineMatch("On","ST_OFF_ON")==1 &&
      !StrCmp(DefineSub(0,"ST_OFF_ON"),"Off"))
    checks|=1;
  if (DefineMatch("DefineStr","ST_HTT_TYPES")==2 &&
      DefineMatch("Private","ST_HTT_TYPES")==23 &&
      DefineMatch("Local","ST_HTT_TYPES")==31 &&
      !StrCmp(DefineSub(16,"ST_HTT_TYPES"),"Frame Ptr") &&
      !StrCmp(DefineSub(17,"ST_HTT_TYPES")," ") &&
      !StrCmp(DefineSub(22,"ST_HTT_TYPES")," "))
    checks|=2;
  if (DefineMatch("C","ST_FILE_ATTRS")==11 &&
      DefineMatch("X","ST_FILE_ATTRS")==8 &&
      !StrCmp(DefineSub(6,"ST_FILE_ATTRS")," ") &&
      !StrCmp(DefineSub(7,"ST_FILE_ATTRS")," "))
    checks|=4;
  if (DefineMatch("RAM","ST_BLKDEV_TYPES")==1 &&
      DefineMatch("ATAPI","ST_BLKDEV_TYPES")==5 &&
      DefineMatch("REDSEA","ST_DRV_TYPES")==1 &&
      DefineMatch("ISO9660","ST_DRV_TYPES")==3)
    checks|=8;
  dollar=DefineSub(13,"ST_FILE_UTIL_FLAGS");
  if (dollar && dollar[0]==36 && !dollar[1] &&
      DefineMatch("$$","ST_FILE_UTIL_FLAGS")==13 &&
      DefineMatch("lb","ST_FILE_UTIL_FLAGS")==23 &&
      DefineMatch("la","ST_FILE_UTIL_FLAGS")==24)
    checks|=16;
  text=MStrPrint("%Z|%Z|%Z",0,"ST_COLORS",7,"ST_COLORS",15,"ST_COLORS");
  if (text && !StrCmp(text,"BLACK|LTGRAY|WHITE") &&
      DefineMatch("LTRED","ST_COLORS")==12)
    checks|=32;
  Free(text);
  text=MStrPrint("%Z|%Z|%Z|%Z",14,"ST_INT_NAMES",48,"ST_INT_NAMES",
      49,"ST_INT_NAMES",50,"ST_INT_NAMES");
  if (text && !StrCmp(text,"Page Fault|MP Crash|Wake|Dbg") &&
      !StrCmp(DefineSub(15,"ST_INT_NAMES")," ") &&
      !StrCmp(DefineSub(20,"ST_INT_NAMES")," ") &&
      !StrCmp(DefineSub(47,"ST_INT_NAMES")," "))
    checks|=64;
  Free(text);
  // Demo/DateTime.HC's consumers with a fixed 2020-02-29 input.
  date.date=737848;
  date.time=0;
  Date2Struct(&ds,date);
  *date_text=MStrPrint("%03tZ|%03tZ",ds.day_of_week,"ST_DAYS_OF_WEEK",
      ds.mon-1,"ST_MONTHS");
  if (*date_text && !StrCmp(*date_text,"Sat|Feb") &&
      ds.year==2020 && ds.mon==2 && ds.day_of_week==6)
    checks|=128;
  return checks;
}

U0 SystemDefinesProbe()
{
  U64 lists,copied=0,indexed=0,consumers=0,flags=0,first_flags=0;
  I64 entries=0,bytes=0;
  U8 *date_text=NULL;
  SystemDefinesCasesInit;
  lists=SystemDefinesInspect(&entries,&bytes,&copied,&indexed);
  if (lists==0x3FF) {
    consumers=SystemDefinesConsumers(&date_text);
    ScanFlags(&flags,Define("ST_FILE_UTIL_FLAGS"),"+r+d+$$");
    first_flags=flags;
    ScanFlags(&flags,Define("ST_FILE_UTIL_FLAGS"),"+la+lb+f-r-d-$$");
  }
  DocClear(,TRUE);
  "SY lists %X entries %d bytes %d\n",lists,entries,bytes;
  "SY storage %X %X\n",copied,indexed;
  "SY consumers %X\n",consumers;
  "SY flags %X %X\n",first_flags,flags;
  "SY date %s\n",date_text;
  Free(date_text);
  "SY DONE\n";
}

SystemDefinesProbe;
