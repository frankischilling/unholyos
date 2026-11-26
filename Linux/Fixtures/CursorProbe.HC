U0 CursorBox(CDC *dc,I64 x,I64 y)
{
  GrRect(dc,x,y,5,4);
}

U0 CursorGrab(CDC *dc,I64 x,I64 y,Bool closed)
{
  GrRect(dc,x,y,3,3+closed);
}

U0 CursorWait(U8 *phase,I64 key)
{
  Sleep(110);
  "cursor %s\n",phase;
  while (GetKey()!=key) {}
}

U0 CursorZoomState()
{
  "cursor zoom state %d %d %d %X %X %X %d %d %d\n",
     gr.scrn_zoom,gr.sx,gr.sy,ms.scale.x(I64),ms.scale.y(I64),
     ms.scale.z(I64),ms.offset.x,ms.offset.y,ms.offset.z;
}

U0 CursorProbe()
{
  CDC *dc=DCAlias;
  U8 *draw=gr.fp_draw_ms,*grab=gr.fp_draw_grab_ms;
  Bool show=ms.show;
  I64 mx=ms.pos.x,my=ms.pos.y,mz=ms.pos.z;
  I64 ox=ms.offset.x,oy=ms.offset.y,oz=ms.offset.z;
  F64 fx=ms.scale.x,fy=ms.scale.y,fz=ms.scale.z;
  Bool old_grab=winmgr.grab_scroll,old_closed=winmgr.grab_scroll_closed;
  dc->flags&=~DCF_SCRN_BITMAP;
  dc->color=BLUE;
  GrRect(dc,136,112,82,52);
  ms.show=TRUE;
  MsSet(160,128,16,FALSE,FALSE);
  CursorWait("normal",'n');
  MsSet(172,136,16,TRUE,FALSE);
  CursorWait("left",'l');
  MsSet(184,142,16,FALSE,TRUE);
  CursorWait("right",'r');
  ms.show=FALSE;
  CursorWait("hidden",'h');
  ms.show=TRUE;
  gr.fp_draw_ms=&CursorBox;
  MsSet(160,128,16,FALSE,FALSE);
  CursorWait("custom",'c');
  gr.fp_draw_grab_ms=&CursorGrab;
  winmgr.grab_scroll=TRUE;
  winmgr.grab_scroll_closed=TRUE;
  CursorWait("grab",'g');
  winmgr.grab_scroll=old_grab;
  winmgr.grab_scroll_closed=old_closed;
  gr.fp_draw_ms=draw;
  gr.fp_draw_grab_ms=grab;

  "cursor shortcut\n";
  while (gr.scrn_zoom==1)
    Sleep(1);
  CursorZoomState;
  CursorWait("zoom",'z');

  MsSet(330,270,16,FALSE,FALSE);
  Sleep(110);
  CursorZoomState;
  CursorWait("recenter",'m');
  GrScaleZoom(1.75);
  CursorZoomState;
  GrScaleZoom(0.0);
  CursorZoomState;

  ms.scale.x=fx;
  ms.scale.y=fy;
  ms.scale.z=fz;
  ms.offset.x=ox;
  ms.offset.y=oy;
  ms.offset.z=oz;
  ms.show=show;
  MsSet(mx,my,mz,FALSE,FALSE);
  dc->color=TRANSPARENT;
  GrRect(dc,136,112,82,52);
  DCDel(dc);
  "cursor done\n";
}

CursorProbe;
