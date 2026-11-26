U8 pen_elems[77]={
  SPT_THICK,5,0,0,0,
  SPT_COLOR,LTGREEN,
  SPT_PT,6,0,0,0,3,0,0,0,
  SPT_LINE,10,0,0,0,2,0,0,0,20,0,0,0,9,0,0,0,
  SPT_POLYPT,2,0,0,0,3,0,0,0,25,0,0,0,4|7<<3,
  SPT_POLYLINE,3,0,0,0,2,0,0,0,18,0,0,0,10,0,0,0,21,0,0,0,14,0,0,0,16,0,0,0,
  SPT_END
};

U64 PenHash(CDC *dc,Bool depth=FALSE)
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

U0 PenProbe()
{
  CDC *dc=DCNew(88,77);
  U64 pens=0,circles=0;
  I64 i,j,a,b,n;
  I64 r[16],*old_r=dc->r;
  for (i=0;i<GR_PEN_BRUSHES_NUM;i++) {
    pens=pens*31+PenHash(gr.pen_brushes[i]);
    pens=pens*31+PenHash(gr.collision_pen_brushes[i]);
    pens=pens*31+PenHash(gr.even_pen_brushes[i]);
    pens=pens*31+PenHash(gr.odd_pen_brushes[i]);
    for (j=0;j<i;j++)
      circles=circles*31+gr.circle_lo[i][j]*17+gr.circle_hi[i][j];
  }
  "pen tables %016X %016X\n",pens,circles;

  DCFill(dc,WHITE);
  dc->thick=8;
  dc->color=RED;
  GrPlot3B(dc,6,3,0);
  GrPlot3(dc,6,11,0);
  dc->thick=13;
  dc->color=ROPF_DITHER|(YELLOW<<16)|BLUE;
  GrPlot3(dc,18,17,0);
  GrPlot3(dc,25,18,0);
  "pen flat %016X %X\n",PenHash(dc),dc->color;

  DCExtentsInit(dc);
  dc->flags|=DCF_LOCATE_NEAREST|DCF_DONT_DRAW;
  dc->cur_x=9; dc->cur_y=12; dc->nearest_dist=I64_MAX;
  dc->thick=10;
  GrPlot3(dc,3,12,0);
  "pen record %d %d %d %d %d\n",dc->min_x,dc->min_y,dc->max_x,dc->max_y,dc->nearest_dist;
  dc->flags&=~(DCF_RECORD_EXTENTS|DCF_LOCATE_NEAREST|DCF_DONT_DRAW);

  DCFill(dc,WHITE);
  dc->color=GREEN;
  a=GrFillCircle(dc,0,6,,17);
  dc->color=PURPLE;
  b=GrFillCircle(dc,-2,57,0,64);
  dc->thick=65;
  dc->color=LTBLUE;
  GrPlot3(dc,80,35,0);
  "pen circles %d %d %016X\n",a,b,PenHash(dc);

  DCDepthBufAlloc(dc);
  DCFill(dc,WHITE);
  dc->thick=12;
  dc->color=RED;
  GrPlot3(dc,22,24,8);
  dc->color=ROPF_DITHER|(YELLOW<<16)|BLUE;
  GrPlot3(dc,25,24,3);
  GrPlot3(dc,22,23,12);
  GrPlot3(dc,21,23,-1);
  "pen depth %016X %016X\n",PenHash(dc),PenHash(dc,TRUE);

  dc->bkcolor=WHITE;
  dc->color=ROP_COLLISION|RED;
  GrPlot3(dc,24,25,0);
  a=dc->collision_cnt;
  dc->color=ROP_COLLISION|WHITE;
  GrPlot3(dc,24,25,0);
  dc->color=ROP_COLLISION|TRANSPARENT;
  GrPlot3(dc,24,25,0);
  "pen collision %d %d %016X\n",a,dc->collision_cnt,PenHash(dc,TRUE);

  DCFill(dc,WHITE);
  DCDepthBufRst(dc);
  dc->thick=8;
  dc->color=RED;
  a=GrLine3(dc,5,5,15,62,25,4);
  dc->thick=7;
  dc->color=BLUE;
  b=GrLine3(dc,59,66,1,37,4,19);
  dc->color=GREEN;
  n=GrLine3(dc,43,54,60,51,56,4,1,17);
  dc->color=LTRED;
  GrLine3(dc,65,35,3,65,35,3);
  GrLine3(dc,69,41,2,69,41,6);
  dc->thick=64;
  dc->color=ROP_XOR|YELLOW;
  GrLine3(dc,85,50,0,83,57,0);
  "pen lines %d %d %d %016X %016X\n",a,b,n,PenHash(dc),PenHash(dc,TRUE);

  DCFill(dc,WHITE);
  DCDepthBufRst(dc);
  Mat4x4IdentEqu(r);
  Mat4x4Scale(r,1.5);
  DCMat4x4Set(dc,r);
  dc->flags|=DCF_TRANSFORMATION;
  dc->x=5; dc->y=3; dc->z=2;
  dc->thick=3;
  DCThickScale(dc);
  a=dc->thick;
  DCSymmetrySet(dc,24,0,24,30);
  dc->flags|=DCF_SYMMETRY;
  dc->color=RED;
  GrLine3(dc,2,1,4,19,10,6,3,2);
  dc->brush=gr.pen_brushes[3];
  dc->color=BLUE;
  dc->flags|=DCF_JUST_MIRROR;
  GrPlot3B(dc,6,18,1);
  GrLine3(dc,5,20,2,13,22,2);
  dc->brush=NULL;
  "pen steps %d %016X %016X %X\n",a,PenHash(dc),PenHash(dc,TRUE),dc->flags;

  DCFill(dc,WHITE);
  DCDepthBufRst(dc);
  dc->flags&=~(DCF_SYMMETRY|DCF_JUST_MIRROR);
  Sprite3(dc,10,6,4,pen_elems);
  "pen sprite %d %d %016X %016X\n",SpriteSize(pen_elems),dc->thick,PenHash(dc),PenHash(dc,TRUE);
  dc->r=old_r;
  DCDel(dc);
}
PenProbe;
