U0 Fat32RejectProbe()
{
  U8 *buf;
  CFile *f;
  I64 res=0;
  if (!IsDir("C:/")) {
    "FAT32 invalid geometry rejected\n";
    return;
  }
  if (!(buf=FileRead("C:/LOOP.BIN"))) res|=1;
  Free(buf);
  if (!(buf=FileRead("C:/BAD.BIN"))) res|=2;
  Free(buf);
  if (!(buf=FileRead("C:/SHORT.BIN"))) res|=4;
  Free(buf);
  f=FOpen("C:/FRAG.BIN","w",2);
  if (!f) res|=8;
  FClose(f);
  if (!FileWrite("C:/NEW.BIN","x",1) && !FileFind("C:/NEW.BIN") &&
	!Del("C:/FRAG.BIN") && FileFind("C:/FRAG.BIN") &&
	!DirMk("C:/NEW")) res|=16;
  "FAT32 reject probe %02X\n",res;
}

Fat32RejectProbe;
