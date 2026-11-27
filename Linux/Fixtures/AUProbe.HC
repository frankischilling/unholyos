U0 AUProbeAdd(CSndData *head,F64 tS,I8 ona)
{
  CSndData *s=CAlloc(sizeof(CSndData));
  s->tS=tS;
  s->ona=ona;
  QueIns(s,head->last);
}

U0 AUProbeClear(CSndData *head)
{
  CSndData *s;
  while (head->next!=head) {
    s=head->next;
    QueRem(s);
    Free(s);
  }
}

U0 AUProbe()
{
  CSndData head;
  CAUData *a;
  I64 res=0,i,n,size,origin=0x123456789ABC;
  static I64 ticks[5]={-12427,12427,12427,37282,68351};
  static I8 notes[5]={0,61,62,63,64};
  Bool old_mute;
  if (sizeof(CAUData)==9 && offset(CAUData.ona)==8 &&
      sizeof(CSndData)==25 && offset(CSndData.tS)==16 &&
      sizeof(CScrnCastGlbls)==68 && offset(CScrnCastGlbls.t0_now)==25 &&
      offset(CScrnCastGlbls.ona)==67)
    res|=1;
  QueInit(&head);
  head.tS=10;
  head.ona=0;
  AUProbeAdd(&head,10.125,60);
  AUProbeAdd(&head,10.25,61);
  AUProbeAdd(&head,10.25,62);
  AUProbeAdd(&head,10.75,63);
  AUProbeAdd(&head,11.375,64);
  SndShift(&head);
  if (head.next->tS==10.125 && head.last->tS==11.375)
    res|=2;
  SndShift(&head,0.25);
  "shift %d %X %d %d %X %X\n",head.next->ona,head.next->tS(I64),
      head.next->last==&head,head.next->next->ona,
      head.next->next->tS(I64),head.last->tS(I64);
  // Pow10I64 in the original lexer rounds 10.25 one ULP high.
  if (head.next->ona==61 && head.next->tS(I64)==0x4024000000000001 &&
      head.next->last==&head && head.next->next->ona==62 &&
      head.next->next->tS(I64)==0x4024000000000001 && head.last->tS==11.125)
    res|=4;
  SndShift(&head,-0.5);
  "back %X %X %X\n",head.tS(I64),head.next->tS(I64),head.last->tS(I64);
  if (head.tS==10 && head.next->tS(I64)==0x4025000000000001 &&
      head.last->tS==11.625)
    res|=8;
  n=AUWrite("B:/AUProbe",&head,origin(CDate *),10.25);
  a=FileRead("B:/AUProbe.AU",&size);
  if (a && n==45 && size==45) {
    for (i=0;i<5;i++)
      if (a[i].cdt(I64)!=origin+ticks[i] || a[i].ona!=notes[i])
        break;
    if (i==5)
      res|=16;
    for (i=0;i<5;i++)
      "AU event %X %d\n",a[i].cdt(I64)-origin,a[i].ona;
  }
  Free(a);
  SndShift(&head,100.0);
  if (head.next==&head && head.last==&head &&
      AUWrite("B:/AUEmpty.AU",&head,origin(CDate *),10)==9) {
    a=FileRead("B:/AUEmpty.AU",&size);
    if (a && size==9 && a->cdt(I64)==origin && a->ona==0)
      res|=32;
    Free(a);
  }
  AUProbeClear(&head);

  // Record through the actual speaker API and export the recorded events.
  old_mute=Mute(FALSE);
  ScrnCast(FALSE,TRUE);
  AUProbeClear(&scrncast.snd_head);
  ScrnCast(TRUE,TRUE);
  Snd(60);
  Sleep(25);
  Snd(60);
  Snd;
  ScrnCast(FALSE,TRUE);
  n=AUWrite("B:/AURecorded.AU",&scrncast.snd_head,
      scrncast.t0_now,scrncast.t0_tS);
  a=FileRead("B:/AURecorded.AU",&size);
  if (a && n==45 && size==45 && !scrncast.record &&
      scrncast.just_audio && !scrncast.ona &&
      a[0].cdt==scrncast.t0_now && a[0].ona==0 &&
      a[1].ona==0 && a[2].ona==60 && a[3].ona==60 && a[4].ona==0) {
    for (i=1;i<5;i++)
      if (a[i].cdt<a[i-1].cdt)
        break;
    if (i==5 && a[3].cdt>a[2].cdt)
      res|=64;
  }
  Free(a);
  AUProbeClear(&scrncast.snd_head);
  Mute(old_mute);
  "AU probe %X\n",res;
}

AUProbe;
