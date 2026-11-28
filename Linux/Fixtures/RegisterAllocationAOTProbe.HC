// Run on a fresh guest before loading RegisterAllocationProbe.HC.
U0 RegisterAllocationAOTRun()
{
  CBlkDev *bd;
  if (!Let2Drv('B',FALSE)) {
    bd=BlkDevNextFreeSlot('B',BDT_RAM);
    bd->max_blk=511;
    if (BlkDevAdd(bd,,FALSE,TRUE)!=1) return;
  }
  "Register AOT compile %d\n",Cmp("::/RegisterAllocationProbe.HC",,"B:/RegisterAllocationProbe.BIN.Z");
  "Register AOT load %d\n",!!Load("B:/RegisterAllocationProbe.BIN.Z",LDF_SILENT);
}

RegisterAllocationAOTRun;
