U0 DateOffsetProbe()
{
  I64 res=0;
  CDate old_offset=local_time_offset,zero_date,cdt,before,after,utc;
  CDateStruct ds,back;

  local_time_offset=0;
  MemSet(&ds,0,sizeof(ds));
  ds.year=2024;
  ds.mon=2;
  ds.day_of_mon=29;
  zero_date=Struct2Date(&ds);
  if (sizeof(CDateStruct)==12 && offset(CDateStruct.year)==8 &&
      zero_date==Str2Date("2/29/24") && !zero_date.time &&
      zero_date.date==YearStartDate(2024)+59 &&
      mon_start_days1[2]==59 && mon_start_days2[2]==60)
    res|=1;

  ds.hour=1;
  ds.min=23;
  ds.sec=45;
  ds.sec100=67;
  ds.sec10000=89;
  cdt=Struct2Date(&ds);
  Date2Struct(&back,cdt);
  if (back.year==ds.year && back.mon==ds.mon &&
      back.day_of_mon==ds.day_of_mon && back.hour==ds.hour &&
      back.min==ds.min && back.sec==ds.sec && back.sec100==ds.sec100 &&
      back.sec10000==ds.sec10000 && back.day_of_week==DayOfWeek(cdt.date))
    res|=2;
  "date fraction %X\n",cdt.time;

  before=Now;
  local_time_offset=0x40000000;
  after=Now+local_time_offset;
  if (after>=before && after-before<0x100000)
    res|=4;

  cdt=Str2Date(" 2 / 29 / 24 ");
  if (cdt==zero_date-local_time_offset && cdt.date==zero_date.date-1 &&
      cdt.time==0xC0000000)
    res|=8;
  "date local %D %T utc %X\n",cdt,cdt,cdt;
  utc=cdt;

  if (Str2Date("SM(2/29/24)")==utc-28<<32 &&
      Str2Date("EM(2/1/24)")==utc &&
      Str2Date("SY(2/29/24)")==utc-59<<32 &&
      Str2Date("EY(2/29/24)")==utc+306<<32)
    res|=16;

  before=Now;
  cdt=Str2Date("*+2");
  after=Now;
  if (cdt>=before+2<<32 && cdt<=after+2<<32)
    res|=32;
  cdt=Now+local_time_offset;
  Date2Struct(&ds,cdt);
  cdt=Str2Date("2/1")+local_time_offset;
  if (cdt.date==YearStartDate(ds.year)+31 && !cdt.time)
    res|=64;

  if (Str2Date("2junk/3junk/24")==Str2Date("2/3/24") &&
      Str2Date("258/257/24")==Str2Date("2/1/24"))
    res|=128;
  local_time_offset=-0x40000000;
  cdt=Str2Date("2/29/24");
  if (cdt==zero_date+0x40000000 && cdt.time==0x40000000)
    res|=256;
  "date negative %D %T utc %X\n",cdt,cdt,cdt;
  local_time_offset=0;
  if (Str2Date("EM(12/1/24)")==Str2Date("12/31/24") &&
      Str2Date("EM(12/1/23)")==Str2Date("11/29/24"))
    res|=512;
  local_time_offset=old_offset;
  "Date offset %03X\n",res;
}

DateOffsetProbe;
