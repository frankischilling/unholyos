#include "T:/Kernel/KernelA.HH"
#include "T:/Compiler/CompilerA.HH"

_intern IC_FS CTask *ProbeDocTask();
_intern IC_STRLEN I64 ProbeDocLen(U8 *str);
_import _STRCMP I64 ProbeDocCmp(U8 *a,U8 *b);
_import _FREE U0 ProbeDocFree(U8 *p);
_import _MEMCPY U8 *ProbeDocCopy(U8 *dst,U8 *src,I64 count);
import U8 *CAlloc(I64 size,CTask *task=NULL);
import CDoc *DocNew(U8 *filename=NULL,CTask *task=NULL);
import U0 DocDel(CDoc *doc);
import I64 DocSize(CDoc *doc);
import U0 DocLoad(CDoc *doc,U8 *src,I64 size);
import U8 *DocSave(CDoc *doc,I64 *size=NULL);
import Bool DocWrite(CDoc *doc,Bool prompt=FALSE);
import CDocEntry *DocPrint(CDoc *doc,U8 *fmt,...);
import CDocEntry *DocPutLine(CDoc *doc,CDocEntry *entry);
import U8 *Doc2PlainText(CDoc *doc,CDocEntry *entry);
import U0 DocBinsValidate(CDoc *doc);
import U8 *MStrPrint(U8 *fmt,...);
import CBlkDev *BlkDevNextFreeSlot(U8 letter,I64 type);
import I64 BlkDevAdd(CBlkDev *bd,I64 prt_num,Bool whole,Bool make_free);
import U8 *FileRead(U8 *name,I64 *size,I64 *attr);

#assert sizeof(CDoc)==768 && sizeof(CDocEntryBase)==88 && sizeof(CDocEntry)==208
#assert sizeof(CDocBin)==56 && offset(CDocBin.start)==32 && offset(CDocBin.end)==48
#assert sizeof(CEdFindText)==304 && offset(CDoc.bin_head)==632

public I64 DocProbeResult;

Bool ProbeDocString(U8 *owned,U8 *expected)
{
  Bool ok=owned && !ProbeDocCmp(owned,expected);
  ProbeDocFree(owned);
  return ok;
}

Bool ProbeDocNew()
{
  CTask *task=ProbeDocTask();
  I64 before=task->data_heap->used_u8s;
  CDoc *doc=DocNew;
  if (!doc)
    return FALSE;
  Bool ok=doc->doc_signature==DOC_SIGNATURE_VAL &&
      doc->head.type_u8==DOCT_ERROR && doc->head.next==doc && doc->head.last==doc &&
      doc->cur_entry==doc && doc->bin_head.next==&doc->bin_head &&
      doc->win_task==task && doc->mem_task==task && !doc->owning_task &&
      !doc->locked_flags && doc->max_entries==I64_MAX && doc->cur_bin_num==1 &&
      doc->find_replace->scan_fwd && doc->find_replace->match_case &&
      doc->dollar_buf_size==84 && doc->settings_head.page_len==66 &&
      doc->settings_head.dft_text_attr==0xF0 &&
      !ProbeDocCmp(doc->filename.name,"~/Tmp.DD.Z") && DocSize(doc)==0x4B8;
  DocDel(doc);
  DocDel(NULL);
  return ok && !DocSize(NULL) && task->data_heap->used_u8s==before;
}

Bool ProbeDocText()
{
  CTask *task=ProbeDocTask();
  I64 before=task->data_heap->used_u8s,size;
  U8 *raw="A\tB\r\n$$FG,4$$hi$$$$x$$LK,\"ref\",A=\"FI:::/Doc/X.DD\"$$\nend";
  CDoc *doc=DocNew;
  DocLoad(doc,raw,ProbeDocLen(raw));
  CDocEntry *a=doc->head.next,*tab=a->next,*b=tab->next,*cr=b->next,
      *fg=cr->next,*text=fg->next,*link=text->next,*lf=link->next,*end=lf->next;
  Bool ok=doc->cur_entry==a && a->type==0xF000 && a->max_col==1 &&
      tab->type_u8==DOCT_TAB && tab->x==1 && b->x==8 &&
      cr->type_u8==DOCT_NEW_LINE && cr->y==0 &&
      fg->type_u8==DOCT_FOREGROUND && fg->attr==4 && fg->y==1 &&
      text->type==0xF400 && text->max_col==4 && !ProbeDocCmp(text->tag,"hi$$x") &&
      link->type==0x8000F41C && link->x==4 && link->max_col==1 &&
      !ProbeDocCmp(link->aux_str,"FI:::/Doc/X.DD") &&
      lf->y==1 && end->y==2 && end->next==doc && DocSize(doc)==0x9E8;
  U8 *saved=DocSave(doc,&size);
  ok=ProbeDocString(saved,"A\tB\n$$FG,4$$hi$$$$x$$LK,\"ref\",A=\"FI:::/Doc/X.DD\"$$\nend") &&
      ok && size==0x30;
  DocDel(doc);
  return ok && task->data_heap->used_u8s==before;
}

Bool ProbeDocCursor()
{
  I64 size;
  U8 *raw="A\x05" "B\nC\tD$$";
  CDoc *doc=DocNew;
  doc->flags|=DOCF_PLAIN_TEXT_TABS|DOCF_CARRIAGE_RETURN;
  DocLoad(doc,raw,ProbeDocLen(raw));
  Bool ok=doc->cur_entry==doc->head.next->next && !doc->cur_col &&
      !ProbeDocCmp(doc->cur_entry->tag,"B");
  U8 *saved=DocSave(doc,&size);
  ok=ProbeDocString(saved,"A\x05" "B\r\nC\tD$$") && ok && size==9;
  DocDel(doc);
  return ok;
}

Bool ProbeDocPrint()
{
  CDoc *doc=DocNew;
  CDocEntry *entry=DocPrint(doc,"$$TX+UL,\"%s\",U=0x%X$$\n","DOC-LINE",0x55);
  Bool ok=entry && entry->type_u8==DOCT_TEXT && entry->user_data==0x55 &&
      ProbeDocString(Doc2PlainText(doc,entry),"TX+UL,\"DOC-LINE\",U=0x55") &&
      ProbeDocString(MStrPrint("%$$F",doc),"$$TX+UL,\"DOC-LINE\",U=0x55$$\n");
  I64 doc_size=DocSize(doc);
  ok=ok && DocPutLine(doc,entry)==doc && DocSize(doc)==doc_size;
  DocDel(doc);
  return ok;
}

Bool ProbeDocBins()
{
  CTask *task=ProbeDocTask();
  I64 before=task->data_heap->used_u8s;
  U8 *markup="$$IB,\"blob\",BI=7$$$$IS,\"size\",BI=7$$\n";
  I64 len=ProbeDocLen(markup), size;
  U8 *raw=CAlloc(len+1+16+3+16+1),*saved;
  ProbeDocCopy(raw,markup,len+1);
  U32 *first=raw+len+1,*orphan=raw+len+1+19;
  first[0]=7;
  first[1]=0x43;
  first[2]=3;
  first[3]=29;
  (first(U8 *))[16]='x';
  (first(U8 *))[18]='Y';
  orphan[0]=88;
  orphan[2]=1;
  CDoc *doc=DocNew;
  DocLoad(doc,raw,len+1+16+3+16+1);
  CDocBin *blob=doc->bin_head.next;
  CDocEntry *entry=doc->head.next,*next=entry->next;
  Bool ok=blob!=&doc->bin_head && blob->next==&doc->bin_head && blob->num==1 &&
      blob->flags==0x43 && blob->size==3 && blob->use_cnt==2 &&
      !ProbeDocCmp(blob->tag,"blob") && blob->data[0]=='x' && !blob->data[1] &&
      blob->data[2]=='Y' && doc->cur_bin_num==2 && entry->bin_num==1 &&
      next->bin_num==1 && entry->bin_data==blob && next->bin_data==blob;
  DocBinsValidate(doc);
  saved=DocSave(doc,&size);
  U32 *header=saved+len+1;
  ok=ok && size==len+1+19 &&
     !ProbeDocCmp(saved,"$$IB,\"blob\",BI=1$$$$IS,\"size\",BI=1$$\n") &&
     header[0]==1 && header[1]==0x43 && header[2]==3 && header[3]==2 &&
     (header(U8 *))[16]=='x' && !(header(U8 *))[17] && (header(U8 *))[18]=='Y';
  ProbeDocFree(saved);
  ProbeDocFree(raw);
  DocDel(doc);
  return ok && task->data_heap->used_u8s==before;
}

Bool ProbeDocWrite()
{
  CBlkDev *bd=BlkDevNextFreeSlot('B',BDT_RAM);
  bd->max_blk=511;
  if (!BlkDevAdd(bd,I64_MIN,FALSE,TRUE))
    return FALSE;
  CDoc *doc=DocNew("B:/DocProbe.DD");
  I64 size;
  DocPrint(doc,"disk:%d\n$$BLUE$$ok",38);
  doc->flags|=DOCF_NO_CURSOR;
  Bool ok=DocWrite(doc);
  U8 *saved=FileRead("B:/DocProbe.DD",&size,NULL);
  ok=ProbeDocString(saved,"disk:38\n$$FG,1$$ok") && ok && size==16;
  DocDel(doc);
  return ok;
}

Bool ProbeDocSource()
{
  CTask *task=ProbeDocTask();
  I64 before=task->data_heap->used_u8s;
  I64 input_size,output_size,entries=0,links=0,i;
  U8 *source=FileRead("T:/StartOS.HC",&input_size,NULL);
  if (!source)
    return FALSE;
  CDoc *doc=DocNew("T:/StartOS.HC");
  doc->flags|=DOCF_NO_CURSOR;
  DocLoad(doc,source,input_size);
  CDocEntry *entry=doc->head.next;
  while (entry!=doc) {
    entries++;
    if (entry->type_u8==DOCT_LINK)
      links++;
    entry=entry->next;
  }
  U8 *saved=DocSave(doc,&output_size);
  U64 hash=0xCBF29CE484222325;
  for (i=0;i<output_size;i++)
    hash=(hash^saved[i])*0x100000001B3;
  Bool ok=input_size==0x486 && output_size==input_size && entries==86 &&
       links==2 && hash==0x68B531597D446B26 && !ProbeDocCmp(source,saved);
  ProbeDocFree(saved);
  ProbeDocFree(source);
  DocDel(doc);
  return ok && task->data_heap->used_u8s==before;
}

U0 DocProbeMain()
{
  if (ProbeDocNew)
    DocProbeResult|=1;
  if (ProbeDocText)
    DocProbeResult|=2;
  if (ProbeDocCursor)
    DocProbeResult|=4;
  if (ProbeDocPrint)
    DocProbeResult|=8;
  if (ProbeDocBins)
    DocProbeResult|=16;
  if (ProbeDocWrite)
    DocProbeResult|=32;
  if (ProbeDocSource)
    DocProbeResult|=64;
}
DocProbeMain;
