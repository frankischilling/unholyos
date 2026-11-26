I64 doc_bin_lex_size=$IS,"External",BI=1,BP="::/Demo/Graphics/SpritePut.HC,2"$;

U64 DocBinLinkHash(U8 *data,I64 size)
{
  I64 i;
  U64 hash=0xCBF29CE484222325;
  for (i=0;i<size;i++)
    hash=(hash^data[i])*0x100000001B3;
  return hash;
}

Bool DocBinLinkData(CDocBin *b)
{
  return b && b->size==12546 && b->data &&
	DocBinLinkHash(b->data,b->size)==0xC429C519AE220BAF;
}

U0 DocBinLinkProbe()
{
  I64 res=0,size=0;
  CDrv *drive=Fs->cur_dv;
  CDoc *absolute=DocRead("::/Demo/Graphics/SpritePutExt.HC",8);
  CDoc *relative=DocRead("::/Demo/Graphics/DocBinLink.DD",8);
  CDocBin *number,*tag;
  U8 *saved;

  if (absolute && absolute->bin_head.next!=&absolute->bin_head &&
	DocBinLinkData(absolute->bin_head.next))
    res|=1;
  if (relative) {
    number=relative->bin_head.next;
    tag=number->next;
    if (number!=&relative->bin_head && number->num==1 && DocBinLinkData(number))
      res|=2;
    if (tag!=&relative->bin_head && tag->num==2 &&
	tag->next==&relative->bin_head && DocBinLinkData(tag) &&
	tag->data!=number->data)
      res|=4;
    saved=DocSave(relative,&size);
    if (saved && size==106 &&
	DocBinLinkHash(saved,size)==0xD47D941BC390AB90 &&
	relative->bin_head.last==tag)
      res|=16;
    Free(saved);
  }
  if (Fs->cur_dv==drive && Fs->cur_dir[0]=='/' && Fs->cur_dir[1]=='H' &&
	doc_bin_lex_size==12546)
    res|=8;

  DocDel(relative);
  DocDel(absolute);
  "Doc bin links %02X\n",res;
}

DocBinLinkProbe;
