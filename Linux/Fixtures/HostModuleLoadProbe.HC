class CHostModuleSample
{
  U64 header,base,end,image_end,result,anchor,value,values[5],checks;
};

Bool HostModuleInside(CHostModuleSample *s,U64 address,U64 size)
{
  return address>=s->base && address<=s->end && size<=s->end-address;
}

U0 HostModuleTake(CHostModuleSample *s)
{
  CBinFile *header=Load("B:/HostModuleProbe.BIN",LDF_SILENT);
  CHashGeneric *module;
  CHashExport *result,*anchor;
  if (!header)
    return;
  module=HashFind("/HostModuleProbe",Fs->hash_table,HTT_MODULE);
  result=HashFind("HostModuleResult",Fs->hash_table,HTT_EXPORT_SYS_SYM);
  anchor=HashFind("HostModuleAnchor",Fs->hash_table,HTT_EXPORT_SYS_SYM);
  if (!module || !result || !anchor)
    return;
  s->header=header;
  if (module->user_data0!=s->header ||
      header->bin_signature!=BIN_SIGNATURE_VAL || header->org!=INVALID_PTR ||
      header->patch_table_offset<sizeof(CBinFile) ||
      header->patch_table_offset>=header->file_size)
    return;
  s->base=s->header+sizeof(CBinFile);
  s->end=s->header+header->patch_table_offset;
  s->image_end=s->header+header->file_size;
  s->result=result->val;
  s->anchor=anchor->val;
  s->checks=32;
  if (!HostModuleInside(s,s->result,8) || !HostModuleInside(s,s->anchor,40))
    return;
  MemCpy(&s->value,s->result,8);
  MemCpy(s->values,s->anchor,40);
  if (s->values[0]==s->anchor)
    s->checks|=1;
  if (HostModuleInside(s,s->values[1],32) &&
      HostModuleInside(s,s->values[2],8) &&
      HostModuleInside(s,s->values[3],1))
    s->checks|=2;
  if (s->values[2]==s->values[1]+16)
    s->checks|=4;
  if (s->values[4]==char_bmp_alpha)
    s->checks|=8;
  if (!(s->base&63) && !(s->values[1]&63))
    s->checks|=16;
}

U0 HostModuleLoadProbe()
{
  CHostModuleSample samples[2],*first,*second;
  I64 i,pair=0;
  Bool distinct,offsets,retained;
  MemSet(samples,0,sizeof(samples));
  first=&samples[0];
  second=&samples[1];
  HostModuleTake(first);
  HostModuleTake(second);
  if (first->checks&1 && second->checks&1) {
    if (first->image_end<=second->header || second->image_end<=first->header)
      pair|=1;
    distinct=first->result!=second->result;
    offsets=first->result-first->base==second->result-second->base;
    retained=*first->result(U64 *)==first->value;
    for (i=0;i<5;i++) {
      if (i<4) {
        distinct=distinct && first->values[i]!=second->values[i];
        offsets=offsets && first->values[i]-first->base==
                           second->values[i]-second->base;
      }
      retained=retained && first->anchor(U64 *)[i]==first->values[i];
    }
    if (distinct) pair|=2;
    if (offsets) pair|=4;
    if (first->values[4]==second->values[4]) pair|=8;
    if (retained) pair|=16;
  }
  "HM results %016X %016X\n",first->value,second->value;
  "HM local %016X %016X\n",first->checks,second->checks;
  "HM pair %016X\nHM DONE\n",pair;
}

HostModuleLoadProbe;
