U0 RedSeaISOProbe()
{
  I64 boot_blocks=RedSeaISO("A:/GuestBuild.iso","::/",
                           "::/0000Boot/0000Kernel.BIN.C");
  I64 data_blocks=RedSeaISO("A:/GuestData.iso","B:/Build");
  I64 missing=RedSeaISO("A:/GuestBuild.iso","::/","::/no-such-kernel.BIN.C");
  I64 outside=RedSeaISO("A:/GuestData.iso","B:/Build","::/0000Boot/0000Kernel.BIN.C");
  I64 recursive=RedSeaISO("B:/Build/Nested.iso","B:/Build");
  "ISO probe %d %d %d %d %d\n",boot_blocks>0,data_blocks>0,missing,outside,recursive;
}

RedSeaISOProbe;
