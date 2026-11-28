// Exercises Terry A. Davis's bit input in Adam/God/HolySpirit.HC.
I64 GodInputProbe()
{
  I64 bits=0,n;
  if (sizeof(CGodGlbls)==65 && offset(CGodGlbls.fifo)==32 &&
      offset(CGodGlbls.doodle_fifo)==56 && offset(CGodGlbls.doodle_done)==64)
    bits|=1;
  if (god.fifo && god.doodle_fifo && god.fifo!=god.doodle_fifo &&
      god.fifo->mask==16383 && god.doodle_fifo->mask==16383)
    bits|=2;

  FifoU8Flush(god.fifo);
  GodBitsIns(4,3);
  if (GodBits(4)==12 && !FifoU8Cnt(god.fifo))
    bits|=4;
  GodHexIns("A : 5-f,0");
  if (GodBits(16)==0xA5F0 && !FifoU8Cnt(god.fifo))
    bits|=8;
  GodHexIns(NULL);
  if (!GodBits(0) && !FifoU8Cnt(god.fifo))
    bits|=16;
  GodBitsIns(70,-1);
  if (GodBits(70)==-1 && !FifoU8Cnt(god.fifo))
    bits|=32;

  n=GodBits(1,"God timing input");
  if (0<=n<=1 && FifoU8Cnt(god.fifo)==GOD_GOOD_BITS-1)
    bits|=64;
  FifoU8Flush(god.fifo);
  return bits;
}

"God input %X\n",GodInputProbe();
