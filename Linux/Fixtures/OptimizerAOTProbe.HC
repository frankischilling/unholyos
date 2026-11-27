// Run on a fresh guest with OptimizerProbe.HC on the boot volume.
U0 OptimizerAOTRun()
{
  CBlkDev *bd;
  if (!Let2Drv('B',FALSE)) {
    bd=BlkDevNextFreeSlot('B',BDT_RAM);
    bd->max_blk=511;
    if (BlkDevAdd(bd,,FALSE,TRUE)!=1) return;
  }
  "Optimizer AOT compile %d\n",Cmp("::/OptimizerProbe.HC",,"B:/OptimizerProbe.BIN.Z");
  "Optimizer AOT load %d\n",!!Load("B:/OptimizerProbe.BIN.Z",LDF_SILENT);
}

OptimizerAOTRun;
