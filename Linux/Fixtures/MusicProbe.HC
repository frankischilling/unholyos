U0 MusicProbeClear()
{
  CSndData *s,*head=&scrncast.snd_head;
  while (head->next!=head) {
    s=head->next;
    QueRem(s);
    Free(s);
  }
}

U0 MusicProbe()
{
  I64 res=0,i,j,n,size;
  F64 old_correction=music.tM_correction,t0,b0;
  Bool old_mute=Mute(FALSE),old_music_mute=music.mute;
  CAUData *a;
  static I8 notes[13]={0,0,60,0,63,0,50,0,53,55,0,0,0};
  music.mute=FALSE;
  MusicSettingsRst;
  if (sizeof(CMusicGlbls)==104 && offset(CMusicGlbls.note_map)==32 &&
      offset(CMusicGlbls.tempo)==56 && offset(CMusicGlbls.last_tM)==96 &&
      music.octave==4 && music.note_len==1 && music.tempo==2.5 &&
      music.meter_top==4 && music.meter_bottom==4)
    res|=1;
  for (i=0;i<12;i++) {
    j=Note2Ona(i);
    if (Ona2Note(j)!=i || Ona2Octave(j)!=4)
      break;
  }
  if (i==12 && Note2Ona(3)==51 && Note2Ona(0,5)==72)
    res|=2;
  if (*MusicSetMeter("M3/4M7/8A")=='A' && music.meter_top==7 &&
      music.meter_bottom==8 && *MusicSetOctave("35A")=='A' &&
      music.octave==5 && *MusicSetNoteLen("ht.A")=='A' && music.note_len==2)
    res|=4;
  ScrnCast(FALSE,TRUE);
  MusicProbeClear;
  music.tempo=20;
  music.stacatto_factor=0.75;
  music.tM_correction=0;
  ScrnCast(TRUE,TRUE);
  Play("M3/4" "4qAB#Cb(DeER","one\0 \0three\0four\0five\0six\n\0");
  ScrnCast(FALSE,TRUE);
  n=AUWrite("B:/MusicProbe.AU",&scrncast.snd_head,scrncast.t0_now,scrncast.t0_tS);
  a=FileRead("B:/MusicProbe.AU",&size);
  if (a && size==117 && n==117) {
    for (i=0;i<13;i++) {
      "%d ",a[i].ona;
    }
    "\n";
    for (i=0;i<13;i++)
      if (a[i].ona!=notes[i] || (i && a[i].cdt<a[i-1].cdt))
        break;
    if (i==13 && a[3].cdt>a[2].cdt && a[9].cdt>a[8].cdt && !scrncast.ona)
      res|=8;
  }
  Free(a);
  "music state %d %d %d %d %X %X\n",music.play_note_num,music.octave,
      music.meter_top,music.meter_bottom,music.note_len(I64),music.tM_correction(I64);
  if (music.play_note_num==6 && music.octave==4 && music.meter_top==3 &&
      music.meter_bottom==4 && music.note_len==0.5 &&
      music.tM_correction(I64)==0x401800000000000A)
    res|=16;
  MusicProbeClear;
  music.mute=TRUE;
  ScrnCast(TRUE,TRUE);
  Play("sA");
  ScrnCast(FALSE,TRUE);
  if (music.play_note_num==1 && scrncast.snd_head.next==scrncast.snd_head.last &&
      scrncast.snd_head.next->ona==0 && !scrncast.ona)
    res|=32;
  MusicProbeClear;
  MusicSettingsRst;
  b0=Beat;
  t0=tM;
  Sleep(20);
  if (Beat>b0 && tM>t0) {
    music.tempo=0;
    b0=Beat;
    Sleep(20);
    if (Beat==b0)
      res|=64;
  }
  music.mute=old_music_mute;
  music.tM_correction=old_correction;
  MusicSettingsRst;
  Mute(old_mute);
  "Music probe %X\n",res;
}

MusicProbe;
