// Compile the byte-verified common bit fixture in TempleOS.
I64 bit_aot_errors=-1;
U8 *bit_aot_module=NULL;
U8 *bit_aot_project=
    "#include \"::/Kernel/KernelA.HH\"\n"
    "#include \"::/Compiler/CompilerA.HH\"\n"
    "#exe {Option(OPTf_EXTERNS_TO_IMPORTS,ON);}\n"
    "#include \"::/Kernel/KernelB.HH\"\n"
    "#include \"::/Kernel/KernelC.HH\"\n"
    "#include \"::/Compiler/CompilerB.HH\"\n"
    "#include \"::/Adam/AExts.HC\"\n"
    "#exe {Option(OPTf_EXTERNS_TO_IMPORTS,OFF);}\n"
    "#include \"B:/BitProbe.HC\"\n";
DocClear(,TRUE);
if (FileWrite("B:/BitAOT.PRJ",bit_aot_project,StrLen(bit_aot_project)))
  bit_aot_errors=Cmp("B:/BitAOT.PRJ",,"B:/BitProbe.BIN.Z");
DocClear(,TRUE);
if (!bit_aot_errors)
  bit_aot_module=Load("B:/BitProbe.BIN.Z");
"BIT AOT compile %d\n",bit_aot_errors;
"BIT AOT load %d\n",bit_aot_module!=NULL;
"BIT AOT DONE\n";
