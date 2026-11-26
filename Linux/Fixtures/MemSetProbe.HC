U0 MemSetProbe()
{
  U8 a[5];
  U16 b[5];
  U32 c[5];
  I64 d[5],i,res=0;
  MemSet(a,0,sizeof(a));
  MemSet(b,0,sizeof(b));
  MemSet(c,0,sizeof(c));
  MemSet(d,0,sizeof(d));
  if (MemSetU8(a+1,0x1234,3)==a+4 && MemSetU8(a+4,1,0)==a+4)
    res|=1;
  if (MemSetU16(b+1,0x12345678,3)==b+4 && MemSetU16(b+4,1,0)==b+4)
    res|=2;
  if (MemSetU32(c+1,0x123456789ABCDEF0,3)==c+4 && MemSetU32(c+4,1,0)==c+4)
    res|=4;
  if (MemSetI64(d+1,0x123456789ABCDEF0,3)==d+4 && MemSetI64(d+4,1,0)==d+4)
    res|=8;
  if (!a[0]&&!a[4]&&!b[0]&&!b[4]&&!c[0]&&!c[4]&&!d[0]&&!d[4])
    res|=16;
  for (i=1;i<4;i++)
    if (a[i]!=0x34 || b[i]!=0x5678 || c[i]!=0x9ABCDEF0 || d[i]!=0x123456789ABCDEF0)
      break;
  if (i==4) res|=32;
  if (ATTRF_BLINK==1<<ATTRf_BLINK && ATTRF_INVERT==1<<ATTRf_INVERT &&
      ATTRF_SEL==1<<ATTRf_SEL && ATTRF_UNDERLINE==1<<ATTRf_UNDERLINE)
    res|=64;
  CDoc *doc=DocNew;
  I64 flags=doc->flags;
  if (DocCursor(0,doc) && !DocCursor(0,doc) &&
      doc->flags&DOCF_HIDE_CURSOR && !DocCursor(1,doc) &&
      DocCursor(1,doc) && doc->flags==flags)
    res|=128;
  if (DocHighlightCursor(0,doc) && !DocHighlightCursor(0,doc) &&
      doc->flags&DOCF_DONT_HIGHLIGHT_CURSOR &&
      !DocHighlightCursor(1,doc) && doc->flags==flags)
    res|=256;
  if (DocScroll(0,doc) && !DocScroll(0,doc) &&
      doc->flags&DOCF_NO_SCROLL_BARS && !DocScroll(1,doc) &&
      DocScroll(1,doc) && doc->flags==flags)
    res|=512;
  DocDel(doc);
  "Memory set probe %03X\n",res;
}
MemSetProbe;
