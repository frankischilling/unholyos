// Run on a fresh guest with AddressFoldProbe.HC on the boot volume.
U0 AddressFoldAOTRun()
{
  CBlkDev *bd;
  if (!Let2Drv('B',FALSE)) {
    bd=BlkDevNextFreeSlot('B',BDT_RAM);
    bd->max_blk=511;
    if (BlkDevAdd(bd,,FALSE,TRUE)!=1) return;
  }
  "Address fold AOT compile %d\n",Cmp("::/AddressFoldProbe.HC",,
      "B:/AddressFoldProbe.BIN.Z");
  "Address fold AOT load %d\n",!!Load("B:/AddressFoldProbe.BIN.Z",LDF_SILENT);
}

AddressFoldAOTRun;
