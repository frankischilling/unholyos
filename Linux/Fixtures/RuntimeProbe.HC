#include "T:/Kernel/KernelA.HH"
#include "T:/Compiler/CompilerA.HH"

_intern IC_FS CTask *ProbeCurrentTask();
_intern IC_BT Bool ProbeBit(U8 *bitmap, I64 bit);

_import _MALLOC U8 *ProbeAlloc(I64 size, CTask *mem_task);
import U8 *CAlloc(I64 size, CTask *mem_task);
_import _FREE U0 ProbeFree(U8 *ptr);
_import _MSIZE I64 ProbeSize(U8 *ptr);
_import _MSIZE2 I64 ProbeSize2(U8 *ptr);
_import _MHEAP_CTRL CHeapCtrl *ProbeHeap(U8 *ptr);
import U8 *MAllocIdent(U8 *ptr, CTask *mem_task);
_import _MEMSET U8 *ProbeSet(U8 *dst, I64 value, I64 count);
_import _MEMSET_I64 I64 *ProbeSet64(I64 *dst, I64 value, I64 count);
import U8 *StrNew(U8 *src, CTask *mem_task);
import U8 *AStrNew(U8 *src);
_import _STRCMP I64 ProbeStrCmp(U8 *a, U8 *b);
_import _STRCPY U0 ProbeStrCpy(U8 *dst, U8 *src);
_import _STRIMATCH U8 *ProbeStrIMatch(U8 *needle, U8 *haystack);
import I64 StrOcc(U8 *src, I64 ch);
import CHashTable *HashTableNew(I64 size, CTask *mem_task);
_import _HASH_ADD U0 ProbeAdd(CHash *entry, CHashTable *table);
_import _HASH_ADD_AFTER U0 ProbeAddAfter(CHash *entry, CHash *pred,
                                       CHashTable *table);
_import _HASH_FIND CHash *ProbeFind(U8 *name, CHashTable *table,
                                   I64 mask, I64 instance);
_import _HASH_SINGLE_TABLE_FIND CHash *ProbeSingle(U8 *name, CHashTable *table,
                                                  I64 mask, I64 instance);
import I64 HashTypeNum(CHash *entry);
_import _BEQU Bool ProbeBequ(U8 *addr, I64 bit, Bool value);
_import _CALL I64 ProbeCall(I64 entry);
_import _CLAMP_I64 I64 ProbeClamp(I64 value, I64 lo, I64 hi);
import U64 CeilU64(U64 num, U64 to);
import F64 Pow10I64(I64 exponent);
import CTask *adam_task;
_import MEM_BOOT_BASE U32 ProbeBootBase;
_import SYS_RUN_LEVEL U32 ProbeRunLevel;
_import SYS_VAR_INIT_FLAG U8 ProbeVarInitFlag;
_import SYS_VAR_INIT_VAL U8 ProbeVarInitVal;
import I64 LstMatch(U8 *needle, U8 *list, I64 flags);
import U0 LinkedLstDel(U8 **node);
import I64 LinkedLstSize(U8 **node);
import U0 QueDel(CQue *head, Bool remove);
import CQueVectU8 *QueVectU8New(I64 min_idx);
import U0 QueVectU8Put(CQueVectU8 *v, I64 idx, U8 ch);
import I64 QueVectU8Get(CQueVectU8 *v, I64 idx);
import U0 QueVectU8Del(CQueVectU8 *v);
import U0 QSortI64(I64 *base, I64 count, I64 (*cmp)(I64 a,I64 b));
import U0 QSort(U8 *base, I64 count, I64 width, I64 (*cmp)(U8 *a,U8 *b));
import U32 char_bmp_alpha[16];
import U32 char_bmp_alpha_numeric[16];
import U32 char_bmp_alpha_numeric_no_at[16];
import U32 char_bmp_word[16];
import U32 char_bmp_filename[16];
import U32 char_bmp_dec_numeric[16];
import U32 char_bmp_hex_numeric[16];
import U32 char_bmp_white_space[16];
import U32 char_bmp_non_eol_white_space[16];
import U32 char_bmp_zero_cr_nl_cursor[16];
import U32 char_bmp_zero_tab_cr_nl_cursor[16];
import U32 char_bmp_zero_tab_cr_nl_cursor_dollar[16];
import U32 char_bmp_macro[16];
import U32 char_bmp_printable[16];
import U32 char_bmp_displayable[16];
import U32 char_bmp_safe_dollar[16];
import U32 char_bmp_non_eol[16];

Bool ProbeMemory()
{
  U8 *src=ProbeAlloc(24,NULL), *zero=CAlloc(9,adam_task);
  I64 *values=src(I64 *);
  CTask *task=ProbeCurrentTask();
  Bool ok=ProbeSize(src)==24 && ProbeSize2(src)==40 &&
          ProbeHeap(src)==task->data_heap &&
          ProbeHeap(zero)==adam_task->data_heap &&
          ProbeSize(NULL)==0 && ProbeSize2(NULL)==0 && !ProbeHeap(NULL) &&
          !MAllocIdent(NULL,NULL) && !zero[0] && !zero[8];
  ok=ok && ProbeSet64(values,0x0123456789ABCDEF,3)==values+3 &&
           values[0]==0x0123456789ABCDEF && values[2]==values[0] &&
           ProbeSet(src,0x15A,1)==src+1 && src[0]==0x5A && src[1]==0xCD;
  U8 *copy=MAllocIdent(src,adam_task->code_heap);
  ok=ok && copy!=src && ProbeHeap(copy)==adam_task->code_heap &&
           copy[0]==0x5A && (copy(I64 *))[2]==values[2];
  values[0]=-8;
  ok=ok && ProbeSize(src+8)==24 && ProbeSize2(src+8)==40 &&
           ProbeHeap(src+8)==task->data_heap;
  ProbeFree(src+8);
  ProbeFree(copy);
  ProbeFree(zero);
  ProbeFree(NULL);
  return ok;
}

Bool ProbeStrings()
{
  U8 buf[8], *str=StrNew("ABCabc",NULL), *empty=AStrNew(NULL);
  Bool ok=!empty[0] && ProbeHeap(empty)==adam_task->data_heap &&
          ProbeStrCmp(str,"ABCabc")==0 && ProbeStrCmp("","A")==-1 &&
          ProbeStrCmp("a","A")==1 && StrOcc(str,'a')==1 &&
          StrOcc(str,0)==0 && StrOcc(NULL,'a')==0 &&
          ProbeStrIMatch("bCa",str)==str+1 && ProbeStrIMatch("",str)==str &&
          !ProbeStrIMatch("ba",str) && !ProbeStrIMatch("a",NULL);
  ProbeStrCpy(buf,str);
  ok=ok && ProbeStrCmp(buf,str)==0;
  ProbeStrCpy(buf,NULL);
  ProbeStrCpy(NULL,str);
  ProbeFree(empty);
  ProbeFree(str);
  return ok && !buf[0];
}

Bool ProbeHash()
{
  CHashTable *top=HashTableNew(8,NULL), *base=HashTableNew(4,adam_task);
  CHash a,b,c;
  a.str=b.str=c.str="RuntimeProbeKey";
  a.type=c.type=HTT_EXPORT_SYS_SYM;
  b.type=HTT_IMPORT_SYS_SYM|HTF_PUBLIC;
  a.use_cnt=b.use_cnt=c.use_cnt=0;
  top->next=base;
  ProbeAdd(&a,top);
  ProbeAddAfter(&b,&a,top);
  ProbeAdd(&c,base);
  I64 mask=HTT_EXPORT_SYS_SYM|HTT_IMPORT_SYS_SYM;
  Bool ok=top->mask==7 && base->mask==3 && a.next==&b && !b.next &&
          !ProbeFind(a.str,top,mask,0) && !ProbeSingle(a.str,top,mask,3) &&
          ProbeSingle(a.str,top,mask,2)==&b &&
          ProbeFind(a.str,top,mask,3)==&c &&
          ProbeFind(a.str,top,HTT_EXPORT_SYS_SYM,2)==&c &&
          ProbeFind(a.str,top,HTT_EXPORT_SYS_SYM,1)==&a &&
          a.use_cnt==1 && b.use_cnt==1 && c.use_cnt==2 &&
          HashTypeNum(&b)==1 && HashTypeNum(NULL)==-1;
  ProbeFree(top->body);
  ProbeFree(base->body);
  ProbeFree(top);
  ProbeFree(base);
  return ok;
}

I64 ProbeCallback()
{
  return 123;
}

Bool ProbeNumbers()
{
  I64 bits[2];
  bits[0]=bits[1]=0;
  return !ProbeBequ(bits,65,TRUE) && bits[1]==2 &&
         ProbeBequ(bits+1,-63,FALSE)==FALSE &&
         ProbeBequ(bits,65,FALSE) && !bits[1] &&
         ProbeCall(&ProbeCallback)==123 && !ProbeCall(0) &&
         ProbeClamp(-9,-7,11)==-7 && ProbeClamp(12,-7,11)==11 &&
         ProbeClamp(5,8,3)==3 && CeilU64(13,5)==15 &&
         CeilU64(15,5)==15 && Pow10I64(2)(I64)==0x4058FFFFFFFFFFFE &&
         Pow10I64(-309)==0.0 && Pow10I64(309)(I64)==0x7FF0000000000000;
}

Bool ProbeCharTables()
{
  I64 ch;
  for (ch=0;ch<512;ch++) {
    Bool cp437=128<=ch<=255, ascii=32<=ch<=126;
    Bool letter='A'<=ch<='Z' || 'a'<=ch<='z' || ch=='_' || cp437;
    Bool decimal='0'<=ch<='9', alpha=letter || ch=='@';
    Bool display=ascii || cp437 || ch==31;
    Bool eol=ch=='\r' || ch=='\n', white=ch==' ' || ch=='\t' || ch==31;
    Bool cursor=ch==0 || ch==5 || eol, printable=display || eol || ch=='\t';
    Bool filename=(ascii || cp437) && ch!=34 && ch!='*' && ch!='+' &&
                  ch!='/' && ch!=':' && ch!=';' && ch!='<' && ch!='=' &&
                  ch!='>' && ch!='?' && ch!=92 && ch!='|';
    if (ProbeBit(char_bmp_alpha,ch)!=alpha ||
        ProbeBit(char_bmp_alpha_numeric,ch)!=(alpha || decimal) ||
        ProbeBit(char_bmp_alpha_numeric_no_at,ch)!=(letter || decimal) ||
        ProbeBit(char_bmp_word,ch)!=(letter || decimal || ch==39) ||
        ProbeBit(char_bmp_filename,ch)!=filename ||
        ProbeBit(char_bmp_dec_numeric,ch)!=decimal ||
        ProbeBit(char_bmp_hex_numeric,ch)!=(decimal || 'A'<=ch<='F' || 'a'<=ch<='f') ||
        ProbeBit(char_bmp_white_space,ch)!=(white || eol) ||
        ProbeBit(char_bmp_non_eol_white_space,ch)!=white ||
        ProbeBit(char_bmp_zero_cr_nl_cursor,ch)!=cursor ||
        ProbeBit(char_bmp_zero_tab_cr_nl_cursor,ch)!=(cursor || ch=='\t') ||
        ProbeBit(char_bmp_zero_tab_cr_nl_cursor_dollar,ch)!=(cursor || ch=='\t' || ch==36) ||
        ProbeBit(char_bmp_macro,ch)!=(printable && ch!='%') ||
        ProbeBit(char_bmp_printable,ch)!=printable ||
        ProbeBit(char_bmp_displayable,ch)!=display ||
        ProbeBit(char_bmp_safe_dollar,ch)!=(display && ch!=36) ||
        ProbeBit(char_bmp_non_eol,ch)!=(ch!=0 && !eol))
      return FALSE;
  }
  U8 *names="Alpha\0@Alias\0Alpine\0Beta\0";
  return LstMatch("Alias",names,0)==0 &&
         LstMatch("al",names,LMF_IGNORE_CASE)==-2 &&
         LstMatch("alpine",names,LMF_IGNORE_CASE)==1 &&
         LstMatch("Al",names,LMF_EXACT)==-1 &&
         LstMatch("Al","Alpha\0Al\0",0)==1 && LstMatch("Al",NULL,0)==-1;
}

class CProbeNode
{
  CProbeNode *next;
  I64 value;
};

Bool ProbeCollections()
{
  CHeapCtrl *heap=ProbeCurrentTask()->data_heap;
  I64 before=heap->used_u8s;
  CProbeNode *a=ProbeAlloc(sizeof(CProbeNode),NULL),
             *b=ProbeAlloc(sizeof(CProbeNode)+17,NULL);
  a->next=b;
  b->next=NULL;
  Bool ok=LinkedLstSize(a)==ProbeSize2(a)+ProbeSize2(b) &&
          !LinkedLstSize(NULL);
  LinkedLstDel(a);
  LinkedLstDel(NULL);

  CQue head, *first=ProbeAlloc(sizeof(CQue),NULL),
             *last=ProbeAlloc(sizeof(CQue),NULL);
  head.next=first;
  head.last=last;
  first->last=last->next=&head;
  first->next=last;
  last->last=first;
  QueDel(&head,TRUE);
  ok=ok && head.next==&head && head.last==&head;

  first=ProbeAlloc(sizeof(CQue),NULL);
  head.next=head.last=first;
  first->next=first->last=&head;
  QueDel(&head,FALSE);
  ok=ok && head.next==first && head.last==first;

  CQueVectU8 *vec=QueVectU8New(-7);
  ok=ok && vec->next==vec && vec->last==vec && !vec->total_cnt &&
     !vec->node_cnt && vec->min_idx==-7;
  QueVectU8Put(vec,-8,0x11);
  QueVectU8Put(vec,-7,0x22);
  QueVectU8Put(vec,1017,0xFE);
  QueVectU8Put(vec,504,0x44);
  QueVectU8Put(vec,505,0x55);
  ok=ok && vec->total_cnt==1025 && vec->node_cnt==512 &&
     vec->next->node_cnt==512 && vec->last->node_cnt==1 &&
     vec->last->next==vec && vec->last->last==vec->next &&
     !QueVectU8Get(vec,-8) && QueVectU8Get(vec,-7)==0x22 &&
     !QueVectU8Get(vec,503) && QueVectU8Get(vec,504)==0x44 &&
     QueVectU8Get(vec,505)==0x55 && !QueVectU8Get(vec,506) &&
     QueVectU8Get(vec,1017)==0xFE && !QueVectU8Get(vec,1018);
  QueVectU8Del(vec);
  QueVectU8Del(NULL);
  return ok && heap->used_u8s==before;
}

class CProbeSortRecord
{
  I64 key,tag;
};

I64 ProbeSortCalls;

I64 ProbeCompare(I64 a,I64 b)
{
  ProbeSortCalls++;
  return (a>b)-(a<b);
}

I64 ProbeCompare8(I64 *a,I64 *b)
{
  return ProbeCompare(*a,*b);
}

I64 ProbeCompare16(CProbeSortRecord *a,CProbeSortRecord *b)
{
  return ProbeCompare(a->key,b->key);
}

Bool ProbeSort()
{
  I64 values[6],wide[6];
  CProbeSortRecord records[6];
  I64 i;
  values[0]=values[2]=values[4]=2;
  values[1]=values[5]=0;
  values[3]=-1;
  for (i=0;i<6;i++) {
    records[i].key=wide[i]=values[i];
    records[i].tag=i;
  }
  ProbeSortCalls=0;
  QSortI64(NULL,0,&ProbeCompare);
  QSort(NULL,6,0,&ProbeCompare16);
  QSortI64(values,6,&ProbeCompare);
  Bool ok=ProbeSortCalls==21;
  ProbeSortCalls=0;
  QSort(wide,6,8,&ProbeCompare8);
  ok=ok && ProbeSortCalls==21;
  ProbeSortCalls=0;
  QSort(records,6,sizeof(CProbeSortRecord),&ProbeCompare16);
  for (i=0;i<6;i++)
    ok=ok && values[i]==wide[i] && records[i].key==values[i];
  return ok && ProbeSortCalls==21 && values[0]==-1 && values[1]==0 &&
         values[2]==0 && values[3]==2 && values[4]==2 && values[5]==2 &&
         records[0].tag==3 && records[1].tag==5 && records[2].tag==1 &&
         records[3].tag==4 && records[4].tag==2 && records[5].tag==0;
}

public I64 RuntimeProbeResult;
RuntimeProbeResult=ProbeMemory()+2*ProbeStrings()+4*ProbeHash()
    +8*ProbeNumbers()+16*(ProbeBootBase==0x7C20 &&
        (ProbeRunLevel & 0x25)==0x25 && !ProbeVarInitFlag && !ProbeVarInitVal)
    +32*ProbeCharTables()+64*ProbeCollections()+128*ProbeSort();
