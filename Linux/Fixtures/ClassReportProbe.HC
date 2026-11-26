class ReportBase
{
  U16 marker;
};

class ReportInner
{
  I64 value;
};

class ReportRecord:ReportBase
{
  I8 ch;
  U8 name[6];
  U8 *caption;
  I16 small;
  U32 number;
  I64 total;
  U64 wide;
  F64 real;
  U16 samples[3];
  ReportInner inner;
  ReportInner *missing;
};

U0 ClassReportProbe()
{
  I64 res=0,n=0;
  ReportRecord *r=CAlloc(sizeof(ReportRecord));
  CDoc *doc;
  CDocEntry *head,*e,*number=NULL,*real=NULL,*caption=NULL,*tree=NULL;
  U8 *st;
  r->marker=0x1234;
  r->ch='A';
  MemCpy(r->name,"Hello",6);
  r->caption="World";
  r->small=-2;
  r->number=0x1234;
  r->total=0x123456789ABCDEF;
  r->wide=0x123456789;
  r->real=1.5;
  r->samples[0]=1;
  r->samples[1]=2;
  r->samples[2]=3;
  r->inner.value=99;
  doc=DocNew;
  head=doc;
  if (ClassRep2(doc,r,,0,1,FALSE,HTT_CLASS,-r))
    res|=1;
  DocRecalc(doc);
  e=head->next;
  "report static:\n";
  while (e!=head) {
    st=DocScanLine(doc,e,,&e);
    "%s\n",st;
    Free(st);
    if (e!=head)
      e=e->next;
  }
  DocDel(doc);
  doc=DocNew;
  head=doc;
  if (ClassRep2(doc,r,,0,2,TRUE,HTT_CLASS,-r))
    res|=2;
  DocRecalc(doc);
  e=head->next;
  while (e!=head) {
    if (e->type_u8==DOCT_TREE)
      tree=e;
    if (e->type_u8==DOCT_DATA) {
      n++;
      if (e->data==&r->number)
        number=e;
      if (e->data==&r->real)
        real=e;
      if (e->data==r->caption && !(e->de_flags&DOCEF_DEREF_DATA))
        caption=e;
    }
    e=e->next;
  }
  if (number && real && caption && n==20)
    res|=4;
  if (number && !StrCmp(number->tag,"00001234") &&
      number->de_flags&DOCEF_REFRESH_DATA &&
      number->de_flags&DOCEF_UPDATE_DATA) {
    r->number=0xAABBCCDD;
    DocRecalc(doc);
    if (!StrCmp(number->tag,"AABBCCDD"))
      res|=8;
    MemCpy(number->tag,"0000002A",9);
    DocDataScan(doc,number);
    if (r->number==42)
      res|=16;
  }
  if (real) {
    MemCpy(real->tag,"2.5",4);
    DocDataScan(doc,real);
    if (r->real==2.5)
      res|=32;
  }
  if (caption && !(caption->de_flags&DOCEF_UPDATE_DATA))
    res|=64;
  if (number) {
    DocRecalc(doc);
    doc->cur_entry=number;
    doc->cur_col=7;
    DocPutKey(doc,'F',0);
    if (r->number==0x2F)
      res|=512;
  }
  if (tree) {
    doc->cur_entry=tree;
    doc->cur_col=tree->min_col;
    DocPutKey(doc,' ',0);
    DocRecalc(doc);
    if (tree->tag[0]=='-')
      res|=1024;
  }
  DocDel(doc);
  doc=DocNew;
  if (!ClassRep2(doc,r,"NoSuchReportClass",0,2,FALSE) &&
      !ClassRep2(doc,NULL,"ReportRecord",0,2,FALSE) &&
      ClassRep2(doc,NULL,"ReportRecord",2,2,FALSE))
    res|=128;
  DocDel(doc);
  if (Gs->addr==Gs && ChkPtr(Gs) && sizeof(CCPU)==256 &&
      offset(CCPU.start_stk)==128)
    res|=256;
  Free(r);
  "Class report %03X data %d\n",res,n;
}

ClassReportProbe;
