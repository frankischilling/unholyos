Bool CapturePixels(CDC *dc,I64 x,I64 y,I64 scale)
{
  I64 i,j,a,b;
  if (!dc)
    return FALSE;
  for (j=0;j<8;j++)
    for (i=0;i<32;i++)
      for (b=0;b<scale;b++)
        for (a=0;a<scale;a++)
          if (dc->body[(y+j*scale+b)*dc->width_internal+x+i*scale+a]!=((i+j*3)&15))
            return FALSE;
  return TRUE;
}

U0 CaptureProbe()
{
  CDC *dc=DCAlias,*backup=DCCopy(gr.dc),*normal,*zoomed,*plain,*loaded;
  CDC *dst=DCNew(32,8);
  I64 i,j,zoom=gr.scrn_zoom,sx=gr.sx,sy=gr.sy;
  I64 mx=ms.pos.x,my=ms.pos.y,result=0,size,success,missing;
  CBGR48 saved[COLORS_NUM],expected;
  GrPaletteGet(saved);
  dc->flags&=~DCF_SCRN_BITMAP;
  gr.scrn_zoom=1;
  gr.sx=0;
  gr.sy=0;
  MsSet(24,24);
  for (j=0;j<8;j++)
    for (i=0;i<32;i++) {
      dc->color=(i+j*3)&15;
      GrPlot0(dc,324+i,244+j);
    }
  Refresh(2,TRUE);
  normal=DCScrnCapture;
  if (normal && normal->width==640 && normal->height==480 &&
      !(normal->flags&DCF_SCRN_BITMAP) && normal->body!=gr.scrn_image->body)
    result|=1;
  if (CapturePixels(normal,324,244,1))
    result|=2;
  MsSet(480,360);
  gr.sx=320;
  gr.sy=240;
  gr.scrn_zoom=2;
  Refresh(2,TRUE);
  zoomed=DCScrnCapture(TRUE);
  plain=DCScrnCapture(FALSE);
  if (CapturePixels(zoomed,8,8,2))
    result|=4;
  if (CapturePixels(plain,324,244,1))
    result|=8;
  GrPaletteColorSet(RED,0x123456789ABC);
  expected=GrPaletteColorGet(RED);
  size=GRScrnCaptureWrite("B:/CaptureProbe",FALSE);
  loaded=GRRead("B:/CaptureProbe");
  if (size>0 && CapturePixels(loaded,324,244,1) && loaded->palette[RED]==expected)
    result|=16;
  dst->color=RED;
  success=GRScrnCaptureRead("B:/CaptureProbe",dst,-324,-244);
  if (success==TRUE &&
      dst->color==ROP_EQU && CapturePixels(dst,0,0,1))
    result|=32;
  missing=GRScrnCaptureRead("B:/CaptureProbeMissing",dst);
  if (missing==FALSE)
    result|=64;
  MemCpy(gr.dc->body,backup->body,gr.dc->width_internal*gr.dc->height);
  gr.scrn_zoom=zoom;
  gr.sx=sx;
  gr.sy=sy;
  MsSet(mx,my);
  GrPaletteSet(saved);
  DCDel(normal);
  DCDel(zoomed);
  DCDel(plain);
  DCDel(loaded);
  DCDel(dst);
  DCDel(backup);
  DCDel(dc);
  Refresh(2,TRUE);
  "capture probe %X\n",result;
}
CaptureProbe;
