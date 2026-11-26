U8 *clock_saved_home=blkdev.home_dir,*clock_saved_dir=DirCur;
blkdev.home_dir=StrNew("B:/");

#include "::/Apps/TimeClock/Load.HC"

U0 TimeClockInputProbe()
{
  I64 res=0,count=0;
  CDate before=Now,after;
  TimeEntry *header=MAlloc(sizeof(TimeEntry)),*e,*f,*p;

  "TimeClock input in\n";
  PunchIn;
  "TimeClock input out\n";
  PunchOut;
  after=Now;
  TimeFileRead(header);
  for (p=header->next;p!=header;p=p->next)
    count++;
  e=header->last->last;
  f=header->last;
  if (count==4 && e!=header && f!=header && e->type==TET_PUNCH_IN &&
      f->type==TET_PUNCH_OUT && e->next==f && f->last==e)
    res|=1;
  if (before<=e->datetime && e->datetime<=f->datetime && f->datetime<=after)
    res|=2;
  if (!StrCmp(e->desc,"start\nwork") && !StrCmp(f->desc,"done"))
    res|=4;
  "TimeClock input descriptions <%s> <%s>\n",e->desc,f->desc;
  TimeEntriesDel(header);
  Free(blkdev.home_dir);
  blkdev.home_dir=clock_saved_home;
  Cd(clock_saved_dir);
  Free(clock_saved_dir);
  "TimeClock input %02X\n",res;
}

TimeClockInputProbe;
