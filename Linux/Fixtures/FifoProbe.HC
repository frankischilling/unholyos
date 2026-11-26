I64 FifoProbe()
{
  CFifoU8 *b=FifoU8New(4);
  CFifoI64 *q=FifoI64New(4,Fs);
  U8 v=0xAA;
  I64 n=0x123456789ABCDEF, res=0, i, flags=GetRFlags;
  Bool okay, irq_ok=TRUE;

  if (sizeof(CFifoU8)==32 && sizeof(CFifoI64)==32 &&
      offset(CFifoU8.buf)==0 && offset(CFifoI64.buf)==0 &&
      offset(CFifoU8.mask)==8 && offset(CFifoI64.mask)==8 &&
      offset(CFifoU8.in_ptr)==16 && offset(CFifoI64.in_ptr)==16 &&
      offset(CFifoU8.out_ptr)==24 && offset(CFifoI64.out_ptr)==24 &&
      b->buf && q->buf && b->mask==3 && q->mask==3 &&
      !b->in_ptr && !b->out_ptr && !q->in_ptr && !q->out_ptr)
    res|=1;
  if (!FifoU8Rem(b,&v) && !FifoU8Peek(b,&v) && v==0xAA &&
      !FifoU8Rem(b,NULL) && !FifoU8Peek(b,NULL) && !FifoU8Cnt(b) &&
      !FifoI64Rem(q,&n) && !FifoI64Peek(q,&n) && n==0x123456789ABCDEF &&
      !FifoI64Rem(q,NULL) && !FifoI64Peek(q,NULL) && !FifoI64Cnt(q))
    res|=2;
  if (FifoU8Ins(b,0xFE) && FifoU8Ins(b,0x80) && FifoU8Ins(b,0) &&
      !FifoU8Ins(b,42) && FifoU8Cnt(b)==3 &&
      b->in_ptr==3 && b->out_ptr==0 &&
      b->buf[0]==0xFE && b->buf[1]==0x80 && b->buf[2]==0 &&
      FifoU8Peek(b,&v) && v==0xFE && FifoU8Cnt(b)==3)
    res|=4;
  if (FifoU8Rem(b,&v) && v==0xFE && FifoU8Ins(b,42) &&
      b->in_ptr==0 && b->out_ptr==1 && FifoU8Cnt(b)==3 &&
      FifoU8Rem(b,&v) && v==0x80 && FifoU8Rem(b,&v) && v==0 &&
      FifoU8Rem(b,&v) && v==42 && !FifoU8Cnt(b))
    res|=8;
  if (FifoI64Ins(q,-0x123456789ABCDEF) &&
      FifoI64Ins(q,0x7FFFFFFFFFFFFFFF) && FifoI64Ins(q,0) &&
      !FifoI64Ins(q,-1) && FifoI64Cnt(q)==3 &&
      q->buf[0]==-0x123456789ABCDEF && q->buf[1]==0x7FFFFFFFFFFFFFFF &&
      FifoI64Peek(q,&n) && n==-0x123456789ABCDEF && FifoI64Cnt(q)==3)
    res|=16;
  if (FifoI64Rem(q,&n) && n==-0x123456789ABCDEF && FifoI64Ins(q,-1) &&
      q->in_ptr==0 && q->out_ptr==1 && FifoI64Cnt(q)==3 &&
      FifoI64Rem(q,&n) && n==0x7FFFFFFFFFFFFFFF &&
      FifoI64Rem(q,&n) && n==0 && FifoI64Rem(q,&n) && n==-1 &&
      !FifoI64Cnt(q))
    res|=32;
  okay=FifoU8Ins(b,11) && FifoI64Ins(q,-11);
  FifoU8Flush(b);
  FifoI64Flush(q);
  if (okay && b->out_ptr==1 && q->out_ptr==1 &&
      !FifoU8Cnt(b) && !FifoI64Cnt(q))
    res|=64;
  for (i=0;i<2;i++) {
    if (i) {
      STI
    } else {
      CLI
    }
    okay=FifoU8Ins(b,1);
    irq_ok &= okay && (GetRFlags&0x200)==i*0x200;
    okay=FifoU8Peek(b,&v);
    irq_ok &= okay && (GetRFlags&0x200)==i*0x200;
    n=FifoU8Cnt(b);
    irq_ok &= n==1 && (GetRFlags&0x200)==i*0x200;
    okay=FifoU8Rem(b,&v);
    irq_ok &= okay && (GetRFlags&0x200)==i*0x200;
    FifoU8Flush(b);
    irq_ok &= (GetRFlags&0x200)==i*0x200;
    okay=FifoU8Rem(b,&v);
    irq_ok &= !okay && (GetRFlags&0x200)==i*0x200;
    okay=FifoI64Ins(q,-1);
    irq_ok &= okay && (GetRFlags&0x200)==i*0x200;
    okay=FifoI64Peek(q,&n);
    irq_ok &= okay && (GetRFlags&0x200)==i*0x200;
    n=FifoI64Cnt(q);
    irq_ok &= n==1 && (GetRFlags&0x200)==i*0x200;
    okay=FifoI64Rem(q,&n);
    irq_ok &= okay && (GetRFlags&0x200)==i*0x200;
    FifoI64Flush(q);
    irq_ok &= (GetRFlags&0x200)==i*0x200;
    okay=FifoI64Peek(q,&n);
    irq_ok &= !okay && (GetRFlags&0x200)==i*0x200;
  }
  if (!(flags&0x200)) {
    CLI
  }
  if (irq_ok)
    res|=128;
  FifoU8Del(b);
  FifoI64Del(q);

  // Non-power-of-two sizes retain the reference mask and count quirk.
  b=FifoU8New(6,Fs);
  q=FifoI64New(6);
  if (FifoU8Ins(b,11) && FifoU8Rem(b,&v) && FifoU8Ins(b,12) &&
      b->mask==5 && b->in_ptr==0 && b->out_ptr==1 && FifoU8Cnt(b)==5 &&
      FifoI64Ins(q,-11) && FifoI64Rem(q,&n) && FifoI64Ins(q,-12) &&
      q->mask==5 && q->in_ptr==0 && q->out_ptr==1 && FifoI64Cnt(q)==5)
    res|=256;
  FifoU8Del(b);
  FifoI64Del(q);

  return res;
}

"FIFO probe %X\n",FifoProbe;
