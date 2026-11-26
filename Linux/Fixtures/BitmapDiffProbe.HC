U64 BitmapDiffHash(U8 *data,I64 size)
{
  U64 hash=0xCBF29CE484222325;
  I64 i;
  for (i=0;i<size;i++)
    hash=(hash^data[i])*0x100000001B3;
  return hash;
}

U0 BitmapDiffProbe()
{
  CDC *base=DCNew(16,10),*update,*diff,*ext,*pad=DCNew(13,5),*pad2;
  CDC *dst=DCNew(13,5);
  I64 i,j,result=0,err=0,sz;
  U8 *sprite;
  CDoc *doc=DocNew;
  CDocBin *bin;
  DCFill(base,WHITE);
  update=DCCopy(base);
  if (!DCDiff(base,update))
    result|=1;
  update->color=RED;
  GrPlot0(update,2,3);
  GrPlot0(update,10,7);
  diff=DCDiff(base,update);
  if (diff && diff->x0==2 && diff->y0==3 && diff->width==9 && diff->height==5) {
    GrBlot(base,diff->x0,diff->y0,diff);
    if (BitmapDiffHash(base->body,160)==BitmapDiffHash(update->body,160))
      result|=2;
  }
  ext=DCExt(update,4,3,-2,-1);
  if (ext && ext->width==7 && ext->height==5) {
    for (j=0;j<5;j++)
      for (i=0;i<7;i++) {
        if (i<2 || !j) {
          if (GrPeek0(ext,i,j)!=TRANSPARENT) err++;
        } else if (GrPeek0(ext,i,j)!=GrPeek0(update,i-2,j-1)) err++;
      }
    if (!err) result|=4;
  }
  DCFill(pad,BLACK);
  pad2=DCCopy(pad);
  pad2->body[76]=RED;
  //The reference's top scan advances by width, omitting this padded-row pixel.
  if (!DCDiff(pad,pad2))
    result|=8;
  for (i=0;i<80;i++)
    pad->body[i]=i%16;
  pad->body[17]=TRANSPARENT;
  sprite=DC2Sprite(pad);
  sz=SpriteSize(sprite);
  if (sz==98 && sprite[0]==SPT_BITMAP && sprite[9]==13 && sprite[13]==5 &&
      BitmapDiffHash(sprite+17,80)==BitmapDiffHash(pad->body,80) && !sprite[97])
    result|=16;
  DCFill(dst,BLUE);
  Sprite3(dst,0,0,0,sprite);
  err=0;
  for (j=0;j<5;j++)
    for (i=0;i<13;i++) {
      if (GrPeek0(pad,i,j)==TRANSPARENT) {
        if (GrPeek0(dst,i,j)!=BLUE) err++;
      } else if (GrPeek0(dst,i,j)!=GrPeek0(pad,i,j)) err++;
    }
  if (!err) result|=32;
  GRWrite("B:/BitmapDiffProbe",pad);
  DocGR(doc,"B:/BitmapDiffProbe");
  bin=doc->bin_head.next;
  if (bin!=&doc->bin_head && bin->size==sz && bin->next==&doc->bin_head &&
      BitmapDiffHash(bin->data,sz)==BitmapDiffHash(sprite,sz))
    result|=64;
  "bitmap diff %X %016X\n",result,BitmapDiffHash(sprite,sz);
  Free(sprite);
  DocDel(doc);
  DCDel(ext);
  DCDel(diff);
  DCDel(base);
  DCDel(update);
  DCDel(pad);
  DCDel(pad2);
  DCDel(dst);
}
BitmapDiffProbe;
