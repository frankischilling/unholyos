U0 DocTreeAOTProbe()
{
  CBlkDev *bd;
  if (!Let2Drv('B',FALSE)) {
    bd=BlkDevNextFreeSlot('B',BDT_RAM);
    bd->max_blk=1023;
    if (BlkDevAdd(bd,,FALSE,TRUE)!=1) return;
  }
  if (!Cmp("::/DocTreeProbe.HC",,"B:/DocTreeProbe.BIN.Z") &&
      Load("B:/DocTreeProbe.BIN.Z",LDF_SILENT))
    "Tree AOT done\n";
}

DocTreeAOTProbe;
