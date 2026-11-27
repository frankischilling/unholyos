U0 MPMod(U8 *st)
{
  MusicSettingsRst;
  MusicSetNoteLen(st);
  "MP %s %X\n",st,music.note_len(I64);
}
U0 MusicPrecision()
{
  F64 a=0.25,b=0.9,c=2.5;
  "MP initial %X %X %X\n",music.note_len(I64),music.tempo(I64),music.stacatto_factor(I64);
  music.note_len=0;
  music.tempo=0;
  music.stacatto_factor=0;
  MusicSettingsRst;
  "MP reset %X %X %X\n",music.note_len(I64),music.tempo(I64),music.stacatto_factor(I64);
  "MP literal %X %X %X\n",a(I64),c(I64),b(I64);
  MPMod("w"); MPMod("h"); MPMod("q"); MPMod("e"); MPMod("s");
  MPMod("wt"); MPMod("ht"); MPMod("qt"); MPMod("et"); MPMod("st");
  MPMod("w."); MPMod("h."); MPMod("q."); MPMod("e."); MPMod("s.");
  MPMod("wt."); MPMod("ht."); MPMod("qt."); MPMod("et."); MPMod("st.");
  MPMod("w.t"); MPMod("h.t"); MPMod("q.t"); MPMod("e.t"); MPMod("s.t");
  MPMod("qtt"); MPMod("stt"); MPMod("q.."); MPMod("s..");
  MusicSettingsRst;
  "MP DONE\n";
}
MusicPrecision;