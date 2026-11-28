U0 AdamTaskAOTRun()
{
  CBlkDev *bd;
  if (!Let2Drv('B',FALSE)) {
    bd=BlkDevNextFreeSlot('B',BDT_RAM);
    bd->max_blk=511;
    if (BlkDevAdd(bd,,FALSE,TRUE)!=1) return;
  }
  "Adam tasks AOT compile %d\n",Cmp("::/AdamTaskProbe.HC",,"B:/AdamTaskProbe.BIN.Z");
  "Adam tasks AOT load %d\n",!!Load("B:/AdamTaskProbe.BIN.Z",LDF_SILENT);
}

AdamTaskAOTRun;
