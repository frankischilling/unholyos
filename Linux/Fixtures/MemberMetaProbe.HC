I64 MetaProbeFunction(I64 n)
{
  return n+7;
}

class MetaProbeBase
{
  I64 base_field count 11;
};

class MetaProbeRecord:MetaProbeBase
{
  U8 name[32] format "Name:" "%s" zero 0;
  I64 age dft_val 6/2 dft_val 38 percentile 54.20,
      rank output_fun &MetaProbeFunction;
};

U0 MemberMetaProbe()
{
  I64 res=0;
  CHashClass *cls=HashFind("MetaProbeRecord",Fs->hash_table,HTT_CLASS);
  CMemberLst *m;
  CMemberLstMeta *meta;
  I64 (*fn)(I64 n);
  if (sizeof(CMemberLstMeta)==32 && offset(CMemberLstMeta.user_data)==24 &&
	sizeof(CMemberLst)==144 && offset(CMemberLst.meta)==64 &&
	sizeof(CHashClass)==128 && offset(CHashClass.member_lst_and_root)==88)
    res|=1;
  if (cls) {
    if (cls->size==56 && cls->member_cnt==3 && cls->base_class &&
	!StrCmp(cls->str,"MetaProbeRecord"))
      res|=2;
    m=MemberFind("name",cls);
    if (m && m->offset==8 && m->dim.total_cnt==32 && m->size==32 &&
	m->meta && !StrCmp(m->meta->str,"zero") &&
	!MemberMetaData("zero",m) && MemberMetaFind("zero",m) &&
	!MemberMetaFind("missing",m) && !MemberMetaData("missing",m))
      res|=4;
    if (m && (meta=MemberMetaFind("format",m)) &&
	meta->flags==MLMF_IS_STR && !StrCmp(meta->user_data,"Name:%s"))
      res|=8;
    m=MemberFind("age",cls);
    if (m && m->offset==40 && MemberMetaData("dft_val",m)==38 &&
	m->meta && !StrCmp(m->meta->str,"percentile") &&
	MemberMetaData("percentile",m)==54.20(I64))
      res|=16;
    m=MemberFind("rank",cls);
    if (m && m->offset==48 && (fn=MemberMetaData("output_fun",m)) && fn(5)==12)
      res|=32;
    m=MemberFind("base_field",cls);
    if (m && !m->offset && MemberMetaData("count",m)==11 &&
	!MemberFind("absent",cls))
      res|=64;
  }
  "Member metadata %02X\n",res;
}

MemberMetaProbe;
