class LastClassItem
{
  I64 value dft_val 6/2;
  U8 name[4] format "Name:%s";
};

U8 *LastClassName(U8 *d,U8 *class_name=lastclass)
{
  return class_name;
}

U8 *LastClassDefault(U8 *d,I64 n=7,U8 *class_name=lastclass)
{
  return class_name;
}

U8 *LastClassString(U8 *d,U8 *class_name="default")
{
  return class_name;
}

U0 LastClassProbe()
{
  LastClassItem item,*ptr=&item;
  I64 n=7,res=0;
  U8 *s;
  if (!StrCmp(LastClassName(&item),"LastClassItem") &&
	!StrCmp(LastClassName(ptr),"LastClassItem") &&
	!StrCmp(LastClassName(&ptr),"LastClassItem"))
    res|=1;
  if (!StrCmp(LastClassName(&item,"explicit"),"explicit") &&
	!StrCmp(LastClassName(&item,),"LastClassItem"))
    res|=2;
  if (!StrCmp(LastClassDefault(&item),"LastClassItem") &&
	!StrCmp(LastClassString(&item),"default"))
    res|=4;
  "Lastclass names %s %s %s %s\n",LastClassName(&n),LastClassName(n),
	LastClassName(42),LastClassName(1.5);
  s=LastClassName(Fs);
  if (!StrCmp(s,"CTask"))
    res|=8;
  "Lastclass probe %02X\n",res;
}

LastClassProbe;
