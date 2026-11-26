I64 doc_cb_stage,doc_cb_checks,doc_cb_key,doc_cb_result;
CDocEntry *doc_cb_entry,*doc_cb_tag;

U8 *DocCBTag(CDoc *doc,CDocEntry *entry,CTask *mem_task)
{
  U8 *buf=MAlloc(16,mem_task);
  if (doc->owning_task==Fs && entry==doc_cb_tag && mem_task==doc->mem_task)
    doc_cb_checks|=1;
  if (Fs==sys_winmgr_task)
    doc_cb_checks|=2;
  if (doc_cb_stage<2)
    MemCpy(buf,"LIVE",5);
  else
    MemCpy(buf,"NEXT",5);
  return buf;
}

I64 DocCBLeft(CDoc *doc,CDocEntry *entry)
{
  if (Fs==doc->win_task && doc->owning_task==NULL && entry==doc_cb_entry &&
      DocLock(doc)) {
    doc_cb_checks|=4;
    DocUnlock(doc);
  }
  return 101;
}

I64 DocCBRight(CDoc *doc,CDocEntry *entry)
{
  if (Fs==doc->win_task && doc->owning_task==NULL && entry==doc_cb_entry &&
      DocLock(doc)) {
    doc_cb_checks|=8;
    DocUnlock(doc);
  }
  return 202;
}

I64 DocCBThrow(CDoc *,CDocEntry *)
{
  throw('DocCB',TRUE);
  return 1;
}

U0 DocCBTask(I64)
{
  CDoc *doc=DocNew;
  if (!doc)
    return;
  doc->flags|=DOCF_HIDE_CURSOR|DOCF_DONT_HIGHLIGHT_CURSOR;
  WinHorz(18,35);
  WinVert(20,28);
  Fs->text_attr=0x1F;
  Fs->border_attr=0x1E;
  DocPrint(doc,"$$TX+H,\"K\"$$old");
  DocClear(doc);
  if (doc->cur_entry->last->tag[0]=='K')
    doc_cb_checks|=16;
  DocClear(doc,TRUE);
  if (doc->cur_entry->last==doc->cur_entry)
    doc_cb_checks|=32;
  doc_cb_tag=DocPrint(doc,"$$TX+TC,\" \"$$");
  doc_cb_tag->tag_cb=&DocCBTag;
  doc_cb_entry=DocPrint(doc,"\n  $$BT-X+LC+RC,\"CALL\"$$\n");
  doc_cb_entry->left_cb=&DocCBLeft;
  doc_cb_entry->right_cb=&DocCBRight;
  Fs->display_doc=doc;
  Fs->win_inhibit&=~(WIF_SELF_DOC|WIF_SELF_MS_L|WIF_SELF_MS_R);
  Fs->display_flags=(Fs->display_flags|1)&~8;
  WinZBufUpdate;
  MsSet(21*8+3,21*8+3);
  Sleep(200);
  doc_cb_stage=1;

  GetMsg(&doc_cb_key,,1<<MSG_KEY_DOWN);
  DocPutKey(doc,doc_cb_key);
  GetMsg(&doc_cb_result,,1<<MSG_CMD);
  doc_cb_stage=2;
  while (doc_cb_stage==2)
    Sleep(8);

  GetMsg(&doc_cb_key,,1<<MSG_KEY_DOWN);
  DocPutKey(doc,doc_cb_key);
  GetMsg(&doc_cb_result,,1<<MSG_CMD);

  Fs->display_flags&=~1;
  WinZBufUpdate;
  Fs->display_doc=NULL;
  DocDel(doc);
  doc_cb_stage=4;
}

U0 DocCallback()
{
  CTask *task;
  doc_cb_stage=doc_cb_checks=0;
  task=Spawn(&DocCBTask,0,"Document callbacks");
  TaskQueRem(task);
  TaskQueIns(task,sys_winmgr_task);
  while (doc_cb_stage<1)
    Sleep(8);
  "doc callback ready %d\n",doc_cb_checks;
  while (doc_cb_stage<2)
    Sleep(8);
  "doc callback left %d %d\n",doc_cb_key,doc_cb_result;
  Sleep(120);
  doc_cb_stage=3;
  "doc callback right ready\n";
  while (doc_cb_stage<4)
    Sleep(8);
  "doc callback right %d %d\n",doc_cb_key,doc_cb_result;
  "doc callback checks %d\n",doc_cb_checks;
}

CDoc *doc_cb_throw_doc;

U0 DocCBThrowTest()
{
  CDocEntry *entry;
  doc_cb_throw_doc=DocNew;
  entry=DocPrint(doc_cb_throw_doc,"$$BT-X+LC,\"THROW\"$$");
  entry->left_cb=&DocCBThrow;
  doc_cb_throw_doc->cur_entry=entry;
  doc_cb_throw_doc->cur_col=1;
  doc_cb_throw_doc->cmd_U8=' ';
  DocEntryRun(doc_cb_throw_doc,entry,TRUE);
}

I64 DocCBThrowCheck()
{
  I64 okay;
  if (!doc_cb_throw_doc)
    return 0;
  okay=doc_cb_throw_doc->owning_task==NULL &&
       doc_cb_throw_doc->cur_entry==doc_cb_throw_doc;
  DocDel(doc_cb_throw_doc);
  doc_cb_throw_doc=NULL;
  return okay;
}

DocCallback;
