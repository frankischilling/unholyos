U0 QueueProbe()
{
  CQue head,a,b,c;
  I64 res=0;
  U8 buf[16],*dir=__DIR__;

  QueInit(&head);
  if (sizeof(CQue)==16 && offset(CQue.last)==8 &&
      head.next==&head && head.last==&head)
    res|=1;
  QueIns(&a,&head);
  QueIns(&b,&a);
  if (head.next==&a && a.next==&b && b.next==&head &&
      head.last==&b && b.last==&a && a.last==&head)
    res|=2;
  QueInsRev(&c,&b);
  if (a.next==&c && c.last==&a && c.next==&b && b.last==&c)
    res|=4;
  QueRem(&c);
  if (a.next==&b && b.last==&a)
    res|=8;
  QueRem(&a);
  QueRem(&b);
  if (head.next==&head && head.last==&head)
    res|=16;
  StrCpy(buf,"queue");
  if (StrLen(buf)==5 && !StrCmp(buf,"queue") && !StrLen(""))
    res|=32;
  buf[1]=0xFF;
  if (StrLen(buf)==5 && StrLen(buf+5)==0)
    res|=64;
  if (dir[1]==':' && !StrCmp(dir+2,"/Doc"))
    res|=128;
  "Queue probe %02X\n",res;
}

QueueProbe;
