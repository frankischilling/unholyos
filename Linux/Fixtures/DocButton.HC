I64 doc_button_stage,doc_button_key,doc_button_msg,doc_button_arg1,
    doc_button_arg2;

U0 DocButtonTask(I64)
{
  CDoc *doc=DocNew;
  I64 key;
  if (!doc)
    return;
  doc->flags|=DOCF_HIDE_CURSOR|DOCF_DONT_HIGHLIGHT_CURSOR;
  WinHorz(18,35);
  WinVert(20,28);
  Fs->text_attr=0x1F;
  Fs->border_attr=0x1E;
  DocPrint(doc,"\n  $$BT-X,\"RUN\",LE=73$$\n\n  $$BT,\"EXIT\",LE=19$$\n");
  Fs->display_doc=doc;
  Fs->win_inhibit&=~(WIF_SELF_DOC|WIF_SELF_MS_L);
  Fs->display_flags=(Fs->display_flags|1)&~8;
  WinZBufUpdate;
  MsSet(21*8+3,21*8+3);
  Sleep(120);
  doc_button_stage=1;

  GetMsg(&doc_button_key,,1<<MSG_KEY_DOWN);
  DocPutKey(doc,doc_button_key);
  doc_button_msg=GetMsg(&doc_button_arg1,&doc_button_arg2,
        (1<<MSG_CMD)|(1<<MSG_KEY_DOWN));

  MsSet(21*8+3,23*8+3);
  Sleep(120);
  doc_button_stage=2;
  GetMsg(&key,,1<<MSG_KEY_DOWN);
  DocPutKey(doc,key);
  doc_button_msg=GetMsg(&doc_button_arg1,&doc_button_arg2,
        (1<<MSG_CMD)|(1<<MSG_KEY_DOWN));

  Fs->display_flags&=~1;
  WinZBufUpdate;
  Fs->display_doc=NULL;
  DocDel(doc);
  doc_button_stage=3;
}

U0 DocButton()
{
  CTask *task;
  doc_button_stage=0;
  task=Spawn(&DocButtonTask,0,"Document buttons");
  TaskQueRem(task);
  TaskQueIns(task,sys_winmgr_task);
  while (doc_button_stage<1)
    Sleep(8);
  "doc button ready\n";
  while (doc_button_stage<2)
    Sleep(8);
  "doc button key %d\n",doc_button_key;
  "doc button command %d %d %d\n",doc_button_msg,doc_button_arg1,doc_button_arg2;
  "doc button exit ready\n";
  while (doc_button_stage<3)
    Sleep(8);
  "doc button cancel %d %d %d\n",doc_button_msg,doc_button_arg1,doc_button_arg2;
}

DocButton;
