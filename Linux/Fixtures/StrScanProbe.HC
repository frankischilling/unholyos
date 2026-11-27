I64 StrScanProbeError(I64 kind)
{
  I64 value=0;
  try {
    switch (kind) {
      case 0: StrScan("123",NULL); break;
      case 1: StrScan("123","%d"); break;
      case 2: StrScan("123","%*d"); break;
      case 3: StrScan("1.5","%3.*f"); break;
      case 4: StrScan("red","%z",&value); break;
      case 5: StrScan("red","%Z",&value); break;
      case 6: StrScan("red","%Z",&value,"ST_SCAN_PROBE_MISSING"); break;
      case 7: StrScan("abc","%s"); break;
      case 8: StrScan("abc","%c"); break;
      case 9: StrScan("abc","%C"); break;
      case 10: StrScan("ABC","%X"); break;
      case 11: StrScan("101","%b"); break;
      case 12: StrScan("1.5","%e"); break;
      case 13: StrScan("2/29/24","%D"); break;
      case 14: StrScan("123","%*d",3); break;
      case 15: StrScan("1.5","%3.*f",2); break;
    }
  } catch {
    value=Fs->except_ch;
    Fs->catch_except=TRUE;
  }
  return value;
}

U0 StrScanProbe()
{
  I64 res=0,a=0,b=0,c=0,d=0,i,n=0,errors=0;
  U8 text[128],dest[32],guard[16],*ptr,*out=dest;
  U8 *list="red\0@rouge\0green\0gray\0";
  U8 *(*scan)(U8 *src,U8 *fmt,...);
  F64 f=0,g=0,h=0,j=0;
  CDate date,old_offset=local_time_offset;

  StrCpy(text,"id=--12,0x2A,1011!");
  ptr=StrScan(text,"id=%d,%X,%b!",&a,&b,&c);
  "scan integers %d %d %d %d\n",a,b,c,ptr-text;
  if (a==12 && b==42 && c==11 && ptr==text+StrLen(text))
    res|=1;

  StrCpy(text,"abc:xyzTAIL");
  ptr=StrScan(text,"%3s:%*s",&out,3,&out);
  "scan width %s %d\n",dest,ptr-text;
  if (!StrCmp(dest,"xyz") && ptr==text+7)
    res|=2;

  StrCpy(text,"1.25;-2.5;3e2;0.5");
  ptr=StrScan(text,"%f;%e;%g;%n",&f,&g,&h,&j);
  "scan f %X %X %X %X %d\n",f(I64),g(I64),h(I64),j(I64),ptr-text;
  if (f==Str2F64("1.25") && g==Str2F64("-2.5") &&
      h==Str2F64("3e2") && j==Str2F64("0.5") && ptr==text+StrLen(text))
    res|=4;

  StrCpy(text,"1.25;1.25;1.25;1.25");
  ptr=StrScan(text,"%4.2f;%-4.*f;%*.1f;%*.*f",&f,7,&g,4,&h,4,9,&j);
  "scan p %X %X %X %X %d\n",f(I64),g(I64),h(I64),j(I64),ptr-text;
  if (f==Str2F64("1.25") && f==g && f==h && f==j && ptr==text+StrLen(text))
    res|=8;

  MemSet(guard,0xA5,sizeof(guard));
  StrCpy(text,"aB");
  ptr=StrScan(text,"%1C%1c",guard+1,guard+2);
  if (guard[0]==0xA5 && guard[1]=='A' && guard[2]=='B' &&
      guard[3]==0xA5 && ptr==text+2)
    res|=16;
  StrCpy(text,"12345678");
  StrScan(text,"%8d",guard+1);
  MemCpy(&a,guard+1,8);
  "scan stores %d %X %X\n",a,guard[0],guard[9];
  if (a==12345678 && guard[0]==0xA5 && guard[9]==0xA5)
    res|=32;

  StrCpy(text,"rouge|gre|gr|missing");
  ptr=StrScan(text,"%z|%z|%z|%z",&a,list,&b,list,&c,list,&d,list);
  "scan lists %d %d %d %d %d\n",a,b,c,d,ptr-text;
  if (a==0 && b==1 && c==-2 && d==-1 && ptr==text+StrLen(text))
    res|=64;
  DefineLstLoad("ST_SCAN_PROBE",list);
  StrCpy(text,"rouge|green|gray");
  ptr=StrScan(text,"%Z|%Z|%Z",&a,"ST_SCAN_PROBE",&b,"ST_SCAN_PROBE",
              &c,"ST_SCAN_PROBE");
  "scan defines %d %d %d %d\n",a,b,c,ptr-text;
  if (a==0 && b==1 && c==2 && ptr==text+StrLen(text))
    res|=128;

  local_time_offset=0x40000000;
  StrCpy(text,"2/29/24!");
  ptr=StrScan(text,"%D!",&date);
  "scan date %X %d\n",date,ptr-text;
  if (date==Str2Date("2/29/24") && ptr==text+StrLen(text))
    res|=256;
  local_time_offset=old_offset;

  StrCpy(text,"zq42");
  ptr=StrScan(text,"A%%%d",&a);
  if (a==42 && ptr==text+4)
    res|=512;
  MemSet(text,'x',sizeof(text));
  text[1]=0;
  text[2]='Q';
  text[3]='R';
  text[4]=0;
  MemSet(dest,0xA5,sizeof(dest));
  ptr=StrScan(text,"%3s%1C",&out,guard);
  "scan nul %X %X %X %d\n",dest[0],dest[1],guard[0],ptr-text;
  if (dest[0]=='x' && !dest[1] && dest[2]==0xA5 && guard[0]=='R' && ptr==text+4)
    res|=1024;

  StrCpy(text,"abc42");
  ptr=StrScan(text,"%3o%2d",&a);
  if (a==42 && ptr==text+5)
    res|=2048;
  MemSet(text,0,sizeof(text));
  StrCpy(text,"1.25");
  f=7;
  ptr=StrScan(text,"%.2f",&f);
  "scan zero-width precision %X %d\n",f(I64),ptr-text;
  if (f==7 && ptr==text+4)
    res|=4096;

  for (i=0;i<16;i++) {
    a=StrScanProbeError(i);
    if (i==6)
      b='UndefDef';
    else
      b='Scan';
    if (a==b)
      errors|=1<<i;
  }
  "scan errors %X\n",errors;
  if (errors==0xFFFF)
    res|=8192;
  scan=&StrScan;
  for (i=0;i<100;i++) {
    StrCpy(text,"17:xyz");
    ptr=scan(text,"%d:%s",&a,&out);
    if (a==17 && !StrCmp(dest,"xyz") && ptr==text+6)
      n++;
  }
  "scan indirect %d\n",n;
  if (n==100)
    res|=16384;
  "StrScan probe %X\n",res;
}

StrScanProbe;
