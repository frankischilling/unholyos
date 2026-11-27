// Test views preserve Kernel/KernelA.HH's hash record layouts.
class CLoadApiModule:CHash
{
  I64 user_data0,user_data1,user_data2;
};

class CLoadApiExport:CHashSrcSym
{
  I64 val;
};

class CLoadApiImage
{
  CBinFile *header;
  U8 *base,*end,*call;
  U64 *count,*anchor;
};

#assert sizeof(CBinFile)==32
#assert offset(CLoadApiModule.user_data0)==24
#assert offset(CLoadApiExport.val)==64

Bool LoadApiBytes(U8 *src,I64 size,U8 value)
{
  I64 i;
  for (i=0;i<size;i++)
    if (src[i]!=value)
      return FALSE;
  return TRUE;
}

Bool LoadApiRead(U8 *name,U8 *src,I64 size)
{
  I64 actual,i;
  U8 *read=FileRead(name,&actual);
  Bool okay=read && actual==size;
  for (i=0;okay && i<size;i++)
    okay=read[i]==src[i];
  Free(read);
  return okay;
}

Bool LoadApiWrite(U8 *name,U8 *src,I64 size)
{
  return FileWrite(name,src,size)>0 && LoadApiRead(name,src,size);
}

Bool LoadApiInside(CLoadApiImage *s,U64 addr,U64 size)
{
  return addr>=s->base && addr<=s->end && size<=s->end-addr;
}

Bool LoadApiImage(CLoadApiImage *s,CBinFile *header,U8 *key,
                 I64 org=INVALID_PTR)
{
  CLoadApiModule *module;
  CLoadApiExport *count,*anchor,*call;
  if (!header || header->bin_signature!=BIN_SIGNATURE_VAL ||
      header->org!=org || header->patch_table_offset<sizeof(CBinFile) ||
      header->patch_table_offset>=header->file_size)
    return FALSE;
  module=HashFind(key,Fs->hash_table,0x4000);
  if (!module || module->user_data0!=header ||
      !(module->type&0x1000000) || StrCmp(module->str,key))
    return FALSE;
  s->header=header;
  s->base=header(U8 *)+sizeof(CBinFile);
  s->end=header(U8 *)+header->patch_table_offset;
  count=HashFind("LoadApiCount",Fs->hash_table,1);
  anchor=HashFind("LoadApiAnchor",Fs->hash_table,1);
  call=HashFind("LoadApiCall",Fs->hash_table,1);
  if (!count || !anchor || !call || !LoadApiInside(s,count->val,8) ||
      !LoadApiInside(s,anchor->val,8) || !LoadApiInside(s,call->val,8))
    return FALSE;
  s->count=count->val;
  s->anchor=anchor->val;
  s->call=call->val;
  return TRUE;
}

Bool LoadApiCallOkay(CLoadApiImage *s,U64 target)
{
  U8 *site=s->call+3;
  if (HashFind("LateImportTarget",Fs->hash_table,2) ||
      s->call[0]!=0x6A || s->call[1]!=7 || s->call[2]!=0xE8 ||
      site+4+*site(I32 *)!=target)
    return FALSE;
  return Call(s->call)==42;
}

U64 LoadApiOverride(U8 *src,I64 size,U8 *name,U8 *key,I64 org,U64 target)
{
  U64 checks=0;
  I64 capacity;
  U8 *storage=MAlloc(size+128,Fs->code_heap),*destination;
  CBinFile *header;
  CLoadApiImage image;
  if (!storage)
    return 0;
  capacity=MSize(storage);
  MemSet(storage,0xA5,capacity);
  destination=storage+35;
  src(CBinFile *)->org=org;
  if (!LoadApiWrite(name,src,size)) {
    src(CBinFile *)->org=INVALID_PTR;
    Free(storage);
    return 0;
  }
  src(CBinFile *)->org=INVALID_PTR;
  header=Load(name,0,destination);
  if (header==destination && ((destination+sizeof(CBinFile))&63))
    checks|=1;
  if (header==destination && LoadApiImage(&image,header,key,org) &&
      *image.count==1 && *image.anchor==image.anchor &&
      LoadApiCallOkay(&image,target))
    checks|=2;
  if (LoadApiBytes(storage,35,0xA5) &&
      LoadApiBytes(destination+size,capacity-size-35,0xA5))
    checks|=4;
  // Nondebug CMemUsed stores the owner and size immediately before storage.
  if (storage(U64 *)[-2]==Fs->code_heap && MSize(storage)==capacity)
    checks|=8;
  // Keep caller storage alive while module and export records refer to it.
  return checks;
}

U0 LoadApiProbe()
{
  CLoadApiImage first,current;
  CLoadApiExport *target;
  CBinFile *provider,*header;
  U8 *saved=DirCur,*src=NULL,*storage=NULL,*destination;
  I64 size=0,count_offset=0,anchor_offset=0,capacity=0,error=0,align_bits=0;
  U64 paths=0,flags=0,override1=0,override2=0,failures=0,small=0,retained=0;
  U64 normal=0,compressed=0,plain=0,explicit_z=0,raw_anchor=0,target_addr=0;
  MemSet(&first,0,sizeof(first));
  if (!saved || !(src=FileRead("B:/LoadApiConsumer.BIN",&size)) ||
      size<sizeof(CBinFile) || !Cd("B:/LoadApi",TRUE))
    goto done;
  provider=Load("B:/LateImportProvider.BIN",LDF_SILENT);
  target=HashFind("LateImportTarget",Fs->hash_table,1);
  if (!provider || !target || target->val<provider(U8 *)+sizeof(CBinFile) ||
      target->val>=provider(U8 *)+provider->patch_table_offset)
    goto done;
  target_addr=target->val;

  if (!LoadApiWrite("Base.BIN",src,size) ||
      !LoadApiImage(&first,Load("Base"),"/LoadApi/Base"))
    goto done;
  count_offset=first.count(U8 *)-first.header(U8 *);
  anchor_offset=first.anchor(U8 *)-first.header(U8 *);
  raw_anchor=*(src+anchor_offset)(U64 *);
  align_bits=src(CBinFile *)->module_align_bits;
  normal=*first.count;
  if (normal==1 && *first.anchor==first.anchor && !(first.base(U64)&63) &&
      LoadApiCallOkay(&first,target_addr))
    paths|=1;

  *(src+count_offset)(U64 *)=16;
  if (!LoadApiWrite("Choice.BIN",src,size))
    goto done;
  *(src+count_offset)(U64 *)=32;
  if (!LoadApiWrite("Choice.BIN.Z",src,size))
    goto done;
  *(src+count_offset)(U64 *)=0;
  if (LoadApiImage(&current,Load("Choice"),"/LoadApi/Choice")) {
    compressed=*current.count;
    if (compressed==33 && LoadApiCallOkay(&current,target_addr))
      paths|=2;
  }
  if (LoadApiImage(&current,Load("Choice.BIN"),"/LoadApi/Choice")) {
    plain=*current.count;
    if (plain==17 && LoadApiCallOkay(&current,target_addr))
      paths|=4;
  }
  if (LoadApiImage(&current,Load("Choice.BIN.Z"),"/LoadApi/Choice")) {
    explicit_z=*current.count;
    if (explicit_z==33 && LoadApiCallOkay(&current,target_addr))
      paths|=8;
  }
  if (LoadApiWrite("Name.part.BIN",src,size) &&
      LoadApiImage(&current,Load("Name.part.BIN"),"/LoadApi/Name") &&
      !HashFind("/LoadApi/Name.part",Fs->hash_table,0x4000))
    paths|=16;
  if (Cd("Dotted.part",TRUE)) {
    if (LoadApiWrite("Inside.BIN",src,size) &&
        LoadApiImage(&current,Load("Inside.BIN"),"/LoadApi/Dotted") &&
        !HashFind("/LoadApi/Dotted.part/Inside",Fs->hash_table,0x4000))
      paths|=32;
    if (!Cd("B:/LoadApi"))
      goto done;
  }

  if (LoadApiImage(&current,Load("Base",LDF_JUST_LOAD),"/LoadApi/Base") &&
      !*current.count && *current.anchor==current.anchor &&
      LoadApiCallOkay(&current,target_addr))
    flags|=1;
  if (LoadApiImage(&current,Load("Base",LDF_NO_ABSS),"/LoadApi/Base")) {
    if (*current.count==1 && *current.anchor==raw_anchor)
      flags|=2;
    if (LoadApiCallOkay(&current,target_addr))
      flags|=4;
  }
  *(src+anchor_offset+4)(U32 *)=0x12345678;
  if (LoadApiWrite("High.BIN",src,size) &&
      LoadApiImage(&current,Load("High"),"/LoadApi/High") &&
      current.anchor(U32 *)[0]==(current.anchor(U64)&0xFFFFFFFF) &&
      current.anchor(U32 *)[1]==0x12345678 && *current.count==1 &&
      LoadApiCallOkay(&current,target_addr))
    flags|=8;
  *(src+anchor_offset)(U64 *)=raw_anchor;

  override1=LoadApiOverride(src,size,"Override.BIN","/LoadApi/Override",
                             0x12345000,target_addr);
  override2=LoadApiOverride(src,size,"Overflow.BIN","/LoadApi/Overflow",
                             0xFFFFFFFFFFFFFFF0,target_addr);
  storage=MAlloc(size+128,Fs->code_heap);
  if (!storage)
    goto done;
  capacity=MSize(storage);
  MemSet(storage,0x5A,capacity);
  destination=storage+35;
  if (!Load("Missing",0,destination) &&
      !HashFind("/LoadApi/Missing",Fs->hash_table,0x4000))
    failures|=1;
  if (LoadApiBytes(storage,capacity,0x5A) &&
      storage(U64 *)[-2]==Fs->code_heap && MSize(storage)==capacity)
    failures|=2;
  src(CBinFile *)->bin_signature^=1;
  if (LoadApiWrite("Bad.BIN",src,size)) {
    try {
      Load("Bad",0,destination);
    } catch {
      error=Fs->except_ch;
      Fs->catch_except=TRUE;
    }
  }
  src(CBinFile *)->bin_signature^=1;
  if (error=='BinModul' && !HashFind("/LoadApi/Bad",Fs->hash_table,0x4000))
    failures|=4;
  if (LoadApiBytes(storage,capacity,0x5A) &&
      storage(U64 *)[-2]==Fs->code_heap && MSize(storage)==capacity)
    failures|=8;

  src(CBinFile *)->module_align_bits=3;
  if (LoadApiWrite("Small.BIN",src,size) &&
      LoadApiImage(&current,Load("Small",LDF_JUST_LOAD),"/LoadApi/Small")) {
    header=current.header;
    if (header->module_align_bits==3 && !*current.count &&
        *current.anchor==current.anchor && LoadApiCallOkay(&current,target_addr))
      small|=1;
    if (Fs->code_heap==Fs->data_heap && header(I64 *)[-1]>0)
      small|=2;
    if (header(I64 *)[-1]>0 && header(U64 *)[-2]==Fs->data_heap &&
        MSize(header)>=size+1)
      small|=4;
  }
  src(CBinFile *)->module_align_bits=align_bits;
  if (*first.count==1 && *first.anchor==first.anchor &&
      LoadApiCallOkay(&first,target_addr))
    retained|=1;
  if (LoadApiRead("B:/LoadApiConsumer.BIN",src,size))
    retained|=2;
done:
  if (saved)
    Cd(saved);
  Free(saved);
  Free(src);
  Free(storage);
  DocClear(,TRUE);
  "LA paths %X counts %d %d %d %d\n",paths,normal,compressed,plain,explicit_z;
  "LA flags %X\nLA override %X %X\n",flags,override1,override2;
  "LA failures %X\nLA small %X\nLA retained %X\nLA DONE\n",
    failures,small,retained;
}

LoadApiProbe;
