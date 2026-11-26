U0 SettingsProbeDraw(CTask *,CDC *)
{
}

I64 settings_probe_ended=0;

U0 SettingsProbeEnd()
{
  settings_probe_ended++;
  Exit;
}

U0 SettingsProbeIdle(I64)
{
  while (TRUE)
    Sleep(10);
}

U0 TaskSettingsProbe()
{
  CTask *task=Fs,*anim,*song,*replacement;
  CDoc *doc=DocPut;
  CTaskSettings *outer=SettingsPush(task),*inner;
  U8 *path=StrNew(outer->cur_dir),*restored_path;
  CBGR48 palette[16];
  I64 doc_mask=DOCF_HIDE_CURSOR|DOCF_DONT_HIGHLIGHT_CURSOR|DOCF_NO_SCROLL_BARS;
  I64 old_doc=0,a,b,c,d,e;
  "settings layout %d %d %d %d\n",sizeof(CTaskSettings),
    offset(CTaskSettings.draw_it),offset(CTaskSettings.palette),
    offset(CTaskSettings.autocomplete);
  if (doc)
    old_doc=doc->flags&doc_mask;

  task->draw_it=&SettingsProbeDraw;
  task->task_end_cb=&SettingsProbeEnd;
  task->scroll_x=13;
  task->scroll_y=-15;
  task->scroll_z=22;
  task->text_attr=0xE1;
  task->border_attr=0x28;
  task->task_title[0]='X';

  anim=Spawn(&SettingsProbeIdle,NULL,"Settings Animate");
  song=Spawn(&SettingsProbeIdle,NULL,"Settings Song");
  task->animate_task=anim;
  task->song_task=song;
  inner=SettingsPush(,TSF_SAME_SONG);
  "settings push %d %d %d %d %d\n",task->next_settings==inner,
    inner->next==outer,inner->animate_task==anim && !task->animate_task,
    IsSuspended(anim),task->song_task==song && !IsSuspended(song);

  replacement=Spawn(&SettingsProbeIdle,NULL,"Settings Replacement");
  task->animate_task=replacement;
  replacement->task_end_cb=&SettingsProbeEnd;
  task->draw_it=NULL;
  task->task_end_cb=NULL;
  task->scroll_x=task->scroll_y=task->scroll_z=0;
  task->text_attr=0x31;
  task->border_attr=0x48;
  task->task_title[0]='Z';
  if (doc)
    doc->flags^=doc_mask;
  a=WinBorder(!inner->border);
  b=WinBorder(inner->border);
  WinBorder(!inner->border);
  WinHorz(10,30);
  WinVert(12,40);
  Cd("/");
  GrPaletteGet(palette);
  palette[2].b^=0xFFFF;
  GrPaletteSet(palette);
  "settings border %d %d\n",a==inner->border,b!=inner->border;

  SettingsPop(,TSF_SAME_SONG);
  GrPaletteGet(palette);
  restored_path=DirCur;
  a=!StrCmp(restored_path,path);
  Free(restored_path);
  b=task->draw_it==&SettingsProbeDraw && task->task_end_cb==&SettingsProbeEnd;
  c=task->scroll_x==13 && task->scroll_y==-15 && task->scroll_z==22;
  d=task->text_attr==0xE1 && task->border_attr==0x28 && task->task_title[0]=='X';
  e=!doc || (doc->flags&doc_mask)==old_doc;
  "settings restore %d %d %d %d %d %d\n",a,b,c,d,e,
    !MemCmp(palette,outer->palette,sizeof(palette));
  "settings children %d %d %d\n",
    !TaskValidate(replacement) && settings_probe_ended==1,
    task->animate_task==anim && !IsSuspended(anim),task->song_task==song;
  a=task->win_left==outer->left && task->win_right==outer->right &&
    task->win_top==outer->top && task->win_bottom==outer->bottom;
  b=task->draw_it!=outer->draw_it;
  SettingsPop;
  "settings nested %d %d %d %d %d\n",a,b,!TaskValidate(anim),
    !TaskValidate(song),!task->next_settings;
  Free(path);
}
TaskSettingsProbe;
