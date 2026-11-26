U0 ScreenProbe()
{
  CDC *dc=DCAlias;
  I64 i,zoom=gr.scrn_zoom,sx=gr.sx,sy=gr.sy;
  I64 mx=ms.pos.x,my=ms.pos.y;
  dc->flags&=~DCF_SCRN_BITMAP;
  for (i=0;i<16;i++) {
    dc->color=i;
    GrRect(dc,321+i*11,241,11,18);
  }
  dc->color=WHITE;
  GrPlot0(dc,319,240);
  GrPlot0(dc,497,259);
  Sleep(80);
  "screen normal\n";
  while (GetKey()!='n') {}

  dc->color=LTPURPLE;
  GrRect(dc,376,249,11,1);
  Sleep(80);
  "screen delta\n";
  while (GetKey()!='d') {}

  MsSet(480,360);
  gr.sx=320;
  gr.sy=240;
  gr.scrn_zoom=2;
  Sleep(80);
  "screen zoom\n";
  while (GetKey()!='z') {}

  dc->color=TRANSPARENT;
  GrRect(dc,321,241,176,18);
  GrPlot0(dc,319,240);
  GrPlot0(dc,497,259);
  gr.scrn_zoom=zoom;
  gr.sx=sx;
  gr.sy=sy;
  MsSet(mx,my);
  DCDel(dc);
  Sleep(80);
  "screen restored\n";
}
ScreenProbe;
