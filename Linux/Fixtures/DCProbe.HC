U64 DCProbeHash(U8 *data,I64 size)
{
  U64 hash=0xCBF29CE484222325;
  I64 i;
  for (i=0;i<size;i++)
    hash=(hash^data[i])*0x100000001B3;
  return hash;
}

U0 DCProbe()
{
  CDC *dc=DCNew(13,5),*alias,*copy,*loaded,*blank;
  CD3I32 p1,p2,p3;
  I64 x=1,y=2,z=3,size,used,ret;
  U8 *data;
  "dc layout %d %d %d %d %d %d\n",sizeof(CDC),offset(CDC.end),
      offset(CDC.body),offset(CDC.depth_buf),sizeof(CGrGlbls),offset(CGrGlbls.scrn_zoom);
  "dc screen %d %d %X %X %d\n",gr.dc->width,gr.dc->height,gr.dc->flags,
      gr.dc1->flags,gr.scrn_zoom;
  "dc new %d %d %d %X %d %d %016X\n",dc->width,dc->width_internal,
      dc->height,dc->dc_signature,dc->body[79],dc->thick,dc->r[15];
  DCFill(dc,RED);
  alias=DCAlias(dc);
  alias->color=GREEN;
  alias->body[17]=BLUE;
  "dc alias %d %d %d %X\n",alias->body==dc->body,dc->body[17],dc->color,alias->flags;
  DCDel(alias);
  DCDepthBufAlloc(dc);
  "dc depth %X %X\n",dc->depth_buf[0],dc->depth_buf[79];
  dc->depth_buf[7]=123;
  copy=DCCopy(dc,Fs);
  copy->body[17]=WHITE;
  copy->r[0]=2<<32;
  "dc copy %d %d %d %d %d\n",dc->body[17],copy->body[17],
      copy->depth_buf[7],copy->depth_buf!=dc->depth_buf,copy->r!=dc->r;
  DCDepthBufRst(copy);
  copy->x=10;
  copy->y=-6;
  DCTransform(copy,&x,&y,&z);
  "dc transform %d %d %d %X\n",x,y,z,copy->depth_buf[7];
  p1.x=1; p1.y=0; p1.z=0;
  p2.x=0; p2.y=0; p2.z=0;
  p3.x=0; p3.y=1; p3.z=0;
  DCLighting(copy,&p1,&p2,&p3,LTRED);
  "dc light %X %d\n",copy->color,copy->dither_probability_u16;
  DCExtentsInit(copy);
  DCRst(copy);
  "dc reset %X %d %d %d %d\n",copy->flags,copy->x,copy->body[17],
      copy->ls.y,copy->min_x==0x7FFFFFFFFFFFFFFF;
  blank=DCNew(13,5,,TRUE);
  "dc null %X %d %d\n",blank->flags,!blank->body,!blank->depth_buf;
  DCDel(blank);

  ret=DCColorChg(dc,RED);
  DCMono(dc,TRANSPARENT,6,7);
  dc->x0=-7;
  dc->y0=8;
  dc->palette[BLUE]=0x123456789ABC;
  data=DCSave(dc,&size,0);
  "dc raw %d %016X\n",size,DCProbeHash(data,size);
  loaded=DCLoad(data,&used);
  "dc load %d %d %d %X %X %d\n",used,loaded->x0,loaded->y0,loaded->flags,
      loaded->palette[BLUE],loaded->body[17];
  Free(data);
  DCDel(loaded);
  data=DCSave(dc,&size);
  "dc compressed %d %016X\n",size,DCProbeHash(data,size);
  loaded=DCLoad(data,&used);
  "dc expanded %d %d %d %d\n",used,loaded->body[17],loaded->body[79],ret;
  Free(data);
  DCDel(loaded);
  size=GRWrite("B:/DCProbe",dc);
  loaded=GRRead("B:/DCProbe");
  "dc file %d %X %d\n",size,loaded->palette[BLUE],loaded->body[17];
  DCDel(loaded);
  DCDel(dc);
  DCDel(copy);
}
DCProbe;
