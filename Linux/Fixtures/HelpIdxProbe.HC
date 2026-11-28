// Exercise Terry A. Davis's generated help and symbol reports in Adam/AHash.HC.
#help_index "PortGuide/A;PortGuide/B"
#help_file "::/Doc/HelpSystem.DD"
public I64 PortHelpZeta()
{ // Zeta comment from source
  return 7;
}

#help_index "PortGuide/A"
public I64 PortHelpAlpha()
{ /* Alpha comment from source */
  return 3;
}

I64 PortHelpPrivate()
{ // Only public symbols should appear.
  return 9;
}

#help_index "PopupGuide"
#help_file "::/Doc/HelpSystem.DD"
#help_index ""
I64 HelpIdxLine(CDoc *doc,U8 *tag,I64 start=I64_MIN)
{
  CDocEntry *entry;
  for (entry=doc->head.next;entry!=doc;entry=entry->next)
    if (entry->y+1>=start && entry->de_flags&DOCEF_TAG && entry->tag &&
        StrMatch(tag,entry->tag))
      return entry->y+1;
  return 0;
}

U0 HelpIdxSymbol(CHashTable *table,U8 *name,I64 value)
{
  CHashGeneric *entry=CAlloc(sizeof(CHashGeneric));
  entry->str=StrNew(name);
  entry->type=HTT_FRAME_PTR|HTF_PUBLIC;
  entry->user_data0=value;
  HashAdd(entry,table);
}

I64 HelpIdxProbe()
{
  I64 bits=0,a,b,alpha,zeta,heading,doc_type,num;
  U8 *link,*path=NULL,*needle=NULL;
  CDoc *doc=DocNew;
  CHashTable *parent=HashTableNew(8),*child=HashTableNew(1);
  DocHelpIdx(doc,"PortGuide");
  a=HelpIdxLine(doc,"PortGuide/A");
  b=HelpIdxLine(doc,"PortGuide/B");
  alpha=HelpIdxLine(doc,"PortHelpAlpha");
  zeta=HelpIdxLine(doc,"PortHelpZeta");
  if (a>0 && a<alpha && alpha<zeta && zeta<b &&
      HelpIdxLine(doc,"PortHelpZeta",b)>b)
    bits|=1;
  heading=HelpIdxLine(doc,"The help index preprocessor");

  if (a<heading && heading<alpha &&
      HelpIdxLine(doc,"The help index preprocessor",b)>b)
    bits|=2;
  if (HelpIdxLine(doc,"Alpha comment from source") &&
      HelpIdxLine(doc,"Zeta comment from source"))
    bits|=4;
  if (DocFind(doc,,"PortHelpAlpha")) {
    link=DocEntryLink(doc,doc->cur_entry);
    if (link && StrMatch("HelpIdxProbe.HC,",link))
      bits|=8;
    Free(link);
  }
  if (!HelpIdxLine(doc,"PortHelpPrivate"))
    bits|=16;
  DocClear(doc,TRUE);
  DocHelpIdx(doc,"portguide");
  if (HelpIdxLine(doc,"No Match"))
    bits|=32;

  child->next=parent;
  HelpIdxSymbol(child,"ReportA",3);
  HelpIdxSymbol(child,"ReportZ",1);
  HelpIdxSymbol(parent,"ReportP",2);
  DocClear(doc,TRUE);
  Who("-r",child,,doc);
  a=HelpIdxLine(doc,"ReportA");
  zeta=HelpIdxLine(doc,"ReportZ");
  if (a>0 && a<zeta && !HelpIdxLine(doc,"ReportP") && HashDepthRep(child)==2) {
    DocClear(doc,TRUE);
    Who("+m",child,,doc);
    zeta=HelpIdxLine(doc,"ReportZ");
    b=HelpIdxLine(doc,"ReportP");
    a=HelpIdxLine(doc,"ReportA");
    if (zeta>0 && zeta<b && b<a)
      bits|=64;
  }
  HashTableDel(child);
  HashTableDel(parent);
  DocDel(doc);

  doc_type=EdLinkCvt("hI:PortGuide/A",&path,&needle,&num);
  if (doc_type==LK_HELP_INDEX && path && !StrCmp(path,"PortGuide/A") &&
      !needle && num==1)
    bits|=128;
  Free(path);
  Free(needle);
  if (Ed("HI:PortGuide/Empty",EDF_BAIL) &&
      !Ed("HI:PortGuide/Empty",EDF_BAIL|EDF_WAS_WRITE))
    bits|=256;
  try
    Who("+x");
  catch {
    if (Fs->except_ch=='FUF')
      bits|=512;
    Fs->catch_except=TRUE;
  }
  return bits;
}

public U0 HelpIdxOpen()
{
  Bool opened;
  "Help index checks %X\n",HelpIdxProbe();
  opened=Ed("HI:PortGuide/A");

  "Help index editor %X\n",opened;
}
