// Intrinsic declarations and memory operations from Terry A. Davis's
// Kernel/KernelB.HH, Compiler/BackB.HC and Kernel/KUtils.HC.
// TempleOS c26482bb6ad3f80106d28504ec5db3c6a360732c.
U64 bit_probe_field[5];

I64 BitProbeScanF(I64 val)
{
  return Bsf(val);
}

I64 BitProbeScanR(I64 val)
{
  return Bsr(val);
}

U0 BitProbe()
{
  I64 i,n=0;
  U64 val,checks=0;
  U8 *p=&bit_probe_field[1];
  for (i=0;i<64;i++) {
    val=1<<i;
    if (BitProbeScanF(val)==i && BitProbeScanR(val)==i &&
        BitProbeScanF(val|0x8000000000000000)==i &&
        BitProbeScanR(val|1)==i)
      n++;
  }
  "BIT scan %d %d %d\n",n,BitProbeScanF(0),BitProbeScanR(0);

  MemSet(bit_probe_field,0,sizeof(bit_probe_field));
  if (!Bts(p,0) && Bts(p,0) && Bt(p,0) && bit_probe_field[1]==1)
    checks|=1;
  if (Btc(p,0) && !Bt(p,0) && !Btr(p,0))
    checks|=2;
  if (!Bts(p,31) && !Bts(p,32) && !Bts(p,63) &&
      Bt(p,31) && Bt(p,32) && Bt(p,63) &&
      bit_probe_field[1]==0x8000000180000000)
    checks|=4;
  i=70;
  if (!Bts(p,i) && Bt(p,i) && bit_probe_field[2]==64)
    checks|=8;
  if (Btr(p,i) && !Bt(p,i) && !bit_probe_field[2])
    checks|=16;
  i=-1;
  if (!Bts(p,i) && Bt(p,i) &&
      bit_probe_field[0]==0x8000000000000000 && Btr(p,i))
    checks|=32;
  if (Btc(p,63) && !Bt(p,63) && bit_probe_field[1]==0x180000000)
    checks|=64;
  "BIT memory %X\n",checks;

  MemSet(bit_probe_field,0,sizeof(bit_probe_field));
  Bts(p,-1);
  "BIT immediate %016X %016X\n",bit_probe_field[0],bit_probe_field[1];

  MemSet(bit_probe_field,0,sizeof(bit_probe_field));
  checks=0;
  if (!LBts(p,7) && LBts(p,7) && LBtc(p,7) && !LBtr(p,7))
    checks|=1;
  i=129;
  if (!LBts(p,i) && LBtc(p,i) && !Bt(p,i))
    checks|=2;
  i=-1;
  if (!LBtc(p,i) && LBtr(p,i) && !bit_probe_field[0])
    checks|=4;
  if (!BEqu(p,4,TRUE) && BEqu(p,4,FALSE) && !Bt(p,4))
    checks|=8;
  if (!LBEqu(p,65,TRUE) && Bt(p,65) && LBEqu(p,65,FALSE) &&
      !Bt(p,65))
    checks|=16;
  "BIT locked %X\n",checks;

  MemSet(bit_probe_field,0,sizeof(bit_probe_field));
  BFieldOrU32(p,60,0xA5B6C7D8);
  BFieldOrU32(p,60,7);
  "BIT field %016X %016X %08X %X %X\n",bit_probe_field[1],
        bit_probe_field[2],BFieldExtU32(p,60,32),
        BFieldExtU32(p,64,8),BFieldExtU32(p,60,0);
  "BIT DONE\n";
}
BitProbe;
