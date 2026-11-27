U64 SCUpper(U64 n)
{
  U64 noreg result;
  asm {
    MOV RAX,&n[RBP]
    IMPORT TO_UPPER;
    CALL TO_UPPER
    MOV &result[RBP],RAX
  }
  return result;
}

U0 SCRow(U8 *tag,U8 *a,U8 *b,I64 n)
{
  I64 i=StrICmp(a,b),c=StrNCmp(a,b,n),d=StrNICmp(a,b,n);
  I64 (*ip)(U8 *,U8 *);
  I64 (*np)(U8 *,U8 *,I64);
  I64 (*nip)(U8 *,U8 *,I64);
  ip=&StrICmp;
  np=&StrNCmp;
  nip=&StrNICmp;
  "SC %s %d %d %d %d\n",tag,i,c,d,
      i==ip(a,b) && c==np(a,b,n) && d==nip(a,b,n);
}

U0 StringCompareProbe()
{
  U8 a[4],b[4];
  I64 i,j,fail=0;
  U64 hi=0xCBF29CE484222325,hn=hi,hni=hi,hu=hi,v,w;
  SCRow("empty","","",0);
  SCRow("left","","a",3);
  SCRow("right","a","",3);
  SCRow("case","TempleOS","tEMPLEos",8);
  SCRow("order","a","B",1);
  SCRow("punct","[","`",1);
  SCRow("upperedge","Z","[",1);
  SCRow("loweredge","`","a",1);
  SCRow("late","abc","aBc",3);
  SCRow("short","abc","abd",2);
  SCRow("long","abc","abd",3);
  SCRow("term","ab","abc",3);
  SCRow("negative","Same","sAME",-1);
  SCRow("negdiff","abc","ABD",-1);
  SCRow("mincount","abc","ABC",0x8000000000000000);
  SCRow("embedded","ab\0x","ab\0y",9);
  StrCpy(a,"aab");
  SCRow("overlap",a,a+1,3);
  MemSet(a,0,sizeof(a));
  MemSet(b,0,sizeof(b));
  a[0]=0x80; b[0]=0xFF;
  SCRow("high",a,b,1);
  a[0]=0xE0; b[0]=0xC0;
  SCRow("extended",a,b,-1);
  "SC null %d %d\n",StrNCmp(NULL,NULL,0),StrNICmp(NULL,NULL,0);
  "SC occ %d %d\n",StrOcc("a/b/c",'/'),StrOcc("abc",0);
  for (i=0;i<256;i++) {
    a[0]=i;
    for (j=0;j<256;j++) {
      b[0]=j;
      hi=(hi^(StrICmp(a,b)+1))*0x100000001B3;
      hn=(hn^(StrNCmp(a,b,1)+1))*0x100000001B3;
      hni=(hni^(StrNICmp(a,b,1)+1))*0x100000001B3;
    }
    v=0x123456789ABCDE00|i;
    w=v;
    if ('a'<=i<='z') w-=0x20;
    v=SCUpper(v);
    if (v!=w) fail++;
    hu=(hu^v)*0x100000001B3;
  }
  "SC bytes %X %X %X\n",hi,hn,hni;
  "SC upper %X %d\n",hu,fail;
  "SC DONE\n";
}
StringCompareProbe;
