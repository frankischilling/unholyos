#assert 0
#assert 6*7==42

I64 AssertWarningValue(I64 value)
{
  return value*3;
}

"AW result %d\n",AssertWarningValue(14);
"AW DONE\n";
