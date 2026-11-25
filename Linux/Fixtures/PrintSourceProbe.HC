I64 print_log=0;

I64 PrintStamp(I64 n)
{
  print_log=10*print_log+n;
  return n;
}

U0 PrintBody(I64 enabled)
{
  "join" " %s %d %d %6.2f\n","a" "b",PrintStamp(1),PrintStamp(2),-2.5;
  if (enabled)
    Print("paren %d %s %%\n",7,"kept");
  'raw ';
  '' ('A'+1);
  PutChars('\n');
  PutS("100% raw\n");
}

PrintBody(1);
"order %d\n",print_log;
