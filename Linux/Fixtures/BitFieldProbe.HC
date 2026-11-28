// Exercises Terry A. Davis's bit-field routines in Kernel/KUtils.HC,
// TempleOS c26482bb6ad3f80106d28504ec5db3c6a360732c.

U0 BitFieldProbe()
{
  U8 bits[24];
  I64 checks=0;
  MemSet(bits,0,sizeof(bits));

  BFieldOrU32(bits,7,0xFEDCBA98);
  if (BFieldExtU32(bits,7,32)==0xFEDCBA98)
    checks|=1;
  if (!BFieldExtU32(bits,7,0) && !BFieldExtU32(bits,7,64))
    checks|=2;
  BFieldOrU32(bits,9,1);
  if (BFieldExtU32(bits,7,32)==0xFEDCBA9C && !BFieldExtU32(bits,0,7))
    checks|=4;

  BFieldOrU32(bits,63,0x89ABCDEF);
  if (BFieldExtU32(bits,63,32)==0x89ABCDEF && !BFieldExtU32(bits,47,16))
    checks|=8;
  if (BFieldExtU32(bits,64,8)==0xF7 && BFieldExtU32(bits,63,65)==1)
    checks|=16;
  "Bit field checks %X\n",checks;
}

BitFieldProbe;
