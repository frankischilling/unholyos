class CSplineProbe
{
  U64 hash;
  I64 cnt,stop;
};

Bool SplinePlot(CSplineProbe *a,I64 x,I64 y,I64 z)
{
  a->hash=(a->hash^x)*0x100000001B3;
  a->hash=(a->hash^y)*0x100000001B3;
  a->hash=(a->hash^z)*0x100000001B3;
  return ++a->cnt!=a->stop;
}

U0 SplineReset(CSplineProbe *a,I64 stop=0)
{
  a->hash=0xCBF29CE484222325;
  a->cnt=0;
  a->stop=stop;
}

U64 SplineHash(CDC *dc,Bool depth=FALSE)
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

U0 SplineProbe()
{
  CSplineProbe a;
  CD3I32 c[5];
  CDC *dc=DCNew(96,72);
  I64 i,j,k,r[16],*old_r=dc->r;
  c[0].x=-7; c[0].y=4; c[0].z=3;
  c[1].x=23; c[1].y=-19; c[1].z=7;
  c[2].x=51; c[2].y=37; c[2].z=-4;
  c[3].x=9; c[3].y=43; c[3].z=2;
  c[4].x=-11; c[4].y=17; c[4].z=-9;
  "spline distance %d %016X %d\n",D3I32DistSqr(c,c+1),
      D3I32Dist(c,c+1)(U64),D3I32NormSqr(c);
  SplineReset(&a);
  i=Line(&a,-8,9,3,21,-5,-7,&SplinePlot,3,2);
  "spline line %d %d %016X\n",i,a.cnt,a.hash;
  SplineReset(&a);
  i=RegPoly(&a,9,11,-3,13,8,7,&SplinePlot,0.375,2,0.25,5.5);
  "spline polygon %d %d %016X\n",i,a.cnt,a.hash;
  SplineReset(&a);
  i=RegPoly(&a,0,0,0,12,15,5,&SplinePlot);
  "spline polygon full %d %d %016X\n",i,a.cnt,a.hash;
  SplineReset(&a);
  i=Bezier2(&a,c,&SplinePlot);
  "spline bezier2 %d %d %016X\n",i,a.cnt,a.hash;
  SplineReset(&a);
  i=Bezier3(&a,c,&SplinePlot,FALSE);
  "spline bezier3 %d %d %016X\n",i,a.cnt,a.hash;
  SplineReset(&a,7);
  i=Bezier3(&a,c,&SplinePlot);
  "spline abort %d %d %016X\n",i,a.cnt,a.hash;
  SplineReset(&a);
  i=BSpline2(&a,c,5,&SplinePlot);
  "spline quadratic %d %d %016X\n",i,a.cnt,a.hash;
  SplineReset(&a);
  i=BSpline2(&a,c,5,&SplinePlot,TRUE);
  "spline quadratic closed %d %d %016X\n",i,a.cnt,a.hash;
  SplineReset(&a);
  i=BSpline3(&a,c,5,&SplinePlot);
  "spline cubic %d %d %016X\n",i,a.cnt,a.hash;
  SplineReset(&a);
  i=BSpline3(&a,c,5,&SplinePlot,TRUE);
  "spline cubic closed %d %d %016X\n",i,a.cnt,a.hash;
  SplineReset(&a);
  i=BSpline2(&a,c,2,&SplinePlot);
  j=BSpline3(&a,c,1,&SplinePlot);
  k=RegPoly(&a,0,0,0,0,7,5,&SplinePlot);
  "spline empty %d %d %d %d\n",i,j,k,a.cnt;

  DCDepthBufAlloc(dc);
  DCFill(dc,WHITE);
  dc->color=RED;
  i=GrRegPoly(dc,8,18,17,11,5,0.25);
  dc->color=BLUE;
  j=Gr2Bezier(dc,c);
  k=Gr3BSpline(dc,c,5,TRUE);
  "spline flat %d %d %d %016X %016X\n",i,j,k,SplineHash(dc),SplineHash(dc,TRUE);
  DCFill(dc,WHITE);
  DCDepthBufRst(dc);
  dc->thick=3;
  dc->color=RED;
  i=Gr2Bezier3(dc,c);
  dc->color=GREEN;
  j=Gr3Bezier3(dc,c);
  dc->color=BLUE;
  k=GrRegPoly3(dc,32,27,4,17,11,6);
  "spline depth %d %d %d %016X %016X\n",i,j,k,SplineHash(dc),SplineHash(dc,TRUE);
  DCFill(dc,WHITE);
  DCDepthBufRst(dc);
  Mat4x4IdentEqu(r);
  r[0]=0x180000000;
  r[5]=0x0C0000000;
  Mat4x4RotZ(r,0.25);
  DCMat4x4Set(dc,r);
  dc->flags|=DCF_TRANSFORMATION;
  dc->x=21; dc->y=18;
  dc->color=RED;
  i=Gr2BSpline3(dc,c,5,TRUE);
  dc->color=BLUE;
  j=Gr3BSpline3(dc,c,5,FALSE);
  dc->color=GREEN;
  k=GrRegPoly3(dc,17,16,2,12,7,5,0.5);
  "spline transform %d %d %d %X %016X %016X\n",i,j,k,dc->flags,SplineHash(dc),SplineHash(dc,TRUE);
  dc->r=old_r;
  DCDel(dc);
}
SplineProbe;
