U0 AdamMemAOTRun()
{
  CBlkDev *bd;
  if (!Let2Drv('B',FALSE)) {
    bd=BlkDevNextFreeSlot('B',BDT_RAM);
    bd->max_blk=511;
    if (BlkDevAdd(bd,,FALSE,TRUE)!=1) return;
  }
  "Adam memory AOT compile %d\n",Cmp("::/AdamMemProbe.HC",,"B:/AdamMemProbe.BIN.Z");
  "Adam memory AOT load %d\n",!!Load("B:/AdamMemProbe.BIN.Z",LDF_SILENT);
}

AdamMemAOTRun;
