class CArcProbe
{
  U64 hash;
  I64 cnt,stop;
};

Bool ArcPlot(CArcProbe *a,I64 x,I64 y,I64 z)
{
  a->hash=(a->hash^x)*0x100000001B3;
  a->hash=(a->hash^y)*0x100000001B3;
  a->hash=(a->hash^z)*0x100000001B3;
  return ++a->cnt!=a->stop;
}

U0 ArcReset(CArcProbe *a,I64 stop=0)
{
  a->hash=0xCBF29CE484222325;
  a->cnt=0;
  a->stop=stop;
}

U64 ArcHash(CDC *dc,Bool depth=FALSE)
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

U0 ArcProbe()
{
  CArcProbe a;
  CDC *dc=DCNew(96,69);
  I64 i,j,k,r[16],*old_r=dc->r;
  ArcReset(&a);
  i=Circle(&a,7,-3,5,13,&ArcPlot);
  "arc circle %d %d %016X\n",i,a.cnt,a.hash;
  ArcReset(&a,5);
  i=Circle(&a,-8,11,-2,27,&ArcPlot,-3,0.7,2.25);
  "arc callback %d %d %016X\n",i,a.cnt,a.hash;
  ArcReset(&a);
  i=Ellipse(&a,3,2,9,7,21,&ArcPlot,0.4,-1,0.625,5.0);
  "arc ellipse %d %d %016X\n",i,a.cnt,a.hash;
  ArcReset(&a);
  i=Circle(&a,0,0,0,0,&ArcPlot);
  j=Ellipse(&a,0,0,0,1,3,&ArcPlot,,0);
  k=Ellipse(&a,0,0,0,3,1,&ArcPlot,0.3,1,0,-1.0);
  "arc empty %d %d %d %d\n",i,j,k,a.cnt;

  DCDepthBufAlloc(dc);
  DCFill(dc,WHITE);
  dc->thick=5;
  dc->color=RED;
  i=GrCircle(dc,6,12,19);
  dc->color=ROP_XOR|BLUE;
  j=GrEllipse(dc,50,20,31,6,0.5,-2,0.7,4.5);
  k=GrCircle(dc,50,52,13,3,-1.0,2.0);
  "arc flat %d %d %d %016X %016X\n",i,j,k,ArcHash(dc),ArcHash(dc,TRUE);

  DCFill(dc,WHITE);
  DCDepthBufRst(dc);
  dc->thick=5;
  dc->color=RED;
  i=GrCircle3(dc,25,23,7,17);
  dc->color=GREEN;
  j=GrEllipse3(dc,38,28,2,24,11,-0.6,2);
  dc->color=BLUE;
  k=GrCircle3(dc,34,30,9,20,-3,1.1,3.7);
  "arc depth %d %d %d %016X %016X\n",i,j,k,ArcHash(dc),ArcHash(dc,TRUE);

  DCFill(dc,WHITE);
  DCDepthBufRst(dc);
  Mat4x4IdentEqu(r);
  r[0]=0x180000000;
  r[5]=0x0C0000000;
  Mat4x4RotZ(r,0.25);
  DCMat4x4Set(dc,r);
  dc->flags|=DCF_TRANSFORMATION;
  dc->x=9; dc->y=5; dc->z=2;
  dc->color=RED;
  i=GrCircle3(dc,16,19,3,12);
  dc->color=BLUE;
  j=GrEllipse3(dc,29,21,1,17,10,0.375,-2,0.25,4.5);
  "arc transform %d %d %X %016X %016X\n",i,j,dc->flags,ArcHash(dc),ArcHash(dc,TRUE);
  dc->r=old_r;
  DCDel(dc);
}
ArcProbe;
