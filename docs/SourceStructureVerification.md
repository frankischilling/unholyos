# Kernel source structure verification

The code change is commit
`0f93d3526a0cd7dbe4dcdb563fd2bb56ed0e54a9`, based on
`bbed28c487c67002f78fd30b91b7b0dacb8646ea`. Verification ran on
2026-11-26.

## Source review

The separate TempleOS checkout was clean at
`c26482bb6ad3f80106d28504ec5db3c6a360732c`.
Its `Kernel/KMain.HC` keeps kernel initialization together. UnholyOS now
keeps `SysGlblsInit`, `TimersInit`, and `KMainCpp` in that file's
corresponding C++ location. Their execution sequence is unchanged.

The 1,302-line verification helper region and 144-line reporting and
dispatch region moved to `Linux/BootProbe.cpp`. A comparison with the
baseline found identical source text after normalizing line endings.
The probe call remains after `WinMgrInit` and before user task setup.
The fixture bytes and host test expectations did not change.

Both ISO variants still include the verification component. This change
restores the separation between initialization and test code; the remaining
startup differences are listed in [Source layout](SourceLayout.md).

## Local build and VM checks

The build used GCC 13.3.0, NASM 2.16.01, and QEMU 8.2.2 in Ubuntu under WSL:

```sh
make -s -j4 test BUILD=build/gcc
```

The command exited successfully. The archive codec reproduced the pinned
streams byte for byte. All nine VM cases passed: the standard image on
PC at 512, 3,072, and 4,608 MiB; PC with HPET disabled; PC with the DVD on
the secondary IDE slave; Q35; and the startup-test image at all three RAM
sizes. Q35 retains the known AHCI boot-DVD mount limitation.

The tests exercised boot records, CPU and memory setup, interrupts, native
HolyC compilation, runtime modules, and guest-built ISO contents. Desktop
checks covered terminal movement and resizing, title-bar close, scrollbars,
pulldown menus, and the original graphics demos. The source refactor adds
no new claim of TempleOS behavior parity.

A clean build from the committed source used:

```sh
make -s -j4 all build/committed/unholyos-startup-test.iso BUILD=build/committed
```

Both ISOs and the kernel matched the tested artifacts byte for byte.
Their SHA-256 values were:

| Artifact under either build directory | SHA-256 |
| --- | --- |
| `unholyos.iso` | `2d87a34efce84b3be8b215d65964753e5a75d60811a8b9e7eccce25150a7408a` |
| `unholyos-startup-test.iso` | `3efb5105efdc2a531e21e291df6f5d786f2cd02675d35de2628fb9ec8d726911` |
| `0000Boot/0000Kernel.BIN.C` | `132be36e9e85d17a4d067ba52c0b9b210994ec619dfd68ba616faa2dcf7bd843` |

## Interactive boot

A fresh Windows QEMU 9.2.0 process used the tested standard ISO:

```sh
qemu-system-x86_64 -name UnholyOS-source-structure -machine pc -cpu qemu64 -m 512 -boot d -cdrom build/gcc/unholyos.iso -display gtk -serial file:build/interactive-serial.log -monitor none -qmp tcp:127.0.0.1:49888,server=on,wait=off -no-reboot -no-shutdown
```

The 640×480 display showed the kernel-entry banner and two tiled terminals
at `T:/Home>`. Serial output reported the mounted RedSea DVD, root
cluster `0x5a`, kernel cluster `0x5d`, and
`UnholyOS: kernel entered in 64-bit mode`. QMP reported the VM running.
The local screenshot is `build/interactive-screen.png`.

The rewrite and full compatibility milestones remain incomplete.
The later bootstrap and self-hosting work follows the order in
[Development milestones](Development.md).
