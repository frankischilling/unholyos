// Compile and load the byte-verified common primitive fixture in TempleOS.
I64 internal_types_aot_errors;
U8 *internal_types_aot_module=NULL;
U8 *internal_types_aot_project=
    "#include \"::/Kernel/KernelA.HH\"\n"
    "#include \"::/Compiler/CompilerA.HH\"\n"
    "#exe {Option(OPTf_EXTERNS_TO_IMPORTS,ON);}\n"
    "#include \"::/Kernel/KernelB.HH\"\n"
    "#include \"::/Kernel/KernelC.HH\"\n"
    "#include \"::/Compiler/CompilerB.HH\"\n"
    "#include \"::/Adam/AExts.HC\"\n"
    "#exe {Option(OPTf_EXTERNS_TO_IMPORTS,OFF);}\n"
    "#include \"B:/InternalTypesProbe.HC\"\n";
DocClear(,TRUE);
internal_types_aot_errors=-1;
if (FileWrite("B:/InternalTypesAOT.PRJ",internal_types_aot_project,
    StrLen(internal_types_aot_project)))
  internal_types_aot_errors=Cmp("B:/InternalTypesAOT.PRJ",,
      "B:/InternalTypesProbe.BIN.Z");
if (!internal_types_aot_errors)
  internal_types_aot_module=Load("B:/InternalTypesProbe.BIN.Z");
"IT AOT compile %d\n",internal_types_aot_errors;
"IT AOT load %d\n",internal_types_aot_module!=NULL;
"IT AOT DONE\n";
