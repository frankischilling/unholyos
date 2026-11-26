I64 VaPtrTarget(I64 bias,...)
{
  I64 i,result=bias+100*argc;
  for (i=0;i<argc;i++)
    result+=argv[i];
  return result;
}

I64 VaPtrOnly(...)
{
  if (!argc)
    return 0;
  return 100*argc+argv[0];
}

I64 VaPtrCall(I64 (*cb)(I64,...))
{
  return cb(6,2,3);
}

U0 VariadicPtrProbe()
{
  I64 (*cb)(I64,...);
  I64 (*only)(...);
  I64 i,result=0;
  cb=&VaPtrTarget;
  only=&VaPtrOnly;
  "variadic pointer %d %d %d %d %d\n",cb(3),cb(2,4,5),
      only(),only(8),VaPtrCall(cb);
  for (i=0;i<100;i++)
    result+=cb(1,only(2),3);
  "variadic pointer cleanup %d\n",result;
}

VariadicPtrProbe;
