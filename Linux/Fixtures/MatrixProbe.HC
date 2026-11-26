U64 MatrixHash(I64 *m)
{
  U64 hash=0xCBF29CE484222325;
  I64 i;
  for (i=0;i<16;i++)
    hash=(hash^m[i])*0x100000001B3;
  return hash;
}

U0 MatrixProbe()
{
  I64 m[16],n[16],dst[16],*allocated;
  I64 x=3,y=-6,z=7,i,before,idle,full=0;
  Bool a,b,c,d;
  F64 p=ã,rx=0,ry=0,rz=0;
  CDC *dc=DCNew(5,6);
  I64 *old_r=dc->r;
  CTask *task=Fs;
  CDoc *doc=DocPut;
  Mat4x4IdentEqu(m);
  Mat4x4TranslationEqu(m,3,-5,7);
  Mat4x4TranslationAdd(m,4,2,-1);
  Mat4x4RotX(m,p/2.0);
  Mat4x4RotY(m,-p/8.0);
  Mat4x4RotZ(m,0.625);
  DCMat4x4Set(dc,m);
  DCTransform(dc,&x,&y,&z);
  "matrix pi %016X\n",p(I64);
  "matrix rotation %016X %d %d %d\n",MatrixHash(m),x,y,z;

  Mat4x4IdentEqu(n);
  Mat4x4RotY(n,p/13.0);
  Mat4x4Scale(n,1.25);
  Mat4x4MulMat4x4Equ(dst,m,n);
  allocated=Mat4x4MulMat4x4New(m,n);
  "matrix product %016X %016X\n",MatrixHash(dst),MatrixHash(allocated);
  Free(allocated);

  for (i=0;i<64;i++) {
    Mat4x4IdentEqu(n);
    Mat4x4RotX(n,rx);
    Mat4x4RotY(n,ry);
    Mat4x4RotZ(n,rz);
    rx+=2.1*p/180.0;
    ry+=1.1*p/180.0;
    rz+=0.5*p/180.0;
  }
  "matrix sweep %016X\n",MatrixHash(n);
  x=7; y=3; z=6;
  a=DCSymmetrySet(dc,-3,7,4,11);
  b=DCSymmetrySet(dc,4,3,4,3);
  DCReflect(dc,&x,&y,&z);
  "matrix symmetry %d %d %d %d %d %d %d\n",a,b,dc->sym.snx,
    dc->sym.sny,x,y,z;

  idle=task->task_flags&(1<<TASKf_IDLE);
  if (doc) {
    full=doc->flags&DOCF_DO_FULL_REFRESH;
    doc->flags&=~DOCF_DO_FULL_REFRESH;
  }
  before=winmgr.updates;
  Refresh(2,TRUE);
  a=winmgr.updates-before>=2;
  b=!doc || !(doc->flags&DOCF_DO_FULL_REFRESH);
  if (doc) doc->flags|=DOCF_DO_FULL_REFRESH;
  before=winmgr.updates;
  Refresh(2);
  c=winmgr.updates-before>=2;
  d=!doc || (doc->flags&DOCF_DO_FULL_REFRESH)!=0;
  if (doc) doc->flags=(doc->flags&~DOCF_DO_FULL_REFRESH)|full;
  "matrix refresh %d %d %d %d %d\n",a,b,c,d,
    (task->task_flags&(1<<TASKf_IDLE))==idle;
  dc->r=old_r;
  DCDel(dc);
}
MatrixProbe;
