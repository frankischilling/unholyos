I64 view_math_order;

I64 ViewMathArg(I64 n)
{
  view_math_order=10*view_math_order+n;
  return n;
}

U0 ViewAnglesProbe()
{
  CCtrl *c=ViewAnglesNew;
  CViewAngles *s=c->state;
  CD3I32 p;
  I64 m,n,z=-4;
  F64 a,b,d;

  "view layout %d %d %d %d\n",sizeof(CViewAngles),
      offset(CViewAngles.ax),offset(CViewAngles.cx),offset(CViewAngles.cfg);
  "view new %d %d %d %d %d\n",c==ViewAnglesNew(Fs),c->type,c->flags,
      c->right-c->left,c->bottom-c->top;
  s->sx=5;
  s->sy=48;
  s->sz=-3;
  (*c->update_derived_vals)(c);
  "view snap %d %d %d %.6f %.6f %.6f\n",s->sx,s->sy,s->sz,s->ax,s->ay,s->az;
  (*c->left_click)(c,c->left+22,c->top+33,TRUE);
  (*c->left_click)(c,c->left+45,c->top+57,TRUE);
  (*c->left_click)(c,c->left+68,c->top+69,FALSE);
  "view choices %d %d %d\n",s->sx,s->sy,s->sz;
  ViewAnglesDel(Fs);
  ViewAnglesDel;
  "view gone %d\n",!CtrlFindUnique(Fs,CTRLT_VIEWING_ANGLES);

  p.x=-3;p.y=4;p.z=12;
  a=Sin(pi/2);
  b=Cos(pi);
  d=Sqrt(81)+D3I32Norm(&p);
  view_math_order=0;
  m=MaxI64(ViewMathArg(3),ViewMathArg(7));
  n=MinI64(z,0);
  "view math %.0f %.0f %.0f %d %d %d %d\n",a,b,d,SqrI64(z),m,n,view_math_order;
}

ViewAnglesProbe;
