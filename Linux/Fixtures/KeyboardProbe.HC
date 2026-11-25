U0 KeyboardProbe()
{
  I64 ch,sc;
  "kbd map %X %X %d %d %d\n",Char2ScanCode('A'),Char2ScanCode(1),
      ScanCode2Char(SCF_CAPS|2),ScanCode2Char(SCF_E0_PREFIX|0x48),
      sizeof(CKbdStateGlbls);
  "kbd empty %d\n",ScanKey(&ch,&sc);
  "kbd wait key\n";
  ch=GetKey(&sc);
  "kbd key %d %016X\n",ch,sc;
  "kbd wait char\n";
  ch=GetChar(&sc,FALSE);
  "kbd char %d %016X\n",ch,sc;
  GetMsg(,,1<<MSG_KEY_UP);
  GetMsg(,,1<<MSG_KEY_UP);
  "kbd irq %d\n",kbd.irqs_working;
}

KeyboardProbe;
