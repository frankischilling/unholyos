// Run on a fresh guest with BackendProbe.HC on the boot volume.
U0 BackendAOTRun()
{
  CBlkDev *bd;
  if (!Let2Drv('B',FALSE)) {
    bd=BlkDevNextFreeSlot('B',BDT_RAM);
    bd->max_blk=511;
    if (BlkDevAdd(bd,,FALSE,TRUE)!=1) return;
  }
  "Backend AOT compile %d\n",Cmp("::/BackendProbe.HC",,"B:/BackendProbe.BIN.Z");
  "Backend AOT load %d\n",!!Load("B:/BackendProbe.BIN.Z",LDF_SILENT);
}

BackendAOTRun;
