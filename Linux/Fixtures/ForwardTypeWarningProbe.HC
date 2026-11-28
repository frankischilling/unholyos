// Forward type warnings from Terry A. Davis's Compiler/PrsStmt.HC,
// TempleOS c26482bb6ad3f80106d28504ec5db3c6a360732c.
U8 *forward_warning_source =
  "extern class NoUseClass;\n"
  "class NoUseClass {I64i value;};\n"
  "extern union NoUseUnion;\n"
  "union NoUseUnion {I64i value;};\n"
  "extern class UsedClass;\n"
  "UsedClass *used_class;\n"
  "class UsedClass {I64i value;};\n"
  "extern union UsedUnion;\n"
  "UsedUnion *used_union;\n"
  "union UsedUnion {I64i value;};\n"
  "I64i ForwardRead(){UsedClass c;UsedUnion u;c.value=29;u.value=13;return c.value+u.value;}\n";
if (FileWrite("Warn.PRJ", forward_warning_source, StrLen(forward_warning_source)))
  "WW %X\n", 1;
