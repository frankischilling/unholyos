U0 FSRow(U8 *tag,U8 *src)
{
  U8 *end;
  F64 d=Str2F64(src,&end);
  "FS %s %X %d\n",tag,d(I64),end-src;
}

U0 FloatScanProbe()
{
  U8 st[4];
  FSRow("empty","\0\0\0");
  FSRow("space"," \t\r\n\0\0\0");
  FSRow("zero","0");
  FSRow("minuszero","-0");
  FSRow("minus","-\0\0\0");
  FSRow("plus","+1.25");
  FSRow("minusminus","--1");
  FSRow("integer","123tail");
  FSRow("minusint","-123");
  FSRow("quarter",".25!");
  FSRow("dotted","1.2.3");
  FSRow("bareexp","e2!");
  FSRow("dot",".");
  FSRow("exponent","12.5e-1!");
  FSRow("plusexp","1e+2");
  FSRow("emptyexp","1e-");
  FSRow("inf","inf");
  FSRow("inftail","infinity");
  FSRow("minusinf","-inf?");
  FSRow("upperinf","INF");
  MemSet(st,0,sizeof(st));
  st[0]=0xEC;
  FSRow("byte",st);
  st[0]='x'; st[1]=0xEC;
  FSRow("lookahead",st);
  st[0]='1';
  FSRow("digitlook",st);
  st[0]='-';
  FSRow("minusbyte",st);
  st[1]='x'; st[2]=0xEC;
  FSRow("minuslook",st);
  st[0]=0; st[1]=0xEC; st[2]=0;
  FSRow("nullahead",st);
  FSRow("maxint","9223372036854775807");
  FSRow("wrapint","18446744073709551615");
  FSRow("minuswrap","-18446744073709551615");
  FSRow("wrapfrac","1844674407370955161.6");
  FSRow("largefrac","900719925474099.3");
  FSRow("largefrac2","900719925474099.5");
  FSRow("largeexp","9007199254740993e-1");
  FSRow("minuslarge","-900719925474099.3");
  FSRow("wrapexpzero","1e18446744073709551616");
  FSRow("wrapexpone","1e18446744073709551617");
  FSRow("wrapexpneg","1e18446744073709551615");
  FSRow("negwrapexp","1e-18446744073709551615");
  FSRow("minexp","1e-9223372036854775808");
  FSRow("wrapexpfrac","1.2e18446744073709551616");
  FSRow("zeroinf","0e309");
  FSRow("underflow","-1e-309");
  FSRow("nantext","NaN");
  FSRow("signspace","- 1");
  "FS DONE\n";
}
FloatScanProbe;
