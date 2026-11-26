U0 WinScrollProbe()
{
  CD3I64 old,s;
  WinScrollNull(Fs,&old);
  Fs->scroll_x=48;
  Fs->scroll_y=-24;
  Fs->scroll_z=3;
  WinScrollNull(Fs,&s);
  "scroll saved %d %d %d\n",s.x,s.y,s.z;
  "scroll cleared %d %d %d\n",Fs->scroll_x,Fs->scroll_y,Fs->scroll_z;
  WinScrollRestore(Fs,&s);
  "scroll restored %d %d %d\n",Fs->scroll_x,Fs->scroll_y,Fs->scroll_z;
  WinScrollRestore(Fs,&old);
}

WinScrollProbe;
