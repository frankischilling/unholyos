extern U0 LoadPass2(U8 *src,U8 *module_base,I64 flags);

class CLoadPass2Module:CHash
{
  I64 user_data0,user_data1,user_data2;
};

class CLoadPass2Export:CHashSrcSym
{
  I64 val;
};

class CLoadPass2Import:CHashSrcSym
{
  U8 *module_base,*module_header_entry;
};

class CLoadPass2Image
{
  U64 header,base,end,image_end;
};

#assert offset(CLoadPass2Module.user_data0)==24
#assert offset(CLoadPass2Export.val)==64
#assert offset(CLoadPass2Import.module_base)==64
#assert offset(CLoadPass2Import.module_header_entry)==72

U64 LoadPass2Order=0,LoadPass2Calls=0,LoadPass2Never=0;

U0 LoadPass2First()
{
  LoadPass2Order=LoadPass2Order*10+1;
  LoadPass2Calls++;
}

U0 LoadPass2Second()
{
  LoadPass2Order=LoadPass2Order*10+2;
  LoadPass2Calls++;
}

U0 LoadPass2Forbidden()
{
  LoadPass2Never++;
}

U0 LoadPass2Throw()
{
  LoadPass2Order=LoadPass2Order*10+9;
  LoadPass2Calls++;
  throw('Pass2Tag');
}

U8 *LoadPass2Record(U8 *dst,U8 type,U32 value,U8 *name)
{
  I64 size=StrLen(name)+1;
  *dst++=type;
  *dst(U32 *)=value;
  dst+=sizeof(U32);
  MemCpy(dst,name,size);
  return dst+size;
}

U8 *LoadPass2Tail(U8 *dst,U8 type,U32 count)
{
  I64 size=count*sizeof(U32);
  dst=LoadPass2Record(dst,type,count,"skip tail");
  // Kernel/KernelA.HH: ABS=20, CODE/ZEROED_CODE=21/22, DATA/ZEROED_DATA=23/24.
  if (type==21 || type==22)
    size+=sizeof(U32);
  else if (type==23 || type==24)
    size+=sizeof(U64);
  MemSet(dst,0,size);
  return dst+size;
}

Bool LoadPass2Same(U8 *a,U8 *b,I64 size)
{
  I64 i;
  for (i=0;i<size;i++)
    if (a[i]!=b[i])
      return FALSE;
  return TRUE;
}

Bool LoadPass2Inside(CLoadPass2Image *s,U64 address,U64 size)
{
  return address>=s->base && address<=s->end && size<=s->end-address;
}

Bool LoadPass2Image(CLoadPass2Image *s,CBinFile *header,U8 *key)
{
  CLoadPass2Module *module;
  if (!header || header->bin_signature!=BIN_SIGNATURE_VAL ||
      header->org!=INVALID_PTR || header->patch_table_offset<sizeof(CBinFile) ||
      header->patch_table_offset>=header->file_size)
    return FALSE;
  module=HashFind(key,Fs->hash_table,0x4000);
  if (!module || module->user_data0!=header)
    return FALSE;
  s->header=header;
  s->base=s->header+sizeof(CBinFile);
  s->end=s->header+header->patch_table_offset;
  s->image_end=s->header+header->file_size;
  return TRUE;
}

U0 LoadPass2Probe()
{
  U8 *storage=MAlloc(1024),*snapshot=MAlloc(1024),*src,*dst,*record,*site;
  U64 first=&LoadPass2First,second=&LoadPass2Second;
  U64 forbidden=&LoadPass2Forbidden,thrower=&LoadPass2Throw,checks=0;
  U64 order1=0,order2=0,order3=0,table_calls=0,unsigned1=0,unsigned2=0;
  U64 zero=0,end=0,error=0,throw_order=0,throw_calls=0,resumed=0,returned=0;
  U64 just=0,pass1=0,pass2=0,resolved=0,result=0,again=0,offset=0;
  I64 type;
  CLoadPass2Image consumer,provider;
  CLoadPass2Export *count,*call,*target;
  CLoadPass2Import *pending;
  if (!storage || !snapshot || first>0xFFFFFFFF || second>0xFFFFFFFF ||
      forbidden>0xFFFFFFFF || thrower>0xFFFFFFFF)
    goto done;
  MemSet(storage,0xA5,1024);
  src=storage+17;
  dst=LoadPass2Record(src,25,first,"first entry");
  for (type=20;type<=24;type++) {
    dst=LoadPass2Tail(dst,type,0);
    dst=LoadPass2Tail(dst,type,2);
  }
  dst=LoadPass2Record(dst,8,0,"ignored import");
  dst=LoadPass2Record(dst,16,0xDEADBEEF,"ignored export");
  dst=LoadPass2Record(dst,255,0x80000000,"unknown record");
  dst=LoadPass2Record(dst,25,second,"second entry");
  *dst++=0;
  dst=LoadPass2Record(dst,25,forbidden,"after END");
  *dst++=0;
  if (dst>storage+1008 || !(src(U64)&7))
    goto done;
  MemCpy(snapshot,storage,1024);
  LoadPass2(src,0,0);
  order1=LoadPass2Order;
  if (order1==12 && LoadPass2Calls==2 && !LoadPass2Never)
    checks|=1;
  LoadPass2(src,0,LDF_JUST_LOAD);
  order2=LoadPass2Order;
  if (order2==1212 && LoadPass2Calls==4 && !LoadPass2Never)
    checks|=2;
  LoadPass2(src,0,LDF_NO_ABSS|LDF_SILENT|0x8000000000000000);
  order3=LoadPass2Order;
  table_calls=LoadPass2Calls;
  if (order3==121212 && table_calls==6 && !LoadPass2Never)
    checks|=4;
  if (LoadPass2Same(storage,snapshot,1024))
    checks|=8;

  dst=LoadPass2Record(src,25,0x80000000,"high bit");
  *dst=0;
  LoadPass2Order=LoadPass2Calls=0;
  LoadPass2(src,first-0x80000000,0);
  unsigned1=LoadPass2Order;
  if (unsigned1==1 && LoadPass2Calls==1)
    checks|=16;
  dst=LoadPass2Record(src,25,0xFFFFFFFF,"maximum U32");
  *dst=0;
  LoadPass2Order=LoadPass2Calls=0;
  LoadPass2(src,second-0xFFFFFFFF,0);
  unsigned2=LoadPass2Order;
  if (unsigned2==2 && LoadPass2Calls==1)
    checks|=32;

  dst=LoadPass2Record(src,25,0,"null target");
  dst=LoadPass2Record(dst,25,first,"");
  *dst=0;
  LoadPass2Order=LoadPass2Calls=0;
  LoadPass2(src,0,LDF_JUST_LOAD);
  zero=LoadPass2Order==1 && LoadPass2Calls==1;
  if (zero)
    checks|=64;
  *src=0;
  dst=LoadPass2Record(src+1,25,forbidden,"never reached");
  *dst=0;
  LoadPass2(src,0,-1);
  end=LoadPass2Order==1 && LoadPass2Calls==1 && !LoadPass2Never;
  if (end)
    checks|=128;

  dst=LoadPass2Record(src,25,first,"before throw");
  dst=LoadPass2Record(dst,25,thrower,"throw");
  dst=LoadPass2Record(dst,25,forbidden,"after throw");
  *dst=0;
  LoadPass2Order=LoadPass2Calls=0;
  try {
    LoadPass2(src,0,0);
    returned=1;
  } catch {
    error=Fs->except_ch;
    Fs->catch_except=TRUE;
    resumed=1;
  }
  throw_order=LoadPass2Order;
  throw_calls=LoadPass2Calls;
  if (error=='Pass2Tag' && resumed && !returned)
    checks|=256;
  if (throw_order==19 && throw_calls==2 && !LoadPass2Never)
    checks|=512;

  if (!LoadPass2Image(&consumer,
        Load("B:/LateImportConsumer.BIN",LDF_SILENT|LDF_JUST_LOAD),
        "/LateImportConsumer"))
    goto done;
  count=HashFind("LateImportCount",Fs->hash_table,1);
  call=HashFind("LateImportCall",Fs->hash_table,1);
  pending=HashFind("LateImportTarget",Fs->hash_table,2);
  if (!count || !call || !LoadPass2Inside(&consumer,count->val,8) ||
      !LoadPass2Inside(&consumer,call->val,8) || !pending ||
      pending->module_base!=consumer.base ||
      HashFind("LateImportTarget",Fs->hash_table,2,2))
    goto done;
  record=pending->module_header_entry;
  if (record<consumer.end || record>consumer.image_end-22 || *record!=8 ||
      StrCmp(record+5,"LateImportTarget"))
    goto done;
  offset=*(record+1)(U32 *);
  site=consumer.base+offset;
  if (!LoadPass2Inside(&consumer,site,4) || site<=consumer.base || site[-1]!=0xE8)
    goto done;
  just=*count->val(U64 *);
  if (!just)
    checks|=1024;
  LoadPass2(consumer.end,consumer.base,LDF_JUST_LOAD);
  pass1=*count->val(U64 *);
  if (pass1==1 && pending->type==2 &&
      HashFind("LateImportTarget",Fs->hash_table,2)==pending)
    checks|=2048;
  LoadPass2(consumer.end,consumer.base,0);
  pass2=*count->val(U64 *);
  if (pass2==2 && pending->type==2 &&
      HashFind("LateImportTarget",Fs->hash_table,2)==pending)
    checks|=4096;
  if (!LoadPass2Image(&provider,Load("B:/LateImportProvider.BIN",LDF_SILENT),
                      "/LateImportProvider"))
    goto done;
  target=HashFind("LateImportTarget",Fs->hash_table,1);
  resolved=*count->val(U64 *);
  if (!target || !LoadPass2Inside(&provider,target->val,1) ||
      pending->type || HashFind("LateImportTarget",Fs->hash_table,2) ||
      site+4+*site(I32 *)!=target->val)
    goto done;
  if (resolved==2)
    checks|=8192;
  result=Call(call->val);
  if (result==42 && *count->val(U64 *)==2)
    checks|=16384;
  LoadPass2(consumer.end,consumer.base,LDF_NO_ABSS);
  again=*count->val(U64 *);
  if (again==3 && !LoadPass2Never)
    checks|=32768;
done:
  Free(snapshot);
  Free(storage);
  DocClear(,TRUE);
  "LP table %d %d %d %d\n",order1,order2,order3,table_calls;
  "LP unsigned %d %d\nLP empty %d %d\n",unsigned1,unsigned2,zero,end;
  "LP throw %X %d %d %d\n",error,throw_order,throw_calls,resumed;
  "LP module %d %d %d %d %d %d\n",just,pass1,pass2,resolved,result,again;
  "LP checks %X\nLP DONE\n",checks;
}

LoadPass2Probe;
