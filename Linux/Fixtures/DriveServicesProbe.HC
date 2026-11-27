// Drive and block device records from Terry A. Davis's
// Kernel/BlkDev/DskDrv.HC and DskBlkDev.HC.
U0 DriveServicesProbe()
{
  I64 res=0,i;
  CDrv *dvd=Let2Drv(':'),*ram,*slot,saved_slot;
  CBlkDev *bd=Let2BlkDev(':'),*rbd;
  U8 *saved=DirCur,*name,*model,*serial;
  U16 id[256],*old_id;
  U8 old_let;
  CDrv *old_map;

  if (!Let2Drv('B',FALSE)) {
    rbd=BlkDevNextFreeSlot('B',BDT_RAM);
    rbd->max_blk=2047;
    BlkDevAdd(rbd,,FALSE,TRUE);
  }
  ram=Let2Drv('B',FALSE);
  if (ram && dvd && bd && Let2Drv()==Fs->cur_dv &&
      Let2Let()==Drv2Let && Let2Let(':')==blkdev.boot_drv_let &&
      Let2Let('~')==*blkdev.home_dir && Let2Let('b')=='B')
    res|=1;
  if (Let2BlkDevType('A')==BDT_RAM && Let2BlkDevType('b')==BDT_RAM &&
      Let2BlkDevType('C')==BDT_ATA && Let2BlkDevType('L')==BDT_ATA &&
      Let2BlkDevType('M')==BDT_ISO_FILE_READ && Let2BlkDevType('P')==BDT_ISO_FILE_READ &&
      Let2BlkDevType('Q')==BDT_ISO_FILE_WRITE && Let2BlkDevType('S')==BDT_ISO_FILE_WRITE &&
      Let2BlkDevType('T')==BDT_ATAPI && Let2BlkDevType('Z')==BDT_ATAPI &&
      Let2BlkDevType('?')==BDT_NULL)
    res|=2;
  if (DrvChk(dvd)==dvd && BlkDevChk(bd)==bd &&
      !DrvChk(NULL,FALSE) && !BlkDevChk(NULL,FALSE) &&
      !Let2Drv('?',FALSE) && !Let2BlkDev('?',FALSE) &&
      dvd->dv_signature==DRV_SIGNATURE_VAL && bd->bd_signature==BD_SIGNATURE_VAL &&
      bd->type==BDT_ATAPI && dvd->bd==bd)
    res|=4;
  if (ram && DrvIsWritable('B')==ram->bd && !DrvIsWritable(':') &&
      !DrvIsWritable('?') && !DrvSerialNum('B') && !DrvModelNum('B'))
    res|=8;
  if (DrvTextAttrGet('A')==0xB4 && DrvTextAttrGet('B')==0xB0 &&
      DrvTextAttrGet('C')==0xF1 && DrvTextAttrGet('M')==0xA4 &&
      DrvTextAttrGet('Q')==0xC0 && DrvTextAttrGet('T')==0x90 &&
      DrvTextAttrGet('?')==WHITE)
    res|=16;
  if (!Let2Drv('S',FALSE) && DrvNextFreeLet('S')=='S') {
    slot=&blkdev.drvs['S'-'A'];
    MemCpy(&saved_slot,slot,sizeof(CDrv));
    if (DrvMakeFreeSlot('S')==slot && slot->drv_let=='S' &&
        !slot->dv_signature && !slot->bd)
      res|=32;
    MemCpy(slot,&saved_slot,sizeof(CDrv));
    old_map=blkdev.let_to_drv['S'-'A'];
    old_let=Drv2Let(dvd);
    if (DrvMap('S',dvd) && Let2Drv('S')==dvd && Drv2Let(dvd)=='S' &&
        !DrvMap('?',dvd))
      res|=64;
    DrvMap(old_let,dvd);
    blkdev.let_to_drv['S'-'A']=old_map;
  }
  if (ram && Drv('B') && Drv2Let=='B') {
    name=DirCur;
    if (name && !StrCmp(name,"B:/"))
      res|=128;
    Free(name);
  }
  if (Cd(saved))
    res|=256;
  Free(saved);
  if (ram) {
    rbd=ram->bd;
    old_id=rbd->dev_id_record;
    for (i=0;i<256;i++)
      id[i]=0x2020;
    id[11]=0x534E;
    id[12]=0x3132;
    id[27]=0x4D4F;
    id[28]=0x4445;
    id[29]=0x4C20;
    rbd->dev_id_record=id;
    serial=DrvSerialNum('B');
    model=DrvModelNum('B');
    if (serial && model && !StrCmp(serial,"SN12") && !StrCmp(model,"MODEL"))
      res|=512;
    Free(serial);
    Free(model);
    rbd->dev_id_record=old_id;
  }
  model=DrvModelNum(':');
  serial=DrvSerialNum(':');
  if (!model && !serial && !bd->dev_id_record &&
      sizeof(CBlkDev)==124 && sizeof(CDrv)==160 &&
      offset(CBlkDev.dev_id_record)==68 && offset(CDrv.bd)==104)
    res|=1024;
  Free(model);
  Free(serial);
  "Drive services %X\n",res;
}

DriveServicesProbe;
