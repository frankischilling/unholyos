U0 PaletteProbe()
{
  CBGR48 saved[COLORS_NUM],colors[COLORS_NUM],value;
  I64 i,gray=0;
  GrPaletteGet(saved);
  "palette tables %d %X %X %d %Z %Z\n",sizeof(CBGR48),
    gr_palette_std[BROWN],gr_palette_gray[LTGRAY],gr_rainbow_10[3],
    BROWN,"ST_COLORS",3,"ST_RAINBOW_10";
  "palette start %X %X %X\n",saved[BLACK],saved[BROWN],saved[WHITE];
  GrPaletteSet(gr_palette_gray);
  GrPaletteGet(colors);
  for (i=0;i<COLORS_NUM;i++)
    if (colors[i].b==colors[i].g && colors[i].g==colors[i].r)
      gray++;
  "palette gray %d %X\n",gray,colors[LTGRAY];

  value.b=0xCDEF;
  value.g=0x5678;
  value.r=0x1234;
  value.pad=0xABCD;
  GrPaletteColorSet(BLUE,value);
  value=GrPaletteColorGet(BLUE);
  "palette channels %X %X %X %X\n",value.b,value.g,value.r,value.pad;
  GrPaletteColorSet(WHITE,0xFFFFFFFF0000);
  "palette yellow %X\n",GrPaletteColorGet(WHITE);
  while (GetKey()!='p') {}
  GrPaletteSet(saved);
  "palette restored %X %X\n",GrPaletteColorGet(WHITE),GrPaletteColorGet(BLUE);
}
PaletteProbe;
