Bool ScrollProbeInside(CCtrl *,I64 x,I64 y)
{
  return x==20 && y==40;
}

U0 ScrollProbeClick(CCtrl *c,I64 x,I64 y,Bool down)
{
  c->type=x-y+down;
}

U0 ScrollControlsProbe()
{
  CCtrl *c=CAlloc(sizeof(CCtrl));
  "ctrl layout %d %d %d %d\n",sizeof(CCtrl),sizeof(CWinScroll),
      offset(CCtrl.left_click),offset(CWinScroll.flags);
  c->flags=CTRLF_SHOW;
  c->scrn_left=10;
  c->scrn_right=17;
  c->scrn_top=30;
  c->scrn_bottom=37;
  "ctrl box %d %d %d\n",CtrlInside(c,10,30),CtrlInside(c,17,37),
      CtrlInside(c,18,37);
  c->inside_ctrl=&ScrollProbeInside;
  "ctrl shape %d %d\n",CtrlInside(c,10,30),CtrlInside(c,20,40);
  c->left_click=&ScrollProbeClick;
  (*c->left_click)(c,9,2,TRUE);
  "ctrl callback %d\n",c->type;
  c->flags=0;
  "ctrl hidden %d\n",CtrlInside(c,20,40);
  Free(c);
}

ScrollControlsProbe;
