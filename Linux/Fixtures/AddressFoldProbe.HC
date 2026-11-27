// Addressing and value semantics shared with the pinned compiler.
I64 AddressFoldRead1(U8 *p,I64 i)
{
  return p[i];
}

I64 AddressFoldRead2(I16 *p,I64 i)
{
  return p[i];
}

I64 AddressFoldRead4(I32 *p,I64 i)
{
  return p[i];
}

I64 AddressFoldRead8(I64 *p,I64 i)
{
  return p[i];
}

class AddressFoldPair
{
  U8 pad,value;
};

class AddressFoldRecord
{
  U8 tag;
  AddressFoldPair pair;
  I8 sign;
  U32 word;
};

class AddressFoldThree
{
  U8 tag;
  I16 value;
};

class AddressFoldLarge
{
  U8 bytes[128];
  I64 value;
};

I64 AddressFoldMember(AddressFoldRecord *p,I64 i)
{
  return p[i].pair.value+p[i].sign;
}

U0 AddressFoldWrite(AddressFoldRecord *p,I64 i,I64 n)
{
  p[i].pair.value=n;
  p[i].sign=-7;
  p[i].word=0xFEDCBA98;
}

I64 address_fold_values[2],address_fold_order;

I64 AddressFoldIndex()
{
  address_fold_order=address_fold_order*10+1;
  return 0;
}

I64 AddressFoldRhs()
{
  address_fold_order=address_fold_order*10+2;
  address_fold_values[0]=30;
  return 3;
}

I64 AddressFoldMark(I64 n)
{
  address_fold_order=address_fold_order*10+n;
  return n;
}

I64 AddressFoldFrame(I64 n)
{
  I64 noreg value=n;
  I64 noreg old,pre;
  value=3*n+1;
  value+=4;
  old=value++;
  pre=--value;
  if (old==pre && value==pre) return value;
  return -1;
}

I64 AddressFoldImmediate(I64 n)
{
  return ((n+127)*8-128)^0x12345678;
}

I64 AddressFoldWide(I64 n)
{
  return n+0x100000000;
}

U0 AddressFoldProbe()
{
  U8 a[3];
  I16 b[3];
  I32 c[3];
  I64 d[3],res=0,alias,base,n,i;
  I64 samples[4],*sample,*ptrs[4],**pp,*result;
  Bool okay;
  AddressFoldRecord records[3];
  AddressFoldThree three[3],*t,*old_t,*pre_t;
  AddressFoldLarge large[3],*p,*old_p,*pre_p;

  a[0]=0xE1; a[1]=0x53; a[2]=0xF2;
  b[0]=-23456; b[1]=1234; b[2]=-3210;
  c[0]=-123456789; c[1]=123456; c[2]=-98765432;
  d[0]=-0x100000001; d[1]=0x123456789; d[2]=0x100000002;
  if (AddressFoldRead1(&a[1],-1)==0xE1 &&
      AddressFoldRead1(&a[1],1)==0xF2) res|=1;
  if (AddressFoldRead2(&b[1],-1)==-23456 &&
      AddressFoldRead2(&b[1],1)==-3210) res|=2;
  if (AddressFoldRead4(&c[1],-1)==-123456789 &&
      AddressFoldRead4(&c[1],1)==-98765432) res|=4;
  if (AddressFoldRead8(&d[1],-1)==-0x100000001 &&
      AddressFoldRead8(&d[1],1)==0x100000002) res|=8;

  MemSet(records,0xA5,sizeof(records));
  AddressFoldWrite(&records[1],-1,0xE3);
  if (sizeof(AddressFoldRecord)==8 &&
      AddressFoldMember(&records[1],-1)==220 &&
      records[0].word==0xFEDCBA98 &&
      records[0].tag==0xA5 && records[0].pair.pad==0xA5 &&
      records[1].word==0xA5A5A5A5) res|=16;

  address_fold_order=0;
  address_fold_values[0]=8;
  address_fold_values[1]=13;
  alias=address_fold_values[AddressFoldIndex()]+=AddressFoldRhs();
  if (alias==33 && address_fold_values[0]==33 &&
      address_fold_values[1]==13 && address_fold_order==12) res|=32;

  if (AddressFoldFrame(5)==20 && AddressFoldFrame(-7)==-16) res|=64;
  address_fold_order=0;
  if (1<AddressFoldMark(2)<4 && address_fold_order==2 &&
      !(AddressFoldRead8(&d[1],0)<AddressFoldRead8(&d[1],-1)<0))
    res|=128;

  three[0].value=-17;
  three[1].value=222;
  three[2].value=333;
  t=&three[1];
  old_t=t++;
  okay=sizeof(AddressFoldThree)==3 && old_t==&three[1] &&
      t==&three[2] && t[-2].value==-17;
  pre_t=--t;
  if (okay && pre_t==&three[1] && t==&three[1] &&
      pre_t->value==222) res|=256;

  large[0].value=111;
  large[1].value=222;
  large[2].value=333;
  p=&large[1];
  old_p=p++;
  okay=sizeof(AddressFoldLarge)==136 && old_p==&large[1] &&
      p==&large[2] && p[-2].value==111;
  pre_p=--p;
  if (okay && pre_p==&large[1] && p==&large[1] &&
      pre_p->value==222) res|=512;

  if (AddressFoldImmediate(5)==(928^0x12345678) &&
      AddressFoldWide(17)==0x100000011) res|=1024;

  // Width-correct stores must leave both adjacent bytes untouched.
  MemSet(a,0xA5,sizeof(a));
  a[1]=0xF1;
  b[1]=-32767;
  c[1]=-2147483647;
  if (a[0]==0xA5 && a[1]==0xF1 && a[2]==0xA5 &&
      b[0]==-23456 && b[1]==-32767 && b[2]==-3210 &&
      c[0]==-123456789 && c[1]==-2147483647 &&
      c[2]==-98765432) res|=2048;

  // PrsUnaryModifier's arrow changes the preceding instruction's class.
  // In the pinned compiler, a fused update of a class pointer consequently
  // uses a byte stride. Initialize every byte for deterministic unaligned
  // prefix loads; postfix loads still read the original member.
  MemSet(three,0,sizeof(three));
  three[0].value=0x1112; three[1].value=0x2324;
  three[2].value=0x3536;
  base=(&three[0])(I64);
  t=&three[1]; n=(++t)->value;
  okay=t(I64)-base==4 && n==0x23;
  t=&three[2]; n=(--t)->value;
  okay=okay && t(I64)-base==5 && n==0x3600;
  t=&three[1]; n=(t++)->value;
  okay=okay && t(I64)-base==4 && n==0x2324;
  t=&three[2]; n=(t--)->value;
  if (okay && t(I64)-base==5 && n==0x3536) res|=0x1000;

  MemSet(large,0,sizeof(large));
  large[0].value=0x11121314; large[1].value=0x23242526;
  large[2].value=0x35363738;
  base=(&large[0])(I64);
  p=&large[1]; n=(++p)->value;
  okay=p(I64)-base==137 && n==0x232425;
  p=&large[2]; n=(--p)->value;
  okay=okay && p(I64)-base==271 && n==0x3536373800;
  p=&large[1]; n=(p++)->value;
  okay=okay && p(I64)-base==137 && n==0x23242526;
  p=&large[2]; n=(p--)->value;
  if (okay && p(I64)-base==271 && n==0x35363738) res|=0x2000;

  // Direct dereference does not exhibit the arrow's byte-stride quirk.
  samples[0]=111; samples[1]=222; samples[2]=333; samples[3]=444;
  sample=&samples[1]; n=*(++sample);
  okay=sample==&samples[2] && n==333;
  sample=&samples[2]; n=*(--sample);
  okay=okay && sample==&samples[1] && n==222;
  sample=&samples[1]; n=*(sample++);
  okay=okay && sample==&samples[2] && n==222;
  sample=&samples[2]; n=*(sample--);
  if (okay && sample==&samples[1] && n==333) res|=0x4000;

  for (i=0;i<4;i++) ptrs[i]=&samples[i];
  pp=&ptrs[1]; result=*(++pp);
  okay=pp==&ptrs[2] && result==&samples[2];
  pp=&ptrs[2]; result=*(--pp);
  okay=okay && pp==&ptrs[1] && result==&samples[1];
  pp=&ptrs[1]; result=*(pp++);
  okay=okay && pp==&ptrs[2] && result==&samples[1];
  pp=&ptrs[2]; result=*(pp--);
  if (okay && pp==&ptrs[1] && result==&samples[2]) res|=0x8000;
  "Address fold %X\n",res;
}

AddressFoldProbe;
