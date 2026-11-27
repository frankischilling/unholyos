U64 NativeImportAddress(U8 *name)
{
  CHash *entry=HashFind(name,Fs->hash_table,HTG_ALL-HTT_IMPORT_SYS_SYM);
  if (!entry)
    return 0;
  if (entry->type&HTT_FUN)
    return entry(CHashFun *)->exe_addr;
  return entry(CHashExport *)->val;
}

U0 NativeImportLoadProbe()
{
  U64 public_put=NativeImportAddress("PutS"),native_put,native_cmp,
      value=0,checks=0,*bindings;
  CHashExport *result,*pointers;
  if (!public_put || NativeImportAddress("__unholyos_sysv.PutS") ||
      NativeImportAddress("__unholyos_sysv.StrCmpCpp"))
    throw('Native');
  if (!Load("B:/NativeImportReference.BIN",LDF_SILENT))
    throw('Native');
  native_put=NativeImportAddress("__unholyos_sysv.PutS");
  native_cmp=NativeImportAddress("__unholyos_sysv.StrCmpCpp");
  "NI START\n";
  Load("B:/NativeImportProbe.BIN",LDF_SILENT);
  result=HashFind("NativeImportResult",Fs->hash_table,HTT_EXPORT_SYS_SYM);
  pointers=HashFind("NativeImportBindings",Fs->hash_table,HTT_EXPORT_SYS_SYM);
  if (result && pointers) {
    value=*result->val(U64 *);
    bindings=pointers->val;
    if (native_put && bindings[0]==native_put && native_put!=public_put)
      checks|=1;
    if (native_cmp && bindings[1]==native_cmp)
      checks|=2;
    if (bindings[2]==public_put && NativeImportAddress("PutS")==public_put)
      checks|=4;
  }
  "NI result %016X\n",value;
  "NI bindings %016X\nNI DONE\n",checks;
}

NativeImportLoadProbe;
