U8 *warning_source = "#assert 0\nI64i NoValue(){return;}\nU0 VoidValue(){return 3;}\nextern I64i WarnUnused(I64i a);\nI64i WarnUnused(I64i a){return a;}\n";
if (FileWrite("Warn.PRJ", warning_source, StrLen(warning_source)))
  "WW %X\n", 1;
