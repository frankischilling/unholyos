U0 SoundProbe()
{
  "sound notes %016X %016X %016X %016X\n",
    Ona2Freq(0)(U64),Ona2Freq(60)(U64),Ona2Freq(62)(U64),Ona2Freq(-1)(U64);
  "sound keys %d %d %d %d %d\n",
    Freq2Ona(0.0),Freq2Ona(440.0),Freq2Ona(459.0),
    Freq2Ona(1.0),Freq2Ona(1.0e9);
  Beep;
  Beep(55,1);
}
SoundProbe;
