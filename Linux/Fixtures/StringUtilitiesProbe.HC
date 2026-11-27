U64 SUHash(U64 h,U8 *st)
{
  while (*st)
    h=(h^*st++)*1099511628211;
  return (h^255)*1099511628211;
}

I64 SUPos(U8 *st,U8 *p)
{
  if (p) return p-st;
  return -1;
}

U0 StringUtilitiesProbe()
{
  I64 i,j,k,res=0,changed;
  U64 h=14695981039346656037,t=h,u=h,v=h,used;
  U8 all[256],a[512],b[512],*p,*q;
  F64 factors[6]={0.0,0.5,1.5,2.0,0.25,0.75};
  U8 *(*find)(U8 *needle,U8 *haystack,I64 flags)=&StrFind;
  U8 *(*alloc)(U8 *src,I64 flags,F64 scale)=&MStrUtil;
  for (i=0;i<255;i++) all[i]=i+1;
  all[255]=0;
  for (i=0;i<2048;i++) {
    StrCpy(a,all);
    if (StrUtil(a,i)!=a) break;
    h=SUHash(h,a);
    StrCpy(a," \t\r\n\x1F aBc$$\x01\x02 Z \t ");
    h=SUHash(h,StrUtil(a,i));
  }
  if (i==2048) res|=1;
  StrCpy(a," \t"); StrUtil(a,SUF_REM_TRAILING);
  if (!StrCmp(a," ")) res|=2;
  for (i=0;i<16;i++)
    for (j=0;j<25;j++) {
      for (k=0;k<i;k++) a[k]='A';
      for (;k<i+j;k++) a[k]=' ';
      StrCpy(a+k,"\tB\n        C");
      changed=Spaces2Tabs(b,a);
      t=SUHash((t^changed)*1099511628211,b);
      StrUtil(a,SUF_S2T);
      if (StrCmp(a,b)) res|=0x10000;
    }
  used=Fs->data_heap->used_u8s;
  for (i=0;i<40;i++) {
    for (j=0;j<i;j++) a[j]='\t';
    StrCpy(a+j,"X\n\tY");
    p=Tabs2Spaces(a);
    u=SUHash(u,p);
    Free(p);
  }
  for (i=0;i<20;i++) {
    for (j=0;j<i;j++) a[j]=' ';
    StrCpy(a+j,"Ab\tC");
    for (j=0;j<6;j++) {
      p=ScaleIndent(a,factors[j]);
      v=SUHash(v,p);
      Free(p);
    }
  }
  if (used==Fs->data_heap->used_u8s) res|=4;
  "SU trans %X %X %X %X\n",h,t,u,v;
  h=14695981039346656037;
  used=Fs->data_heap->used_u8s;
  for (i=0;i<2048;i++) {
    p=MStrUtil(" \t aBc$$\x01 Z \t ",i,0.5);
    h=SUHash(h,p);
    Free(p);
  }
  if (used==Fs->data_heap->used_u8s) res|=8;
  "SU allocated %X\n",h;
  StrCpy(a,"a/b,c");
  if (StrFirstOcc(a,",/")==a+1 && StrLastOcc(a,",/")==a+3 &&
      !StrFirstOcc(a,"!") && !StrLastOcc(a,"!")) res|=16;
  p=StrFirstRem(a,",/",b);
  if (p==b && !StrCmp(a,"b,c") && !StrCmp(b,"a")) res|=32;
  p=StrLastRem(a,",/",b);
  if (p==b && !StrCmp(a,"b") && !StrCmp(b,"c")) res|=64;
  StrFirstRem(a,"/",b);
  if (!*a && !StrCmp(b,"b")) res|=128;
  StrCpy(a,"x/y"); p=StrFirstRem(a,"/");
  q=StrLastRem(a,"/");
  if (!p && !q && !*a) res|=256;
  MemSet(a,0,sizeof(a));
  StrCpy(a,"cat_ xcat cat@ Cat cat.\x80" "cat cat");
  "SU find";
  for (i=0;i<8;i++) " %d",SUPos(a,StrFind("cat",a,i));
  "\nSU empty";
  for (i=0;i<8;i++) " %d",SUPos(a,StrFind("",a,i));
  "\n";
  if ((*find)("CAT",a,SFF_IGNORE_CASE)==a &&
      StrMatch("cat",a)==a && StrIMatch("CAT",a)==a &&
      StrMatch("",a)==a && !StrMatch("x",NULL) &&
      !StrMatch(NULL,a)) res|=512;
  p=(*alloc)("  aB  ",SUF_REM_LEADING|SUF_REM_TRAILING|SUF_TO_UPPER,0);
  if (!StrCmp(p,"AB")) res|=1024;
  Free(p);
  "String utilities %X\n",res;
}

StringUtilitiesProbe;
