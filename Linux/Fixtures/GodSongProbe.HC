// Exercises Terry A. Davis's Adam/God/GodSong.HC through both forms.
I64 god_song_feed_done=0;

U0 GodSongFeed(CTask *owner)
{
  I64 i;
  for (i=0;i<2;i++) {
    while (!progress4_max || !owner->popup_task)
      Sleep(1);
    if (i)
      GodHexIns("0402064600780a05c1e0826000");
    else
      GodHexIns("0000246038ac04e2040202600378900f");
    god_song_feed_done=i+1;
    while (progress4_max)
      Sleep(1);
  }
}

I64 GodSongProbe()
{
  I64 bits=0;
  CDoc *doc=DocPut;
  CTask *child;
  U8 *a="5q4GeB5CetDEFsG4A5G4AGAGAe5C4Get5CDDqG",
     *b="5M6/8e.Rs4AeBs5CRDReEetF4G5Gs4A5C4A5CqR",*score=MStrPrint("%s%s%s%s",a,a,b,b);
  Bool old_mute=Mute(TRUE);
  F64 limit;
  if (sizeof(CMakeSongSettings)==26 &&
      offset(CMakeSongSettings.octave)==10)
    bits|=1;

  DocClear(doc,TRUE);
  DocPutKey(doc,0,SC_F6|SCF_KEY_DESC);
  if (!StrCmp(keydev.desc,"Cmd /God Song"))
    bits|=2;
  child=Spawn(&GodSongFeed,Fs,"Score source",,Fs);
  DocPutKey(doc,0,SC_F6);
  limit=tS+2.0;
  while (tS<limit && (!music.cur_song_task || god_song_feed_done!=2))
    Sleep(2);
  if (god_song_feed_done==2)
    bits|=4;
  if (DocFind(doc,,"<Song>") && doc->cur_entry->aux_str &&
      !StrCmp(doc->cur_entry->aux_str,score))
    bits|=8;
  if (music.cur_song && !StrCmp(music.cur_song,score))
    bits|=16;
  if (TaskValidate(music.cur_song_task) &&
      !StrCmp(music.cur_song_task->task_name,"Song"))
    bits|=32;
  if (!progress4 && !progress4_max && music.octave==4)
    bits|=64;
  DocClear(doc,TRUE);
  limit=tS+2.0;
  while (tS<limit && music.cur_song_task)
    Sleep(2);
  if (!music.cur_song_task && !music.cur_song)
    bits|=128;
  if (TaskValidate(child))
    Kill(child);
  Free(score);
  Mute(old_mute);
  return bits;
}

"God song checks %X\n",GodSongProbe();
