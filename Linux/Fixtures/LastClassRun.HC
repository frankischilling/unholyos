U0 LastClassRun()
{
  CDoc *doc=DocPut;
  CDocEntry *head=doc,*e;
  I64 old_max=doc->max_entries,res=0,data=0;
  ExeFile("::/Demo/LastClass.HC");
  e=head->next;
  while (e!=head) {
    if (e->tag) {
      if (!StrCmp(e->tag,"Class:\"CCPU\""))
        res|=1;
      if (!StrCmp(e->tag,"Class:\"CTask\""))
        res|=2;
    }
    if (e->type_u8==DOCT_DATA && e->data)
      data++;
    e=e->next;
  }
  if (data)
    res|=4;
  DocMax(old_max);
  "LastClass reports %X\n",res;
}

LastClassRun;
