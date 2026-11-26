U8 sprite_flood_stream[31]={
  SPT_DITHER_COLOR,RED,BLUE,
  SPT_FLOOD_FILL,0,0,0,0,0,0,0,0,
  SPT_FLOOD_FILL_NOT,8,0,0,0,0,0,0,0,
  SPT_PT,255,255,255,255,0,0,0,0,
  SPT_END
};

U8 sprite_flood_single[19]={
  SPT_FLOOD_FILL,255,255,255,255,1,0,0,0,
  SPT_PT,6,0,0,0,0,0,0,0,
  SPT_END
};

U64 SpriteFloodHash(CDC *dc,Bool depth=FALSE)
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

U0 SpriteFloodProbe()
{
  CDC *dc=DCNew(16,8),*brush=DCNew(2,2);
  I64 flags,old_color,check=0;
  DCFill(dc,RED);
  dc->color=WHITE;
  GrRect(dc,2,2,4,2);
  GrRect(dc,10,2,4,2);
  dc->color=GREEN;
  GrPlot(dc,11,2);
  dc->color=ROPF_DITHER|(YELLOW<<16)|GREEN;
  dc->color2=PURPLE;
  old_color=dc->color;
  flags=dc->flags;
  Sprite3(dc,2,2,0,sprite_flood_stream);
  if (GrPeek(dc,2,2)==RED && GrPeek(dc,3,2)==BLUE &&
      GrPeek(dc,5,3)==RED) check|=1;
  if (GrPeek(dc,10,2)==RED && GrPeek(dc,11,2)==RED &&
      GrPeek(dc,13,3)==RED) check|=2;
  if (GrPeek(dc,1,2)==BLUE && dc->color==old_color &&
      dc->color2==PURPLE && dc->flags==flags) check|=4;
  "sprite flood stream %016X\n",SpriteFloodHash(dc);

  DCFill(dc,BLACK);
  dc->color=WHITE;
  GrRect(dc,5,3,4,2);
  DCDepthBufAlloc(dc);
  dc->color=GREEN;
  dc->brush=brush;
  dc->x=5; dc->y=2; dc->z=1;
  dc->flags|=DCF_TRANSFORMATION;
  flags=dc->flags;
  Sprite3(dc,2,0,3,sprite_flood_single,TRUE);
  if (GrPeek(dc,5,3)==GREEN && GrPeek(dc,8,4)==GREEN &&
      GrPeek(dc,13,2)==BLACK && dc->depth_buf[3*16+6]==4 &&
      dc->flags==flags && dc->color==GREEN && dc->brush==brush) check|=8;
  "sprite flood single %016X %016X\n",SpriteFloodHash(dc),SpriteFloodHash(dc,TRUE);
  DCDel(brush); DCDel(dc);
  "sprite flood %X\n",check;
}
SpriteFloodProbe;
