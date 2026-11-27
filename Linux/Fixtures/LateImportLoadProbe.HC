extern U0 LoadPass2(U8 *src,U8 *module_base,I64 flags);

class CLateImportImage
{
  U64 header,base,end,image_end,count,call;
};

Bool LateImportInside(CLateImportImage *s,U64 address,U64 size)
{
  return address>=s->base && address<=s->end && size<=s->end-address;
}

Bool LateImportImage(CLateImportImage *s,CBinFile *header,U8 *name)
{
  CHashGeneric *module;
  if (!header)
    return FALSE;
  module=HashSingleTableFind(name,Fs->hash_table,HTT_MODULE);
  if (!module || module->user_data0!=header ||
      header->bin_signature!=BIN_SIGNATURE_VAL || header->org!=INVALID_PTR ||
      header->patch_table_offset<sizeof(CBinFile) ||
      header->patch_table_offset>=header->file_size)
    return FALSE;
  s->header=header;
  s->base=s->header+sizeof(CBinFile);
  s->end=s->header+header->patch_table_offset;
  s->image_end=s->header+header->file_size;
  return TRUE;
}

Bool LateImportExports(CLateImportImage *s)
{
  CHashExport *count,*call;
  count=HashSingleTableFind("LateImportCount",Fs->hash_table,HTT_EXPORT_SYS_SYM);
  call=HashSingleTableFind("LateImportCall",Fs->hash_table,HTT_EXPORT_SYS_SYM);
  if (!count || !call || !LateImportInside(s,count->val,8) ||
      !LateImportInside(s,call->val,1) || count->val&7)
    return FALSE;
  s->count=count->val;
  s->call=call->val;
  return TRUE;
}

U64 LateImportPending()
{
  U64 count=0;
  while (HashSingleTableFind("LateImportTarget",Fs->hash_table,
                            HTT_IMPORT_SYS_SYM,count+1))
    count++;
  return count;
}

U0 LateImportLoadProbe()
{
  CLateImportImage first,provider,second;
  CHashImport *pending;
  CHashExport *target;
  U8 *record,*site;
  U64 checks=0,default_count=0,pending_default=0,repeat_count=0,pending_repeat=0;
  U64 resolved_count=0,pending_resolved=0,result=0,again_count=0,just_count=0;
  U64 pass1=0,pass2=0,offset=0;
  MemSet(&first,0,sizeof(first));
  MemSet(&provider,0,sizeof(provider));
  MemSet(&second,0,sizeof(second));
  if (!LateImportImage(&first,Load("B:/LateImportConsumer.BIN",LDF_SILENT),
                       "/LateImportConsumer"))
    goto done;
  checks|=1;
  if (!LateImportExports(&first))
    goto done;
  default_count=*first.count(U64 *);
  pending_default=LateImportPending;
  if (default_count==1)
    checks|=2;
  pending=HashSingleTableFind("LateImportTarget",Fs->hash_table,
                              HTT_IMPORT_SYS_SYM);
  if (!pending || pending_default!=1 || pending->module_base!=first.base)
    goto done;
  record=pending->module_header_entry;
  if (record<first.end || record>first.image_end-22 || *record!=IET_REL_I32 ||
      StrCmp(record+5,"LateImportTarget"))
    goto done;
  offset=*(record+1)(U32 *);
  if (!LateImportInside(&first,first.base+offset,4))
    goto done;
  site=first.base+offset;
  checks|=4;
  LoadPass2(first.end,first.base,0);
  repeat_count=*first.count(U64 *);
  pending_repeat=LateImportPending;
  if (repeat_count==2 && pending_repeat==1 && pending->type==HTT_IMPORT_SYS_SYM &&
      HashSingleTableFind("LateImportTarget",Fs->hash_table,
                          HTT_IMPORT_SYS_SYM)==pending)
    checks|=8;
  if (!LateImportImage(&provider,Load("B:/LateImportProvider.BIN",LDF_SILENT),
                       "/LateImportProvider"))
    goto done;
  target=HashSingleTableFind("LateImportTarget",Fs->hash_table,
                             HTT_EXPORT_SYS_SYM);
  if (!target || !LateImportInside(&provider,target->val,1))
    goto done;
  checks|=16;
  resolved_count=*first.count(U64 *);
  pending_resolved=LateImportPending;
  if (pending_resolved || pending->type!=HTT_INVALID)
    goto done;
  if (resolved_count==2)
    checks|=32;
  if (site<=first.base || site[-1]!=0xE8 || site+4+*site(I32 *)!=target->val)
    goto done;
  checks|=64;
  result=Call(first.call);
  if (result==42 && *first.count(U64 *)==2)
    checks|=128;
  LoadPass2(first.end,first.base,0);
  again_count=*first.count(U64 *);
  if (again_count==3)
    checks|=256;
  if (!LateImportImage(&second,
        Load("B:/LateImportConsumer.BIN",LDF_SILENT|LDF_JUST_LOAD),
        "/LateImportConsumer") || !LateImportExports(&second))
    goto done;
  if (second.header!=first.header)
    checks|=512;
  just_count=*second.count(U64 *);
  if (LateImportPending)
    goto done;
  if (!just_count)
    checks|=1024;
  LoadPass2(second.end,second.base,0);
  pass1=*second.count(U64 *);
  if (pass1==1)
    checks|=2048;
  LoadPass2(second.end,second.base,0);
  pass2=*second.count(U64 *);
  if (pass2==2)
    checks|=4096;
  site=second.base+offset;
  if (LateImportInside(&second,site,4) && site>second.base && site[-1]==0xE8 &&
      site+4+*site(I32 *)==target->val &&
      *first.count(U64 *)==3 && Call(second.call)==42)
    checks|=8192;
  if (first.count!=second.count && first.call!=second.call &&
      first.count-first.base==second.count-second.base &&
      first.call-first.base==second.call-second.base)
    checks|=16384;
  if (!(first.base&15) && !(provider.base&15) && !(second.base&15) &&
      (first.image_end<=provider.header || provider.image_end<=first.header) &&
      (first.image_end<=second.header || second.image_end<=first.header) &&
      (provider.image_end<=second.header || second.image_end<=provider.header))
    checks|=32768;
done:
  "LI default %d %d\n",default_count,pending_default;
  "LI repeat %d %d\n",repeat_count,pending_repeat;
  "LI resolved %d %d\n",resolved_count,pending_resolved;
  "LI call %d\nLI again %d\nLI just %d\n",result,again_count,just_count;
  "LI passes %d %d\nLI checks %X\nLI DONE\n",pass1,pass2,checks;
}

LateImportLoadProbe;
