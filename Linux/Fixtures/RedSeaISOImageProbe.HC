// Build a bootable image in a writable mounted image, using a mounted
// read-only source. Remount both levels to check persistence.
Bool ISOImageMatches(U8 *name,U8 *buf,I64 size)
{
  I64 n;
  U8 *data=FileRead(name,&n);
  Bool res=data && n==size && !MemCmp(data,buf,n);
  Free(data);
  return res;
}

U0 RedSeaISOImageProbe()
{
  CBlkDev *a=NULL,*b=NULL,*outer=NULL;
  CDrv *dv;
  CFile *f=NULL;
  CDirEntry de;
  U8 *saved=DirCur,*buf=MAlloc(2*BLK_SIZE),*out=MAlloc(DVD_BLK_SIZE),
      *requested,source=0,writer=0,reader=0,outer_read=0,
      src[STR_LEN],stage2[STR_LEN],image[STR_LEN],payload[STR_LEN],
      note[STR_LEN],after[STR_LEN];
  I64 i,blocks,root=0,lba=-1,sectors=0,shift=-1,c,res=0;

  if (!saved || !buf || !out) goto done;
  if (!Let2Drv('A',FALSE)) {
    a=BlkDevNextFreeSlot('A',BDT_RAM);
    a->max_blk=511;
    if (BlkDevAdd(a,,FALSE,TRUE)!=1) goto cleanup;
  }
  if (!Let2Drv('B',FALSE)) {
    b=BlkDevNextFreeSlot('B',BDT_RAM);
    b->max_blk=1023;
    if (BlkDevAdd(b,,FALSE,TRUE)!=1) goto cleanup;
  }
  for (i=0;i<2*BLK_SIZE;i++) buf[i]=i*31+(i>>8)+9;
  if (!DirMk("A:/FbImageSrc") || !DirMk("A:/FbImageSrc/Tree") ||
      !DirMk("A:/FbImageSrc/Tree/Nest") ||
      !FileWrite("A:/FbImageSrc/Tree/Payload.BIN.C",buf,777) ||
      !FileWrite("A:/FbImageSrc/Tree/Nest/Note.TXT",buf,333) ||
      RedSeaISO("A:/FbImageRead.ISO.C","A:/FbImageSrc")<=0) goto cleanup;
  source=MountFile("A:/FbImageRead.ISO.C");
  if (!source || !Let2Drv(source,FALSE) || DrvIsWritable(source)) goto cleanup;
  res|=1;

  writer=DrvNextFreeLet('Q');
  outer=BlkDevNextFreeSlot(writer,BDT_ISO_FILE_WRITE);
  requested=StrNew("B:/FbOuter.ISO.C");
  outer->file_dsk_name=requested;
  outer->max_blk=511;
  outer->drv_offset=88;
  if (BlkDevAdd(outer,,TRUE,TRUE)!=1) {
    if (outer->file_dsk_name==requested) {
      Free(requested);
      outer->file_dsk_name=NULL;
    }
    goto cleanup;
  }
  dv=Let2Drv(writer,FALSE);
  if (!dv) goto cleanup;
  RedSeaISO9660("B:/FbOuter.ISO.C",writer);
  f=FOpen("B:/FbOuter.ISO.C","rc");
  if (f && FSize(f)==512*BLK_SIZE && FBlkRead(f,out,64,1) &&
      out[0]==1 && !MemCmp(out+1,"CD001",5) &&
      *(out+152)(U32 *)==dv->root_clus) res|=2;
  FClose(f);
  f=NULL;

  StrPrint(src,"%C:/Tree",source);
  StrPrint(stage2,"%C:/Tree/Payload.BIN.C",source);
  StrPrint(image,"%C:/Inner.ISO.C",writer);
  StrPrint(after,"%C:/After.TXT",writer);
  if (RedSeaISO(image,src,stage2)<=0 ||
      !FileFind(image,&de,FUF_JUST_FILES)) goto cleanup;
  blocks=de.size/BLK_SIZE;
  if (de.size%DVD_BLK_SIZE==0 && blocks>88 &&
      Let2Drv(writer,FALSE)==dv && DrvIsWritable(writer)==outer &&
      DrvNextFreeLet(writer)==writer+1) res|=4;
  Free(de.full_name);
  f=FOpen(image,"rc");
  if (f && FBlkRead(f,out,88,1) && out[3]==0x88 &&
      *(out+8)(U64 *)==88 && *(out+16)(U64 *)==blocks-88 &&
      out[510]==0x55 && out[511]==0xAA) {
    root=*(out+24)(U64 *);
    if (FBlkRead(f,out,64,1) && *(out+80)(U32 *)==blocks/4 &&
        *(out+152)(U32 *)==root && FBlkRead(f,out,84,4)) {
      for (i=0;i+24<=DVD_BLK_SIZE;i++)
        if (!MemCmp(out+i,"UNHOLYOS-CD-LBA!",16)) {
          lba=*(out+i+16)(U32 *);
          sectors=*(out+i+20)(U16 *);
          shift=*(out+i+22)(U16 *);
          res|=8;
          break;
        }
    }
  }
  FClose(f);
  f=NULL;

  reader=MountFile(image);
  if (!reader || !Let2Drv(reader,FALSE)) goto cleanup;
  StrPrint(payload,"%C:/Payload.BIN.C",reader);
  StrPrint(note,"%C:/Nest/Note.TXT",reader);
  if (!DrvIsWritable(reader) && ISOImageMatches(payload,buf,777) &&
      ISOImageMatches(note,buf,333)) res|=16;
  if (FileFind(payload,&de,FUF_JUST_FILES)) {
    c=de.clus;
    if (de.size==777 && lba==c/4 && shift==c%4 &&
        sectors==1+(shift!=0) && lba+sectors<=blocks/4) res|=32;
    Free(de.full_name);
  }
  Cd(saved);
  Unmount(reader);
  reader=0;
  if (DrvIsWritable(writer)==outer && FileWrite(after,buf,333) &&
      ISOImageMatches(after,buf,333) && Let2Drv(source,FALSE)) res|=64;

  Unmount(source);
  source=0;
  Unmount(writer);
  writer=0;
  outer_read=MountFile("B:/FbOuter.ISO.C");
  if (!outer_read || !Let2Drv(outer_read,FALSE)) goto cleanup;
  StrPrint(image,"%C:/Inner.ISO.C",outer_read);
  StrPrint(after,"%C:/After.TXT",outer_read);
  if (ISOImageMatches(after,buf,333)) {
    reader=MountFile(image);
    if (reader && Let2Drv(reader,FALSE)) {
      StrPrint(payload,"%C:/Payload.BIN.C",reader);
      StrPrint(note,"%C:/Nest/Note.TXT",reader);
      if (ISOImageMatches(payload,buf,777) && ISOImageMatches(note,buf,333))
        res|=128;
    }
  }
cleanup:
  FClose(f);
  Cd(saved);
  if (reader && Let2Drv(reader,FALSE)) Unmount(reader);
  if (outer_read && Let2Drv(outer_read,FALSE)) Unmount(outer_read);
  if (writer && Let2Drv(writer,FALSE)) Unmount(writer);
  if (source && Let2Drv(source,FALSE)) Unmount(source);
  Del("B:/FbOuter.ISO.C",,,FALSE);
  Del("A:/FbImageRead.ISO.C",,,FALSE);
  DelTree("A:/FbImageSrc");
  if (a && a->bd_signature==BD_SIGNATURE_VAL) BlkDevDel(a);
  if (b && b->bd_signature==BD_SIGNATURE_VAL) BlkDevDel(b);
done:
  Free(saved);
  Free(buf);
  Free(out);
  "RedSea ISO image %02X\n",res;
}

RedSeaISOImageProbe;
