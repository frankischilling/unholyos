U0 MouseProbe()
{
  I64 x,y,code,i,old=Fs->win_inhibit;
  I64 mask=1<<MSG_MS_MOVE|1<<MSG_MS_L_DOWN|1<<MSG_MS_L_UP|
          1<<MSG_MS_R_DOWN|1<<MSG_MS_R_UP;
  Fs->win_inhibit=WIF_SELF_MS_L|WIF_SELF_MS_R|
                  WIF_FOCUS_TASK_MS_L_D|WIF_FOCUS_TASK_MS_R_D;
  GridInit;
  MsSet(160,160,0,FALSE,FALSE);
  Sleep(50);
  FlushMsgs;
  "mouse ready %d %d %d %d %d %d\n",sizeof(CMsStateGlbls),
      sizeof(CMsHardStateGlbls),sizeof(CGridGlbls),
      ms_hard.installed,ms.has_wheel,ms_hard.pkt_size;
  for (i=0;i<5;i++) {
    code=GetMsg(&x,&y,mask);
    "mouse msg %d %d %d %d %d\n",code,x+Fs->pix_left+Fs->scroll_x,
        y+Fs->pix_top+Fs->scroll_y,!!(kbd.scan_code&SCF_MS_L_DOWN),
        !!(kbd.scan_code&SCF_MS_R_DOWN);
  }
  "mouse wheel ready\n";
  while (!ms.pos.z)
    Yield;
  "mouse wheel %d\n",ms.pos.z;
  "mouse wheel back\n";
  while (ms.pos.z)
    Yield;
  "mouse irq %d\n",ms_hard.irqs_working;
  ms_grid.snap=TRUE;
  MsSet(171,157,0,FALSE,FALSE);
  "mouse snap %d %d %d %d\n",ms.pos.x,ms.pos.y,ms.presnap.x,ms.presnap.y;
  GridInit;
  MsSet(160,160,0,FALSE,FALSE);
  Fs->win_inhibit=WIF_SELF_MS_L|WIF_SELF_MS_R;
  ms.dbl_time=4.0;
  Sleep(50);
  FlushMsgs;
  "mouse double ready\n";
  for (i=0;i<2;i++) {
    code=GetMsg(&x,&y,1<<MSG_MS_L_DOWN|1<<MSG_MS_L_UP|
                      1<<MSG_MS_L_D_DOWN|1<<MSG_MS_L_D_UP);
    "mouse double %d %d %d\n",code,x+Fs->pix_left+Fs->scroll_x,
        y+Fs->pix_top+Fs->scroll_y;
  }
  ms.dbl_time=0.350;
  Fs->win_inhibit=old;
}

MouseProbe;
