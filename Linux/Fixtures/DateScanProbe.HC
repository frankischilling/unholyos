U0 DateScanProbe()
{
  I64 res=0,i;
  CDate old_offset=local_time_offset,base=0,a,b,c,d,before,after,cdt;

  local_time_offset=0;
  base.date=YearStartDate(2024);
  a=Str2Date("SM(2/29/24)+3");
  b=Str2Date("EM(2/1/24)-1");
  c=Str2Date("SY(8/31/2024)+31");
  d=Str2Date("EY(2/29/24)-2");
  "DS boundary %X %X %X %X\n",a,b,c,d;
  if (a==base+(34<<32) && b==base+(58<<32) &&
      c==base+(31<<32) && d==base+(363<<32))
    res|=1;

  a=Str2Date(" s m ( 2 / 29 / 24 ) + 3 ");
  b=Str2Date("\te m(\n2/1/24 )-1\r");
  c=Str2Date("s y(8/31/2024)+31");
  d=Str2Date(" e y (2/29/24)-2 ");
  "DS space %X %X %X %X\n",a,b,c,d;
  if (a==base+(34<<32) && b==base+(58<<32) &&
      c==base+(31<<32) && d==base+(363<<32))
    res|=2;

  a=Str2Date("2/29/24+3");
  b=Str2Date("2/29/2024-60");
  c=Str2Date("SY(12/31/24)-1");
  d=Str2Date("EY(1/1/24)+1");
  "DS suffix %X %X %X %X\n",a,b,c,d;
  if (a==base+(62<<32) && b==base-(1<<32) &&
      c==base-(1<<32) && d==base+(366<<32))
    res|=4;

  local_time_offset=0x40000000;
  a=Str2Date("SM(2/29/24)+3");
  b=Str2Date("EM(2/1/24)-1");
  c=Str2Date("SY(8/31/2024)+31");
  d=Str2Date("EY(2/29/24)-2");
  "DS positive %X %X %X %X\n",a,b,c,d;
  if (a==base+(34<<32)-local_time_offset &&
      b==base+(58<<32)-local_time_offset &&
      c==base+(31<<32)-local_time_offset &&
      d==base+(363<<32)-local_time_offset && a.time==0xC0000000)
    res|=8;

  local_time_offset=-0x40000000;
  a=Str2Date("sm(2/29/24)+3");
  b=Str2Date("em(2/1/24)-1");
  c=Str2Date("sy(8/31/2024)+31");
  d=Str2Date("ey(2/29/24)-2");
  "DS negative %X %X %X %X\n",a,b,c,d;
  if (a==base+(34<<32)-local_time_offset &&
      b==base+(58<<32)-local_time_offset &&
      c==base+(31<<32)-local_time_offset &&
      d==base+(363<<32)-local_time_offset && a.time==0x40000000)
    res|=16;

  local_time_offset=0x40000000;
  for (i=0;i<3;i++) {
    before=Now+local_time_offset;
    cdt=Str2Date(" s m ( * - 7 ) + 3 ")+local_time_offset;
    after=Now+local_time_offset;
    if (before.date==after.date) {
      before.date=FirstDayOfMon(before.date-7)+3;
      after.date=FirstDayOfMon(after.date-7)+3;
      if (cdt>=before && cdt<=after)
        res|=32;
      break;
    }
  }
  local_time_offset=old_offset;
  "Date scan %X\n",res;
}

DateScanProbe;
