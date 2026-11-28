// Sample the waveform functions from Terry A. Davis's Adam/ASnd.HC.
U64 WaveHash(F64 (*f)(F64 t,F64 period),U64 *times,I64 count,F64 period)
{
  I64 i;
  U64 hash=0xCBF29CE484222325;
  F64 sample;
  for (i=0;i<count;i++) {
    sample=f(times[i](F64),period);
    hash=(hash^sample(U64))*0x100000001B3;
  }
  return hash;
}

U64 wave_regular[24]={0xC008000000000000,0xC004000000000000,0xC002000000000000,
    0xC000000000000000,0xBFFC000000000000,0xBFF8000000000000,0xBFF4000000000000,
    0xBFF0000000000000,0xBFE8000000000000,0xBFE0000000000000,0xBFD0000000000000,
    0x8000000000000000,0,0x3FD0000000000000,0x3FE0000000000000,
    0x3FE8000000000000,0x3FF0000000000000,0x3FF4000000000000,0x3FF8000000000000,
    0x3FFC000000000000,0x4000000000000000,0x4002000000000000,0x4004000000000000,
    0x4008000000000000};
U64 wave_outside[7]={0xFEDCB9876543210F,0x7EDCB9876543210F,0x7FE0000000000001,
    0xFFE0000000000001,0x8000000000000001,1,0x0010000000000001};
U64 wave_special[6]={0x8000000000000000,0,0x7FF0000000000000,0xFFF0000000000000,
    0x7FF8000000000013,0xFFF8000000000027};
U0 WaveProbe(U8 *name,F64 (*f)(F64 t,F64 period))
{
  U64 a,b,c,d;
  a=WaveHash(f,wave_regular,sizeof(wave_regular)/8,1.0);
  b=WaveHash(f,wave_regular,sizeof(wave_regular)/8,-1.3);
  c=WaveHash(f,wave_outside,sizeof(wave_outside)/8,3.1);
  d=WaveHash(f,wave_outside,sizeof(wave_outside)/8,0x7FEFFFFFFFFFFFFF(F64));
  "W%s %016X %016X %016X %016X\n",name,a,b,c,d;
  a=WaveHash(f,wave_special,sizeof(wave_special)/8,1.0);
  b=WaveHash(f,wave_special,sizeof(wave_special)/8,0.0);
  c=WaveHash(f,wave_special,sizeof(wave_special)/8,0x8000000000000000(F64));
  d=WaveHash(f,wave_special,sizeof(wave_special)/8,0x7FF0000000000000(F64));
  "E%s %016X %016X %016X %016X\n",name,a,b,c,d;
}

WaveProbe("Saw",&Saw);
WaveProbe("FullSaw",&FullSaw);
WaveProbe("Caw",&Caw);
WaveProbe("FullCaw",&FullCaw);
WaveProbe("Tri",&Tri);
WaveProbe("FullTri",&FullTri);
U0 WaveDirect()
{
  F64 sample=Saw(0.5,2.0);
  "Wave direct %016X\n",sample(U64);
}
WaveDirect;
"Waveforms done\n";
