// Compile and load the unchanged byte-verified common function-record fixture.
I64 function_records_aot_errors;
U8 *function_records_aot_module=NULL;
U8 *function_records_aot_project=
    "#include \"::/Kernel/KernelA.HH\"\n"
    "#include \"::/Compiler/CompilerA.HH\"\n"
    "#exe {Option(OPTf_EXTERNS_TO_IMPORTS,ON);}\n"
    "#include \"::/Kernel/KernelB.HH\"\n"
    "#include \"::/Kernel/KernelC.HH\"\n"
    "#include \"::/Compiler/CompilerB.HH\"\n"
    "#include \"::/Adam/AExts.HC\"\n"
    "#exe {Option(OPTf_EXTERNS_TO_IMPORTS,OFF);}\n"
    "#include \"B:/FunctionRecordsProbe.HC\"\n";
DocClear(,TRUE);
function_records_aot_errors=-1;
if (FileWrite("B:/FunctionRecordsAOT.PRJ",function_records_aot_project,
    StrLen(function_records_aot_project)))
  function_records_aot_errors=Cmp("B:/FunctionRecordsAOT.PRJ",,
      "B:/FunctionRecordsProbe.BIN.Z");
if (!function_records_aot_errors)
  function_records_aot_module=Load("B:/FunctionRecordsProbe.BIN.Z");
"FR AOT compile %d\n",function_records_aot_errors;
"FR AOT load %d\n",function_records_aot_module!=NULL;
"FR AOT DONE\n";
