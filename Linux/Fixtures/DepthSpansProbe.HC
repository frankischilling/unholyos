U64 DepthSpansHash(CDC *dc,Bool depth=FALSE)
{
  U64 hash=0xCBF29CE484222325,value;
  I64 i;
  for (i=0;i<dc->width_internal*dc->height;i++) {
    if (depth)
      value=dc->depth_buf[i];
    else
      value=dc->body[i];
    hash=(hash^value)*0x100000001B3;
  }
  return hash;
}

U0 DepthSpansProbe()
{
  CDC *dc=DCNew(20,19);
  CTask *task=Fs;
  I64 h,v,i,n,x,save[7],seed=task->rand_seed;
  U64 sample=0x23E6A571,hash=0xCBF29CE484222325;
  U32 flags=task->task_flags;
  CD3I32 p[4];
  U8 rect[18];

  DCFill(dc,WHITE);
  dc->color=GREEN;
  h=GrHLine(dc,5,-2,3);
  v=GrVLine(dc,3,5,-2);
  "zs flat %d %d %016X\n",h,v,DepthSpansHash(dc);

  DCFill(dc,WHITE);
  DCDepthBufAlloc(dc);
  dc->color=RED;
  h=GrHLine(dc,14,-3,2,10,2);
  dc->color=BLUE;
  v=GrVLine(dc,10,-3,22,4,12);
  n=GrHLine(dc,0,12,2,30,30);
  "zs depth %d %d %d %016X %016X\n",h,v,n,
    DepthSpansHash(dc),DepthSpansHash(dc,TRUE);

  DCExtentsInit(dc);
  dc->flags|=DCF_LOCATE_NEAREST|DCF_DONT_DRAW;
  dc->cur_x=7;
  dc->cur_y=2;
  dc->nearest_dist=100000;
  GrVLine(dc,-1,-4,23);
  h=GrHLine(dc,22,-3,6);
  "zs extents %d %d %d %d %d %d\n",h,dc->min_x,dc->min_y,
    dc->max_x,dc->max_y,dc->nearest_dist;
  dc->flags&=~(DCF_RECORD_EXTENTS|DCF_LOCATE_NEAREST|DCF_DONT_DRAW);

  DCFill(dc,WHITE);
  DCDepthBufRst(dc);
  dc->bkcolor=WHITE;
  dc->body[2]=RED;
  dc->body[3]=TRANSPARENT;
  dc->color=ROP_COLLISION;
  h=GrHLine(dc,0,8,0,3,3);
  Seed(0x12345678);
  dc->color=ROPF_PROBABILITY_DITHER|(YELLOW<<16)|BLUE;
  dc->dither_probability_u16=32768;
  v=GrVLine(dc,2,-3,8,17,0);
  dc->color=ROP_XOR|ROPF_DITHER|(WHITE<<16)|GREEN;
  n=GrHLine(dc,0,8,1,0,0);
  "zs dither %d %d %d %d %016X %X\n",h,v,n,dc->collision_cnt,
    DepthSpansHash(dc),task->rand_seed;
  task->rand_seed=seed;
  task->task_flags=(task->task_flags&~(1<<TASKf_NONTIMER_RAND))|
    (flags&(1<<TASKf_NONTIMER_RAND));

  DCFill(dc,WHITE);
  DCDepthBufRst(dc);
  save[0]=task->pix_left; save[1]=task->pix_right;
  save[2]=task->pix_top; save[3]=task->pix_bottom;
  save[4]=task->scroll_x; save[5]=task->scroll_y; save[6]=task->scroll_z;
  task->pix_left=3; task->pix_right=17; task->pix_top=2; task->pix_bottom=16;
  task->scroll_x=-2; task->scroll_y=1; task->scroll_z=5;
  dc->win_task=task;
  dc->flags|=DCF_SCRN_BITMAP|DCF_ON_TOP;
  dc->color=RED;
  h=GrHLine(dc,-3,24,6,10,-3);
  v=GrVLine(dc,10,-4,19,-3,10);
  dc->flags&=~(DCF_SCRN_BITMAP|DCF_ON_TOP);
  task->pix_left=save[0]; task->pix_right=save[1];
  task->pix_top=save[2]; task->pix_bottom=save[3];
  task->scroll_x=save[4]; task->scroll_y=save[5]; task->scroll_z=save[6];
  "zs window %d %d %016X %016X\n",h,v,DepthSpansHash(dc),
    DepthSpansHash(dc,TRUE);

  DCFill(dc,WHITE);
  DCDepthBufRst(dc);
  dc->color=ROP_XOR|BLUE;
  p[0].x=-3; p[0].y=7; p[0].z=0;
  p[1].x=10; p[1].y=1; p[1].z=17;
  p[2].x=12; p[2].y=13; p[2].z=4;
  h=GrFillTri0(dc,p,p+1,p+2);
  for (n=0;n<3;n++) {
    x=p[n].x;
    p[n].x=p[n].y+2;
    p[n].y=x+4;
  }
  v=GrFillTri0(dc,p,p+1,p+2);
  "zs triangle %d %d %016X %016X\n",h,v,DepthSpansHash(dc),
    DepthSpansHash(dc,TRUE);

  for (i=0;i<64;i++) {
    DCFill(dc,WHITE);
    DCDepthBufRst(dc);
    for (n=0;n<3;n++) {
      sample=(sample*1664525+1013904223)&0xFFFFFFFF;
      p[n].x=(sample>>16)%35-8;
      sample=(sample*1664525+1013904223)&0xFFFFFFFF;
      p[n].y=(sample>>16)%35-8;
      sample=(sample*1664525+1013904223)&0xFFFFFFFF;
      p[n].z=(sample>>16)%32-5;
    }
    h=GrFillTri0(dc,p,p+1,p+2);
    hash=(hash^h)*0x100000001B3;
    hash=(hash^DepthSpansHash(dc))*0x100000001B3;
    hash=(hash^DepthSpansHash(dc,TRUE))*0x100000001B3;
  }
  "zs sweep %016X\n",hash;

  DCFill(dc,WHITE);
  DCDepthBufRst(dc);
  dc->color=RED;
  dc->r[1]=1<<31;
  dc->x=2; dc->y=1; dc->z=3;
  dc->flags|=DCF_TRANSFORMATION;
  p[0].x=1; p[0].y=2; p[0].z=2;
  p[1].x=7; p[1].y=1; p[1].z=7;
  p[2].x=8; p[2].y=8; p[2].z=7;
  p[3].x=2; p[3].y=9; p[3].z=4;
  h=GrFillPoly3(dc,4,p);
  "zs polygon %d %016X %016X\n",h,DepthSpansHash(dc),
    DepthSpansHash(dc,TRUE);

  DCFill(dc,WHITE);
  DCDepthBufRst(dc);
  DCSymmetry3Set(dc,6,0,0,6,8,0,6,8,1);
  dc->flags|=DCF_SYMMETRY|DCF_JUST_MIRROR;
  h=GrFillPoly3(dc,4,p);
  dc->flags&=~(DCF_SYMMETRY|DCF_JUST_MIRROR);
  "zs mirror %d %016X %016X\n",h,DepthSpansHash(dc),DepthSpansHash(dc,TRUE);

  DCFill(dc,WHITE);
  DCDepthBufRst(dc);
  h=GrRect3(dc,2,3,4,5,6);
  "zs rect %d %016X %016X\n",h,DepthSpansHash(dc),DepthSpansHash(dc,TRUE);
  MemSet(rect,0,sizeof(rect));
  rect[0]=12;
  rect[1]=2; rect[5]=3;
  rect[9]=7; rect[13]=9;
  DCFill(dc,WHITE);
  DCDepthBufRst(dc);
  Sprite3(dc,0,0,4,rect);
  "zs sprite %016X %016X\n",DepthSpansHash(dc),DepthSpansHash(dc,TRUE);
  DCDel(dc);
}
DepthSpansProbe;
