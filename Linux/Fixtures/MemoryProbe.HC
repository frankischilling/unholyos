#define PROBE_COLS 3
I32 probe_grid[2][PROBE_COLS];
I32 *probe_row=&probe_grid[1][0];
I64 probe_order=0, probe_bits=0x0123456789ABCDEF;
U8 *probe_text="ABC";
I8 probe_byte;

I64 PtrMark(I64 n)
{
  probe_order=10*probe_order+n;
  return n;
}

I32 *PtrNext(I32 *p,I64 n)
{
  return p+n;
}

I8 PtrNarrow(I64 n)
{
  return n;
}

I8 PtrByte(I8 n)
{
  return n;
}

I64 PtrLocal(I64 n)
{
  I16 local[3];
  I16 *p=&local[0], **slot=&p;
  *p=-10;
  **slot=**slot+n;
  p[1]=8;
  p[2]=p[0]++ + ++p[1];
  p++;
  return 1000*sizeof(local)+100*p[-1]+10*p[0]+p[1];
}

probe_grid[1][PtrMark(1)]=PtrMark(2);
probe_row[0]=-280;
probe_row[2]=1100000;
"grid %d %d %d %d\n",probe_order,probe_grid[1][0],probe_grid[1][1],probe_grid[1][2];
"ptr %d %d %d %d\n",*(PtrNext(probe_row,2)-1),PtrNext(probe_row,2)-probe_row,
    sizeof(probe_grid),sizeof(I32 **);
"local %d narrow %d\n",PtrLocal(3),PtrNarrow(255);
"byte %d %d %d %d\n",PtrByte(255),PtrByte(87)+PtrByte(100),
    255(I8),(probe_byte=255);
probe_bits(U8)=0xF1;
"cast %016X %d %X %X\n",probe_bits,probe_bits(I8),
    (probe_bits)(U16),0x3FF0000000000000(F64)(I64);
"text %s %c byte %d\n",probe_text,probe_text[1],probe_byte;
