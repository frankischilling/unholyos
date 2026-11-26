U64 Blot3Hash(CDC *dc)
{
  U64 hash=0xCBF29CE484222325;
  I64 i;
  for (i=0;i<dc->width_internal*dc->height;i++)
    hash=(hash^dc->body[i])*0x100000001B3;
  return hash;
}

I64 blot3_calls;

U0 Blot3Transform(CDC *dc,I64 *x,I64 *y,I64 *z)
{
  I64 old_x=*x;
  *x=dc->x-*y;
  *y=dc->y+old_x;
  *z+=dc->z+old_x;
  blot3_calls++;
}

U0 Blot3Probe()
{
  CDC *dc=DCNew(32,20),*img=DCNew(4,3);
  U8 bitmap[42],stream[102];
  I64 ret,x,y,z;
  DCFill(img,TRANSPARENT);
  img->body[0]=RED;
  img->body[2]=BLUE;
  img->body[3]=BLACK;
  img->body[8]=GREEN;
  img->body[9]=YELLOW;
  img->body[11]=PURPLE;
  img->body[16]=BLACK;
  img->body[17]=WHITE;
  img->body[18]=LTRED;

  DCFill(dc,WHITE);
  dc->r[0]=0;
  dc->r[1]=-(1<<32);
  dc->r[4]=3<<31;
  dc->r[5]=0;
  dc->r[8]=1<<32;
  dc->x=12;
  dc->y=3;
  dc->z=6;
  dc->flags|=DCF_TRANSFORMATION;
  DCExtentsInit(dc);
  ret=GrBlot3(dc,0,0,0,img);
  "b3 matrix %d %016X %d %d %d %d\n",ret,Blot3Hash(dc),
      dc->min_x,dc->min_y,dc->max_x,dc->max_y;

  DCFill(dc,WHITE);
  DCDepthBufAlloc(dc);
  dc->flags&=~DCF_TRANSFORMATION;
  GrBlot3(dc,2,2,12,img);
  dc->color=ROP_MONO|GREEN;
  GrBlot3(dc,2,2,14,img);
  GrBlot3(dc,2,2,-2,img);
  GrBlot3(dc,3,2,6,img);
  "b3 depth %016X %d %d %d %d\n",Blot3Hash(dc),GrPeek(dc,2,2),
      GrPeek(dc,3,2),dc->depth_buf[66],dc->depth_buf[67];

  DCFill(dc,WHITE);
  DCDepthBufRst(dc);
  dc->color=BLACK;
  x=7;
  y=4;
  z=3;
  ret=DCSymmetry3Set(dc,10,1,2,10,6,2,10,6,11);
  DCReflect(dc,&x,&y,&z);
  dc->flags|=DCF_SYMMETRY|DCF_JUST_MIRROR;
  GrBlot3(dc,5,2,4,img);
  "b3 mirror %d %d %d %d %016X\n",ret,x,y,z,Blot3Hash(dc);
  dc->flags&=~(DCF_SYMMETRY|DCF_JUST_MIRROR);

  MemSet(bitmap,0,sizeof(bitmap));
  bitmap[0]=23;
  bitmap[9]=4;
  bitmap[13]=3;
  MemCpy(bitmap+17,img->body,24);
  DCFill(dc,WHITE);
  DCDepthBufRst(dc);
  dc->transform=&Blot3Transform;
  dc->brush=img;
  blot3_calls=0;
  Sprite3B(dc,15,5,11,bitmap);
  "b3 callback %016X %d %d %d %d %d\n",Blot3Hash(dc),blot3_calls,
      dc->brush==img,dc->x,dc->y,dc->z;

  MemSet(stream,0,sizeof(stream));
  stream[0]=5;
  MemCpy(stream+1,bitmap,sizeof(bitmap)-1);
  stream[42]=6;
  stream[43]=4;
  stream[44]=5;
  stream[52]=5;
  stream[56]=8;
  MemCpy(stream+60,bitmap,sizeof(bitmap));
  DCFill(dc,WHITE);
  DCDepthBufRst(dc);
  dc->sym.sx=123;
  Sprite3(dc,1,5,12,stream);
  "b3 sprite %016X %d %d %d\n",Blot3Hash(dc),dc->sym.sx,
      dc->flags&(DCF_SYMMETRY|DCF_TRANSFORMATION),blot3_calls;
  DCDel(dc);
  DCDel(img);
}
Blot3Probe;
