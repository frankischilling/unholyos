// Primitive rows and registration from Terry A. Davis's Compiler/CInit.HC,
// Compiler/AsmInit.HC and PrsLib.HC, TempleOS
// c26482bb6ad3f80106d28504ec5db3c6a360732c.
class CInternalTypesRow
{
  U8 type,size,name[8];
};

class CInternalTypesExport:CHashSrcSym
{
  I64 val;
};

I16 union CInternalTypesAlias
{
  U8 bytes[2];
};

#assert sizeof(CInternalTypesRow)==10
#assert offset(CInternalTypesRow.type)==0
#assert offset(CInternalTypesRow.size)==1
#assert offset(CInternalTypesRow.name)==2
#assert sizeof(CHashClass)==128
#assert offset(CHashClass.ptr_stars_cnt)==84
#assert offset(CHashClass.raw_type)==85
#assert offset(CHashClass.flags)==86
#assert offset(CInternalTypesExport.val)==64
#assert RT_I0==2
#assert RT_U0==3

asm {
_INTERNAL_TYPES_RSP::
  LEA RAX,8[RSP]
  RET
}

_extern _INTERNAL_TYPES_RSP U64 InternalTypesRSP();

CInternalTypesRow InternalTypesExpected[17];
I64 InternalTypesEffects=0;

U0 InternalTypesSet(I64 i,U8 type,U8 size,U8 *name)
{
  InternalTypesExpected[i].type=type;
  InternalTypesExpected[i].size=size;
  StrCpy(InternalTypesExpected[i].name,name);
}

U0 InternalTypesInit()
{
  MemSet(InternalTypesExpected,0,sizeof(InternalTypesExpected));
  InternalTypesSet(0,2,0,"I0i");
  InternalTypesSet(1,2,0,"I0");
  InternalTypesSet(2,3,0,"U0i");
  InternalTypesSet(3,3,0,"U0");
  InternalTypesSet(4,4,1,"I8i");
  InternalTypesSet(5,4,1,"I8");
  InternalTypesSet(6,4,1,"Bool");
  InternalTypesSet(7,5,1,"U8i");
  InternalTypesSet(8,5,1,"U8");
  InternalTypesSet(9,6,2,"I16i");
  InternalTypesSet(10,7,2,"U16i");
  InternalTypesSet(11,8,4,"I32i");
  InternalTypesSet(12,9,4,"U32i");
  InternalTypesSet(13,10,8,"I64i");
  InternalTypesSet(14,11,8,"U64i");
  InternalTypesSet(15,14,8,"F64i");
  InternalTypesSet(16,14,8,"F64");
}

U64 InternalTypesTable(I64 *table_size,Bool *writable)
{
  CInternalTypesExport *entry=HashFind("internal_types_table",Fs->hash_table,1);
  CInternalTypesRow *rows;
  U64 checks=0;
  U8 saved,snapshot[170];
  I64 i,len;
  if (!entry || !entry->val)
    return 0;
  rows=entry->val;
  MemCpy(snapshot,rows,sizeof(snapshot));
  for (i=0;i<17;i++) {
    len=StrLen(InternalTypesExpected[i].name);
    if (rows[i].type==InternalTypesExpected[i].type &&
        rows[i].size==InternalTypesExpected[i].size && len<8 &&
        !MemCmp(rows[i].name,InternalTypesExpected[i].name,len+1)) {
      checks|=1<<i;
      *table_size+=10;
    }
  }
  // Touch unused name padding, then restore before any further compilation.
  saved=rows[0].name[7];
  rows[0].name[7]=0xA5;
  *writable=rows[0].name[7]==0xA5 &&
      HashFind("internal_types_table",Fs->hash_table,1)==entry;
  rows[0].name[7]=saved;
  if (MemCmp(rows,snapshot,sizeof(snapshot)))
    *writable=FALSE;
  return checks;
}

U64 InternalTypesMetadata(I64 *pointers)
{
  CInternalTypesRow *row;
  CHashClass *cls,*ptr;
  U64 checks=0;
  I64 i,j;
  for (i=0;i<17;i++) {
    row=&InternalTypesExpected[i];
    cls=HashFind(row->name,Fs->hash_table,32);
    if (cls && MSize(cls)>=5*sizeof(CHashClass)) {
      if (cls->type==32 && cls->flags==2 && !cls->ptr_stars_cnt &&
          cls->raw_type==row->type && cls->size==row->size &&
          !cls->member_cnt && !cls->base_class && !cls->fwd_class &&
          !StrCmp(cls->str,row->name))
        checks|=1<<i;
      for (j=1;j<=4;j++) {
        ptr=cls+j;
        if (ptr->type==16 && !ptr->flags && ptr->ptr_stars_cnt==j &&
            ptr->raw_type==10 && ptr->size==8 && !ptr->str &&
            !ptr->member_cnt && !ptr->base_class && !ptr->fwd_class)
          (*pointers)++;
      }
    }
  }
  return checks;
}

Bool InternalTypesViewValue(I64 i)
{
  I16 a;
  U16 b;
  I32 c;
  U32 d;
  I64 e;
  U64 f;
  switch (i) {
    case 0:
      a=0x80FF;
      return a.u8[0]==255 && a.i8[0]==-1 &&
          a.u8[1]==128 && a.i8[1]==-128;
    case 1:
      b=0x80FF;
      return b.u8[0]==255 && b.i8[0]==-1 &&
          b.u8[1]==128 && b.i8[1]==-128;
    case 2:
      c=0x8000FFFF;
      return c.u16[0]==65535 && c.i16[0]==-1 &&
          c.u16[1]==32768 && c.i16[1]==-32768;
    case 3:
      d=0x8000FFFF;
      return d.u16[0]==65535 && d.i16[0]==-1 &&
          d.u16[1]==32768 && d.i16[1]==-32768;
    case 4:
      e=0x80000000FFFFFFFF;
      return e.u32[0]==0xFFFFFFFF && e.i32[0]==-1 &&
          e.u32[1]==0x80000000 && e.i32[1]==-0x80000000;
    case 5:
      f=0x80000000FFFFFFFF;
      return f.u32[0]==0xFFFFFFFF && f.i32[0]==-1 &&
          f.u32[1]==0x80000000 && f.i32[1]==-0x80000000;
  }
  return FALSE;
}

U64 InternalTypesViews(I64 *members)
{
  U8 *name="I16\0U16\0I32\0U32\0I64\0U64\0",backing[8];
  U8 *member_names="i8\0u8\0i16\0u16\0i32\0u32\0",*member_name;
  CHashClass *cls,*raw,*member_type;
  CMemberLst *member;
  I64 i,j,size=2,count,width;
  U64 checks=0;
  Bool okay;
  for (i=0;i<6;i++) {
    if (i==2)
      size=4;
    if (i==4)
      size=8;
    count=size;
    if (size==8)
      count=6;
    StrPrint(backing,"%si",name);
    cls=HashFind(name,Fs->hash_table,16);
    raw=HashFind(backing,Fs->hash_table,32);
    okay=cls && raw && (cls->type&0xFFFF)==16 && !(cls->flags&2) &&
        !cls->ptr_stars_cnt && cls->raw_type==10 && cls->size==size &&
        cls->fwd_class==raw && raw->raw_type==6+i && cls->member_cnt==count;
    member_name=member_names;
    width=1;
    for (j=0;j<count;j++) {
      if (j==2)
        width=2;
      if (j==4)
        width=4;
      member=NULL;
      if (cls)
        member=MemberFind(member_name,cls);
      member_type=NULL;
      if (member)
        member_type=member->member_class;
      while (member_type && member_type->fwd_class)
        member_type=member_type->fwd_class;
      if (member && !member->offset && member->size==size &&
          member_type && member_type->size==width && member_type->raw_type==4+j &&
          member->dim.total_cnt==size/width && member->dim.next &&
          member->dim.next->cnt==size/width)
        (*members)++;
      else
        okay=FALSE;
      member_name+=StrLen(member_name)+1;
    }
    if (okay && InternalTypesViewValue(i))
      checks|=1<<i;
    name+=StrLen(name)+1;
  }
  return checks;
}

U64 InternalTypesLayout()
{
  U64 checks=0;
  CInternalTypesAlias alias;
  CHashClass *cls=HashFind("CInternalTypesAlias",Fs->hash_table,16);
  CHashClass *view=HashFind("I16",Fs->hash_table,16);
  CHashClass *raw=HashFind("I16i",Fs->hash_table,32);
  alias=-2;
  if (!sizeof(I0) && !sizeof(I0i) && !sizeof(U0) && !sizeof(U0i))
    checks|=1;
  if (sizeof(I8)==1 && sizeof(I8i)==1 && sizeof(Bool)==1 &&
      sizeof(U8)==1 && sizeof(U8i)==1)
    checks|=2;
  if (sizeof(I16i)==2 && sizeof(U16i)==2 && sizeof(I32i)==4 &&
      sizeof(U32i)==4 && sizeof(I64i)==8 && sizeof(U64i)==8 &&
      sizeof(F64i)==8 && sizeof(F64)==8)
    checks|=4;
  if (sizeof(I0 *)==8 && sizeof(I0i *)==8 && sizeof(U0 *)==8 &&
      sizeof(U0i *)==8 && sizeof(I0 **)==8 && sizeof(I0 ***)==8 &&
      sizeof(I0 ****)==8)
    checks|=8;
  if (sizeof(I16)==2 && sizeof(U16)==2 && sizeof(I32)==4 &&
      sizeof(U32)==4 && sizeof(I64)==8 && sizeof(U64)==8)
    checks|=16;
  if (sizeof(InternalTypesExpected)==170 && RT_I0==2 && RT_U0==3 &&
      sizeof(CHashClass)==128 && sizeof(CInternalTypesRow)==10 &&
      sizeof(CInternalTypesAlias)==2 && alias==-2 && alias.bytes[0]==254 &&
      alias.bytes[1]==255 && cls && view && raw && cls->fwd_class==view &&
      view->fwd_class==raw && cls->raw_type==10)
    checks|=32;
  return checks;
}

F64 InternalTypesFloat(I64 value)
{
  InternalTypesEffects+=value;
  return 1.0;
}

I0 InternalTypesI0(I64 value)
{
  InternalTypesFloat(value)(I0);
}

I0i InternalTypesI0i(I64 value)
{
  InternalTypesFloat(value)(I0i);
}

U0 InternalTypesU0(I64 value)
{
  InternalTypesFloat(value)(U0);
}

U0i InternalTypesU0i(I64 value)
{
  InternalTypesFloat(value)(U0i);
}

U64 InternalTypesVoid()
{
  I0 (*i0)(I64);
  I0i (*i0i)(I64);
  U0 (*u0)(I64);
  U0i (*u0i)(I64);
  U64 before,after,stack=0;
  i0=&InternalTypesI0;
  i0i=&InternalTypesI0i;
  u0=&InternalTypesU0;
  u0i=&InternalTypesU0i;
  InternalTypesEffects=0;
  before=InternalTypesRSP;
  InternalTypesI0(1)(U0);
  after=InternalTypesRSP;
  if (before==after) stack|=1;
  before=InternalTypesRSP;
  InternalTypesI0i(2);
  after=InternalTypesRSP;
  if (before==after) stack|=2;
  before=InternalTypesRSP;
  InternalTypesU0(4)(I0i);
  after=InternalTypesRSP;
  if (before==after) stack|=4;
  before=InternalTypesRSP;
  InternalTypesU0i(8);
  after=InternalTypesRSP;
  if (before==after) stack|=8;
  before=InternalTypesRSP;
  i0(16);
  after=InternalTypesRSP;
  if (before==after) stack|=16;
  before=InternalTypesRSP;
  i0i(32);
  after=InternalTypesRSP;
  if (before==after) stack|=32;
  before=InternalTypesRSP;
  u0(64);
  after=InternalTypesRSP;
  if (before==after) stack|=64;
  before=InternalTypesRSP;
  u0i(128);
  after=InternalTypesRSP;
  if (before==after) stack|=128;
  return stack;
}

U64 InternalTypesRuntime()
{
  U64 checks=0;
  U8 *buf=MAlloc(16);
  if (StrLen("types")==5 && !StrCmp("types","types"))
    checks|=1;
  if (buf) {
    MemSet(buf,0xA5,16);
    if (buf[0]==0xA5 && buf[15]==0xA5)
      checks|=2;
    Free(buf);
  }
  if (Sqrt(9.0)==3.0)
    checks|=4;
  if (DefineCnt("ST_RAW_TYPES")==16 && DefineMatch("I0","ST_RAW_TYPES")==2)
    checks|=8;
  return checks;
}

U0 InternalTypesProbe()
{
  I64 table_size=0,pointers=0,members=0;
  Bool writable=FALSE,float_okay;
  U64 table,metadata,views,layout,stack,runtime;
  InternalTypesInit;
  table=InternalTypesTable(&table_size,&writable);
  metadata=InternalTypesMetadata(&pointers);
  views=InternalTypesViews(&members);
  layout=InternalTypesLayout;
  stack=InternalTypesVoid;
  float_okay=InternalTypesFloat(0)+2.0==3.0;
  runtime=InternalTypesRuntime;
  DocClear(,TRUE);
  "IT table %X size %d writable %d\n",table,table_size,writable;
  "IT metadata %X pointers %d\n",metadata,pointers;
  "IT views %X members %d\n",views,members;
  "IT layout %X\n",layout;
  "IT void %X stack %X\n",InternalTypesEffects,stack;
  "IT float %d\n",float_okay;
  "IT runtime %X\n",runtime;
  "IT DONE\n";
}

InternalTypesProbe;
