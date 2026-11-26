U64 FontTextHash(U8 *data,I64 size)
{
  U64 hash=0xCBF29CE484222325;
  I64 i;
  for (i=0;i<size;i++)
    hash=(hash^data[i])*0x100000001B3;
  return hash;
}

U0 FontTextProbe()
{
  CTask *task=Fs;
  I64 l=task->win_left,r=task->win_right,t=task->win_top,b=task->win_bottom,
      sx=task->scroll_x,sy=task->scroll_y;
  U32 cells[4];
  U32 *dst=gr.text_base+2*TEXT_COLS+2;
  U32 saved[3];
  U64 *font=text.font,*aux_font=text.aux_font;
  U64 old=font['A'],glyph=0x8040201008040201;
  CDC *dc=DCNew(8,8);
  I64 i,j,ret,matched=0;

  "font layout %d %d %d %d %d %d\n",sizeof(CTextGlbls),
      offset(CTextGlbls.font),offset(CTextGlbls.aux_font),
      offset(CTextGlbls.border_chars),text.rows,text.cols;
  "font data %016X %016X %X %X\n",
      FontTextHash(font(U8 *),256*FONT_HEIGHT),
      FontTextHash(aux_font(U8 *),256*FONT_HEIGHT),
      text.border_chars[2],text.border_chars[13];

  task->win_left=2; task->win_right=4;
  task->win_top=task->win_bottom=2;
  task->scroll_x=task->scroll_y=0;
  cells[0]=0x1E41; cells[1]=0x2F42;
  cells[2]=0x3043; cells[3]=0x4144;
  for (i=0;i<3;i++)
    saved[i]=dst[i];
  ret=TextLenAttrStr(task,-1,0,4,cells);
  "text attr %d %X %X %X\n",ret,dst[0],dst[1],dst[2];
  TextPrint(task,-1,0,0x1E,"%s%d","AB",7);
  "text print %X %X %X %d\n",dst[0],dst[1],dst[2],
      TextLenAttrStr(task,0,1,4,cells);
  for (i=0;i<3;i++)
    dst[i]=saved[i];
  task->win_left=l; task->win_right=r;
  task->win_top=t; task->win_bottom=b;
  task->scroll_x=sx; task->scroll_y=sy;

  font['A']=glyph;
  DCFill(dc,BLACK);
  dc->color=WHITE;
  GrPutChar(dc,0,0,'A');
  for (i=0;i<8;i++)
    for (j=0;j<8;j++)
      if ((i==j && GrPeek0(dc,j,i)==WHITE) ||
          (i!=j && GrPeek0(dc,j,i)==BLACK))
        matched++;
  font['A']=old;
  DCDel(dc);
  "font pixels %d\n",matched;

  "font swap ready\n";
  while (text.font==font)
    Sleep(10);
  "font swapped %d %d\n",text.font==aux_font,text.aux_font==font;
  while (text.font==aux_font)
    Sleep(10);
  "font restored %d %d\n",text.font==font,text.aux_font==aux_font;
}
FontTextProbe;
