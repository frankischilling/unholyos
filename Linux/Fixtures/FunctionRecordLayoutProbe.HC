// Record layout from Terry A. Davis's Compiler/PrsLib.HC::PrsFunNew,
// TempleOS c26482bb6ad3f80106d28504ec5db3c6a360732c.
_extern _MHEAP_CTRL CHeapCtrl *RecordLayoutOwner(U8 *addr);

I64 RecordLayoutTarget(I64 value)
{
  return value+1;
}

class CRecordLayoutCallback
{
  I64 (*pointer)(I64 value);
};

CHashFun *record_layout_named=HashFind("RecordLayoutTarget",Fs->hash_table,HTT_FUN);
CHashClass *record_layout_cls=HashFind("CRecordLayoutCallback",Fs->hash_table,HTT_CLASS);
CHashFun *record_layout_callback=record_layout_cls->member_lst_and_root->fun_ptr-1;

U0 RecordLayoutCheck(CHashFun *base)
{
  I64 i;
  U32 companions=0;
  for (i=1;i<=4;i++)
    if (base[i].type==HTT_FUN && base[i].size==8 &&
        base[i].raw_type==10 && base[i].ptr_stars_cnt==i)
      companions|=1<<(i-1);
  "FR layout %d %d %d %X\n",MSize(base),MSize2(base),
        RecordLayoutOwner(base)==Fs->code_heap,companions;
}

RecordLayoutCheck(record_layout_named);
RecordLayoutCheck(record_layout_callback);
"FR layout DONE\n";
