U0 InputTrainingProbe()
{
  I64 ch,sc,bits=0;
  U64 field[2];
  U8 *st=Char2KeyName(CH_SHIFT_ESC);
  field[0]=0;
  field[1]=0;
  if (!StrCmp(st,"SHIFT_ESC"))
    bits|=1;
  Free(st);
  if (!LBEqu(field,67,TRUE) && field[1]==8 &&
      LBEqu(field,67,FALSE) && !field[0] && !field[1])
    bits|=2;
  InPrint(,"Training output %d\n",27);
  "Training string\n";
  InGetStr("fg");
  "\nTraining key\n";
  InGetKey(SC_F1);
  ch=GetKey(&sc);
  if (!ch && (sc&255)==SC_F1)
    bits|=4;
  "Training choice\n";
  ch=InGetChar('a','b','c');
  if (ch=='b' && GetKey=='b')
    bits|=8;
  "Training until key\n";
  InUntilKey(SC_CURSOR_RIGHT,255);
  ch=GetKey(&sc);
  if (!ch && sc.u8[0]==SC_CURSOR_RIGHT)
    bits|=16;
  "Training until char\n";
  ch=InUntilChar('d','e');
  if (ch=='d' && GetKey=='d')
    bits|=32;
  "Training view\n";
  if (InView)
    bits|=64;
  InSetMs(0,201,163);
  if (ms.pos.x==201 && ms.pos.y==163)
    bits|=128;
  "Training result %X\n",bits;
}
InputTrainingProbe;
