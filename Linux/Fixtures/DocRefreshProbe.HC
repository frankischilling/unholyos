I64 doc_refresh_tags;

U8 *DocRefreshTag(CDoc *,CDocEntry *,CTask *mem_task)
{
  doc_refresh_tags++;
  return StrNew("Offscreen",mem_task);
}

U0 DocRefreshProbe()
{
  CDoc *doc=DocNew;
  CDocEntry *entry;
  CWinScroll *v=&Fs->vert_scroll,saved_v,saved_h;
  F64 fps=winmgr.fps;
  I64 i,partial_pos,partial_flag,partial_tags,full_pos,full_flag,full_tags,
      forced_pos,forced_flag,bwd_pos,bwd_flag,bwd_movement;

  MemCpy(&saved_v,v,sizeof(CWinScroll));
  MemCpy(&saved_h,&Fs->horz_scroll,sizeof(CWinScroll));
  doc->flags|=DOCF_HIDE_CURSOR;
  for (i=0;i<70;i++)
    DocPrint(doc,"%02d line\n",i);
  entry=DocPrint(doc,"$$TX+TC,\" \"$$");
  entry->tag_cb=&DocRefreshTag;
  DocRecalc(doc);

  winmgr.fps=30.0;
  doc->updates_cnt=1;
  doc_refresh_tags=0;
  v->pos=3;
  v->flags=1<<WSSf_SET_TO_POS;
  DocRecalc(doc,RECALCt_TO_SCRN|RECALCF_HAS_CURSOR);
  partial_pos=doc->top_line_num;
  partial_flag=v->flags;
  partial_tags=doc_refresh_tags;

  doc->updates_cnt=4;
  DocRecalc(doc,RECALCt_TO_SCRN|RECALCF_HAS_CURSOR);
  full_pos=doc->top_line_num;
  full_flag=v->flags;
  full_tags=doc_refresh_tags;

  doc->updates_cnt=1;
  doc->flags|=DOCF_DO_FULL_REFRESH;
  v->pos=4;
  v->flags=1<<WSSf_SET_TO_POS;
  DocRecalc(doc,RECALCt_TO_SCRN|RECALCF_HAS_CURSOR);
  forced_pos=doc->top_line_num;
  forced_flag=v->flags;

  doc->flags&=~DOCF_DO_FULL_REFRESH;
  DocPrint(doc,"$$CM,0,-1$$");
  DocRecalc(doc);
  bwd_movement=!!(doc->flags&DOCF_BWD_MOVEMENT);
  doc->updates_cnt=1;
  v->pos=5;
  v->flags=1<<WSSf_SET_TO_POS;
  DocRecalc(doc,RECALCt_TO_SCRN|RECALCF_HAS_CURSOR);
  bwd_pos=doc->top_line_num;
  bwd_flag=v->flags;

  winmgr.fps=fps;
  MemCpy(v,&saved_v,sizeof(CWinScroll));
  MemCpy(&Fs->horz_scroll,&saved_h,sizeof(CWinScroll));
  DocDel(doc);
  "refresh partial %d %d %d\n",partial_pos,partial_flag,partial_tags;
  "refresh full %d %d %d\n",full_pos,full_flag,full_tags;
  "refresh forced %d %d\n",forced_pos,forced_flag;
  "refresh backwards %d %d %d\n",bwd_pos,bwd_flag,bwd_movement;
}

DocRefreshProbe;
