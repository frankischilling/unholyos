// Run on a fresh guest with UAsmProbe.HC on the boot volume.
U0 UAsmAOTRun()
{
  CBlkDev *bd;
  if (!Let2Drv('B',FALSE)) {
    bd=BlkDevNextFreeSlot('B',BDT_RAM);
    bd->max_blk=511;
    if (BlkDevAdd(bd,,FALSE,TRUE)!=1) return;
  }
  "UAsm AOT compile %d\n",Cmp("::/UAsmProbe.HC",,"B:/UAsmProbe.BIN.Z");
  "UAsm AOT load %d\n",!!Load("B:/UAsmProbe.BIN.Z",LDF_SILENT);
}

UAsmAOTRun;
