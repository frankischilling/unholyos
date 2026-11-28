// Exercise the song command from Terry A. Davis's Adam/DolDoc/DocRecalc.HC.
Bool DocSongWait(I64 ona)
{
  F64 limit=tS+2.0;
  while (tS<limit) {
    if (ona) {
      if (music.cur_song && TaskValidate(music.cur_song_task) &&
          scrncast.ona==ona)
        return TRUE;
    } else if (!music.cur_song && !music.cur_song_task && !scrncast.ona)
      return TRUE;
    Sleep(5);
  }
  return FALSE;
}

U0 DocSongFocus(U8 *)
{
  DocTermNew;
  Fs->win_inhibit&=~WIF_SELF_FOCUS;
  Fs->display_flags|=1;
  WinFocus(Fs);
  while (TRUE)
    Sleep(5);
}

I64 DocSongProbe()
{
  I64 bits=0,number;
  CTask *song_task,*other;
  CDoc *doc=DocNew;
  Bool was_muted=Mute(FALSE);

  DocPrint(doc,"$$SO,\"Other score\",A=\"4eG\"$$");
  DocRecalc(doc);
  if (doc->flags&DOCF_HAS_SONG && !music.cur_song)
    bits|=1;
  DocDel(doc);

  WinFocus(Fs);
  DocClear(,TRUE);
  DocPrint(DocPut,"$$SO,\"Score one\",A=\"4eA\"$$");
  if (DocSongWait(60)) {
    song_task=music.cur_song_task;
    number=song_task->task_num;
    if (!StrCmp(song_task->task_name,"Song") &&
        !StrCmp(music.cur_song,"4eA"))
      bits|=2;
    if (DocPut->flags&DOCF_HAS_SONG && Bt(&Fs->task_flags,TASKf_HAS_SONG))
      bits|=4;

    DocClear(,TRUE);
    DocPrint(DocPut,"$$SO,\"Score two\",A=\"4eB\"$$");
    if (DocSongWait(62) && music.cur_song_task==song_task &&
        song_task->task_num==number && !StrCmp(music.cur_song,"4eB"))
      bits|=8;

    other=Spawn(&DocSongFocus,,"No score");
    if (other) {
      if (DocSongWait(0) && sys_focus_task==other)
        bits|=64;
      WinFocus(Fs);
      if (DocSongWait(62) && music.cur_song_task->task_num!=number)
        bits|=128;
      Kill(other);
    }
  }

  DocClear(,TRUE);
  if (DocSongWait(0))
    bits|=16;
  if (!(DocPut->flags&DOCF_HAS_SONG) && !Bt(&Fs->task_flags,TASKf_HAS_SONG))
    bits|=32;
  Mute(was_muted);
  DocClear(,TRUE);
  return bits;
}

"Doc songs %X\n",DocSongProbe();
