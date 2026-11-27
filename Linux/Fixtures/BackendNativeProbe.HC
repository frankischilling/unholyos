// Inspect a few instruction signatures, not complete machine-code bodies.
I64 BackendNativeSign(I64 n)
{
  return SignI64(n);
}

I64 BackendNativeScan(I64 n)
{
  return Bsf(n)+Bsr(n);
}

U64 BackendNativeUnsigned(U64 a,U64 b)
{
  return MinU64(a,b)^MaxU64(a,b);
}

F64 BackendNativeFloat(F64 x)
{
  return Abs(Sqr(x));
}

F64 BackendNativeTrig(F64 x)
{
  return Tan(ATan(x));
}

I64 BackendNativeClock()
{
  return GetTSC;
}

I64 BackendNativeClobber(I64 noreg value)
{
  asm { MOV R9,0x1122334455667788 }
  return value+1;
}

I64 BackendNativeSpill(I64 noreg a,I64 noreg b)
{
  I64 noreg (*fn)(I64 value);
  fn=&BackendNativeClobber;
  return a+fn(b);
}

Bool BackendNativeMeta(U8 *name,I64 opcode,I64 argc,I64 raw_type)
{
  CHashFun *fn=HashFind(name,Fs->hash_table,HTT_FUN);
  CHashClass *cls;
  if (!fn || !(fn->flags&0x1000) || fn->exe_addr!=opcode ||
      fn->arg_cnt!=argc || !fn->return_class)
    return FALSE;
  cls=fn->return_class;
  if (raw_type==11 && StrCmp(cls->str,"U64"))
    return FALSE;
  while (cls->fwd_class) cls=cls->fwd_class;
  return cls->raw_type==raw_type;
}

Bool BackendNativeBytes(U8 *name,U8 *entry,I64 first,I64 second)
{
  CHashFun *fn=HashFind(name,Fs->hash_table,HTT_FUN);
  I64 size,i;
  if (!fn || !entry || fn->exe_addr!=entry)
    return FALSE;
  size=MSize(entry);
  if (size<2 || size>512)
    return FALSE;
  for (i=0;i+1<size;i++)
    if (entry[i]==first && entry[i+1]==second)
      return TRUE;
  return FALSE;
}

Bool BackendNativeLocked(U8 *p,I64 bit)
{
  return LBts(p,bit);
}

Bool BackendNativeImmediate(U8 *p)
{
  return Btr(p,31);
}

Bool BackendNativeImmediate64(U8 *p)
{
  return Btc(p,63);
}

U64 BackendNativeModulo(U64 *p,U64 d)
{
  return ModU64(p,d);
}

U0 BackendNativePortOut(I64 port,I64 val)
{
  OutU16(port,val);
  OutU16(0x80,val);
  OutU32(port,val);
  OutU8(0x80,val);
}

U0 BackendNativeProbe()
{
  I64 res=0;
  if (BackendNativeMeta("SignI64",0xAA,1,10) &&
      BackendNativeMeta("AbsI64",0xA9,1,10) &&
      BackendNativeMeta("Bsf",0x7E,1,10) &&
      BackendNativeMeta("Bsr",0x7F,1,10) &&
      BackendNativeMeta("MinU64",0xAC,2,11) &&
      BackendNativeMeta("MaxU64",0xAE,2,11) &&
      BackendNativeMeta("SqrU64",0xB1,1,11) &&
      BackendNativeMeta("Sqr",0xB2,1,14) &&
      BackendNativeMeta("Abs",0xB3,1,14) &&
      BackendNativeMeta("Tan",0xB7,1,14) &&
      BackendNativeMeta("ATan",0xB8,1,14) &&
      BackendNativeMeta("ToBool",0x1D,1,4) &&
      BackendNativeMeta("ToUpper",0x1E,1,10) &&
      BackendNativeMeta("GetTSC",0x76,0,10)) res|=1;
  if (BackendNativeBytes("BackendNativeClock",&BackendNativeClock,0x0F,0x31)
      && BackendNativeClock()>0) res|=2;
  if (BackendNativeBytes("BackendNativeScan",&BackendNativeScan,0x0F,0xBC) &&
      BackendNativeBytes("BackendNativeScan",&BackendNativeScan,0x0F,0xBD) &&
      BackendNativeScan(0x8000000000000010)==67) res|=4;
  if (BackendNativeBytes("BackendNativeFloat",&BackendNativeFloat,0xD9,0xE1) &&
      (BackendNativeBytes("BackendNativeFloat",&BackendNativeFloat,0xD8,0xC8) ||
       BackendNativeBytes("BackendNativeFloat",&BackendNativeFloat,0xDC,0xC8)) &&
      BackendNativeFloat(-2.5)(I64)==0x4019000000000000) res|=8;
  if (BackendNativeBytes("BackendNativeTrig",&BackendNativeTrig,0xD9,0xF2) &&
      BackendNativeBytes("BackendNativeTrig",&BackendNativeTrig,0xD9,0xF3) &&
      BackendNativeTrig(0.5)>0.49999999 &&
      BackendNativeTrig(0.5)<0.50000001) res|=16;
  if (BackendNativeBytes("BackendNativeUnsigned",&BackendNativeUnsigned,0x0F,0x47) &&
      BackendNativeBytes("BackendNativeUnsigned",&BackendNativeUnsigned,0x0F,0x42) &&
      BackendNativeUnsigned(0x8000000000000000,3)==0x8000000000000003)
    res|=32;
  if (BackendNativeSign(-9)==-1 && BackendNativeSign(0)==0 &&
      BackendNativeSign(9)==1) res|=64;
  if (BackendNativeSpill(100,7)==108 &&
      BackendNativeSpill(-20,-1)==-20 &&
      BackendNativeBytes("BackendNativeSpill",&BackendNativeSpill,0x41,0x51) &&
      BackendNativeBytes("BackendNativeSpill",&BackendNativeSpill,0x41,0x59))
    res|=128;
  if (BackendNativeMeta("Bt",0x77,2,4) &&
      BackendNativeMeta("Bts",0x78,2,4) &&
      BackendNativeMeta("Btr",0x79,2,4) &&
      BackendNativeMeta("Btc",0x7A,2,4) &&
      BackendNativeMeta("LBts",0x7B,2,4) &&
      BackendNativeMeta("LBtr",0x7C,2,4) &&
      BackendNativeMeta("LBtc",0x7D,2,4) &&
      BackendNativeMeta("ModU64",0xAF,2,11) &&
      BackendNativeMeta("SwapU8",0xA5,2,3) &&
      BackendNativeMeta("SwapU16",0xA6,2,3) &&
      BackendNativeMeta("SwapU32",0xA7,2,3) &&
      BackendNativeMeta("SwapI64",0xA8,2,3)) res|=256;
  if (BackendNativeBytes("BackendNativeLocked",&BackendNativeLocked,0xF0,0x48) &&
      BackendNativeBytes("BackendNativeLocked",&BackendNativeLocked,0x0F,0xAB) &&
      BackendNativeBytes("BackendNativeImmediate",&BackendNativeImmediate,0xBA,0x32) &&
      BackendNativeBytes("BackendNativeImmediate64",&BackendNativeImmediate64,0xBA,0x3A) &&
      BackendNativeBytes("BackendNativeModulo",&BackendNativeModulo,0xF7,0xF1))
    res|=512;
  if (BackendNativeBytes("BackendNativePortOut",&BackendNativePortOut,0x66,0xEF) &&
      BackendNativeBytes("BackendNativePortOut",&BackendNativePortOut,0x66,0xE7) &&
      BackendNativeBytes("BackendNativePortOut",&BackendNativePortOut,0xE6,0x80))
    res|=1024;
  "Backend native %X\n",res;
}

BackendNativeProbe;
