U64 FloodHash(CDC *dc,Bool depth=FALSE)
{
  U64 h=0xCBF29CE484222325,v;
  I64 i;
  for (i=0;i<dc->width_internal*dc->height;i++) {
    if (depth) v=dc->depth_buf[i];
    else v=dc->body[i];
    h=(h^v)*0x100000001B3;
  }
  return h;
}

U0 FloodFillProbe()
{
  CDC *dc=DCNew(16,8),*brush=DCNew(2,2);
  I64 n,flags,check=0;
  DCFill(dc,BLACK);
  dc->color=WHITE;
  GrRect(dc,2,2,12,4);
  dc->color=RED;
  dc->color2=BLUE;
  dc->brush=brush;
  DCExtentsInit(dc);
  dc->flags|=DCF_LOCATE_NEAREST;
  dc->cur_x=5; dc->cur_y=3; dc->nearest_dist=999;
  flags=dc->flags;
  n=GrFloodFill(dc,5,3);
  if (n==48 && GrPeek(dc,2,2)==RED && GrPeek(dc,13,5)==RED &&
      GrPeek(dc,1,2)==BLACK && dc->color2==BLUE && dc->brush==brush &&
      dc->flags==flags && dc->min_x==2 && dc->max_x==13 &&
      dc->min_y==2 && dc->max_y==5 && !dc->nearest_dist) check|=1;
  "fill region %d %016X\n",n,FloodHash(dc);
  dc->brush=NULL;
  dc->flags&=~(DCF_LOCATE_NEAREST|DCF_RECORD_EXTENTS);
  if (!GrFloodFill(dc,5,3) && !GrFloodFill(dc,-1,2)) check|=2;
  dc->flags|=DCF_DONT_DRAW;
  dc->color=GREEN;
  if (!GrFloodFill(dc,5,3) && GrPeek(dc,5,3)==RED) check|=4;
  dc->flags&=~DCF_DONT_DRAW;

  DCFill(dc,BLACK);
  dc->color=WHITE;
  GrRect(dc,2,2,12,4);
  dc->color=RED;
  GrRect(dc,4,3,8,2);
  dc->color=BLACK;
  n=GrFloodFill(dc,3,2,TRUE);
  if (n==48 && GrPeek(dc,5,3)==BLACK && GrPeek(dc,13,5)==BLACK) check|=8;
  "fill boundary %d %016X\n",n,FloodHash(dc);

  DCFill(dc,WHITE);
  dc->color=ROPF_DITHER|(BLUE<<16)|RED;
  n=GrFloodFill(dc,5,3);
  if (n==128 && GrPeek(dc,0,0)==RED && GrPeek(dc,1,0)==BLUE) check|=16;
  "fill dither %d %016X\n",n,FloodHash(dc);

  DCFill(dc,BLACK);
  dc->color=WHITE;
  GrRect(dc,2,2,6,1);
  dc->color=ROP_XOR|BLUE;
  n=GrFloodFill(dc,5,2);
  if (n==6 && GrPeek(dc,2,2)==(WHITE^BLUE)) check|=32;
  "fill xor %d %016X\n",n,FloodHash(dc);

  DCFill(dc,BLACK);
  dc->color=WHITE;
  GrRect(dc,2,2,6,1);
  DCDepthBufAlloc(dc);
  dc->depth_buf[2*16+6]=1;
  dc->color=RED;
  n=GrFloodFill(dc,4,2,FALSE,5,dc->depth_buf);
  if (n==4 && GrPeek(dc,6,2)==WHITE && dc->depth_buf[2*16+6]==1 &&
      dc->depth_buf[2*16+1]==5) check|=64;
  "fill depth %d %016X %016X\n",n,FloodHash(dc),FloodHash(dc,TRUE);

  DCFill(dc,BLACK);
  DCDepthBufRst(dc);
  dc->color=WHITE;
  GrRect(dc,2,2,3,2);
  GrRect(dc,12,2,3,2);
  dc->color=GREEN;
  DCSymmetrySet(dc,8,0,8,7);
  dc->flags|=DCF_SYMMETRY;
  flags=dc->flags;
  n=GrFloodFill3(dc,3,2,1);
  if (n==6 && GrPeek(dc,3,2)==GREEN && GrPeek(dc,13,2)==GREEN && dc->flags==flags) check|=128;
  "fill symmetry %d %016X %016X\n",n,FloodHash(dc),FloodHash(dc,TRUE);
  dc->flags&=~DCF_SYMMETRY;
  DCFill(dc,BLACK);
  DCDepthBufRst(dc);
  dc->color=WHITE;
  GrRect(dc,5,3,4,3);
  dc->color=RED;
  dc->x=3; dc->y=2; dc->z=1;
  dc->flags|=DCF_TRANSFORMATION;
  flags=dc->flags;
  n=GrFloodFill3(dc,3,2,2);
  if (n==12 && GrPeek(dc,6,4)==RED && dc->depth_buf[4*16+6]==3 && dc->flags==flags) check|=256;
  "fill transform %d %016X %016X\n",n,FloodHash(dc),FloodHash(dc,TRUE);
  DCDel(brush); DCDel(dc);
  "flood fill %X\n",check;
}
FloodFillProbe;
