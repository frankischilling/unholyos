I64 keymap_probe_keys=0,keymap_probe_strings=0,keymap_probe_chord=0,
    keymap_probe_remote=0;

Bool KeyMapProbeFirst(I64 ch,I64 sc)
{
  if (!sc && ch=='~') {
    keymap_probe_keys++;
    return TRUE;
  }
  return FALSE;
}

Bool KeyMapProbeKey(I64 ch,I64 sc)
{
  if (sc&SCF_KEY_DESC && !(sc&SCF_SHIFT) && sc.u8[0]==SC_F1) {
    KeyDescSet("Cmd /HelpCust%d",42);
    return TRUE;
  }
  return FALSE;
}

Bool KeyMapProbeString(U8 *st)
{
  if (!StrCmp(st,"KeyMap intercepted text")) {
    keymap_probe_strings++;
    return TRUE;
  }
  return FALSE;
}

U0 KeyMapProbeChord(I64 sc)
{
  keymap_probe_chord=sc;
}

U0 KeyMapProbeReader(I64 unused)
{
  I64 sc;
  LBtr(&Fs->win_inhibit,WIf_SELF_KEY_DESC);
  GetKey(&sc,TRUE);
  if (sc==(SC_F1|SCF_KEY_DESC))
    keymap_probe_remote=1;
}

Bool KeyMapProbeTimestamp()
{
  I64 mouse,key,new_key;
  Bool result;
  PUSHFD
  CLI
  mouse=ms_hard.timestamp;
  key=kbd.timestamp;
  new_key=kbd.new_key_timestamp;
  kbd.timestamp=20;
  kbd.new_key_timestamp=10;
  ms_hard.timestamp=21;
  result=KbdMsEvtTime==21;
  ms_hard.timestamp=20;
  result=result && KbdMsEvtTime==10;
  ms_hard.timestamp=19;
  result=result && KbdMsEvtTime==10;
  ms_hard.timestamp=mouse;
  kbd.timestamp=key;
  kbd.new_key_timestamp=new_key;
  POPFD
  return result;
}

U0 KeyMapProbe()
{
  I64 i,bits=0,priority=0x20000000;
  CKeyDevEntry *entry=KeyDevAdd(&KeyMapProbeFirst,NULL,priority);
  CTask *reader;
  if (sizeof(CKeyDevEntry)==48 && sizeof(CKeyDevGlbls)==240 &&
      keydev.put_key_head.next==entry && entry->priority==priority)
    bits|=1;
  PutKey('~');
  PutChars('~~');
  if (keymap_probe_keys==3)
    bits|=2;
  entry=KeyDevAdd(&KeyMapProbeKey,&KeyMapProbeString,priority,TRUE);
  PutS("KeyMap intercepted text");
  if (keymap_probe_strings==1 && entry->flags&KDF_HAS_DESCS &&
      keydev.put_key_head.next==entry && entry->next->priority>priority)
    bits|=4;
  PutKey(0,SC_F1|SCF_KEY_DESC);
  if (!StrCmp(keydev.desc,"Cmd /HelpCust42") && keydev.hndlr) {
    PutKey(0,SC_F1|SCF_SHIFT|SCF_KEY_DESC);
    if (!StrCmp(keydev.desc,"Cmd /About"))
      bits|=8;
  }
  CtrlAltCBSet('Q',&KeyMapProbeChord,"Cmd /ChordCust");
  "Press the key map probe chord\n";
  for (i=0;i<500 && !keymap_probe_chord;i++)
    Sleep(10);
  if ((keymap_probe_chord&(SCF_CTRL|SCF_ALT))==(SCF_CTRL|SCF_ALT))
    bits|=16;
  KeyMap;
  if (DocFind(DocPut,,"HelpCust42") && DocFind(DocPut,,"ChordCust"))
    bits|=32;
  reader=Spawn(&KeyMapProbeReader);
  if (reader) {
    TaskWait(reader);
    KeyDescSet("");
    PostMsg(reader,MSG_KEY_DOWN,0,SC_F1|SCF_KEY_DESC);
    for (i=0;i<100 && !keymap_probe_remote;i++)
      Sleep(10);
    if (keymap_probe_remote && !StrCmp(keydev.desc,"Cmd /HelpCust42"))
      bits|=64;
    if (TaskValidate(reader))
      Kill(reader);
  }
  if (KeyMapProbeTimestamp)
    bits|=128;
  CtrlAltCBSet('Q',NULL);
  KeyDevRem(entry);
  DocBottom;
  "Key map probe %X\n",bits;
}
KeyMapProbe;
