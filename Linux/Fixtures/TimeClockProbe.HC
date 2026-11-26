#include "::/Apps/TimeClock/Load.HC"

U0 TimeClockProbe()
{
  I64 res=0,size,attr;
  CDate saved_offset=local_time_offset,day;
  TimeEntry *header=MAlloc(sizeof(TimeEntry)),*e,*f;
  U8 *buf,*ptr;

  local_time_offset=0;
  day=Str2Date("02/26/24");
  TimeFileRead(header);
  if (sizeof(TimeEntry)==33 && header->next==header && header->last==header &&
      FileFind("~/TimeClock",,FUF_JUST_DIRS))
    res|=1;
  e=CAlloc(sizeof(TimeEntry));
  f=CAlloc(sizeof(TimeEntry));
  e->type=TET_PUNCH_IN;
  e->datetime=day;
  e->datetime.time=0x40000000;
  e->desc=StrNew("begin\nwork");
  f->type=TET_PUNCH_OUT;
  f->datetime=day;
  f->datetime.time=0x50000000;
  f->desc=StrNew("end");
  QueIns(e,header->last);
  QueIns(f,header->last);
  TimeFileWrite(header);
  TimeEntriesDel(header);

  buf=FileRead(TIME_FILENAME,&size,&attr);
  if (buf) {
    if (size==34 && (attr&RS_ATTR_COMPRESSED) && buf[0]==1 && buf[20]==2 &&
        !StrCmp(buf+9,"begin\nwork") && !StrCmp(buf+29,"end") && !buf[33])
      res|=2;
    ptr=buf+1;
    if (*ptr(CDate *)++==day+0x40000000) {
      ptr=buf+21;
      if (*ptr(CDate *)++==day+0x50000000)
        res|=4;
    }
    Free(buf);
  }

  header=MAlloc(sizeof(TimeEntry));
  TimeFileRead(header);
  e=header->next;
  f=header->last;
  if (e!=header && f!=header && e->next==f && f->last==e &&
      e->last==header && f->next==header && e->type==1 && f->type==2 &&
      !StrCmp(e->desc,"begin\nwork") && !StrCmp(f->desc,"end") &&
      e->datetime==day+0x40000000 && f->datetime==day+0x50000000)
    res|=8;
  TimeEntriesDel(header);
  if (!TimeRep)
    res|=16;
  PunchOut;
  local_time_offset=saved_offset;
  "TimeClock probe %02X\n",res;
}

TimeClockProbe;
