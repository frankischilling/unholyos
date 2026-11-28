// Exercise Noise and Sweep from Terry A. Davis's Adam/ASnd.HC.
CSoundEffectFrame *SoundEffectStarted(CTask *task)
{
  F64 limit=tS+1.0;
  CSoundEffectFrame *frame;
  while (TaskValidate(task) && tS<limit) {
    frame=FramePtr("CSoundEffectFrame",task);
    if (frame)
      return frame;
    Yield;
  }
  return NULL;
}

U0 SoundEffectsProbe()
{
  I64 bits=0;
  F64 limit;
  CTask *noise,*sweep;
  CSoundEffectFrame *frame;
  Bool old_mute=Mute(FALSE);
  I64 old_music_mute=music.mute(U8);
  music.mute=FALSE;
  Snd;
  if (!Noise(0,60,61) && !Sweep(-1,50,51) && !music.mute)
    bits|=1;

  noise=Noise(500,60,60);
  if (TaskValidate(noise) && noise->parent_task==Fs &&
      !StrCmp(noise->task_name,"Noise") && music.mute(U8)==1)
    bits|=2;
  frame=SoundEffectStarted(noise);
  if (frame && sizeof(CSoundEffectFrame)==14 &&
      frame->type==SE_NOISE && frame->ona1==60 && frame->ona2==60 &&
      frame->duration==0.5 && scrncast.ona==60)
    bits|=4;
  sweep=Sweep(500,54,54);
  if (TaskValidate(sweep) && sweep->parent_task==Fs &&
      !StrCmp(sweep->task_name,"Noise") && music.mute(U8)==2)
    bits|=8;
  if (SoundEffectStarted(sweep)) {
    Kill(sweep);
    if (music.mute(U8)==1 && TaskValidate(noise)) {
      Kill(noise);
      if (!music.mute && !scrncast.ona)
        bits|=16;
    }
  }
  if (TaskValidate(sweep)) Kill(sweep);
  if (TaskValidate(noise)) Kill(noise);

  music.mute=TRUE;
  sweep=Sweep(120,47,61);
  frame=SoundEffectStarted(sweep);
  if (frame && frame->type==SE_SWEEP && frame->ona1==47 && frame->ona2==61 &&
      47<=scrncast.ona && scrncast.ona<=61) {
    limit=tS+1.0;
    while (TaskValidate(sweep) && tS<limit)
      Yield;
    if (!TaskValidate(sweep) && music.mute(U8)==1 && !scrncast.ona)
      bits|=32;
  }
  if (TaskValidate(sweep)) Kill(sweep);
  music.mute(U8)=old_music_mute;
  Mute(old_mute);
  "Sound effects %X\n",bits;
}

SoundEffectsProbe;
