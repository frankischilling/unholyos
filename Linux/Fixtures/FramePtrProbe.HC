// Exercise the frame entries from Terry A. Davis's Kernel/KHashB.HC.
U0 FramePtrProbeChild(U8 *opaque)
{
  I64 *bits=opaque;
  CTask *parent=Fs->parent_task;
  if (FramePtr("ProbeFrameParent")==41 &&
      FramePtrDel("ProbeFrameParent")==41 &&
      FramePtr("ProbeFrameParent",parent)==41)
    *bits|=1;
  if (FramePtrSet("ProbeFrameParent",43)==41 &&
      FramePtr("ProbeFrameParent",parent)==43)
    *bits|=2;
  if (FramePtrAdd("ProbeFrameParent",99) &&
      FramePtr("ProbeFrameParent")==99 &&
      FramePtr("ProbeFrameParent",parent)==43)
    *bits|=4;
  if (FramePtrDel("ProbeFrameParent")==99 && FramePtr("ProbeFrameParent")==43)
    *bits|=8;
}

U0 FramePtrProbe()
{
  I64 bits=0,child_bits=0;
  CHashGeneric *older,*newest,*generic;
  CTask *child;
  U8 name[32];
  if (!FramePtr("ProbeFrameMissing") &&
      !FramePtrSet("ProbeFrameMissing",7) && !FramePtrDel("ProbeFrameMissing"))
    bits|=1;

  older=FramePtrAdd("ProbeFrame");
  if (older && older->type==HTT_FRAME_PTR &&
      !older->user_data0 && !older->user_data1 && !older->user_data2)
    bits|=2;
  newest=FramePtrAdd("ProbeFrame",-17,Fs);
  if (newest && newest!=older && FramePtr("ProbeFrame")==-17 &&
      HashVal(newest)==-17)
    bits|=4;
  if (FramePtrSet("ProbeFrame",42,Fs)==-17 && FramePtr("ProbeFrame")==42)
    bits|=8;
  if (FramePtrDel("ProbeFrame",Fs)==42 && !FramePtr("ProbeFrame"))
    bits|=16;
  if (!FramePtrSet("ProbeFrame",-43) && FramePtrDel("ProbeFrame")==-43 &&
      !FramePtr("ProbeFrame"))
    bits|=32;

  generic=HashGenericAdd("ProbeFrameGeneric",HTT_FRAME_PTR,-9,0x123456780,71,Fs);
  if (generic && generic->user_data1==0x123456780 && generic->user_data2==71 &&
      FramePtrDel("ProbeFrameGeneric")==-9)
    bits|=64;
  if (FramePtrAdd("ProbeFrameParent",41)) {
    child=Spawn(&FramePtrProbeChild,&child_bits,"Frame probe",,Fs);
    while (TaskValidate(child))
      Yield;
    if (child_bits==15 && FramePtrDel("ProbeFrameParent")==43)
      bits|=128;
  }
  generic=HashGenericAdd("ProbeFrameGeneric",HTT_FRAME_PTR);
  if (generic && !generic->user_data0 && !generic->user_data1 &&
      !generic->user_data2) {
    FramePtrDel("ProbeFrameGeneric");
    bits|=256;
  }

  StrCpy(name,"ProbeFrameName");
  generic=FramePtrAdd(name,79);
  name[0]='-';
  if (generic && generic->str!=name && FramePtrDel("ProbeFrameName")==79)
    bits|=512;
  "Frame pointers %X\n",bits;
}

FramePtrProbe;
