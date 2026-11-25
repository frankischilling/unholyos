I64 cmpd_values[2]={8,13},cmpd_order=0;
I64 cmpd_left=14,cmpd_right=5;
I8 cmpd_byte=127;
U8 cmpd_unsigned=0xF0;
U64 cmpd_urhs=3;
F64 cmpd_float=7.5;
I64 *cmpd_ptr=&cmpd_values[0];

I64 CompoundIndex()
{
  cmpd_order=10*cmpd_order+1;
  return 0;
}

I64 CompoundRhs()
{
  cmpd_order=10*cmpd_order+2;
  cmpd_values[0]=30;
  return 3;
}

I64 CompoundLocal(I64 n)
{
  I64 total=-17;
  total/=4;
  total*=n;
  total%=8;
  return total;
}

"alias %d\n",cmpd_values[CompoundIndex()]+=CompoundRhs();
"order %d store %d\n",cmpd_order,cmpd_values[0];
"chain %d\n",cmpd_left+=cmpd_right*=3;
"math %d %d %d\n",cmpd_left,cmpd_right,CompoundLocal(3);
"byte %d\n",cmpd_byte+=2;
"byte store %d shift %d\n",cmpd_byte,cmpd_unsigned>>=2;
"bits %X\n",cmpd_unsigned|=0x80;
"bits %X\n",cmpd_unsigned&=0xB0;
"bits %X\n",cmpd_unsigned^=0x12;
"bits %X\n",cmpd_unsigned<<=2;
"bits store %X\n",cmpd_unsigned;
cmpd_ptr+=1;
"pointer %d\n",*cmpd_ptr;
"pointer %d\n",*(cmpd_ptr-=1);
"float %.2f\n",cmpd_float*=2;
"float %.2f\n",cmpd_float%=4.25;
"float %.2f\n",cmpd_float/=2;
"mixed %d\n",cmpd_left-=1.75;
cmpd_left=-16;
"signed shift %d\n",cmpd_left>>=cmpd_urhs;
cmpd_left=-17;
"signed divide %d\n",cmpd_left/=cmpd_urhs;
cmpd_left=1;
"float shift %d\n",cmpd_left<<=2.75;
cmpd_float=1.0;
"float bits %.2f\n",cmpd_float^=0x8000000000000000;
cmpd_byte=126;
"narrow runtime %d\n",cmpd_byte+=cmpd_urhs;
"narrow store %d\n",cmpd_byte;
