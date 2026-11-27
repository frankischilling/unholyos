class DocScanSmall
{
  U8 left;
  U16 value;
  U8 right;
};

U0 DocScanProbe()
{
  I64 res=0,value=0,error=0;
  F64 real=0;
  U8 dest[32],*out=dest,*list="red\0@rouge\0green\0gray\0";
  CDoc *doc=DocNew;
  CDocEntry *e=CAlloc(sizeof(CDocEntry));
  DocScanSmall small;
  e->tag=CAlloc(128);
  e->type_u8=DOCT_DATA;
  e->de_flags=DOCEF_TAG|DOCEF_AUX_STR|DOCEF_DEREF_DATA|DOCEF_HAS_TERMINATOR;
  e->raw_type=RT_U16;
  e->data=&small.value;
  e->len=31;
  e->min_col=2;
  e->aux_str="> %d";
  small.left=0x12;
  small.value=0;
  small.right=0x34;
  StrCpy(e->tag,"> 65538_");
  DocDataScan(doc,e);
  if (small.value==2 && small.left==0x12 && small.right==0x34 &&
      !StrCmp(e->tag,"> 65538_") && !(doc->locked_flags&1))
    res|=1;

  e->de_flags=DOCEF_TAG|DOCEF_AUX_STR|DOCEF_DEREF_DATA;
  e->raw_type=RT_I64;
  e->data=&value;
  e->min_col=0;
  e->aux_str="%z";
  e->my_fmt_data=list;
  StrCpy(e->tag,"rouge");
  DocDataScan(doc,e);
  if (!value)
    res|=2;
  DefineLstLoad("ST_DOC_SCAN_PROBE",list);
  e->aux_str="%Z";
  e->my_fmt_data="ST_DOC_SCAN_PROBE";
  StrCpy(e->tag,"gre");
  DocDataScan(doc,e);
  if (value==1)
    res|=4;

  value=0x1122334455667788;
  e->aux_str="%1C";
  StrCpy(e->tag,"a");
  DocDataScan(doc,e);
  if (value==0x1122334455667741)
    res|=8;
  e->raw_type=RT_F64;
  e->data=&real;
  e->aux_str="%3.1f";
  StrCpy(e->tag,"1.5");
  DocDataScan(doc,e);
  if (real==1.5)
    res|=16;

  e->raw_type=RT_PTR;
  e->data=&out;
  e->min_col=6;
  e->aux_str="Label:%s";
  StrCpy(e->tag,"Label:delta");
  DocDataScan(doc,e);
  if (!StrCmp(dest,"delta"))
    res|=32;
  e->de_flags=DOCEF_TAG|DOCEF_AUX_STR|DOCEF_REMALLOC_DATA|DOCEF_HAS_TERMINATOR;
  e->data=StrNew("old");
  StrCpy(e->tag,"Label:new text_");
  DocDataScan(doc,e);
  if (!StrCmp(e->data,"new text") && !StrCmp(e->tag,"Label:new text_") &&
      e->len==MSize(e->data)-1)
    res|=64;
  Free(e->data);

  e->de_flags=DOCEF_TAG|DOCEF_DEFINE|DOCEF_LST|DOCEF_DEREF_DATA;
  e->type_u8=DOCT_LST;
  e->raw_type=RT_I64;
  e->data=&value;
  e->define_str="ST_DOC_SCAN_PROBE";
  StrCpy(e->tag,"[gre]");
  DocDataScan(doc,e);
  if (value==1)
    res|=128;
  StrCpy(e->tag,"[gr]");
  DocDataScan(doc,e);
  if (value==-2)
    res|=256;
  StrCpy(e->tag,"rouge");
  DocDataScan(doc,e);
  if (!value)
    res|=512;

  e->de_flags=DOCEF_TAG|DOCEF_AUX_STR|DOCEF_DEREF_DATA;
  e->type_u8=DOCT_DATA;
  e->min_col=0;
  e->len=31;
  e->aux_str="%z,%z";
  e->my_fmt_data=list;
  StrCpy(e->tag,"red,green");
  try {
    DocDataScan(doc,e);
  } catch {
    error=Fs->except_ch;
    Fs->catch_except=TRUE;
  }
  if (error=='Scan' && !(doc->locked_flags&1))
    res|=1024;
  Free(e->tag);
  Free(e);
  DocDel(doc);
  "DocScan probe %X\n",res;
}

DocScanProbe;
