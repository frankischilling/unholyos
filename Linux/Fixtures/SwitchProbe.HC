I64 SwitchRange(I64 n)
{
  I64 sum=0;
  switch(n) {
    case 7...4: sum+=10;
    case: sum+=2; break;
    case -2...-1: sum=-1; break;
    default: sum=99;
  }
  return sum;
}

I64 SwitchNested(I64 n)
{
  I64 sum=0;
  switch[n] {
    case 0: sum=1; break;
    start:
      sum+=2;
      case 1: sum+=10; break;
      start:
        sum+=3;
        case 2: sum+=20; break;
        case 3: sum+=30; break;
      end:
      sum+=4; break;
    end:
    sum+=5; break;
  }
  return sum;
}

"switch %d %d %d %d %d %d\n",
  SwitchRange(4),SwitchRange(8),SwitchRange(-2),SwitchRange(0),
  SwitchNested(1),SwitchNested(2);
