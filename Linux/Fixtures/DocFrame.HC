I64 doc_frame_ready,doc_frame_stop,doc_frame_closed;

U0 DocFrameTask(I64 upper)
{
  CDoc *d=DocNew,*b=NULL;
  I64 stop=2;
  if (!d)
    return;
  d->flags|=DOCF_HIDE_CURSOR;
  if (upper) {
    stop=1;
    WinHorz(36,41);
    WinVert(27,30);
    Fs->text_attr=0x2E;
    Fs->border_attr=0x2F;
    DocPrint(d,"TOP\nQ\n");
  } else {
    WinHorz(30,45);
    WinVert(24,33);
    Fs->text_attr=0x1F;
    Fs->border_attr=0x1E;
    DocPrint(d,"ABCD\nE\tF\n"
      "$$BG,4$$MW$$BG$$\n"
      "0123456789abcdef\n"
      "$$UL,1$$U$$UL,0$$\n"
      "$$IV,1$$I$$IV,0$$\n"
      "$$SY,2$$S$$SY,0$$\n");
    b=DocNew;
    b->flags|=DOCF_BORDER_DOC;
    DocPrint(b,"$$CM,1,-1$$$$TX+BD,\"H\"$$");
  }
  Fs->display_doc=d;
  Fs->border_doc=b;
  Fs->win_inhibit&=~WIF_SELF_DOC;
  Fs->display_flags=(Fs->display_flags|1)&~8;
  WinZBufUpdate;
  doc_frame_ready++;
  while (doc_frame_stop<stop)
    Sleep(8);
  Fs->display_flags&=~1;
  WinZBufUpdate;
  Fs->display_doc=NULL;
  Fs->border_doc=NULL;
  DocDel(b);
  DocDel(d);
  doc_frame_closed++;
}

U0 DocFrameWait(U8 *stage,I64 key)
{
  Sleep(120);
  "doc frame %s\n",stage;
  while (GetKey()!=key) {}
}

U0 DocFrame()
{
  CTask *task,*under;
  doc_frame_ready=doc_frame_stop=doc_frame_closed=0;
  task=under=Spawn(&DocFrameTask,0,"Document underlay");
  TaskQueRem(task);
  TaskQueIns(task,sys_winmgr_task);
  while (doc_frame_ready<1)
    Sleep(8);
  "doc depth %d\n",IsPixCovered0(under,36*8,27*8);
  DocFrameWait("lower",'a');
  task=Spawn(&DocFrameTask,1,"Document overlay");
  TaskQueRem(task);
  TaskQueIns(task,sys_winmgr_task);
  while (doc_frame_ready<2)
    Sleep(8);
  "doc depth %d\n",IsPixCovered0(under,36*8,27*8);
  DocFrameWait("overlap",'o');
  doc_frame_stop=1;
  while (doc_frame_closed<1)
    Sleep(8);
  "doc depth %d\n",IsPixCovered0(under,36*8,27*8);
  DocFrameWait("uncovered",'r');
  doc_frame_stop=2;
  while (doc_frame_closed<2)
    Sleep(8);
  DocFrameWait("cleared",'c');
}

DocFrame;
