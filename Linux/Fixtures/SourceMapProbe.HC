// Exercises Terry A. Davis's source maps and address links from Adam/AHash.HC,
// Compiler/CHash.HC, Kernel/FunSeg.HC and Adam/DolDoc/DocLink.HC.
U8 *map_probe_addr, *map_probe_aot_addr;

I64 MapProbeRaw(I64 x)
{
  x=x*7;
  x=x+11;
  return x;
}

Bool MapProbeRawFile(U8 *tag,U32 start)
{
  U8 *text=MStrPrint("$$LK,\"MapProbeRaw  \",A=\"%s\",BI=1$$\n",tag);
  U32 data[11];
  I64 len=StrLen(text)+1;
  U8 *buf=MAlloc(len+sizeof(data));
  Bool okay;
  MemSet(data,0,sizeof(data));
  data[0]=1;
  data[2]=28;
  data[4]=40;
  data[5]=43;
  data[6]=start;
  data[8]=9;
  data[9]=17;
  data[10]=21;
  MemCpy(buf,text,len);
  MemCpy(buf+len,data,sizeof(data));
  okay=FileWrite("B:/MapProbeRaw.MAP.Z",buf,len+sizeof(data))>0;
  Free(buf);
  Free(text);
  return okay;
}

Bool MapProbeLink(U8 *expr,U8 *path,I64 line)
{
  U8 *name=NULL,*needle=NULL;
  I64 num=0,res=EdLinkCvt(expr,&name,&needle,&num);
  Bool okay=res==3 && name && !needle && num==line && !StrCmp(name,path);
  Free(name);
  Free(needle);
  return okay;
}

I64 MapProbeSynthetic()
{
  I64 checks=0;
  U8 *s;
  CHashFun *fun=HashFind("MapProbeRaw",Fs->hash_table,HTT_FUN);
  CHashGeneric *mod;
  CDbgInfo *map;
  if (!fun)
    return 0;
  map_probe_addr=&MapProbeRaw;
  if (fun->dbg_info && SrcLineNum(map_probe_addr)>=1)
    checks|=1;
  Free(fun->dbg_info);
  fun->dbg_info=NULL;
  mod=HashGenericAdd("/MapProbeRaw",HTT_MODULE,map_probe_addr-1-sizeof(CBinFile));
  if (!mod || !MapProbeRawFile("FL:B:/MapProbeCode.HC,9",1))
    return checks;
  MapFileLoad("B:/MapProbeRaw");
  map=fun->dbg_info;
  if (map && map->min_line==40 && map->max_line==43 &&
      map->body[0]==map_probe_addr && !map->body[1] &&
      map->body[2]==map_probe_addr+8 && map->body[4]==map_probe_addr+20)
    checks|=2;
  s=SrcFileName(map_probe_addr,,Fs);
  if (s && !StrCmp(s,"B:/MapProbeCode.HC") &&
      SrcLineNum(map_probe_addr)==40 && SrcLineNum(map_probe_addr+1,4)==42)
    checks|=4;
  Free(s);
  s=SrcEdLink(map_probe_addr+1,4,Fs);
  if (s && !StrCmp(s,"FL:B:/MapProbeCode.HC,42") &&
      !SrcFileName(map_probe_addr+1,1) && !SrcEdLink(map_probe_addr+20,64))
    checks|=8;
  Free(s);
  if (MapProbeLink("AD:map_probe_addr+1,4","B:/MapProbeCode.HC",42) &&
      MapProbeLink("AD:map_probe_addr","B:/MapProbeCode.HC",40) &&
      EdLinkCvt("AD:0")==-1)
    checks|=0x10;
  if (MapProbeRawFile("FL:::/Doc/Start.DD,7",2)) {
    MapFileLoad("B:/MapProbeRaw");
    if (map && map==fun->dbg_info && map->body[0]==map_probe_addr &&
        fun->src_link && fun->src_link[3]==blkdev.boot_drv_let)
      checks|=0x20;
  }
  HashRemDel(mod,Fs->hash_table);
  return checks;
}

I64 MapProbeCompile(Bool reference_headers)
{
  U8 *src="public I64 MapProbePad(I64 x) {return x;}\n"
          "public I64 MapProbeAOT(I64 x)\n"
          "{\n"
          "  x=x+3;\n"
          "  x=x*5;\n"
          "  return x;\n"
          "}\n";
  I64 checks=0;
  CHashSrcSym *sym;
  CDbgInfo *map;
  U8 *s,*prelude="",*project;
  if (reference_headers)
    prelude="#include \"::/Kernel/KernelA.HH\"\n";
  project=MStrPrint("%s#include \"B:/MapProbeCode.HC\"\n",prelude);
  Bool saved=FileWrite("B:/MapProbeCode.PRJ",project,StrLen(project))>0;
  Free(project);
  if (!saved || !FileWrite("B:/MapProbeCode.HC",src,StrLen(src)) ||
      Cmp("B:/MapProbeCode.PRJ","B:/MapProbeCode","B:/MapProbeCode.BIN.Z"))
    return 0;
  checks|=0x40;
  if (!Load("B:/MapProbeCode.BIN.Z",LDF_SILENT))
    return checks;
  MapFileLoad("B:/MapProbeCode");
  sym=HashFind("MapProbeAOT",Fs->hash_table,HTT_EXPORT_SYS_SYM);
  if (!sym || !sym->dbg_info)
    return checks;
  map=sym->dbg_info;
  map_probe_aot_addr=HashVal(sym);
  if (map->min_line==2 && map->max_line==8 &&
      map->body[0]==map_probe_aot_addr && map->body[2]>map->body[0] &&
      map->body[3]>map->body[2])
    checks|=0x80;
  if (SrcLineNum(map_probe_aot_addr)==2 &&
      SrcLineNum(map->body[2])==4 && SrcLineNum(map->body[3])==5 &&
      MapProbeLink("AD:map_probe_aot_addr,32","B:/MapProbeCode.HC",2))
    checks|=0x100;
  s=SrcEdLink(map->body[3]);
  if (s && !StrCmp(s,"FL:B:/MapProbeCode.HC,5"))
    checks|=0x200;
  Free(s);
  return checks;
}

Bool map_probe_child_ready, map_probe_child_stop;

U0 MapProbeChild(I64)
{
  ExeFile2("B:/MapProbeLocal.HC");
  map_probe_child_ready=TRUE;
  while (!map_probe_child_stop)
    Yield;
}

Bool MapProbeTask()
{
  U8 *src="I64 MapProbeLocal(I64 x) {return x+5;}\n",*addr,*link=NULL;
  CTask *child;
  CHash *local;
  I64 offset,i;
  Bool mode,okay=FALSE;
  if (!FileWrite("B:/MapProbeLocal.HC",src,StrLen(src)))
    return FALSE;
  map_probe_child_ready=map_probe_child_stop=FALSE;
  child=Spawn(&MapProbeChild,0,"Source map child",,Fs);
  if (!child)
    return FALSE;
  for (i=0;i<2048 && !map_probe_child_ready && TaskValidate(child);i++)
    Yield;
  if (map_probe_child_ready &&
      (local=HashFind("MapProbeLocal",child->hash_table,HTT_FUN))) {
    addr=HashVal(local);
    mode=DbgMode(FALSE);
    okay=addr && FunSegFind(addr,&offset)!=local;
    DbgMode(TRUE);
    if (okay && FunSegFind(addr,&offset)==local && !offset)
      link=SrcEdLink(addr);
    DbgMode(mode);
    okay=link && !StrCmp(link,"FL:B:/MapProbeLocal.HC,1");
    Free(link);
  }
  map_probe_child_stop=TRUE;
  return okay;
}

U0 MapProbeMain(Bool reference_headers=FALSE)
{
  CBlkDev *bd;
  I64 checks;
  if (!Let2Drv('B',FALSE)) {
    bd=BlkDevNextFreeSlot('B',BDT_RAM);
    bd->max_blk=511;
    if (BlkDevAdd(bd,,FALSE,TRUE)!=1)
      return;
  }
  checks=MapProbeCompile(reference_headers);
  checks|=MapProbeSynthetic;
  if (MapProbeTask)
    checks|=0x400;
  "Source map checks %X\n",checks;
}

U0 MapProbeOpen()
{
  "Address map editor %d\n",Ed("AD:map_probe_aot_addr,32");
}
