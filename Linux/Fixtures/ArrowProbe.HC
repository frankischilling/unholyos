U8 arrow_elems[25]={
  SPT_THICK,4,0,0,0,
  SPT_COLOR,LTGREEN,
  SPT_ARROW,2,0,0,0,5,0,0,0,24,0,0,0,11,0,0,0,
  SPT_END
};

U64 ArrowHash(CDC *dc,Bool depth=FALSE)
{
  U64 hash=0xCBF29CE484222325,value;
  I64 i;
  for (i=0;i<dc->width_internal*dc->height;i++) {
    if (depth)
      value=dc->depth_buf[i];
    else
      value=dc->body[i];
    hash=(hash^value)*0x100000001B3;
  }
  return hash;
}

U0 ArrowProbe()
{
  CDC *dc=DCNew(96,69);
  I64 a,b,c,r[16],*old_r=dc->r;

  DCFill(dc,WHITE);
  dc->thick=3;
  dc->color=RED;
  a=GrArrow3(dc,-7,12,0,26,2,8);
  dc->thick=4;
  dc->color=ROP_XOR|BLUE;
  b=GrArrow3(dc,58,44,8,30,31,0,1.125,3,2);
  dc->thick=1;
  dc->color=GREEN;
  c=GrArrow3(dc,6,5,4,6,5,7);
  "arrow flat %d %d %d %016X\n",a,b,c,ArrowHash(dc);

  DCFill(dc,WHITE);
  DCDepthBufAlloc(dc);
  Mat4x4IdentEqu(r);
  Mat4x4RotZ(r,0.125);
  DCMat4x4Set(dc,r);
  dc->flags|=DCF_TRANSFORMATION;
  dc->x=6; dc->y=4; dc->z=3;
  DCSymmetrySet(dc,37,0,37,60);
  dc->flags|=DCF_SYMMETRY;
  dc->thick=7;
  dc->color=RED;
  a=GrArrow3(dc,6,11,12,25,32,4,,2,1);
  dc->flags|=DCF_JUST_MIRROR;
  dc->color=LTBLUE;
  b=GrArrow3(dc,4,47,2,31,33,0,2.0);
  dc->color=BLUE;
  GrBorder(dc,6,9,26,21,2,1);
  "arrow depth %d %d %X %016X %016X\n",a,b,dc->flags,
    ArrowHash(dc),ArrowHash(dc,TRUE);

  DCFill(dc,WHITE);
  DCDepthBufRst(dc);
  dc->flags&=~DCF_JUST_MIRROR;
  Sprite3(dc,15,34,2,arrow_elems);
  "arrow sprite %d %d %016X %016X\n",SpriteSize(arrow_elems),dc->thick,
    ArrowHash(dc),ArrowHash(dc,TRUE);

  DCFill(dc,WHITE);
  DCDepthBufRst(dc);
  dc->color=BLACK;
  DrawStdMs(dc,15,9);
  DrawStdMs(dc,-2,8);
  DrawStdMs(dc,95,63);
  "arrow cursor %d %X %016X %016X\n",dc->thick,dc->flags,
    ArrowHash(dc),ArrowHash(dc,TRUE);

  DCFill(dc,WHITE);
  DCDepthBufRst(dc);
  dc->color=BLUE;
  dc->cur_x=4; dc->cur_y=5;
  dc->nearest_dist=1000;
  dc->flags|=DCF_LOCATE_NEAREST;
  DCExtentsInit(dc);
  a=GrPutChar(dc,-3,2,'G');
  b=GrPrint(dc,-2,12,"%d\t%c\n$$SX,-2$$$$SY,3$$%s",17,'J',"Q");
  c=GrPutChar(dc,95,67,'M');
  "arrow text %d %d %d %d %d %d %d %d\n",a,b,c,
    dc->min_x,dc->min_y,dc->max_x,dc->max_y,dc->nearest_dist;
  "arrow glyphs %016X %016X\n",ArrowHash(dc),ArrowHash(dc,TRUE);

  dc->color=ROP_COLLISION;
  dc->bkcolor=WHITE;
  a=GrPutChar(dc,-3,2,'G');
  dc->color=ROP_XOR|RED;
  b=GrPutChar(dc,-3,2,'G');
  dc->color=ROP_XOR;
  c=GrPutChar(dc,4,12,'M');
  "arrow glyphrop %d %d %d %d %016X\n",a,b,c,dc->collision_cnt,ArrowHash(dc);
  dc->r=old_r;
  DCDel(dc);
}
ArrowProbe;
