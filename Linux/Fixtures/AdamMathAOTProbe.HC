U0 AdamMathAOTRun()
{
  CBlkDev *bd;
  if (!Let2Drv('B',FALSE)) {
    bd=BlkDevNextFreeSlot('B',BDT_RAM);
    bd->max_blk=511;
    if (BlkDevAdd(bd,,FALSE,TRUE)!=1) return;
  }
  "Adam math AOT compile %d\n",Cmp("::/AdamMathProbe.HC",,"B:/AdamMathProbe.BIN.Z");
  "Adam math AOT load %d\n",!!Load("B:/AdamMathProbe.BIN.Z",LDF_SILENT);
}

AdamMathAOTRun;
