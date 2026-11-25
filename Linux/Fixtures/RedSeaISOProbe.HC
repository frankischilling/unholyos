U0 RedSeaISOProbe()
{
  I64 boot_blocks=RedSeaISO("B:/GuestBuild.iso","::/",
                           "::/0000Boot/0000Kernel.BIN.C");
  I64 data_blocks=RedSeaISO("B:/GuestData.iso","B:/Build");
  I64 missing=RedSeaISO("B:/GuestBuild.iso","::/","::/no-such-kernel.BIN.C");
  I64 outside=RedSeaISO("B:/GuestData.iso","B:/Build","::/0000Boot/0000Kernel.BIN.C");
  I64 recursive=RedSeaISO("B:/Build/Nested.iso","B:/Build");
  "ISO probe %d %d %d %d %d\n",boot_blocks>0,data_blocks>0,missing,outside,recursive;
}

RedSeaISOProbe;
