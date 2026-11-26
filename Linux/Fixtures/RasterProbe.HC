U0 RasterProbe()
{
  CDC *dc=DCNew(13,10),*img=DCNew(5,4),*blank=DCNew(3,2,,TRUE);
  CTask *task=Fs;
  I64 ret,ax=-4,ay=-2,bx=17,by=12,left,top,right,bottom;
  I64 keep[8],rand1,rand2,orig_seed=task->rand_seed;
  U32 orig_flags=task->task_flags;
  DCFill(dc,TRANSPARENT);
  DCExtentsInit(dc);
  dc->color=RED;
  ret=GrRect(dc,-2,-1,7,5);
  "raster rect %d %d %d %d %d %d %d\n",ret,dc->min_x,dc->min_y,
    dc->max_x,dc->max_y,GrPeek(dc,4,3),GrPeek(dc,5,3);
  dc->flags|=DCF_LOCATE_NEAREST;
  dc->cur_x=4; dc->cur_y=5; dc->nearest_dist=100000;
  GrPlot0(dc,3,3);
  GrPlot(dc,-1,3);
  "raster nearest %d %d %d\n",dc->nearest_dist,GrPeek0(dc,3,3),GrPeek(dc,-1,3);
  dc->flags&=~DCF_LOCATE_NEAREST;

  DCDepthBufAlloc(dc);
  dc->db_z=4;
  dc->color=BLUE;
  GrPlot0(dc,3,3);
  dc->db_z=5;
  dc->color=GREEN;
  GrPlot0(dc,3,3);
  ret=GrPeek(dc,3,3);
  GrPlot(dc,3,3);
  "raster depth %d %d %d\n",ret,GrPeek0(dc,3,3),dc->depth_buf[51];

  dc->color=ROP_COLLISION;
  dc->bkcolor=GREEN;
  ret=GrRect(dc,0,0,5,4);
  "raster collision %d %d\n",ret,dc->collision_cnt;
  dc->color=ROP_XOR|BLUE;
  GrPlot(dc,0,0);
  dc->color=ROPF_DITHER|(YELLOW<<16)|GREEN;
  GrRect(dc,5,1,8,3);
  "raster masks %d %d %d %d\n",GrPeek(dc,0,0),GrPeek(dc,5,1),
    GrPeek(dc,6,1),GrPeek(dc,5,2);

  Seed(0x12345678);
  rand1=RandU32;
  rand2=RandU16;
  "raster seed %X %X %X\n",rand1,rand2,task->rand_seed;
  dc->color=ROPF_PROBABILITY_DITHER|(WHITE<<16)|BLUE;
  dc->dither_probability_u16=32768;
  GrRect(dc,5,4,8,4);
  "raster random %d %d %d %d %X\n",GrPeek(dc,5,4),GrPeek(dc,5,5),
    GrPeek(dc,5,6),GrPeek(dc,5,7),task->rand_seed;
  task->rand_seed=orig_seed;
  task->task_flags=(task->task_flags&~(1<<TASKf_NONTIMER_RAND))|
    (orig_flags&(1<<TASKf_NONTIMER_RAND));

  DCFill(img,TRANSPARENT);
  img->body[1]=LTRED;
  img->body[2]=0;
  img->body[9]=LTGREEN;
  img->body[10]=WHITE;
  dc->color=BLACK;
  ret=GrBlot(dc,-1,4,img);
  "raster blot %d %d %d %d\n",ret,GrPeek(dc,0,4),
    GrPeek(dc,1,4),GrPeek(dc,0,5);
  dc->color=ROP_MONO|RED;
  GrBlot(dc,5,6,img);
  "raster brushmask %d %d %d\n",GrPeek(dc,5,6),GrPeek(dc,6,6),GrPeek(dc,7,6);
  img->flags|=DCF_NO_TRANSPARENTS;
  GrBlot(dc,7,2,img);
  "raster opaque %d %d %d\n",GrPeek(dc,7,2),GrPeek(dc,8,2),GrPeek(dc,9,2);
  img->flags&=~DCF_NO_TRANSPARENTS;

  ret=ClipLine(&ax,&ay,&bx,&by,0,0,12,9);
  GrClamp(dc,&left,&top,&right,&bottom,2,1);
  "raster clip %d %d %d %d %d %d %d %d %d\n",ret,ax,ay,bx,by,left,top,right,bottom;
  dc->color=LTCYAN;
  ret=GrLine(dc,-4,-2,17,12);
  GrLine(dc,0,9,12,9,2,1);
  GrRectB(dc,2,8,0,8);
  "raster line %d %d %d %d %d\n",ret,GrPeek(dc,0,0),GrPeek(dc,12,8),
    GrPeek(dc,1,9),GrPeek(dc,2,9);

  keep[0]=task->pix_left; keep[1]=task->pix_right;
  keep[2]=task->pix_top; keep[3]=task->pix_bottom;
  keep[4]=task->scroll_x; keep[5]=task->scroll_y;
  keep[6]=task->win_z_num; keep[7]=gr.win_z_buf[81];
  task->pix_left=1; task->pix_right=12;
  task->pix_top=3; task->pix_bottom=9;
  task->scroll_x=1; task->scroll_y=2;
  task->win_z_num=1; gr.win_z_buf[81]=2;
  dc->flags|=DCF_SCRN_BITMAP;
  dc->color=PURPLE;
  ret=GrPeek(dc,7,3);
  GrPlot(dc,7,3);
  dc->flags|=DCF_ON_TOP;
  GrPlot(dc,7,3);
  "raster window %d %d %d\n",ret,GrPeek(dc,7,3),GrPeek(dc,-2,3);
  task->pix_left=keep[0]; task->pix_right=keep[1];
  task->pix_top=keep[2]; task->pix_bottom=keep[3];
  task->scroll_x=keep[4]; task->scroll_y=keep[5];
  task->win_z_num=keep[6]; gr.win_z_buf[81]=keep[7];
  dc->flags&=~(DCF_ON_TOP|DCF_SCRN_BITMAP);

  DCExtentsInit(blank);
  ret=GrRect(blank,0,0,3,2);
  "raster blank %d %d %d\n",ret,blank->max_x,blank->max_y;
  "raster file %d\n",GRWrite("B:/RasterProbe",dc);
  DCDel(blank); DCDel(img); DCDel(dc);
}
RasterProbe;
