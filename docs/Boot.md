# Booting from DVD

Build on Linux with GNU Make, a C++20 compiler with an x86-64 ELF target, GNU binutils, and NASM. Use QEMU to run the disc. On Windows, run the build in Ubuntu under WSL and use native QEMU to open the VM window.

```sh
make -j2 test
qemu-system-x86_64 -machine pc -cpu qemu64 -m 512 -boot d -cdrom build/unholyos.iso -serial stdio
```

The C++20 host tools build the RedSea image and check it against the kernel built by Make. The test starts fresh PC VMs with 512 MiB, 3 GiB, and 4.5 GiB of RAM. It checks the RedSea boot records, the BIOS handoff, the long-mode registers, the code and data pools, the bootstrap CPU record, Adam's heap and stack, the 32-bit device ranges, the 4 GiB uncached alias, the second-stage interrupt gates, PIT ticks, and the 640×480 framebuffer. It checks PCI reads and boot clock setup on PC, Q35, and PC with HPET disabled. On PC, the guest mounts the DVD, locates `/0000Boot/0000Kernel.BIN.C`, and reports the file's checksum. The test checks the checksum against the build and runs with the DVD attached to either unit on the secondary IDE channel. When Q35 boots from its default AHCI controller, the kernel reports that it could not mount the DVD. With Clang installed, select it with `Makefile` overrides `CXX=clang++ HOSTCXX=clang++`. The window displays `UnholyOS kernel entered in 64-bit mode`.

`make test` also builds `build/unholyos-startup-test.iso` with the original `StartOS.HC.Z`, compiler fixtures, and eleven TOSB module fixtures. The guest asks for `/StartOS.HC` through `FileRead` and reports the length and checksum of the expanded bytes. At 512 MiB, 3 GiB, and 4.5 GiB, the test scans the lexer fixtures, compiles and runs scalar HolyC expressions and functions, compiles and runs the original `Demo/Print.HC` from source, and runs the compiled modules from the RedSea DVD. `build/unholyos.iso` is the standard boot image.

The [kernel source structure verification](SourceStructureVerification.md)
records the build and boot checks for the extraction of guest probes from
`Kernel/KMain.cpp` into `Linux/BootProbe.cpp`.

The freestanding kernel uses `-Os`; the host tools retain `-O2`. The DVD
loader relocates to physical address `0x96600`, as in the pinned TempleOS
loader. The kernel and BSS must fit below that address. On GCC 13.3.0 the
file-stream baseline's kernel shrank from 522,512 to 407,568 bytes without
changing the loader layout. All nine local QEMU cases passed with `-Os`.
`Kernel/SerialDev/Mouse.HC:KbdMsInit` allocates the five PS/2 FIFO records
and their buffers through `FifoU8New` and `FifoI64New` in `Kernel/KDataTypes.HC`.
UnholyOS now follows that allocation order before installing the device
handlers. The buffers take 40,976 bytes out of the boot BSS. The boot
test checks their heap allocation, capacities and the existing input probes.
Clang may emit a System V `memcpy` call for aggregate copies. Its assembly
entry returns the original destination; the HolyC `_MEMCPY` entry retains
the reference calling convention and return value.

## HolyC terminal

Startup opens two DolDoc terminals side by side and focuses the left
one. Each terminal reads `::/Doc/Start.DD` from the boot disc and lists
`T:/Home` before its first prompt. The empty RedSea home directory
shows `.` and `..`. `Dir("::/Doc/St?rt.DD*");` lists the startup
document, and `Cd("::/Doc");` changes the current directory. Run
`Cd;` to return home. `Type("::/Doc/Start.DD");` inserts another copy
at the cursor. Type `6*7;` and press Enter to see `ans=0x0000002A=42`. Open statements
continue on the next line. The cursor keys, Backspace, and Delete edit
the document before the compiler reads the command. `WinMax;` expands
the current window. `WinTileHorz;` stacks the terminals by row, while
`WinTileVert;` puts them side by side. Shift+Escape closes the focused
terminal and returns focus to the other one.

The boot suite checks the title, subtitle, links, and home listing in
both terminals. It sends PS/2 keyboard events to test line editing,
multiline expressions, syntax error recovery, `GetStr`, tiling, and
focus. After closing the right terminal, it types `Type` in the left
terminal and checks that the document appears again. It also runs `Dir`
at the prompt and checks the return count. The suite repeats these checks
on the ISO built in the guest. `DirectoryProbe.HC` checks the native
`CDirEntry` layout, wildcard and exclusion masks, single and recursive
searches, flattening, and directory changes on the DVD and on a writable
RedSea RAM drive.

`DirContextProbe.HC` checks the native record layout, nested contexts,
mask splitting, directory creation, and restore behavior. The DolDoc
probe reads the external attachment from `SpritePutExt.HC` and
checks that a saved document retains its `BP` link. A separate fixture
resolves links by number and tag from its own directory while the caller
stays in `/Home`. It also includes a cycle.
With the DolDoc lexer enabled, `$IS` reads the referenced attachment's
size in a native HolyC expression.

`Demo/Graphics/SpritePut.HC` and `SpritePutExt.HC` are Terry A. Davis's
public-domain programs. The test disc includes both byte for byte at
their original paths. `SpriteProbe.HC` compiles the external map as a
native `$IB` literal and checks its 12,546-byte size. It compares every
pixel against the packed bitmap after drawing onto a filled canvas and
after clipping the bitmap to a smaller canvas. The suite repeats the
probe on the ISO built in the guest.

With QEMU 9.2.0, PC, `qemu64`, and 512 MiB, both UnholyOS and the
pinned TempleOS disc ran `ExeFile("::/Demo/Graphics/SpritePutExt.HC");`
from a maximized terminal after `DocClear(,1);`. Three map regions of
142×87 pixels each matched, including the background through transparent
pixels. The terminal frames and the printed `ExeFile` result differ.
[Pinned TempleOS graphics demos](https://github.com/cia-foundation/TempleOS/tree/c26482bb6ad3f80106d28504ec5db3c6a360732c/Demo/Graphics)

[`Doc/Start.DD`](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Doc/Start.DD)
is Terry A. Davis's public-domain startup document, copied byte for
byte from the pinned source. Its SHA-256 is
`0f5f7dc1ebd091d2acee39b81b4211a8ffcd0febc423e6b8d5075e9854017530`.
The title uses the runtime `DD_OS_NAME_VERSION` definition, currently
`UnholyOS`. Help navigation, the directory chooser, border controls,
menus, autocomplete, and the full editor remain pending.

## Copying RedSea files

`FileCopyProbe.HC` runs with a 2,048-block RAM drive. It checks the
filename attributes, self-copy rejection, a 65,573-byte copy across the
64 KiB buffer boundary, compression and expansion on extension changes,
contiguous files, the exact stored blocks of the DVD's `StartOS.HC.Z`,
and an empty file. The probe also checks logical sizes and timestamps.
Both guests printed the same six copy messages and `File copy probe FF`.
The fixture SHA-256 is
`29a53bb7019d46f1d6b1c84652b68e1e494005c2d906cf6c18f6c9497388ffed`.

The focused UnholyOS build used `make -j4 BUILD=build/copy-check all
build/copy-check/unholyos-startup-test.iso`. The focused disc was built with
the existing `MakeISO` tool, the normal loader and kernel, `StartOS.HC.Z`,
`FileWriteProbe.BIN`, and `FileCopyProbe.HC`.
Both `build/copy-check/file-copy-focused.iso` and the pinned TempleOS live
disc ran under QEMU 8.2.2 with `-machine pc -cpu qemu64 -m 512`.
The reference ran the exact fixture through `ExePutS` after a debugger
transfer to an allocated buffer; the native boot ran it from the disc.
The boot suite checks the full output on startup and on the disc built
in the guest.

## RedSea file and directory operations

`FileOperationsProbe.HC` checks preallocated directories, masked copy and
move, nested tree copies, masked recursive deletion, complete tree removal,
and preservation of file bytes and timestamps. It checks both the first
and last allocated blocks of a deleted file in the RedSea bitmap and
verifies that a read-only RAM drive retains the original file. The two
tree copies return 9 and 7, including the directories, and the final
result is `File operations probe FFF`.

The focused build used `make -j4 BUILD=build/ops-check`; the ISO at
`build/ops-check/file-operations.iso` used the normal loader and kernel,
`StartOS.HC.Z`, `FileWriteProbe.BIN`, and the exact source fixture
(SHA-256 `505323d9de4face5e0c6a6bac2c237a4b1d8242aaf4861b34a0da2c271d6d52d`).
Both this image and the pinned TempleOS distribution ran with QEMU 8.2.2,
`-machine pc -cpu qemu64 -m 512 -boot d -cdrom <iso> -display none`.
The pinned guest ran the same source through `ExePutS` after GDB restored
it into an allocated guest buffer. All 1,102 output bytes, including
copy and deletion messages, matched. The startup and guest-built disc
tests check this exact trace. The original account and distribution
scripts that use these functions also require additional account setup,
applications, and writable drives; they were not run.

## Reading a FAT32 disk

`Linux/MakeFAT32.cpp` generates a deterministic 68,699,136-byte raw disk
with one primary MBR partition, 512-byte sectors, two mirrored FATs,
two sectors per cluster, and 65,536 data clusters. The root directory
and a 2,501-byte file are fragmented. A two-record VFAT long name crosses
the root directory's cluster boundary. The image also includes a nested
directory, an empty file, a contiguous-name file, and malformed file
chains for rejection tests. A second image has an invalid cluster size.

The native guest discovers the partition at `C:` while booting from DVD.
`Fat32ReadProbe.HC` checks geometry, DOS dates and times, exact file bytes,
the long name, recursive masks and parent links, directory context,
parent file lookup, and explicit/next-block stream reads. Both guests
printed these exact 165 bytes:

```text
fat geometry 2 2048 132130 2 2 2080 3102
fat metadata FRAG.BIN 5 2501 20 02/29/24 12:34:56
fat entry SUB 1024
fat entry LongNameExample.txt 333
FAT32 read probe FFF
```

The probe SHA-256 is
`93501ef88a21a3a7387456686745e079a275d52e69ee1a0275211fda22573cc1`.
The shared disk SHA-256 is
`9c63ff36c1dac69265ad59539fda0777e2a01ab182555064cc807ab6ac57e31a`.
Both guests ran on QEMU 8.2.2 with `-machine pc -cpu qemu64 -m 512`
and `-drive file=build/fat-check/fat32.img,if=ide,index=0,format=raw,snapshot=on`.
The reference used the pinned live distribution DVD. It did not mount
the ATA disk automatically, so the reference wrapper called
`ATAMount('C',BDT_ATA,0x1F0,0x3F4,0)`, marked the device read-only, and
executed the unchanged source through `ExePutS` from a GDB-restored
guest allocation. The image hash remained unchanged after both runs.

The native-only `Fat32RejectProbe.HC` checks cycle, reserved/free-link,
and short-chain rejection, plus write, deletion, and directory-creation
refusal. It reports `FAT32 reject probe 1F` on the valid volume and
`FAT32 invalid geometry rejected` on the invalid volume. These malformed
cases are intentionally not run through the pinned reader. The focused
and full-suite commands use generated images and QEMU snapshots:

```sh
make -j4 BUILD=build/fat-check test
build/fat-check/Linux/TestBoot --fat32 qemu-system-x86_64 \
  build/fat-check/fat32-test.iso build/fat-check/fat32.img valid
build/fat-check/Linux/TestBoot --fat32 qemu-system-x86_64 \
  build/fat-check/fat32-test.iso build/fat-check/fat32-invalid.img invalid
```

This fixture retains read-only mounts and malformed-image rejection even
when writable FAT32 is enabled on a different test disk. No physical
drive was used for these tests.

## Persistent ATA block writes

`AtaWriteProbe.HC` writes 130 sectors in the unused tail of a disposable
copy of the FAT32 fixture. It enables the reference's read-only override
only for the block writes, then restores the device flags. The probe
checks the complete pattern, a separately addressed sector, both adjacent
guard sectors, and a zero-length write. Both guests reported:

```text
ATA wrote pattern
ATA write probe 3F
```

After shutting down QEMU and booting again with the same writable raw disk,
both guests reported:

```text
ATA persisted pattern
ATA write probe 3F
```

The reference ran on the pinned distribution DVD in QEMU 8.2.2 with
`-machine pc -cpu qemu64 -m 512` and the raw disk attached as IDE index 0.
As in the read test, the wrapper explicitly mounted C: and executed the
unchanged probe via `ExePutS`. The source SHA-256 is
`b9e5da59568ae263833e331d98088268dcf132457ad165f5a6c40badfc2da489`.
The full 68,699,136-byte reference disk matched the expected image after
both boots, SHA-256
`42fc1e42e394c132e02e0dff386094c7210a41cbcfe6630d86b2672588980162`.
The native test compares every byte of the disk to that same expected
pattern after each shutdown. It also runs `AtaWriteRejectProbe.HC` (1F),
checking read-only protection, partition boundaries, and DVD write refusal,
and preserves the existing FAT32 rejection probe. The original input disk
is never opened for writing by these tests.

```sh
make -j4 BUILD=build/ata-check test
build/ata-check/Linux/TestBoot --ata-write qemu-system-x86_64 \
  build/ata-check/ata-test.iso build/ata-check/fat32.img
```

The ATA implementation uses LBA28 PIO and flushes the device write cache.
It does not yet implement the reference's multi-sector mode, LBA48, retry
and exception behavior, or general disk caching. FAT32 remains read-only
by default, and the block test does not establish file-write compatibility.

## FAT32 file and directory mutations

The separate `writable` image from `Linux/MakeFAT32.cpp` has valid chains,
two FATs and the same partition geometry as the read fixture. Its SHA-256 is
`9e458aed28c8cddcada016e182a64135a07b086bdd351de9ddf91a60bc55568e`.
The driver copies this image into a private temporary file before starting
QEMU. `Fat32WriteProbe.HC` explicitly clears the device's read-only flag
for the duration of the probe and restores it before returning.

Both guests create nested directories, replace a three-cluster long-name
file with a shorter file, check reclamation in both FATs, round-trip
compressed and contiguous files, write an empty file, force directory
growth with 36 entries, and delete a file and an empty directory. They
verify the exact contents, DOS modification dates, both directory-chain
links, and the unchanged original fragmented file. Both print:

```text
Make Directory:C:/Work
Make Directory:C:/Work/Grow
Make Directory:C:/Work/Empty
FAT32 mutation 3F
FAT32 file probe FF
```

On a cold reboot with the same disk, both print:

```text
FAT32 persisted files
FAT32 file probe FF
```

The reference uses the pinned live distribution DVD and explicit `ATAMount`
with QEMU 8.2.2, `-machine pc -cpu qemu64 -m 512`, and a writable raw IDE
index-0 image. The unchanged source SHA-256 is
`3525e241f3b43172d0ca349879cac7fdc2fe28be0c433bed5701670e871d370b`.
The reference disk SHA-256 after the first boot and after the second boot
is `51bdd22f8a92d8641ab781b9d452aa8b195fc1b482c5d563299ff82214eb9c03`.
The native driver compares the entire image after reboot to its first-boot
result, checks both complete FAT copies, verifies the original MBR and boot
sector, and checks the original input image was not modified. Allocation
order and directory timestamps can differ; the images are not asserted
to be byte-identical across the two systems.

```sh
make -j4 BUILD=build/fat-write test
build/fat-write/Linux/TestBoot --fat32-write qemu-system-x86_64 \
  build/fat-write/fat32-write-test.iso build/fat-write/fat32-writable.img
```

FAT32 mounting is still read-only by default. General FAT
cache behavior, cross-linked chain repair, and crash recovery remain
pending.

## FAT32 buffered streams

`Fat32StreamProbe.HC` exercises unbounded growth, preallocation, buffered
updates, seeks, reopening, contiguous streams, and raw compressed-file
copy. The unchanged probe SHA-256 is
`41bcff4d80e16aabd99a53ece93d1891650f3b64427da2ecf71e9cdd8e527932`.
The native and pinned reference guests both report:

```text
Copying C:/ZIP.BIN.Z to C:/ZIP2.BIN.Z
FAT32 stream mutation 3F
FAT32 stream probe 3F
```

On a cold reboot of each guest's own disk:

```text
FAT32 persisted streams
FAT32 stream probe 3F
```

The reference uses QEMU 8.2.2 with `-machine pc -cpu qemu64 -m 512`
and a writable raw IDE index-0 disk. Its disk SHA-256 after both boots is
`260f34c49e2f9d5f3add8d0712fbd6939e7bad0f2b942a6cc7a89ad6c634632b`.
The native test checks complete FAT mirror equality, persistence of the
entire mutated disk across reboot, and preservation of the input image.
The two systems' allocation order and timestamps need not be identical.

```sh
make -j4 BUILD=build/fat-stream test
build/fat-stream/Linux/TestBoot --fat32-stream qemu-system-x86_64 \
  build/fat-stream/fat32-stream-test.iso build/fat-stream/fat32-writable.img
```

## FAT32 fragmented streams and recursive file trees

`Fat32TreeProbe.HC` forces a fragmented stream by allocating a separate
file between two writes. It checks the complete file, random block seeks,
the blocker, nested directories, masked copy and move, recursive copy,
rejection of a destination inside the source, masked deletion, and
restoration of the current directory. The native and pinned TempleOS
guests produce identical operation traces ending in:

```text
FAT32 tree mutation FF
FAT32 tree probe 3F
```

After a cold reboot both report `FAT32 persisted tree` and
`FAT32 tree probe 3F`. The two FAT mirrors are compared in their entirety;
the first native and reference FAT tables also match byte for byte.
Only the four expected new cluster allocations survive the deletion of
the copied trees. The original fixture and the post-mutation image are
unchanged by the second boot. The reference disk SHA-256 after both
boots is `2a08f393a723838485047df0c2ed2d6c9061ce4dbd7fb692a21fc09ea4d4136f`.

```sh
make -j4 BUILD=build/fat-tree test
```

## FAT32 formatting

`Fat32FormatProbe.HC` writes a sentinel at the partition's final sector,
quick-formats the volume, checks its empty root and FATs, and confirms
that the sentinel survived. It writes a file, full-formats the volume,
checks that the old file and sentinel are gone, and saves a new file.
The native and pinned reference guests both report:

```text
FAT32 format mutation 3F
FAT32 format probe 0F
```

After a cold reboot of each guest's own disk:

```text
FAT32 persisted format
FAT32 format probe 0F
```

The probe SHA-256 is
`fc860a08d86c5732346a442b7a43931b50574303cfbe48725ff1e143e9a362a6`.
The pinned live DVD ran under QEMU 8.2.2 with `-machine pc -cpu qemu64
-m 512`, explicit `ATAMount`, and a writable raw IDE index-0 fixture copy.
The reference disk SHA-256 after formatting and after reboot was
`149fe048dbe1454a446e707524a9428523bfbdd6429d967ad8b4cd159b409959`.
The native test checks both complete FAT copies, the boot record,
preservation of all sectors outside the formatted partition except the
expected MBR partition type and active flag, zeroes throughout the
remaining data sectors, and persistence of the complete image on reboot.
On the second native boot the driver also attempts a read-only format
and declines the default confirmation on a writable volume using actual
keyboard input. The image must remain unchanged after both refusals.
Volume serial numbers and file timestamps need not match the reference.

```sh
make -j4 BUILD=build/fat-format test
build/fat-format/Linux/TestBoot --fat32-format qemu-system-x86_64 \
  build/fat-format/fat32-format-test.iso build/fat-format/fat32-writable.img
```

## Building a RedSea ISO in the guest

`RedSeaISO` takes an output filename, a source directory, and an optional
stage-two filename inside that directory. It writes an `.ISO.C` file to a
mounted writable RedSea RAM drive. For example, once `B:` is mounted:

```c
RedSeaISO("B:/GuestBuild.ISO.C","::/","::/0000Boot/0000Kernel.BIN.C");
```

The source can be the mounted DVD or a RedSea RAM directory. The guest sizes
the source tree, sorts each directory, copies file bytes and dates, and
patches the included `BootDVD` loader to the copied stage-two extent. The
host builder uses the same descriptor and loader patch code.
`Linux/MakeISO` uses `--compress-at Doc/Start.DD.Z Doc/Start.DD` to
encode the source using the same routine as the guest. The 200-byte
archive matches `/Doc/Start.DD.Z` on the pinned live CD, and the guest
expands it when opening `::/Doc/Start.DD`. `--dir Home` adds an empty
directory. The boot suite checks both directory records and the
archive checksum. The dates shown by `Dir` come from the image read by
the guest.

The QEMU boot suite runs `RedSeaISOProbe.HC` through `ExeFile` at 512,
3,072, and 4,608 MiB. The boot harness mounts a separate RAM drive `A:`
sized for both source volumes and ISO overhead, leaving the fixed-size
file-operation test drive `B:` intact. It reads the two output files from guest RAM, checks
the boot image against every source file, and checks that the data image
contains the selected nested directory. The probe also checks that missing
and out-of-tree stage-two files and an output path inside the source tree
return zero. The suite cold boots the 512 MiB result and saves it as
`build/unholyos-guest-built.iso` with the default `BUILD` setting.

The local build used GCC 13.3.0, NASM 2.16.01, and QEMU 8.2.2 for
`make -j4 test BUILD=build/gcc`. The resulting image reached
`UnholyOS kernel entered in 64-bit mode` in a new QEMU 9.2.0 window:

```sh
qemu-system-x86_64 -machine pc -cpu qemu64 -m 512 -boot d -cdrom build/gcc/unholyos-guest-built.iso -display gtk -serial stdio
```

The guest copies files already present in the source tree. Rebuilding the
kernel and loader from source inside the guest still requires the guest
toolchain. The file-backed ISO device and persistent disc writes are pending.

## Reference

The source reference is [TempleOS at c26482bb6ad3f80106d28504ec5db3c6a360732c](https://github.com/cia-foundation/TempleOS/tree/c26482bb6ad3f80106d28504ec5db3c6a360732c). Keep the checkout outside UnholyOS and verify the revision before comparing source. The reference ISO used for the VM comparison contained the same `0000Boot/0000Kernel.BIN.C` bytes as that checkout.

- `Adam/Opt/Boot/DskISORedSea.HC` writes the descriptors, El Torito catalog, and stage-one boot image. The host builder and guest writer share the descriptor and loader code.
- `Adam/Opt/Boot/BootDVD.HC` starts at `BDVD_START`, relocates to low memory above the kernel area, reads the kernel one DVD block at a time, and shifts it down to `BOOT_RAM_BASE`. The high half of EAX carries the RedSea shift and EBX carries the DVD block.
- `Kernel/KernelA.HH` defines `CBinFile`, `CKernel`, and `CGDT`. The disk image uses their startup layout. The link fixes the kernel at the DVD address, with an empty patch table. The reference applies its relocation records during startup.
- `Kernel/KStart16.HC`, `Kernel/KStart32.HC`, and `Kernel/KStart64.HC` describe the real-mode firmware calls, the change to flat 32-bit segments, and the change to 64-bit mode.
- `SYS_FIND_PCIBIOS_SERVICE_DIR` and `SYS_FIND_PCI_SERVICES` in `Kernel/PCIBIOS.HC` validate the BIOS32 directory checksum and resolve the PCI service before paging. Both guests found the same SeaBIOS entries. `FarCall32` switches to protected mode for a PCI read and restores long mode, Adam's stack, FS, and GS before returning.
- `Kernel/Mem/PageTables.HC` sets up 2 MiB identity mappings in ascending PML2, PML3, and PML4 order and reserves another 4 GiB of address space for device aliases.
- `SysGlblsInit` in `Kernel/KMain.HC` reads the cache-line width, sets Adam as the focus task, initializes the macro and PCI queues, and allocates the bit tables, extension slots, and decimal powers. The x87 sequence in `Kernel/KMathA.HC` produces the same 618 binary64 slots in QEMU, including the unused zero slot. Disk cache, screen capture, and debugger state still need their boot initialization.
- `Mem32DevInit` in `Kernel/Mem/MemPhysical.HC` starts with the device window at 0xE0000000 and inserts firmware ranges contained in a free span. It returns rejected descriptors and empty tails through `Free`. The boot allocator uses the reference's debug-off heap headers and separate caches for small allocations and page blocks.
- `UncachedAliasAlloc` in `Kernel/Mem/MemPhysical.HC` remaps the reserved top 4 GiB onto the lowest 4 GiB of physical memory with the same uncached page flags. The boot display uses the alias for VGA memory, following `SysGrInit` in `Kernel/KMain.HC`.
- `SYS_INIT_16MEG_SYS_CODE_BP` in `Kernel/Mem/PageTables.HC` records the E801 RAM above the page tables as the first free code block. The block pool uses 512-byte allocation units.
- `Kernel/KStart64.HC` saves the initial FPU state, initializes the bootstrap CPU through `CPUStructInit` in `Kernel/MultiProc.HC`, and sets its address as the GS base with `SET_GS_BASE` from `Kernel/KUtils.HC`. UnholyOS uses the reference stack argument order and callee cleanup for the assembly-to-C++ call. After CPU setup, startup initializes Adam's fixed heap and calls `TaskInit` from `Kernel/KTask.HC`. The task receives the initial FPU image, a 1,024-bucket hash table, a heap-backed stack, and the working directory `/Home`. Startup sets FS and switches to Adam's saved RSP before it jumps to `KMain`. The assembly entry then aligns the stack for the diagnostic C++ function.
- `MemPagTaskAlloc` in `Kernel/Mem/MemPag.HC` takes 512-byte pages from the low-memory code pool. `MAlloc` in `Kernel/Mem/MAllocFree.HC` supplies the small and page-backed allocations used by Adam.
- `BlkPoolsInit` in `Kernel/Mem/BlkPool.HC` extends the code pool after entry. `BIOSTotalMem` in `Kernel/Mem/MemPhysical.HC` determines whether Adam needs a separate data heap. For larger guests, `HeapCtrlInit` links the heap to the data pool, whose free list includes usable E820 RAM above the code limit.

- `IntInit1` in `Kernel/KInts.HC` builds and loads 256 ring-0 interrupt gates. Each points to `IntNop`, which acknowledges both 8259 controllers and the local APIC. A debugger-injected NMI reached that handler and returned to the same instruction and stack in both guests. Interrupts from maskable hardware stay disabled at this stage.
- `TimersInit` in `Kernel/KMain.HC` programs PIT channel 0 with mode byte `0x34` and divisor 1192. It enables HPET through the Intel LPC bridge when present, then checks the timer period and enables the main counter through the uncached alias. In both guests, QEMU's PC and Q35 machines reported a 100 MHz HPET. With `-machine pc,hpet=off`, both used the PIT for the initial `TimeCal` sample. A second call from GDB verified that UnholyOS updates the TSC frequency and kHz values on either clock source. The normal second call belongs to the compiler-loading stage.
- `IntsInit` and `IntInit2` in `Kernel/KInts.HC` program the PIC and install dedicated gates. Both guests have PIC offsets `0x20` and `0x28`, masks `0xFA` and `0xFF`, and the same gate roles after `IntInit2`. UnholyOS enables maskable interrupts after installing those gates. TempleOS enables them before that step.
- `IRQ_TIMER` and the context save and restore in `Kernel/Sched.HC` record Adam's interrupted state. `IntCore0TimerHndlr` in `Kernel/KMisc.HC` increments jiffies, adds 1193 to the timer count, and acknowledges IRQ0. UnholyOS resumes Adam after each tick. The boot test checks the timer and context-restore counters while the guest is halted between interrupts.
- `IntFaultHndlrsNew` in `Kernel/KInts.HC` builds seven-byte exception stubs. In UnholyOS, `INT_FAULT` saves the vector, error code, and interrupted state, prints a serial diagnostic, and halts. In QEMU, GDB triggered an invalid opcode and a general-protection fault. Both diagnostics matched the expected instruction address and error code. TempleOS's debugger and exception recovery still need implementation.
- `BootDVDProbeAll` and `BootDVDProbe` in `Kernel/BlkDev/DskATAId.HC` try PCI IDE ports and the legacy channels. The probe rereads the BIOS-supplied DVD block and compares the kernel compile timestamp at the supplied shift. QEMU traces for both guests show `START STOP UNIT` and `READ(12)` on the secondary IDE device. `ATAPIReadBlks2` in `Kernel/BlkDev/DskATA.HC` supplies the native 2,048-byte sectors for the 512-byte RedSea view.
- `ISOInit` in `Kernel/BlkDev/DskCDDVD.HC` looks for the RedSea publisher in the supplementary descriptor and advances the volume offset for a bootable disc. `RedSeaInit` and `RedSeaFileFind` in `Kernel/BlkDev/FileSysRedSea.HC` read the boot record and the 64-byte directory records. UnholyOS attaches the read-only boot volume to Adam after mounting it. Its file reader can read a byte range from a contiguous file. In a separate test, GDB set the DVD handoff to the pinned TempleOS disc on the second IDE unit, and the guest checksum matched the full 189,760-byte kernel file on that disc. Disk caching remains pending; the read-only ATA/FAT32 path is described above.
- `FileRead` in `Kernel/BlkDev/DskFile.HC` checks Adam's resident file table, tries the requested and alternate names in the target directory, then searches its parents. The native RedSea entry point follows that order and returns the file size and attributes with a zero-terminated buffer. `ArcExpandBuf`, `ArcCompressBuf`, and `ArcEntryGet` in `Kernel/Compress.HC` define the stored, 7-bit, and 8-bit streams, width changes, and dictionary reuse at 12 bits. The host test reproduces both pinned compressed archive streams byte for byte.
- `Cmp` in `Compiler/CMain.HC` emits the `CBinFile` header and patch records read by `LoadPass1` and `LoadPass2` in `Kernel/KLoad.HC`. UnholyOS validates the records, aligns the executable body, and installs symbols in Adam's hash table using the bucket order from `Kernel/KHashA.HC`. It resolves relative imports when a later module publishes an export. A module with unresolved imports can be loaded for linking, and its initializers wait for an explicit call after resolution. Fixed-address modules and the full kernel symbol set remain pending.
- Kernel exports use HolyC stack arguments and callee cleanup. `Kernel/Mem/MAllocFree.HC` supplies the heap and string allocation contracts, including the negative offset before an aligned pointer. `Kernel/KHashA.HC` and `Kernel/KHashB.HC` supply the hash lookup and insertion rules, including matching across parent tables. `Kernel/StrA.HC`, `Kernel/KUtils.HC`, and `Kernel/KMathB.HC` supply the string, bit, memory, and numeric calls. Allocation exceptions and configurable variable initialization at boot are pending.
- The pinned TempleOS compiler built `Linux/Fixtures/LoadProbe.BIN` and `LoadCaller.BIN` from the adjacent HolyC source files using `Cmp`. The two guests produced 40 and 28, preserved a relocated function address, and loaded the probe at its requested 128-byte alignment. The probe also imported `SYS_COMPILE_TIME` and called `_MEMCPY` with HolyC stack arguments. The copy returned the address just past its destination, as implemented in `Kernel/KUtils.HC`. The 4.5 GiB test checks that a data-heap relocation reaches an allocation above 4 GiB.
- The pinned compiler also built `RuntimeProbe.BIN` from `RuntimeProbe.HC`. Both guests reported `0xff` with QEMU PC, `qemu64`, and 512 MiB. The probe checks allocation and aligned heap headers, string calls, hash instances and use counts, numeric and bit calls, and imported globals. It checks the exact `Pow10I64(2)` return bits (`0x4058fffffffffffe`).
- The same probe checks all 512 bits in each character map from `Kernel/StrA.HC`, including the high bytes and the special whitespace used by DolDoc. It checks `LstMatch` aliases and ambiguous prefixes, list sizing and queue cleanup from `Kernel/KDataTypes.HC`, and zero-filled gaps across 512-byte queue-vector nodes. It also checks the callback arguments, call count, and equal-key order from `Kernel/QSort.HC`. A separate link-only run of the pinned `Compiler.BIN.Z` left 49 unresolved import groups, down from 68 before these routines and maps were exported. Initializers stayed disabled.
- The pinned compiler built `FileReadProbe.BIN` from `FileReadProbe.HC`. Both guests reported `0x7f` under QEMU PC with `qemu64` and 512 MiB. It checks `DirNameAbs`, `FileNameAbs`, `DirFile`, `ExtChg`, and `ExtDft` from `Kernel/BlkDev/DskStrA.HC`, including the first-dot extension rule. It reads the expanded `StartOS.HC` by relative name, by its archive name, and through parent search. It also checks the plain kernel file attributes, a missing file, and a resident file table hit. The path helpers use the current task directory and the boot and home aliases for the mounted DVD.
- The same compiler built `FileWriteProbe.BIN`, which mounts a fresh `B:` RAM drive in each guest. `BlkDevInit` in `Kernel/BlkDev/DskBlkDev.HC` supplies the allocation and formatting rules. The probe checks the packed `CBlkDev` layout, disk geometry, bitmap, and root entries. It tests `RedSeaFileWrite` and `RedSeaDirNew` in `Kernel/BlkDev/FileSysRedSea.HC` through block reads and writes, nested directory creation and growth, file replacement, empty files, and resident cache updates. The resulting `StartOS.HC.Z` stream has the pinned 889-byte size and checksum. Hard-disk and FAT32 writes remain pending.
- The pinned compiler built `ExceptProbe.BIN` from HolyC `try`, `catch`, and `throw` statements. `PrsTryBlk` in `Compiler/PrsStmt.HC` emits separate catch and cleanup labels. `SysTry`, `TestExcept`, and `TakeExcept` in `Kernel/KExcept.HC` define the two stack transitions. A catch runs on the throw stack with the handler's saved registers, then resumes at its cleanup label on the saved stack if it accepts the exception. The probe covers early return, nested catches, propagation, caller addresses, interrupt flag restoration, and heap and queue cleanup. UnholyOS reports exceptions on serial. Adam's log, symbolic exception output, and the interactive debugger remain pending.
- The pinned compiler built `FormatProbe.BIN` from `FormatProbe.HC`. Both guests reported `0x1ff` with QEMU PC, `qemu64`, and 512 MiB. `PrsExp.HC` supplies the caller-cleaned stack layout for variadic arguments. `StrPrintJoin` in `Kernel/StrPrint.HC`, the math routines in `Kernel/KMathA.HC`, and `DefineLstLoad` in `Kernel/KDefine.HC` supply the formatting and list behavior. The probe checks all four string entry points, buffer growth, integer and floating-point conversions, quoting, define lists, date and time, raw and linked pointers, file text, and exceptions. Boot tests also check ordinary output, diagnostics, and silent mode on serial. A link-only load of the pinned `Compiler.BIN.Z` at this stage left 26 unresolved import groups, down from 38.
- The pinned compiler also built `HashCaller.BIN` and `HashProbe.BIN`. Both guests reported `0x3f` with QEMU PC, `qemu64`, and 512 MiB. `HashSrcFileSet` and `HashDel` in `Kernel/KHashB.HC` and `ClassMemberLstDel` in `Compiler/LexLib.HC` supply source links and ownership rules for compiler records. The probe checks help indexes, private flags, child tables, members, function pointer records, and owned and borrowed data. `SysSymImportsResolve` in `Kernel/KLoad.HC` patches the caller's function and global imports when the corresponding typed entries appear. The probe checks function names and offsets from `Kernel/FunSeg.HC` and confirms that the relocated call still works after the exports are removed from the table. The boot tests verify that kernel exports and their names belong to Adam's heap. A link-only load of the pinned compiler now leaves 22 unresolved import groups. The compiler initializers and debugger-backed symbol lookup remain pending.
- The pinned compiler built `DocProbe.BIN`. Both guests reported `0x7f` with QEMU PC, `qemu64`, and 512 MiB. `DocNew.HC`, `DocPutS.HC`, `DocPlain.HC`, `DocRecalc.HC`, `DocBin.HC`, and `DocFile.HC` under `Adam/DolDoc` define the document records and storage rules. The probe checks the native ABI, allocation and deletion, text and cursor records, literal markup formatting, binary attachment renumbering, and a write to a RedSea RAM drive. It also reads the unmodified `StartOS.HC` into 86 document entries and saves the original 1,158 bytes unchanged. `%$F` now uses the document serializer. Editor input, prompt dialogs, external binary links, and HolyC expressions in markup need further work. A link-only load of the pinned compiler now leaves 13 unresolved import groups. The compiler initializers remain pending.
- The pinned compiler built `CompilerRuntimeProbe.BIN`. Both guests reported `0x3f` with QEMU PC, `qemu64`, and 512 MiB. `Kernel/BlkDev/DskAddDev.HC` and `DskDrv.HC` define the shared device and drive tables, including the boot and home aliases. The probe changes the home path and scratch document name and checks that path lookup and `DocNew` read both values. It also checks the boot DVD read counter, previous-state returns from `Raw` and `DbgMode` in `Kernel/KMisc.HC`, file and address links from `Kernel/BlkDev/DskDirA.HC`, sparse source maps from `Kernel/FunSeg.HC`, the fix link from `Kernel/EdLite.HC`, and the error message written to Adam's document by `AdamErr` from `Kernel/Job.HC`. Drive registry persistence, the debugger display, and cross-core jobs remain pending.

In a separate QEMU run, GDB loaded the pinned `Compiler.BIN.Z` and called its `CmpCtrlNew` and `CmpCtrlDel` routines. The first call set up the supplied lexer buffer at line 1 and selected `~/Tmp.DD.Z` as the default file. The second restored heap usage, and both restored the caller's stack. Six import groups remained unresolved. Initialization of the pinned compiler module remained pending in that run.

`LexGetChar`, `LexInStr`, and `Lex` in `Compiler/Lex.HC`, and `CmpFillTables` in `Compiler/CInit.HC`, define how plain source becomes tokens. The C++ scanner preserves the token IDs, source line and lookahead fields, and literal encodings. In QEMU 8.2.2 with PC, `qemu64`, and 512 MiB, the pinned compiler and the C++ scanner produced the same FNV-1a signatures over token IDs, lexer positions, and payload bytes. `LexProbe.HC` produced `0x8676a65fcd40fc3d`. A second input with newline, dot, at-sign, and character-constant flags produced `0xbf6dea4d260e4444`. The inputs cover nested comments, dollar markup, long strings, embedded NULs, packed character constants, and the reference's treatment of `5e+2`. The large decimal literals verify that the lexer multiplies the integer mantissa by the power of ten in x87 before storing the binary64 result.

`LexIncludeStr` in `Compiler/Lex.HC` and `LexBackupLastChar` in `Compiler/LexLib.HC` preserve the parent stream's lookahead across includes and define expansions. The keyword records use the IDs in `Compiler/OpCodes.DD` and the hash layout from `Compiler/AsmInit.HC`. In QEMU 8.2.2 with PC, `qemu64`, and 512 MiB, the two guests produced the same signatures for `LexDirectives.HC` in JIT and AOT modes. The signatures include token values, source lines, lookahead, include depth, and define-frame flags. The fixture checks nested includes, a missing include, the default `.HC.Z` extension, define continuation and redefinition, `#ifdef`, `#ifndef`, `#ifjit`, `#ifaot`, `#else`, and `#endif`. Both guests also find the same macro values, the nested header's source link, and the help-file record with its joined index. Tests verify that deleting the lexer and its temporary tables returns heap usage to its starting value.

`LexAttachDoc`, `LexGetChar`, and `LexDollar` in `Compiler/Lex.HC` feed DolDoc entries into the token stream. With the DolDoc run-level bit set, `#include` loads a document and expands defines through plain-text frames. Under the same QEMU configuration, both guests produced `0x5bdc53a30085a3c4` for a root document with tabs, shifted text, and quoted markup. The original `Demo/DolDoc/TextDemo.HC` and `Demo/Graphics/SpritePlot.HC` from the pinned checkout produced `0x22cc259f6558ab8f` and `0xc37e51fd8b8a58ba`. The sprite signature covers the inserted bytes, binary size, and the graphics macros from `Kernel/KernelA.HH`. The last attachment in the pinned `SpritePlot.HC` lacks one byte. TempleOS reads the zero appended by `FileRead`, and UnholyOS supplies that byte when it loads the attachment. Both demo files are Terry A. Davis's public-domain source. The tests check that the document and lexer allocations are released after scanning.

`PrsExpression2` and `LexExpression2Bin` in `Compiler/PrsExp.HC` parse an expression into intermediate code and compile it to a callable routine. The C++ parser uses the precedence from `Compiler/CInit.HC`. The emitter writes x86-64 code into the guest code heap and returns the result in RAX. It handles signed and unsigned 64-bit integers, binary64 values, packed character constants, `defined`, and reads of globals of those numeric types. Operators include comparison chains, bitwise shifts of binary64 representations, and x87 power from `SYS_POW` in `Kernel/KMathA.HC`. Logical operators evaluate both operands when the expression returns a value. `#if` and `#assert` use the same expression path for these forms.

Under the same QEMU configuration, both guests ran the 42 expressions selected by `ExpressionProbe.HC`. Each routine ran twice with a different value of one global. The hash of the returned type IDs and result bits was `0x452a7a6d75541fd2` in both guests. In separate runs, both guests reached a divide error when evaluating `0 && (1 / 0)` as a value. The UnholyOS boot suite also checks the failed-assertion warning, rejection of an incomplete expression, and restoration of both heaps.

`LexStmt2Bin` in `Compiler/CMain.HC` and `ExeCmdLine` in `Kernel/KTask.HC` drive statement compilation and execution. The native compiler handles scalar globals, assignments, and fixed-arity scalar functions. It follows `PrsFunCall` in `Compiler/PrsExp.HC` for default arguments and right-to-left evaluation. The emitter uses the parameter offsets and callee cleanup rules from `PrsFunJoin` and `PrsFun` in `Compiler/PrsStmt.HC` and the x87 conversions from `ICFCvt` in `Compiler/BackFA.HC`.

`PrsIf`, `PrsFor`, `PrsWhile`, and `PrsDoWhile` in `Compiler/PrsStmt.HC` lay out the statement stream with branch targets. Scalar locals use negative RBP offsets as in `PrsVarLst` in `Compiler/PrsVar.HC`. A return branches to the function epilogue and a break branches to the innermost loop exit. The branch rules in `OptBrZero` and `OptBrNotZero` in `Compiler/OptLib.HC` make `&&` and `||` short-circuit in conditions. Prefix and postfix increment and decrement update scalar locals and globals.

In QEMU 8.2.2 with PC, `qemu64`, and 512 MiB, both guests compiled the same `FunctionProbe.HC` source. Its 27 result types and values produced `0x6ebe7d79f6232350`. The fixture covers defaults, nested calls, parameter cleanup, conversions, assignments, short-circuit side effects, local variables, loops, recursion, and calls that retain the earlier definition of a function. It includes Terry A. Davis's unmodified `Recurse2` and `Recurse3` functions from the pinned `Demo/StkGrow.HC`. The boot test also runs a top-level branch, calls a newly compiled function through the kernel's HolyC stack adapter, and checks that invalid definitions leave no symbol and both heaps return to their starting usage.

`LexExtStr` in `Compiler/LexLib.HC` joins adjacent string tokens and includes the trailing zero. The compiler puts string bytes after the generated instructions and keeps the block alive when a command may leave pointers to those bytes. `PrsFunCall` in `Compiler/PrsExp.HC` lowers a leading string or packed character statement into a call to `Print` or `PutChars`. The runtime headers follow `Kernel/KExts.HC`. Variadic calls put the fixed parameters first on the stack, then `argc`, then the inline `argv` slots. The caller removes the arguments and evaluates them from right to left.

The complete, unmodified `Demo/Print.HC` by Terry A. Davis produced the same 230 output bytes in both guests under the same QEMU configuration. The boot test now runs it through `ExeFile`. `PrintSourceProbe.HC` produced another 58 matching bytes. It checks ordinary calls, arguments with side effects, adjacent strings, the packed character shortcut, and a call to `PutS`. The reference test captured bytes at its keyboard device output handler. UnholyOS sent the same bytes to serial.

`ExeFile` and `ExePutS` in `Compiler/CMain.HC` drive source execution through the normal lexer and command loop. `ExeFile` supplies the default `.HC.Z` extension and uses the include path, which can read `.HC` when the compressed file is absent. The first source command installs compiler keywords and types in a table linked from Adam's symbol table, following `AsmHashLoad` in `Compiler/AsmInit.HC`. The task owns the declarations published by source commands. `CCF_JUST_LOAD` compiles the file and skips execution of its emitted top-level statements.

`PrsStreamBlk` in `Compiler/PrsStmt.HC` runs each `#exe` block in the task's symbol context. `StreamPrint` in `Compiler/CMisc.HC` appends generated text, which the lexer reads before resuming the enclosing source. `StreamExePrint` in `Compiler/CMain.HC` evaluates generated code in the saved context and returns its answer. With matching QEMU PC, `qemu64`, and 512 MiB settings, both guests ran `StreamProbe.HC` through `ExeFile` and returned 164. The output matched for load-only execution, nested file execution, a generated function and define, and a local declaration generated inside a function. The boot suite checks recovery after a syntax error and a formatting exception, including an exception propagated through a nested `ExePutS` call. Both heaps and the compiler and exception queues return to their starting state in those recovery checks.

`PrsClassNew` in `Compiler/PrsLib.HC` stores each pointer level next to its base type. `PrsArrayDims` in `Compiler/PrsVar.HC` records the trailing element counts used by multidimensional indexing. The compiler uses these layouts for 8-, 16-, 32-, and 64-bit integers, `F64`, typed pointers, and fixed-size local and global arrays. It follows `PrsUnaryModifier` and `PrsAddOp` in `Compiler/PrsExp.HC` for postfix casts, member access, and scaled pointer arithmetic. Integer unions from `Kernel/KernelA.HH` expose the little-endian subfields of wider integers. The emitter uses the pointed-to type for load and store widths, following `ICDeref` in `Compiler/BackLib.HC` and `ICAssign` in `Compiler/BackC.HC`.

`MemoryProbe.HC` produced 144 matching output bytes in both guests under the same QEMU configuration. It checks multidimensional indexing, pointer arguments and returns, increments, postfix casts, and narrow loads and stores. In the tested calling convention, a function declared `I8` can return the full register value 255 after evaluating a wide expression. Reading that same byte from memory produces -1. The boot suite also verifies the configurable global fill byte and checks that invalid dimensions leave no symbols or live allocations. The complete, unmodified `Demo/SubIntAccess.HC` by Terry A. Davis produced 141 matching output bytes through the native compiler.

`PrsClass` in `Compiler/PrsStmt.HC` and `PrsVarLst` in `Compiler/PrsVar.HC` define packed member offsets, single inheritance, overlapping union members, and the lifetime of static locals. UnholyOS completes a forward declaration in place so existing pointer types refer to the completed class. It supports named unions, anonymous unions inside classes, and scalar-backed unions. `PrsVarInit` and `PrsVarInit2` in `Compiler/PrsVar.HC` define positional initialization of global and static storage, including fixed-size arrays, nested objects, and strings in byte arrays.

`AggregateProbe.HC` produced 145 identical output bytes in the two QEMU guests. It checks packed offsets, pointer links through a forward declaration, inherited fields, union overlays, evaluation order, and persistent static values. The pinned compiler treats an omitted first array dimension as zero, so a nonempty initializer for that dimension fails. It writes the derived class's own initializer fields starting at byte zero. The boot suite checks both cases, completion after a failed class definition, and release of uncommitted storage after a failed initializer. Whole-object assignment and value arguments, metadata declarations, initializers that would write beyond overlapping union storage, `goto`, `switch`, native variadic function definitions, indirect calls, and full ahead-of-time compilation remain pending.

`Compiler/CInit.HC` gives the ten compound assignment operators the same right associativity as `=`. `PrsAddOp` in `Compiler/PrsExp.HC` scales pointer offsets for `+=` and `-=`. The emitter saves the destination address once, evaluates the right operand, then reads and updates the destination. It applies the immediate power-of-two substitutions from `Compiler/OptPass012.HC` when it emits the update. Integer division uses the destination's signedness as in `ICDivEqu` in `Compiler/BackA.HC`. Narrow results can differ from the value read back after the store.

`CompoundProbe.HC` produced 297 identical output bytes in QEMU 8.2.2 with PC, `qemu64`, and 512 MiB in both guests. The pinned guest ran the fixture through `ExePutS` and captured the output through `KeyDevAdd`. The UnholyOS boot suite runs it through `ExeFile` at 512, 3,072, and 4,608 MiB and checks the entire output. The fixture covers all ten operators, a right operand that changes the destination after its address is computed, chained assignments, pointer offsets, narrow results and stores, and floating-point operations. The boot suite also checks that assigning to a constant inside a function publishes no function and leaves both heaps at their starting usage.

A separate QEMU run loaded the probe through the `.BIN.Z` fallback. In another run, the loader rejected a probe with an out-of-range `IET_MAIN` offset. GDB confirmed that the caller's import stayed pending and the rejected module had no hash table entry.

The test archives in `Linux/Fixtures` are Terry A. Davis's public-domain files from the pinned distribution. `StartOS.HC.Z` is the original 889-byte root file. `KDbg.HC.Z` is the original 7,295-byte file in `/Kernel`. The 8-bit test uses the 14,463 bytes returned by `FileRead` in the pinned guest, including the original `0x1F` spacing bytes. In a separate test, GDB selected the pinned DVD in UnholyOS and invoked `FileRead` for `/Kernel/KDbg.HC`. The returned bytes matched the pinned guest. [TempleOS credits](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Doc/Credits.DD)

With matching QEMU 9.2.0 PC and `qemu64` settings and 512 MiB of RAM, both guests reported `CS=0040`, `CR0=80000031`, `CR3=509000`, `CR4=b0`, and `EFER=500`. TempleOS reached the windowed terminal. UnholyOS displayed its kernel entry line.

Debugger stops in both guests matched the boot CPU fields, initial FPU control word, and free code block after `SET_GS_BASE`. At `KMain` entry, the Adam record, heap accounting, FS base, and stack pointer matched. The random seed comes from each boot's timestamp counter. After block-pool initialization, the sampled heap and pool records matched at 512 MiB, 3 GiB, and 4.5 GiB. Injected E820 records matched the reference on range splitting, overlap rejection, and empty-tail removal.

## Compiling a HolyC module in the guest

`Cmp` reads HolyC source or a `.PRJ` file and writes a relocatable TOSB
module. With a writable RAM drive mounted as `B:`:

```c
Cmp("::/CmpProbe","B:/CmpOutput","B:/CmpOutput.BIN.Z",':');
```

The boot test uses the source files from the startup-test DVD. It checks
that the compiler writes the requested DolDoc source map and leaves the
caller's symbol table alone. An invalid source compile returns one error
and preserves the previous output file. Loading the valid module runs
its top-level statements and reports `cmp module 32 ready`. The module
uses recursion, static storage, a constant array initializer, and an
import of the guest's `Print` function.

The tests also compile and load Terry A. Davis's unmodified
`Demo/Print.HC`. Its 230 output bytes match the original guest's output.
With QEMU 9.2.0 PC, `qemu64`, and 512 MiB, pinned TempleOS's `Load`
executed the `CmpProbe` module produced by UnholyOS and printed the same
`cmp module 32 ready` line. The GCC boot suite saves that module as
`CmpOutput.BIN.Z` under `BUILD`.

The linker test builds `ImportCaller.HC` and `ImportProvider.HC` inside
the guest. It loads the caller first and holds its entry points until
the provider exports its function and data. The caller also imports
`_MEMCPY` and `SYS_COMPILE_TIME` under local names and checks the copied
timestamp. Both guests printed `native link 34` from the same two
guest-built modules in QEMU 9.2.0 with PC, `qemu64`, and 512 MiB.

The boot suite also resolves imports against later JIT definitions,
checks JIT calls through forward declarations, and binds typed `extern`
and `_extern` declarations to loaded symbols. The suite checks that an
unresolved function raises `UndefExt` and that JIT `import` is rejected.

Relocatable HolyC initializers, binary debug line tables, and native
kernel compilation remain pending.

## HolyC assembly blocks

The guest now assembles top-level `asm { }` blocks during both source
execution and `Cmp`. The assembler supports `USE16`, `USE32`, and
`USE64`, integer operations and branches, scoped `@@` labels,
`DU8` through `DU64`, `DUP`, `ALIGN`, and named imports. Typed
`_extern` declarations can bind to assembly labels in AOT modules.

Terry A. Davis's `Demo/Asm/DivByHand.HC` and
`Demo/Asm/AsmHelloWorld.HC` fixtures are copied byte for byte from
the pinned TempleOS tree. Both guests printed `123456/10=12345` and
the ten lines from `0A Hello World` through `01 Hello World` in
QEMU 9.2.0 with PC, `qemu64`, and 512 MiB. The boot suite also checks
DivByHand after guest-side `Cmp` and `Load`, inspects the mixed-mode
opcode bytes and loaded alignment, exercises relative imports and
image-base fixups, and checks that an unresolved local label leaves
no exported symbol. The test saves `DivByHand.BIN.Z` and
`AsmProbe.BIN.Z` under `BUILD`.

```sh
make -j4 test BUILD=build/gcc
qemu-system-x86_64 -machine pc -cpu qemu64 -m 512 -boot d -cdrom build/gcc/unholyos-startup-test.iso -display none -serial file:build/gcc/windows-pc512.log -monitor none -no-reboot -no-shutdown
```

The Windows QEMU run reached the kernel entry line after the startup
tests. The TempleOS comparison used the pinned `TOS_Distro.ISO` with
the same machine, CPU, RAM, and DVD boot settings.

The boot suite also runs the original `Demo/Asm/MulByHand.HC` and
`Demo/Asm/PutDec.HC`, both by Terry A. Davis. MulByHand exercises the
typed assembly calls, the packed 128-bit product record, and the
narrow register operands. PutDec prints `122333221` through the raw
text entry point and uses the original `PUSH_C_REGS` and `POP_C_REGS`
macro expansions.

Inside functions, the compiler emits both `asm { }` blocks and
assembly instructions written directly in the function body. The
byte-for-byte copy of Terry A. Davis's `Demo/Asm/AsmAndC3.HC` printed
`Meaning of life:42` three times in both guests with QEMU 9.2.0, PC,
`qemu64`, and 512 MiB. The demo uses carry arithmetic, a kernel
register-call routine, and the compile-time value of
`cnts.time_stamp_freq`.

The startup suite also runs `InlineAsmProbe.HC` directly and after
guest-side `Cmp` and `Load`. It checks a branch across assembly blocks,
frame-relative argument and local addresses, an imported routine, and a
counter field read from the loaded module. The suite also checks that
an unresolved inline label leaves no published function or assembly
symbol. It saves `InlineAsmProbe.BIN.Z` under `BUILD`.

`GotoProbe.HC` exercises forward and backward HolyC `goto` branches and
jumps between HolyC and assembly in both directions. The startup suite
expects `goto 42 5` from the source and from the guest-built
`GotoProbe.BIN.Z`. Pinned TempleOS produced the same output from the
same source and the UnholyOS-built module in QEMU 9.2.0 with PC,
`qemu64`, and 512 MiB. The suite also checks that missing and duplicate
labels prevent publication of the function.

The assembler reads the pinned `Compiler/OpCodes.DD` for instruction
forms and aliases. `OpcodeProbe.HC` covers integer bit operations,
conditional moves, x87 stack arithmetic, a local loop, and a scaled
stack address. The startup suite expects `opcode table 42 2 2 42` from
the source and `OpcodeProbe.BIN.Z`, which it saves under `BUILD`.
It also compares the emitted operand-size and segment prefixes and the
memory encoding for `MOV AX,[BX]` under `USE16` and `USE32`. In
`USE64`, a 32-bit register spelling in the tested memory operand leaves
the address-size prefix clear, as it does in pinned TempleOS.

Pinned TempleOS printed `opcode table 42 2 2 42` from the same source
and the UnholyOS-built module. Its source-assembled bytes matched the
UnholyOS checks for both address modes and the FS and GS prefixes.
The comparison used QEMU 9.2.0 with PC, `qemu64`, and 512 MiB. The
UnholyOS startup image is built and run with the following commands.

```sh
make -j4 test BUILD=build/opcode-table
qemu-system-x86_64 -machine pc -cpu qemu64 -m 512 -boot d -cdrom build/opcode-table/unholyos-startup-test.iso -display none -serial file:build/opcode-table/windows-pc512.log -monitor none -no-reboot -no-shutdown
```

`FixedAsm.HC` places JIT code in a HolyC array with `ORG`, inserts
machine code from a `BINFILE` path without an extension, and calls the
result. The test expects `fixed jit 1 42`. A guest-built
`FixedImage.BIN` places the TOSB header at the requested address. The
suite checks the relocated `DU64` pointer and calls the relocated
code to verify that it returns 42. It also checks that an invalid `ORG` or malformed fixed image
leaves its destination intact. A missing `BINFILE` prints the file
diagnostic and contributes zero bytes.

In QEMU 9.2.0 with PC, `qemu64`, and 512 MiB, pinned TempleOS
printed `fixed jit 1 42` from the same source. It compiled
`FixedImage.HC` with its own allocated origin and loaded it with
the header and relocated pointer at the expected addresses. The
loaded entry returned 42.

```sh
make -j4 test BUILD=build/fixed-image
qemu-system-x86_64 -machine pc -cpu qemu64 -m 512 -boot d -cdrom build/fixed-image/unholyos-startup-test.iso -display none -serial file:build/fixed-image/windows-pc512.log -monitor none -no-reboot -no-shutdown
```

`AsmBranchProbe.HC` checks literal targets, forward and backward
short displacements at the signed limits, conditional jumps, and
explicit `I16` and `I32` encodings. The suite checks bytes and return
values before and after `Cmp` and `Load`. It rejects short branches
one byte beyond either limit. `AsmBranchImport.HC` defers a target
until after loading and verifies the byte, word, and dword patches.
The fixed-origin test also checks the `EB 08` jump and 16-byte payload.
Pinned TempleOS matched the sampled bytes and returned 41 through 45
from the branch probe and the UnholyOS-built module. It also applied
the same three patches to the UnholyOS-built import module. The
comparison used QEMU 9.2.0 with PC, `qemu64`, and 512 MiB.

The startup suite also checks all 100 lines from Terry A. Davis's
unmodified `Demo/Asm/BuzzFizz.HC`. It uses short branches in a loop
with calls to the kernel text routines. [Pinned BuzzFizz source](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Demo/Asm/BuzzFizz.HC)

```sh
make -j4 test BUILD=build/asm-branches
qemu-system-x86_64 -machine pc -cpu qemu64 -m 512 -boot d -cdrom build/asm-branches/unholyos-startup-test.iso -display none -serial file:build/asm-branches/windows-pc512.log -monitor none -no-reboot -no-shutdown
```

`AsmListingProbe.HC` checks `LIST` and `NOLIST` with tabs, blank
lines, comments, wrapped byte rows, and a forward branch before its
displacement is fixed up. The raw JIT listing and the DolDoc listing
during `Cmp` match the pinned guest's 798 bytes. Both guests load the
UnholyOS-built module and get 42 from its exported entry. `AsmListingScope.HC` checks byte formatting at a 64 KiB
boundary, the end of an assembly block, and a listing inside a HolyC
function. Its 308 output bytes also match the pinned guest.

```sh
make -j4 test BUILD=build/asm-listings
qemu-system-x86_64 -machine pc -cpu qemu64 -m 512 -boot d -cdrom build/asm-listings/unholyos-startup-test.iso -display none -serial file:build/asm-listings/windows-pc512.log -monitor none -no-reboot -no-shutdown
```

The TempleOS comparison uses QEMU 9.2.0 with PC, `qemu64`, and
512 MiB. Full opcode, operand, and directive coverage is pending.

## HolyC switch statements

The startup suite runs Terry A. Davis's original `Demo/NullCase.HC` and
`Demo/SubSwitch.HC` from byte-for-byte copies. They exercise implicit
case values and the shared code before and after a `start`/`end`
group. Their output matches pinned TempleOS in QEMU 9.2.0 with PC,
`qemu64`, and 512 MiB.

`SwitchProbe.HC` checks forward and reversed case ranges, fallthrough,
default dispatch, and two levels of `start`/`end` inside an unchecked
`switch []`. The suite expects `switch 12 2 -1 99 17 34` from the
source and the guest-built `SwitchProbe.BIN.Z`. Pinned TempleOS
produced the same line from the source and the UnholyOS-built module.
The suite rejects overlapping cases and saves the module under `BUILD`.

## Cooperative task scheduling

`make -j4 test BUILD=build/scheduler` builds
`build/scheduler/unholyos-startup-test.iso` and runs
`SchedulerProbe.HC` from source and as a guest-built `.BIN.Z` module.
The probe checks the child task pointer, suspended tasks, single-user
mode, stack arguments, PIT wakeups, and x87 state across a switch.
It checks termination by return and by `Exit`. The suite also checks
that the dying queue drains and accounts for the idle task's pages in
the code and data pools.

Pinned TempleOS and UnholyOS printed `sched 1234 1 1 1 1` from both
the same source and the UnholyOS-built module. Both guests ran under
QEMU 9.2.0 with the PC machine, `qemu64`, and 512 MiB.

The scheduler runs on Core 0. `Sleep` and `SleepUntil` yield there, and
the timer wakes the idle task when the runnable queue has no ready work.
The cross-core job queue, window focus and keyboard controls, and task
end callbacks are pending.

## Task jobs and messages

`make -j4 test BUILD=build/jobs` runs `JobProbe.HC` from
`build/jobs/unholyos-startup-test.iso` and builds and runs
`JobProbe.BIN.Z` in the guest. The probe checks message masks, paired
key-down and key-up events, and zeroed output arguments on an empty
scan. It checks source evaluation in another task, completion results,
an input-filter task, and a servant exiting after completing a job.
Pinned TempleOS and UnholyOS printed the same three result lines from
the same source and from the UnholyOS-built module under QEMU 9.2.0 with
the PC machine, `qemu64`, and 512 MiB.

Cross-core dispatch remains pending.

## PS/2 keyboard

`make -j4 test BUILD=build/keyboard` boots
`build/keyboard/unholyos-startup-test.iso` and sends keys through the
QEMU monitor. `KeyboardProbe.HC` checks the conversion tables, an empty
scan, and IRQ1 waking Adam while `GetKey` waits. It checks an extended
cursor key and a shifted character.

The suite runs a byte-for-byte copy of Terry A. Davis's
`Demo/MsgLoop.HC` with the DolDoc lexer enabled. Pinned TempleOS and
UnholyOS produced the same tested message records for normal, shifted,
extended, and Caps Lock keys. The trace includes the two LED command
acknowledgements that TempleOS reports as key-up records after a lock
change. Both guests used QEMU 9.2.0 with the PC machine, `qemu64`,
and 512 MiB. UnholyOS sends text output to serial during the boot probe.

## PS/2 mouse

`make -j4 test BUILD=build/mouse` sends relative motion, button
transitions, and wheel events to the PS/2 device in QEMU.
`MouseProbe.HC` checks the negotiated packet size, IRQ12, both button
bits in `kbd.scan_code`, and window-relative message arguments.
It checks that `MsSet` preserves the unsnapped position alongside the
snapped one, and that a second click in the configured time window
produces double-click messages.
Pinned TempleOS and UnholyOS produced the same 14-line probe trace under
QEMU 9.2.0 with the PC machine, `qemu64`, and 512 MiB.

The bootstrap input service drains the shared controller queues.
The full graphics desktop and interactive window controls remain
pending.

## VGA palette

`make -j4 test BUILD=build/palette` runs `PaletteProbe.HC` in QEMU.
The same probe produced matching traces on pinned TempleOS and
UnholyOS under QEMU 9.2.0 with the PC machine, `qemu64`, and 512 MiB.
It checks palette tables, color names, packed color arguments, VGA DAC
rounding, and restoring a palette read from the device. The boot test
captures the displayed pixels while white is remapped to yellow and
again after the original palette is restored. It repeats the input
and display checks after rebooting the guest-built ISO.

## Graphics device contexts

`make -j4 test BUILD=build/gr-dc` runs `DCProbe.HC` at startup and after
booting from the guest-built ISO. It checks the record offsets,
screen contexts, row padding, alias and copy behavior, depth,
fixed-point transforms, lighting, and compressed and raw GR data.
Pinned TempleOS and UnholyOS produced the same 15-line trace with
QEMU 9.2.0, the PC machine, `qemu64`, and 512 MiB. The 118-byte
`DCProbe.GR.Z` file matched byte for byte.

`BitmapDiffProbe.HC` checks equal images, a changed rectangle, reversed
and clipped extraction, the padded-width difference-scan behavior,
bitmap sprite bytes, transparent replay, and document insertion through
`DocGR`. The focused boot in UnholyOS and the identical source executed
in pinned TempleOS both produced `bitmap diff 7F E76D01B74919B2D4`.
The comparison used QEMU 8.2.2, PC, `qemu64`, and 512 MiB. The boot
suite checks the same trace at startup and after the guest-built ISO
boots. This does not verify the remaining sprite editor or movie APIs.

## 3D bitmap drawing

`Blot3Probe.HC` checks the bitmap contents and context state after
matrix transforms, depth rejection, planar reflection, a custom HolyC
transform callback, and a packed sprite stream that toggles transforms.
The same source produced five matching trace lines on pinned TempleOS
and UnholyOS with QEMU 9.2.0, PC, `qemu64`, and 512 MiB. The boot suite
checks those lines at each tested RAM size and after booting the ISO
built in the guest.

## Round pens and 3D lines

`PenProbe.HC` checks all pen masks and circle span tables, clipped
circles, dither, collision counts, and depth-buffered thick lines.
It also exercises reflected and stepped strokes, custom brushes,
and packed sprite shapes under a scaled transform. The same HolyC
source produced nine matching trace lines on pinned TempleOS and
UnholyOS with QEMU 9.2.0, PC, `qemu64`, and 512 MiB. The boot suite
checks them on the startup-test ISO and on the ISO rebuilt from it
inside the guest.

## Rotating polygons

Both boot discs include Terry A. Davis's unchanged
`Demo/Graphics/3DPoly.HC` from the pinned TempleOS source. Run
`ExeFile("::/Demo/Graphics/3DPoly.HC");` in a maximized HolyC
terminal and press Q to exit. The boot suite starts the original
source and checks that VGA polygon pixels change across frames. It
sends Q, checks that the prompt returns, and checks that the pixels
clear. The guest-built ISO goes through the same check.

`MatrixProbe.HC` compares rotation and matrix multiplication,
a sequence of 64 rotation updates, planar symmetry, and the
refresh counter and flags. Both guests produced the same six
trace lines under QEMU 9.2.0 with PC, `qemu64`, and 512 MiB.

## Task settings and the rotating box

Both boot discs include Terry A. Davis's unchanged
`Demo/Graphics/Box.HC`. Run `ExeFile("::/Demo/Graphics/Box.HC");` in
a maximized HolyC terminal. The task's draw callback renders the box
as it rotates and scales. Press a key to exit. The boot suite checks
for moving VGA pixels, return to the prompt, and removal of the box
on the standard ISO and on the ISO rebuilt in the guest.

`TaskSettingsProbe.HC` checks the native record layout, nested settings,
window borders, callback restoration, animation and song task state,
and restoration of the directory, document flags, and palette. The same
fixture produced six matching trace lines in both guests under QEMU
9.2.0 with PC, `qemu64`, and 512 MiB. Under those settings both guests
also ran the same 1,273 bytes of `Box.HC`, animated the box, and returned
to the prompt on Q.

## Mouse symmetry demo

Both boot discs include Terry A. Davis's unchanged
`Demo/Graphics/Symmetry.HC`. Run `ExeFile("::/Demo/Graphics/Symmetry.HC");`
in the HolyC terminal. Left-drag draws a stroke and its reflection.
Right-drag changes the axis and leaves it visible in red. A stationary
right-click disables reflection. Escape erases the drawing and restores
the terminal window.

On pinned TempleOS and UnholyOS, a sequence of PS/2 mouse events produced
the same strokes and axis pixels under QEMU 9.2.0 with PC, `qemu64`,
and 512 MiB. The boot suite checks the default axis, a user-drawn axis,
the single stroke after disabling reflection, and cleanup on Escape
on the standard ISO and the one built in the guest.

## Window border gestures

Drag the title row or left edge of a bordered window to move it. The
right edge resizes the width, the bottom edge resizes the height, and
the bottom-right corner resizes both. The window comes to the front
when the drag begins. The geometry follows the pointer in 8-pixel
text cells and stays where you release it.

The terminal border shows `MENU` and `[X]`. Clicking `[X]` sends
Shift+Escape to a document-backed task. A task without a put document
is marked for termination. The `MENU` area posts the reference Ctrl+M
key message. The Personal Menu dialog still needs its DolDoc command
handler.

## Terminal scrollbars

Drag the thumb on the right to scroll through lines and the thumb
along the bottom to scroll across columns. The wheel moves the
document in the focused window under the pointer. A thumb takes
the mouse gesture before the window border can resize.

The boot test uses a document of 60 wide lines, checks a wheel notch
and both thumb drags, and verifies that the window bounds stay
fixed. TempleOS can draw a horizontal offset during the drag and
return to the start of a plain text entry after release. The test
checks both frames. `ScrollControlsProbe.HC` checks the control
layouts, inclusive hit bounds, visibility flags, and HolyC callbacks.

`DocRefreshProbe.HC` checks that a partial pass leaves a pending
scroll request in place and skips a live tag below the viewport. It
also checks a scheduled full pass, a forced refresh, and a document
with a backward cursor movement.

## View angles and shading

Run `ExeFile("::/Demo/Graphics/Shading.HC");` to open Terry A. Davis's
unchanged demo. Move the pointer to change the light source and drag
the X, Y, and Z sliders to rotate the sphere. Press a key to leave
the demo and restore the terminal.

The boot suite checks the shaded hemispheres, scale ticks, X slider
capture, and cleanup. `ViewAnglesProbe.HC` checks the record layout,
angle snapping and clamping, the three slider callbacks, and the math
calls used by the demo.

## Pulldown menus

Run `ExeFile("::/Demo/PullDownMenu.HC");` to open Terry A. Davis's
original demo. Move the pointer to the top of the screen, hold the
left button, then drag to a menu item and release. For keyboard
selection, hold the Windows key, use the arrow keys, and release
the Windows key on the highlighted item.

The boot suite selects two commands and exits the demo on the
standard and guest-built discs. `MenuProbe.HC` loads a menu from a
file and checks the public records, nested lookup, command arguments,
and stack restoration. Pinned TempleOS and UnholyOS drew the same
menu bar, selected and checked entries, and shortcut callout under
QEMU 9.2.0 with PC, `qemu64`, and 512 MiB.

## Digits

Both boot discs include Terry A. Davis's unchanged
[Demo/Games/Digits.HC](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Demo/Games/Digits.HC)
(1,933 bytes, SHA-256
`d6b6901052ca2bdb6aa92d2c9cd49250bfbf7bb426775dd8c9848c616fd5ba45`).
Run `ExeFile("::/Demo/Games/Digits.HC");` in the HolyC terminal.
Press a key to start, memorize the colored digits, and enter them after
the next prompt. A wrong answer reveals the pattern and score. Escape
or Shift-Escape returns to the terminal.

With `Seed(12345)` and the same QEMU 8.2.2 PC, `qemu64`, and 512 MiB
settings, both guests displayed `4`, advanced to `40` after a correct
answer, showed score 2 and the correct pattern after entering `41`, and
restarted with `9`. The introduction wraps at the same word boundary.
All 328 compared nonblank character and color-attribute cells matched
across the introduction, patterns, guesses, score, restart, and restored
prompt. The comparison excludes timing-dependent output, cursors, and
the reference's separate autocomplete window. UnholyOS also passed
Shift-Escape at the introduction. The two beeps run through the existing
speaker implementation; their audio was not compared in this test.

The focused check builds and boots the standard disc using:

```sh
make -j4 BUILD=build/digits-check
make BUILD=build/digits-check build/digits-check/Linux/TestBoot
build/digits-check/Linux/TestBoot --digits qemu-system-x86_64 \
  build/digits-check/unholyos.iso build/digits-check/native
```

The QEMU driver sends keyboard input and captures the 80×60 text layer
at each game phase. The same check runs in the startup and guest-built
ISO suite. The reference run used the pinned distribution disc and
`-machine pc -cpu qemu64 -m 512 -boot d -cdrom <reference.iso> -display
none`; QEMU's monitor captured the corresponding text layer after the
same input sequence.

## Character graphics and Maze

Both boot discs include the unchanged originals by Terry A. Davis:

- [Demo/Games/CharDemo.HC](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Demo/Games/CharDemo.HC),
  SHA-256 `f9aa176f08dcd25f259b17f54638e703029878950fd54973c945d46cf4de217b`.
- [Demo/Games/Maze.HC](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Demo/Games/Maze.HC),
  SHA-256 `b6701b8e161069668b19585eb071a43cad21bc05c659599dd2186b42c5a6a6c4`.

`MemSetProbe.HC` exercises typed fills of 8-, 16-, 32- and 64-bit
elements. It checks the end-pointer return value, zero counts, guard
elements, truncation, the packed text attributes and the previous-state
return values of `DocCursor`, `DocHighlightCursor` and `DocScroll`.
The startup test expects `Memory set probe 3FF`.

The focused C++ driver runs the original game sources in a user task.
With `Seed(12345)`, CharDemo cycles the four water glyphs, pans by one
pixel at a time, moves 32 pixels in each direction, returns to its
initial viewport, and regenerates its map on Enter. The test compares
2,925 packed character/attribute cells per phase with fingerprints
recorded from the pinned guest. It checks the original font pointer,
task draw and animation callbacks, cursor and scroll flags, window
geometry, and panning state after Escape.

For Maze, the driver moves the mouse to pixel (240,416), holds the
left button while moving across seven cells, and releases it. The
wall occupies text cells (30,52) through (36,52). Right-click at
pixel (264,392) starts the solver at (33,49). The path goes down
to (33,51), around the right end of the wall, and down to the lower
boundary without overwriting the wall. Enter clears the board and
Escape restores the terminal. The driver verifies the wall and the
first eight path cells; the captures also include the VGA framebuffer.

Run the native regression with QEMU 8.2.2, PC, `qemu64` and 512 MiB:

```sh
make -j4 BUILD=build/character-check
make BUILD=build/character-check build/character-check/Linux/TestBoot
build/character-check/Linux/TestBoot --character-graphics qemu-system-x86_64 \
  build/character-check/unholyos.iso build/character-check/native
```

To replay against the pinned distribution, invoke the same driver from
the repository root with `--character-graphics-reference` and the
reference ISO. It declines installation and the tour, obtains the text
layer address from the guest's debug port, and uses GDB on local port
45362 to copy the exact memory-set probe bytes into a guest allocation.
The original demos execute from the reference DVD. The normal test uses
QMP over a private socket pair and does not require GDB.

This covers the tested keyboard, mouse, text-layer and cleanup paths,
not every map boundary, maze shape, menu action, animation timing, or
speaker output. No complete framebuffer or audio parity is claimed.

## Circle and ellipse drawing, TicTacToe, and breaks

`ArcProbe.HC` exercises the fixed-point walkers from
`Adam/Gr/GrMath.HC` and the drawing calls in `GrPrimatives.HC`. The
same source ran on the pinned TempleOS and UnholyOS guests with QEMU
8.2.2, PC, `qemu64`, and 512 MiB. Both printed:

```text
arc circle 1 83 8522C8530877871E
arc callback 0 5 409AA16A15CF98DD
arc ellipse 1 78 A59227EF9C0EA666
arc empty 1 1 1 0
arc flat 1 1 1 D7FD42D28F7CAFEC 0F40B14DF3F0EAC5
arc depth 1 1 1 5A41603FA1357905 0CFDDB2F46FF60E4
arc transform 1 1 100 648DEC773C30296A 1535EAD7BE9572E3
```

The probe SHA-256 is
`fe1e9b2833d7251ecb528ab2c795bd0134472752b74befcedee151cd266e7ef9`.
It checks callback order and early termination, reverse stepping,
clipping, depth-buffer preservation, thick pens and transformed
ellipses. The eleven-argument `Ellipse` call also exposed a missing
stack argument in the HolyC-to-C++ adapter. It now forwards every
argument beyond the six System V registers on an aligned stack.

Both boot discs include Terry A. Davis's unchanged
[Demo/Games/TicTacToe.HC](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Demo/Games/TicTacToe.HC),
SHA-256 `2a9e2b00fc87e572d81bbfda190646b75f22b95b331610047e122cb5f1b5034b`.
The driver uses real mouse movement and button events. It reads the
compiled board through its guest hash entry, waits for the guest's
refresh count, and checks that invalid clicks leave the board unchanged.
It plays a win for each player, checks the resets, sends Ctrl-Alt-C,
checks the caught exception tag and cleared board, then executes
another terminal command.

All 14 captured 640×480 drawing buffers were byte-identical between the
two guests. The regression checks the pinned fingerprints of the
307×307 board region at each phase. Audio output, frame timing,
extreme arc parameters, and other sprite shapes are outside this
comparison.

`BreakProbe.HC` separately tests direct breaks, previous-state returns
from `BreakLock` and `BreakUnlock`, actual Ctrl-Alt-C while locked,
pending exception delivery, Shift-Esc conversion and cleanup of the
exception queue. Both guests reported `Break probe 7F`. Its SHA-256 is
`f2bddabc37572a13d5988c17aaa2326568a6ad448b0f056827daae3fb9d915c3`.
The scheduler delivers a break at a saved task context. It does not
yet preempt arbitrary busy user code or implement the full callback
registry or debugger. Exception diagnostics still use serial output.

```sh
make -j4 BUILD=build/gr-arcs
make BUILD=build/gr-arcs build/gr-arcs/Linux/TestBoot build/gr-arcs/break-test.iso
build/gr-arcs/Linux/TestBoot --tic-tac-toe qemu-system-x86_64 \
  build/gr-arcs/unholyos.iso build/gr-arcs/tic-native
build/gr-arcs/Linux/TestBoot --break qemu-system-x86_64 \
  build/gr-arcs/break-test.iso build/gr-arcs/break-native
```

The same driver accepts `--tic-tac-toe-reference` and
`--break-reference` with the pinned distribution ISO. The latter
copies the exact probe into allocated guest memory with GDB and
checks the uploaded bytes before executing it. Native regression
runs do not require GDB.

## Flood filling

`FloodFillProbe.HC` compares region fills, boundary-color fills, dither
and XOR pixels, depth rejection and boundary depth updates, symmetry,
and transformed coordinates. It checks restoration of the brush,
secondary color, flags and recorded extents. Both the pinned reference
and native PC guests report `flood fill 1FF`; all seven printed counts
and pixel/depth fingerprints agree. The startup and guest-built ISO
tests require those exact lines. Run the reference fixture with
`build/Linux/TestBoot --flood-fill-reference qemu-system-x86_64
<reference.iso> build/flood-reference`. The driver verifies the uploaded
source bytes before executing it in a 512 MiB QEMU PC guest. Native
verification uses the regular startup test ISO. Coverage does not yet
include overlapping windows, probability dithering or the fixed ray
stack's exhaustion behavior.

`SpriteFloodProbe.HC` exercises both sprite flood-fill elements in a packed
stream. The boundary-color element uses only the primary color, then restores
the dither color for the next point. The probe also checks transformed,
depth-buffered single-element drawing, including restoration of the caller's
color, flags and brush. The native guest reported `sprite flood 0` before
dispatch was enabled. Both guests now report `sprite flood F`, with matching
stream (`95BB364C84E33F56`), single-element bitmap (`026661834AEEFF05`)
and depth (`66901F20C6B79EE1`) fingerprints. The reference driver uploads
and verifies all 1,840 source bytes and runs the fixture on the pinned DVD:

```sh
build/Linux/TestBoot --sprite-flood-reference qemu-system-x86_64 \
  <reference.iso> build/sprite-flood-reference
```

Both focused guests use QEMU 8.2.2, PC, `qemu64` and 512 MiB. The regular
startup and guest-built ISO tests check the same three output lines.

The original [Demo/Graphics/Blot.HC](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Demo/Graphics/Blot.HC)
by Terry A. Davis is included without changes. The imported checkout
contains 735 bytes (SHA-256
`5a98a574fdb3341816fbc55877f125abfa30229e7e06f68c162bc6de6fc0e35e`).
The interactive test runs the animated brush and bitmap composition,
waits for the key prompt, records the graphics layer, and presses Space.
The two guests' complete drawing buffers match byte for byte (SHA-256
`897974d37f94c5224c05da2064c18684626a779738c013d8b8125bd14b7f40f4`).
The cleared buffers also match. Both guests then evaluate another terminal
expression successfully. The native test is part of `make test`; the
reference run uses `build/Linux/TestBoot --blot-reference qemu-system-x86_64
<reference.iso> build/blot-reference`. Both use QEMU 8.2.2, PC,
`qemu64`, 512 MiB and a private QMP socket.

## Regular polygons, Bezier curves, and B-splines

`SplineProbe.HC` exercises the typed callbacks, distance helpers,
partial and complete polygons, early callback refusal, open and closed
splines, short control arrays, and flat, depth-buffered and transformed
drawing. Its SHA-256 is
`67317368378ec3a1d943573ab2fa8c1410126f0cd7fb58c6c08b87f443e868b8`.
All 15 output lines from the pinned guest and the native boot probe
matched. The reference memory capture contains the exact fixture bytes
at offset 4096 of the uploaded 16 KiB buffer. The startup and guest-built
ISO checks require the recorded callback counts, coordinates and bitmap
and depth fingerprints.

Both native discs include Terry A. Davis's unchanged
[Demo/Graphics/BSpline.HC](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Demo/Graphics/BSpline.HC),
SHA-256 `6bf2b7033228a506ab300fb5d2e2c4840e7dde7618711a6310e7ecbbdb8f95b1`
for the byte-identical 733-byte Windows checkout. The pinned Git blob
has LF line endings (698 bytes); the Windows checkout uses CRLF.
The `-text` attribute preserves the imported checkout bytes.
The interactive driver uses five left-button releases at window-relative
pixels (130,130), (360,100), (400,290), (200,320) and (110,230), followed
by a right-button release at (300,380). The original program marks the
control points in green, then draws closed quadratic and cubic curves
in red and blue. All eight drawing buffers (initial, after each control
point, after drawing the curves, and after exit) were identical across
the two guests. The completed 640×480 curve drawing has SHA-256
`02f9a0f7f46b19ece113744ae9f5cd210e0ad26c3129ff2ae54a31577a2a038f`.
The regression compares the complete buffer fingerprints at each phase.
On Enter, both guests clear the drawing to transparent, preserve their
previous window-inhibit mask and three scroll offsets, and execute
another terminal expression returning 63.

Run the focused native and reference tests using QEMU 8.2.2:

```sh
make -j4 BUILD=build/gr-splines
make BUILD=build/gr-splines build/gr-splines/Linux/TestBoot
build/gr-splines/Linux/TestBoot --spline qemu-system-x86_64 \
  build/gr-splines/unholyos.iso build/gr-splines/spline-native
build/gr-splines/Linux/TestBoot --spline-reference qemu-system-x86_64 \
  <reference.iso> build/gr-splines/spline-reference
make -j4 BUILD=build/gr-splines test
```

Both interactive guests use PC, `qemu64`, 512 MiB, CD boot, no display
window, a private QMP socket pair and a separate serial log. The reference
ISO SHA-256 is
`3c68ca395d0b64f779f1ffa330e2685798b17862e3c36ae4e349537fd676b40b`.
The native ISO SHA-256 for this run is
`2b7ad4a7b723c647732efb2612678230042fb56186211b286f6380a8ad58874e`.
The checks cover the tested curve and input paths; extreme coordinates,
allocation failure, all mirror modes and full VGA frame parity remain
outside this comparison.

## Arrows, raster text, and the common ancestor demo

Both boot discs include Terry A. Davis's unchanged
`Demo/Graphics/CommonAncestor.HC`. Run
`ExeFile("::/Demo/Graphics/CommonAncestor.HC");` in the HolyC terminal.
The demo draws the tree with arrowheads and numeric labels while it
builds the tree. Hold Ctrl and drag with the left mouse button to pan
the tree. Release the left button to leave it in place. Ctrl-right
resets the pan. Release Ctrl and press a key to return to the terminal.

With seed 12345, QEMU 9.2.0, PC, `qemu64`, and 512 MiB, both
guests matched the tree at panned offsets of (48, 32) and (-16, -8),
and kept it in place after the first button release. The pixel
comparison excludes the terminal caret cell. The UnholyOS boot suite
also checks the right-button reset and that the tree clears on exit.
`WinScrollProbe.HC` checks all three saved, cleared, and restored
offsets through the native HolyC calls.

`ArrowProbe.HC` produced seven matching traces on pinned TempleOS
and UnholyOS with QEMU 9.2.0, PC, `qemu64`, and 512 MiB. It checks
arrow and glyph pixels, depth, transforms, reflection, the packed
arrow element, cursor clipping, and glyph raster operations. Both guests
also ran the original tree demo with seed 12345 and drew the same
tree pixels. The comparison excludes the terminal caret cell. The boot
suite checks the probe on the startup-test and guest-built discs and
runs the demo on the standard and guest-built discs.

## Mutable fonts and text cells

Both discs include Terry A. Davis's unchanged
[`Demo/ExtChars.HC`](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Demo/ExtChars.HC)
(SHA-256 `c32514d7e3bf31fedf2bae25a10b49f4e3a365c46e5fa9b0256e5f58e07bb663`).
It modifies glyph 255 and uses extended characters in both a string and
a HolyC identifier. Ctrl+Alt+F swaps the standard and Cyrillic fonts.

With QEMU 8.2.2, PC, `qemu64`, and 512 MiB, the pinned guest ran
`FontTextProbe.HC` through `ExePutS` after the source was copied into
guest memory using the debugger. The reference returned a size of 88
for `CTextGlbls`, both full-font hashes, clipped attribute cells
`2F42 3043 4144`, formatted cells `1E42 1E37 4144`, all 64 custom-glyph
pixels, and two successful keyboard-driven font swaps. Running the
original demo from its disc produced glyph `3C42BDC381A5423C`.
The boot suite checks these results in UnholyOS, including the
original demo's extended-character output, and restores the glyph.

## Depth-buffered spans and polygons

`DepthSpansProbe.HC` checks span counts, depth interpolation, window
clipping, extents, collision counts, and dither RNG state. It also
compares the color and depth buffers for 64 triangles that exercise
clipping, a transformed polygon, its reflection, and a rectangular
sprite.
The same source printed 11 matching lines on pinned TempleOS and
UnholyOS under QEMU 9.2.0 with PC, `qemu64`, and 512 MiB. The boot
suite checks those lines at each tested RAM size and on the ISO
built in the guest.

## 2D raster drawing

`make -j4 test BUILD=build/raster` exercises clipping, extents, depth,
window coverage, masks, seeded dithering, and line stepping.
`RasterProbe.HC` produced the same 15 output lines and the same 137-byte
`RasterProbe.GR.Z` file on pinned TempleOS and UnholyOS under QEMU 9.2.0
with the PC machine, `qemu64`, and 512 MiB.

`Linux/Fixtures/Lines.HC` is Terry A. Davis's unchanged
`Demo/Graphics/Lines.HC` from the pinned TempleOS tree. The boot test
checks that it draws into `gr.dc` and clears the bitmap after QEMU
sends a key. Both programs run again after rebooting the guest-built
ISO. The screen test also checks that the demo reaches VGA.

## VGA screen presentation

`make -j4 test BUILD=build/vga-screen` captures 640×480 QEMU frames
from `ScreenProbe.HC`. The test checks all 16 colors, bits on opposite
sides of a byte boundary, the boot glyph, a one-row change, 2× zoom,
and clearing the overlay. It also looks for live drawing from the
unchanged `Lines.HC` demo. The frame checks run again after rebooting
the guest-built ISO.

Pinned TempleOS and UnholyOS use the same four-plane bit order. In
QEMU 9.2.0 with the PC machine, `qemu64`, and 512 MiB, the probe matched
all 3,168 pixels in each of the normal and changed color strips and
all 12,672 pixels in the 2× region.
The reference's desktop remains visible around the probe.

## Cursor and zoom

`make -j4 test BUILD=build/cursor` drives `CursorProbe.HC` and checks
the 640×480 VGA frame at each stop. It checks the default arrow, XOR
colors for left and right mouse buttons, hiding the cursor, a custom
HolyC drawing callback, and dispatch to the grab-cursor callback.
The probe sends Ctrl+Alt+Z, moves the pointer outside the zoomed view,
and checks recentering and fractional zoom. A separate probe throws
from a HolyC cursor callback and checks that the refresh task recovers.
Both probes run again after booting the guest-built ISO.

On matching QEMU 9.2.0 PC guests, 900 pixels matched pinned TempleOS
across the normal, left, right, hidden, custom, and 2× regions. The
recentered 2× cursor matched its shape and location across 400 pixels
on the two desktops' backgrounds. The bootstrap tests grab-cursor
dispatch by setting the grab flags directly. TempleOS resets those
flags in its interactive window loop.

## HolyC function pointers and Grid

`make -j4 test BUILD=build/fptr` runs `FunctionPtrProbe.HC` from
source and as a guest-compiled `.BIN.Z`. The probe checks local, global,
member and parameter declarations, default arguments, pointer arrays,
nested calls, and both stack cleanup conventions. The pinned guest
faulted while running this stress probe.

`VariadicProbe.HC` and `VariadicPtrProbe.HC` exercise native variadic
definitions, default arguments, empty and nonempty argument lists, raw
floating-point and pointer values, writable `argc` and `argv`, recursion,
right-to-left evaluation, and repeated direct and indirect calls. Both
fixtures ran from source and guest-compiled `.BIN.Z` files in QEMU 8.2.2
with PC, `qemu64`, and 512 MiB. All six result lines matched the pinned
TempleOS guest running the same source. TempleOS also warned that `argc`
was unused in `VaRaw`; UnholyOS did not emit that warning.

The focused build used `BUILD=build/variadic-check` and the ISO
`build/variadic-check/variadic-aot.iso`, containing the two fixtures,
`StartOS.HC.Z`, and `FileWriteProbe.BIN` to mount the RAM drive. The VM
command was `qemu-system-x86_64 -machine pc -cpu qemu64 -m 512 -boot d
-cdrom build/variadic-check/variadic-aot.iso -display none -serial
file:build/variadic-check/aot.log -monitor none -no-reboot`. The startup
and guest-built ISO tests also check the six lines in both execution modes.

`Linux/Fixtures/Grid.HC` contains the original 672 bytes of Terry A.
Davis's [Grid demo](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Demo/Graphics/Grid.HC).
QEMU sends PS/2 motion to move the 5×5 cursor to a new grid cell. A
left click ends the demo, and the test checks that the prior callback
and grid settings are restored. The first and moved cursor patches
matched pinned TempleOS across 162 RGB pixels with the same VM settings
and black background. The boot driver runs the probes again from the
guest-built ISO.

## Class member metadata

`MemberMetaProbe.HC` checks the ABI sizes and offsets, inherited member
lookup, array dimensions, adjacent format strings, duplicate metadata,
zero versus missing values, floating-point bits, and a callable metadata
address. Both guests reported `Member metadata 7F`.

The original 2,169-byte [ClassMeta.HC by Terry A. Davis](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Demo/ClassMeta.HC)
is stored unchanged at `Demo/ClassMeta.HC`. Its SHA-256 is
`940a019dce1872cc8053ca34b718714e8dcde762482c066b44e6d13593c46e3e`.
Both guests produced these six lines:

```text
age:44 default:38
color: default:4
rank:3 default:3 Admiral
age:22 default:38 percentile: 54.20
rank:2 default:5
serial_num: 55555 default:123456
```

`LastClassProbe.HC` checks inferred names for class pointers, scalar
aliases, literals, explicit overrides, omitted arguments, and ordinary
string defaults. Source execution and guest-compiled `.BIN.Z` execution
both produced these lines in both guests:

```text
Lastclass names I64i I64i I64i F64
Lastclass probe 0F
```

The reference JIT ran the fixture and original demo with `ExePutS`.
For the reference AOT run, the fixture body was unchanged and preceded
by the normal `KernelA.HH` and `CompilerA.HH` includes and these imports:

```hc
_intern IC_FS CTask *Fs();
_import _STRCMP I64 StrCmp(U8 *a,U8 *b);
import U0 Print(U8 *fmt,...);
```

The pinned AOT compiler starts without those headers. The UnholyOS AOT
compiler seeds its native declarations directly. The reference also
issued unused-parameter warnings that the native compiler does not emit.
Both loaded the resulting module and ran its initializer successfully.

The focused UnholyOS build used the default `-O2` flags and
`make -j2 build/unholyos.iso build/unholyos-startup-test.iso build/Linux/TestBoot`.
The test ISO was made with `build/Linux/MakeISO`, the built DVD loader
and kernel, `Linux/Fixtures/StartOS.HC.Z`, compressed `Doc/Start.DD`,
the Home directory, `FileWriteProbe.BIN`, and the three sources above.
It ran under QEMU 8.2.2 using:

```sh
qemu-system-x86_64 -machine pc -cpu qemu64 -m 512 -boot d \
  -cdrom build/member-focused.iso -display none \
  -serial file:build/member-focused.log -monitor none -no-reboot
```

The pinned `TOS_Distro.ISO` ran with the same machine, CPU, memory and
boot order, using an allocated guest buffer for the exact source bytes
and port `0xE9` for output. The startup and guest-built ISO test driver
also checks all four program outputs. The focused kernel ended its BSS
at `0x9300b`, below the relocated DVD loader limit of `0x96600`.

## Class reports

The original 627-byte [LastClass.HC by Terry A. Davis](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Demo/LastClass.HC)
is stored unchanged at `Demo/LastClass.HC`, SHA-256
`15bc1334d6cb2bde4703551c1ec821577e08fd3e1c09eff6640fd123f3f3a1d0`.

`ClassReportProbe.HC` checks inherited and nested reports, static scalar
formatting, bound scalar entries, refresh and write-back, keyboard editing,
tree expansion, failed lookups, depth limits, and the `CCPU` ABI. Both
guests produced the same static report, including the field offsets and
signed hexadecimal values, followed by `Class report 7FF data 20`.

The pinned guest ran the exact source bytes with `ExePutS`, using the
same PC, qemu64 CPU, and 512 MiB configuration as the native focused ISO.
The original LastClass demo ran to completion after two actual keypresses
in both guests. The inferred names were `CTask`, `Student`, `School`, and
`I64i`. The native interactive driver executes `LastClassRun.HC`, which
loads the original file and checks the CPU and task report headings and
live data entries. It observes `LastClass reports 7` before returning to
the prompt. Report counts and pointer addresses are not expected to match
because native task reflection and runtime records are still partial.

## Document forms and database

The original demos by Terry A. Davis are copied without changes from the
pinned TempleOS checkout:

- [Demo/Dsk/DataBase.HC](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Demo/Dsk/DataBase.HC), SHA-256
  `06f38c093a9a3bae9ef5f8a2b00ab8919e251b2eab35034f8bb80c877854985f`.
- [Demo/DolDoc/Form.HC](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Demo/DolDoc/Form.HC), SHA-256
  `d12fba94ec869f2becd9580f166e8fce1aa55068320450d93dddd6fec45fecbf`.

The interactive regression mounts a temporary 2,048-block RedSea RAM drive
as `B:`, sets the home directory to `B:/`, and runs the original database
from the boot DVD. It enters two accounts, saves an edit to account 7,
cancels a second overwrite, reads account 7 through the demo, then opens
the resulting `DataBaseDemo.BIN` using `FOpen` and verifies its 51,200-byte
size and both records with `FBlkRead`. It checks `Ada / 13 Pine` and
`Bob / 45 Elm`, including backspace editing of the input fields. No host
disk is attached to the VM. The database persists in the guest RAM drive
for this session, not across a reboot.

The original form demo displays four numeric values, a list, a fixed-length
name, two allocated address strings, a date and a checkbox. The test
selects Adult, changes the name to Jane, enters `Unit 2` in the empty
address field, unchecks Check and saves. It compares the reported values
with the pinned guest, including the unchanged numeric defaults and the
first address. The list chooser and field content are tested with QEMU
keyboard events and text-layer captures; full editor chrome and mouse
behavior are not compared.

Run the native regression with QEMU 8.2.2, `pc`, `qemu64`, 512 MiB:

```sh
make -j4 BUILD=build/docform
make BUILD=build/docform build/docform/Linux/TestBoot
build/docform/Linux/TestBoot --database qemu-system-x86_64 \
  build/docform/unholyos.iso build/docform/database-native
```

For the reference replay, use the same driver with `--database-reference`
and the pinned `TOS_Distro.ISO`. It obtains the reference text-layer address
from the guest through the QEMU debug port. `make test` runs the native
regression after the normal boot suite.

The persistent variant runs the same original database from the DVD with
the generated writable FAT32 ATA fixture mounted as `C:`. It sets the
home directory to `C:/`, creates and edits both accounts, cancels the
overwrite, and checks both accounts in the original UI. A second VM boots
from the same disk with writes disabled and reads both saved accounts
again. The host traverses the actual directory and allocation chain and
compares all 51,200 record bytes, including the unused and cancelled
records, with the expected record layout. It also checks the mirrored
FATs, boot sector, unchanged source fixture, and byte-for-byte preservation
of the mutated image across the read-only reboot.

Both the native and pinned TempleOS guests passed the persistent replay
under QEMU 8.2.2 (`pc`, `qemu64`, 512 MiB). The source is the unchanged
`DataBase.HC` identified above. The file data agrees exactly; directory
timestamps and unused FAT directory fields need not match.

```sh
make -j4 BUILD=build/database-disk
make BUILD=build/database-disk build/database-disk/Linux/TestBoot \
  build/database-disk/fat32-writable.img
build/database-disk/Linux/TestBoot --database-persistent qemu-system-x86_64 \
  build/database-disk/unholyos.iso build/database-disk/fat32-writable.img
```

For the reference, substitute `--database-persistent-reference` and the
pinned `TOS_Distro.ISO`. An optional final directory retains the disk and
screen captures. `make test` runs both the native RAM-drive and persistent
ATA-drive regressions.

## Numeric and date prompts

`Demo/Prompt.HC` retains Terry A. Davis's original bytes. The prompt
driver enters an out-of-range integer, floating-point value and date,
then supplies valid values. It runs the original demo and checks its
integer, float, name default and date results.

`DateOffsetProbe.HC` checks the native calendar ABI, fractional-day
round trip, RTC offset, local midnight with positive and negative
offsets, month/year boundaries, relative dates, omitted year, and the
reference's numeric field narrowing. Both guests produced:

```text
date fraction EE4120D
date local 02/29/24 00:00:00 utc B47ECC0000000
date negative 02/29/24 00:00:00 utc B47ED40000000
Date offset 3FF
```

The final bit records the pinned December behavior: `EM(12/1/24)`
returns December 31, 2024, but `EM(12/1/23)` returns November 29, 2024.
The reference `LastDayOfMon` uses month zero when advancing the year.
The native implementation reproduces those observed results without
indexing outside the calendar arrays.

The focused build used `BUILD=build/date-offset`. The ISO contained
`StartOS.HC.Z`, compressed `Doc/Start.DD`, Home, and the exact
`DateOffsetProbe.HC` bytes. The reference executed the same bytes from
an allocated guest buffer using `ExePutS`, with output captured at
port `0xE9`. Both guests used QEMU 8.2.2, PC, `qemu64`, 512 MiB, and
DVD boot. The startup and guest-built ISO tests check the same four
lines and retain the existing prompt driver.

## DolDoc windows

`make -j4 test BUILD=build/doldoc-display` runs `DocFrame.HC` in
the startup guest and again from the guest-built ISO. Two HolyC tasks
open overlapping document windows. QEMU screenshots check the frame,
text palette, tab stop, underline, invert, glyph shift, and border
document. The probe checks depth before and during overlap and after
the upper window closes. Both guests reported depth tests of 0, 1,
and 0. With the same QEMU 9.2.0 PC, `qemu64`, and 512 MiB settings,
the three 144×96 window regions matched pinned TempleOS across
41,472 RGB pixels. The boot driver also checks the cleared region
against the UnholyOS background.

## DolDoc buttons

`make -j4 test BUILD=build/buttons-gcc` drives `DocButton.HC` with
PS/2 left-button transitions. It checks the single and double CP437
outlines while the button is released and pressed. The fixture observes
a Space key followed by `MSG_CMD` with value 73 for `BT-X`. The default
`BT` entry sends Escape. Both sequences also ran on pinned TempleOS.
The boot suite repeats the fixture after booting the guest-built ISO.

Terry A. Davis's original `Demo/DolDoc/TextDemo.HC` ran in a separate
test window with the DolDoc lexer enabled. On QEMU 9.2.0 with PC,
`qemu64`, and 512 MiB in both guests, all 136,832 compared RGB pixels
matched pinned TempleOS, including both button outlines. The capture
comparison covers the text window interior with the pointer area excluded.

## DolDoc callbacks

`make -j4 test BUILD=build/callbacks-gcc` runs `DocCallback.HC` on
startup and from the guest-built ISO. It checks both mouse activation
keys, callback arguments and return values, live tag updates and heap
ownership, hold-aware clearing, and recovery after a callback throws.

Terry A. Davis's original `Demo/DolDoc/CallBack.HC` runs in a separate
test window with the DolDoc lexer enabled. Both live clock tags update,
and the demo's `UndoIt` clears the lower clock.

## HolyC exception statements

`make -j4 test BUILD=build/try-gcc` compiles and runs `TryProbe.HC`
from source and as a guest-built `.BIN.Z`. The probe checks normal and
early returns, accepting or declining an exception, throwing again in a
handler, top-level handlers, and restoration of the task's exception
queue and interrupt flag. The suite rejects a `try` without a `catch`.

Terry A. Davis's original `Demo/Exceptions.HC` is included unchanged.
The boot driver answers `n` and `y` to produce `Point2`. The inner handler
reports the tag, then the outer handler accepts it.

## PC speaker and recorded sound events

`make -j4 test BUILD=build/pc-speaker` builds
`build/pc-speaker/unholyos.iso` and runs the HolyC sound probes.
`SoundProbe.HC` checks note conversion, the default `Beep` and busy
`Beep`, speaker shutoff, mute, and reset. Pinned TempleOS printed the
same four frequency bit patterns and five note numbers from the same
probe.

The QEMU 9.2.0 comparison used `qemu-system-x86_64 -machine
pc,pcspk-audiodev=spk -cpu qemu64 -m 512 -boot d -cdrom <iso>
-display none -audiodev wav,id=spk,path=<wave>`. The recordings
showed pitches of about 490.6 Hz and 327.4 Hz in both guests.
The captured tone durations differed. The UnholyOS test separately
checks the combined beep wait against PIT ticks and HPET elapsed time
when HPET is available.

The suite compiles `SoundRecord.HC` in the guest and saves
`SoundRecord.BIN.Z` under `BUILD`. Loading that module in both systems
produced the event sequence `0, 60, 60, 0` and the default path
`B:/Tmp/%X.GR` in audio-only mode.

`AUProbe.HC` (SHA-256
`b3e29b6624f09455e3e04a78f720df1b82138ce9ce63f6fdaa403ecf1c875606`)
checks the packed sound structures, zero and negative shifts, removal
of expired events, equal timestamps, default and explicit extensions,
and export of a queue recorded through `Snd` and `ScrnCast`. Both
guests on QEMU PC with qemu64 and 512 MiB returned `AU probe 7F`.
The expected timestamps include the one-ULP rounding of the original
decimal lexer. The host test reads the complete 45-byte deterministic
AU file and checks each nine-byte record against the pinned output.
Video capture and desktop playback remain pending.

## RedSea descriptor verification

The source layout and coding rules are recorded in [SourceLayout.md](SourceLayout.md).
The ISO writer uses the packed `CPalindromeU16`, `CPalindromeU32`,
`CISODirEntry`, and `CISOPriDesc` records from `Kernel/KernelA.HH`, along with
the `FillU16Palindrome`, `FillU32Palindrome`, and `CElTorito` definitions in
`Adam/Opt/Boot/DskISORedSea.HC`. The reference compiler places class members
consecutively in `PrsVarLst`, so the root record starts at byte 152. The
writer stores the root-cluster palindrome at that address. The 2,044-byte
descriptor occupies a zeroed 2,048-byte sector.

On 2026-11-25, the media check rejected an ISO from baseline
`76b6f73e7ae901b8939b8e69333a2a9f2eb32dc4` with
`The ISO descriptor root differs from TempleOS`. With the corrected writer,
the standard and startup-test ISOs each differed from the baseline at exactly
eight bytes, all within the primary and supplementary root records. The
kernel binary was byte-for-byte identical. This comparison checks the
format correction against the pinned source; it does not establish complete
ISO construction parity.

The build and boot checks used GCC 13.3.0, NASM 2.16.01, and QEMU 8.2.2:

```sh
make -s -j4 test BUILD=build/current
```

All nine existing VM cases passed: the standard PC image at 512, 3,072,
and 4,608 MiB; PC with HPET disabled; the secondary IDE slave; Q35; and the
startup-test image at all three RAM sizes. The Q35 test retains the documented
AHCI mount limitation. The source-program, runtime-module, archive, and
allocator checks also passed.

The standard ISO at `build/current/unholyos.iso` had SHA-256
`4e1f783bf668313dda9bd4dbde5efe28b047f635eec65874d40acdee83dcc427`.
The startup-test ISO had SHA-256
`84ff91fc5864cf89c4544e699164ab2dd1f644d80e9bf92a3b437435d76ed33c`.
A fresh interactive Windows QEMU 9.2.0 process used:

```sh
qemu-system-x86_64 -name UnholyOS-boot-layout -machine pc -cpu qemu64 -m 512 -boot d -cdrom build/current/unholyos.iso -display gtk -serial file:build/interactive-serial.log -monitor none -qmp tcp:127.0.0.1:4129,server=on,wait=off -no-reboot -no-shutdown
```

The 640×480 display showed `UnholyOS kernel entered in 64-bit mode`.
Serial output reported the mounted RedSea DVD, root cluster `0x5a`, kernel
cluster `0x5d`, and `UnholyOS: kernel entered in 64-bit mode`. QMP reported
the VM running. The screenshot and serial log were captured under `build/`.
This verifies the clean boot to the current kernel; the desktop is pending.

## Native FIFO operations

`Kernel/KDataTypes.HC` defines the byte and signed 64-bit FIFO operations.
Both records have a typed buffer pointer followed by `mask`, `in_ptr`,
and `out_ptr`, at offsets 0, 8, 16, and 24. The ring reserves one slot.
The native insert, remove, peek, flush and count operations preserve the
interrupt flag. Flush advances the read pointer to the write pointer.
Count uses the reference's conditional subtraction, including its
non-power-of-two mask behavior. Allocation retains UnholyOS's existing
failure checks and deletion accepts a null record; those cases differ
from the unchecked reference routines.

The new `Linux/Fixtures/FifoProbe.HC` checks both typed layouts,
empty operations (including null output pointers), capacity, byte and
signed I64 values, peek, wraparound, flush, interrupt state and the
size-six ring quirk. Both guests printed `FIFO probe 1FF`. A second
direct call returned the same value without changing the task's
data-heap usage.

The focused native image was built using GCC 13.3.0:

```sh
make -j4 build/unholyos-startup-test.iso build/Linux/TestBoot
build/Linux/MakeISO build/Adam/Opt/Boot/BootDVD.BIN \
  build/0000Boot/0000Kernel.BIN.C build/fifo-focused.iso \
  --compress-at Doc/Start.DD.Z Doc/Start.DD --dir Home \
  --file Linux/Fixtures/FifoProbe.HC
qemu-system-x86_64 -machine pc -cpu qemu64 -m 512 -boot d \
  -cdrom build/fifo-focused.iso -display none \
  -serial file:build/fifo-focused.log -monitor none -no-reboot -no-shutdown
```

QEMU 8.2.2 booted the focused image and reported the FIFO result, heap
cleanup and entry into 64-bit mode. The 512,176-byte kernel's BSS ended
at `0x88DAB`, below the relocated DVD loader at `0x96600`.
The pinned distribution DVD ran with the same PC, qemu64 and 512 MiB
settings. Its kernel matches the 189,760-byte kernel file in the pinned
checkout at DVD byte offset 52,224. GDB copied the 3,704-byte fixture into
a zeroed guest buffer and read it back; the SHA-256 of both copies was
`28e776de0d4d84d99cb1f1c6dbf68cac8bcaaf6f12b7542588c492dadff75f1e`.
`ExePutS` ran that buffer, and a separate `KeyDevAdd` hook captured the
output through QEMU's debug console. The reference heap check emitted 1.

## TimeClock and queue operations

The original `Apps/TimeClock/Load.HC`, `Install.HC`, and `TimeClk.HC`
are shipped at their original paths without edits. Their SHA-256 hashes
match the pinned source: respectively
`277bbe03345250dcdf1cfc377bbadbad8da437d1b2991596b86acc0bfa137ca8`,
`302dfcb63c56d2431564876bc8021fd9229809e5024f14d2ef6a82bfb9a5a868`, and
`ee45adaa0429b22b7484948d081265561e7551acf7eed7cad46f172c185f61dd`.

`QueueProbe.HC` returned `FF` in both guests and in the native guest's
compiled `.BIN.Z`. It checks the 16-byte queue layout, forward and reverse
insertion, removal, string copy and length, and `__DIR__` inside a nested
source file. The reference ran the exact probe through `ExePutS` with its
source name set to `::/Doc/QueueProbe.HC`.

`TimeClockProbe.HC` loads the original application through `Load.HC`,
using a fresh writable RedSea RAM drive as the home directory. It checks
the 33-byte entry layout, exact expanded record bytes and timestamps,
reloading the queue, and a complete 90-minute session's report. Both
guests returned `1F` and reported `Week Total:01:30:00` and
`Already Punched-Out`. The reference reports a missing-file diagnostic on
the first read of the absent data file; UnholyOS currently returns null
silently. The native setup also prints its directory creation message.
These initial setup traces are not identical.

`TimeClockInputProbe.HC` then exercises the original `PunchIn` and
`PunchOut`. Actual PS/2 keyboard input enters `start`, Enter, `work`,
Escape, followed by `done`, Escape. Both guests reload four records,
verify that the last two are an ordered punch-in/punch-out pair with
timestamps between the before/after readings, and recover the exact
multiline and single-line descriptions. Both report `TimeClock input 07`.
The full boot suite checks the deterministic report in the startup
cases and the rebooted guest-built ISO; the latter also drives the
interactive prompts. The clock times themselves are not fixed.

For focused verification, build with `make -j4 BUILD=build/time-clock all`.
Use `Linux/MakeISO` with the built loader and kernel, `--dir Home`,
`--file Linux/Fixtures/FileWriteProbe.BIN`, both TimeClock fixtures, and
`--file-at` for each original TimeClock source. The focused image was
`build/time-clock/app-input-focused.iso`. Both guests ran under QEMU
8.2.2, PC, `qemu64`, 512 MiB. The native boot log, reference debugcon
trace, and before/after keyboard screenshots were captured in
`build/time-clock/`. Persistent hard-disk writes and the complete
reference file-error diagnostics remain pending.

## Music notation and playback

`MusicProbe.HC` (SHA-256
`d75c1504452d0b9a7f825e111e338f800f9f81a81a77af61867f341eefadeb24`)
checks the packed music state, note-number conversions, meter and note
length modifiers, lyrics, rests, ties, mute, and beat progression.
Both guests returned `Music probe 7F`. The 117-byte AU file contains
13 events; the native regression checks each note and nondecreasing
timestamps. Both guests reported the fractional jiffy correction as
`0x401800000000000A`. The reference probe was executed from a guest
buffer whose bytes were checked against the fixture, using a RAM disk
for the AU file and the debug port to retrieve the output and file.

Terry A. Davis's `Demo/Snd/OhGreat.HC` is copied byte for byte from the
clean pinned Windows checkout (SHA-256
`914e1648a99f587716a2ba896b7a99865538118769716b2e1f0b9fe96a0fc198`).
Git's checkout converts LF to CRLF; removing only those inserted CR bytes
recovers the pinned Git blob `19771968474ab35d0d91c2e64872631bdebf7388`.
The reference distribution's expanded `OhGreat.HC.Z` is 1,086 bytes
(SHA-256 `b73ecbac7b7d25588ddb7cda10816b58da029cd73abc9e1a9d198471aa19bd77`);
it differs from the pinned LF source only at 62 lyric spaces, encoded as
`0x1F` in the distribution and `0x20` in Git. Both versions produce the
same recorded notes.
The QEMU driver starts it through the terminal, sends a real key during
the song, waits for the complete verse and the next terminal prompt,
and walks the recorded sound queue. Both guests produced the same
171-byte note sequence (SHA-256
`96735c04fad3b8acd8dfabca345278e856974e3dc295688348d79c9ba5bb5080`).
The test checks monotonic event times, a silent speaker on exit, and a
subsequent terminal command. The comparison used QEMU 8.2.2, `pc`,
`qemu64`, one CPU, 512 MiB and CD boot. The driver starts QEMU with
`-machine pc -cpu qemu64 -m 512 -boot d -cdrom <iso> -display none
-monitor none -serial file:<log> -qmp stdio -no-reboot`; the reference
run also captures the debug port. The tested reference ISO was
`TOS_Distro.ISO` (SHA-256
`3c68ca395d0b64f779f1ffa330e2685798b17862e3c36ae4e349537fd676b40b`).

The full native suite passed. Repeat it with
`make -j4 BUILD=build/music-play test`. To repeat just the song comparison,
use the following commands and
substitute the path to the pinned distribution ISO:

```sh
make -j4 BUILD=build/music-play all build/music-play/Linux/TestBoot
build/music-play/Linux/TestBoot --music qemu-system-x86_64 \
  build/music-play/unholyos.iso build/music-play/song-native
build/music-play/Linux/TestBoot --music-reference qemu-system-x86_64 \
  /path/to/TOS_Distro.ISO build/music-play/song-reference
```

The capture directories retain the text layer, screenshots and
`music-notes.bin`. Compare the latter directly between guests.
The separate probe captures are `music-native-final.log` and
`music-reference-final.log` in `build/music-play/`.

`MusicPrecision.HC` checks 29 note-length modifier sequences, initial
and reset settings, and the `0.25`, `2.5`, and `0.9` literal bits. Its
SHA-256 is
`ef763c725ad9c64b6a6df6d92ac577f2cc4449760a11a61f421a6184f6206f10`.
The pinned runtime assigns `0x3FD0000000000001` for the sixteenth-note
length; that value and its six tested dotted or triplet variants differ
from calculations starting with C++'s exact `0.25`. All 32 data rows
match when executing the corrected native source and its guest-compiled
TOSB module. The startup and guest-built ISO checks enforce the complete
trace. The reference replay checks the uploaded source and terminating
NUL before execution and verifies every expected line.

```sh
make -j6 build/music-precision-test.iso build/Linux/TestBoot
build/Linux/TestBoot --music-precision qemu-system-x86_64 \
  build/music-precision-test.iso build/music-precision-native
build/Linux/TestBoot --music-precision-reference qemu-system-x86_64 \
  /path/to/TOS_Distro.ISO build/music-precision-reference
```

Playback uses the actual PIT speaker and task sleep. It is tested on
one CPU; waveform fidelity, exact wall-clock durations, SMP music
semaphores, and the remaining sound and media APIs are not established.

## Screen capture

`Demo/Graphics/ScrnCapture.HC` is the original Terry A. Davis source from
the pinned TempleOS tree. Its SHA-256 is
`9d80656342da13734305dd82cc22d24c7f9f05829f5b2cebb87ef5a738f473dd`.
The demo is included in the boot and startup-test ISOs.

`CaptureProbe.HC` draws a 32-by-8 pattern containing all 16 indexed
colors. It checks dimensions and ownership of the captured bitmap, the
original pattern, each doubled pixel at 2x zoom, the unzoomed capture
while zoomed, a GR file with a changed VGA palette, and clipped playback
into a 32-by-8 device context. It stores the playback return in an `I64`
and requires exactly TRUE for an existing image and FALSE for a missing
file. Both guests returned `capture probe 7F`.
The fixture is also checked by the native startup and guest-built ISO
tests. Screen content outside the deterministic pattern differs between
the guests; the full screenshot is not claimed to be byte-identical.

The playback adapter widens the C++ Boolean return before returning to
HolyC. Without that step, the native GCC build returned
`capture probe 5F`: the low byte held TRUE, but the upper return bits
retained the heap lock address from image cleanup. The reference fixture
returned `7F`.
The reference interactive test uploads and verifies the fixture bytes
before running it through `ExePutS`.

The original demo was also driven from the guest terminal in both
systems. A fresh RedSea RAM drive serves as `Home`. The test places a
16-by-8 red rectangle on the screen, presses Space at each of the three
prompts, checks that the image is replayed into the graphics overlay and
that the rectangle survived, and verifies that the overlay is entirely
transparent on exit. Reading `B:/DemoScrnShot.GR.Z` returns a 640-by-480
image with the expected red pixel. Both interactive runs passed.

From the repository root, the interactive tests can be run with:

```sh
build/capture-check/Linux/TestBoot --screen-capture qemu-system-x86_64 \
  build/capture-check/unholyos.iso build/capture-check/demo-native
build/capture-check/Linux/TestBoot --screen-capture-reference qemu-system-x86_64 \
  /path/to/TOS_Distro.ISO build/capture-check/demo-reference
```

The focused native image was built with
`make -j4 BUILD=build/capture-check all` and `Linux/MakeISO`, adding
`Linux/Fixtures/CaptureProbe.HC`, `Linux/Fixtures/FileWriteProbe.BIN`,
the startup document and a `Home` directory. The reference ran the same
fixture via `ExePutS` after creating a RAM drive. Both ran under QEMU
8.2.2, PC, `qemu64`, 512 MiB. Logs are in `build/capture-check/`.

## Kernel math helpers

`MathHelpersProbe.HC` compares the native helpers with
`Kernel/KMathB.HC` from the pinned TempleOS checkout. Its SHA-256 is
`b6e6d2c0a6a9e23438beb99448123339accc2f0e31fb7e474c0a3b5d43fac41a`.
The reference driver verifies the complete uploaded source and trailing
NUL before executing it. The native guest runs the same source, compiles
it with `Cmp`, and executes the resulting `MathHelpersProbe.BIN.Z`.

All 25 trace lines matched across the reference and native source and
module runs. The reference fingerprints are:

| Inputs | Helper calls | Fingerprint |
| --- | ---: | --- |
| Signed values -47 through 47 and both integer limits, with eight positive and negative divisors | 2,376 | `903D605CF130C49A` |
| Six unsigned values and five divisors, including the high bit and maximum value | 60 | `65978B3FBB23DE21` |
| Twelve binary64 inputs, including signed zero, subnormals, infinities, and quiet and signaling NaNs | 2,016 | `023263F76EC7DF9A` |

Additional trace lines check reversed clamp limits, selected NaN payloads,
decimal-power bounds, and indirect calls to floating and integer helpers.
The comparison preserves the reference's `CeilI64(-1,10)==-10` result
and unordered floating-point branches. For example, `Max(NaN,1.0)`
returns its first operand and `Clamp(NaN,-2.0,3.0)` returns 3.0.

Build and run the focused image from the repository root:

```sh
make -j6 build/math-test.iso build/Linux/TestBoot
build/Linux/TestBoot --math-helpers qemu-system-x86_64 \
  build/math-test.iso build/math-native
build/Linux/TestBoot --math-helpers-reference qemu-system-x86_64 \
  /path/to/TOS_Distro.ISO build/math-reference
```

Both guests use QEMU 8.2.2 with `-machine pc -cpu qemu64 -m 512
-boot d -cdrom <image.iso> -display none -monitor none -serial
file:<serial.log> -qmp stdio -no-reboot`. The reference also uses
`-debugcon file:<debugcon.bin> -gdb tcp:127.0.0.1:45362` to transfer
the fixture. Each guest starts from a clean VM. The native startup and
guest-built ISO regressions check the same source and module output.
This evidence covers the listed values and default x87 environment;
zero-divisor faults, signed division overflow, and unmasked floating-point
exceptions are not part of the comparison.

## String scanning and transformations

Five separate HolyC fixtures exercise the translated routines in
`Kernel/StrA.cpp`, `StrB.cpp`, and `StrScan.cpp`, the original comparison
entry in `StrA.asm`, and `DocDataScan` in `Adam/DolDoc/DocForm.cpp`.
The native guest executes each source, compiles it with `Cmp`, and loads
the resulting TOSB module. All ten native traces match the corresponding
fresh TempleOS trace byte for byte: 71 lines for source execution and
the same 71 lines for module execution.

| Fixture | Compared output | Covered behavior |
| --- | --- | --- |
| `StrScanProbe.HC` | 14 lines; `StrScan probe 7FFF` | Integer, float, date, string, character, list, and define conversions; fixed and runtime widths; precision quirks; pointer advancement; narrow stores; 16 caught errors; indirect variadic calls |
| `DocScanProbe.HC` | 1 line; `DocScan probe 3FFF` | Adjacent-byte preservation, list and define fields, floating fields, string pointers, reallocation and `MSize`, terminator restoration, exception propagation, and lists and checkboxes with null tags |
| `StringUtilitiesProbe.HC` | 5 lines; `String utilities 7FF` | All 2,048 flag combinations on two `StrUtil` inputs and one `MStrUtil` input; 400 space/tab cases; 40 growing tab expansions; 120 indentation cases; marker removal, label boundaries, indirect calls, and heap accounting |
| `FloatScanProbe.HC` | 44 data lines and `FS DONE` | Binary64 results and end-pointer offsets for whitespace, signs, decimal fractions, infinity lookahead, signed zero, overflowing integers and exponents, and x87 rounding |
| `DateScanProbe.HC` | 6 lines; `Date scan 3F` | Leap-year boundaries, whitespace and case normalization, suffix arithmetic, positive and negative local-time offsets, and a clock-bracketed relative-date expression |

Fixture SHA-256 values are:

| Fixture | Bytes | SHA-256 |
| --- | ---: | --- |
| `StrScanProbe.HC` | 4,447 | `c6812ec950717996dc340a378e3c8ea4a56b1f311e13bc4b1948bc218496583c` |
| `DocScanProbe.HC` | 3,153 | `4f403181c729562466686ac95d5a725e70b48ceb5889737c995169bf34d0f961` |
| `StringUtilitiesProbe.HC` | 2,902 | `d9fb57d7fb909f358479fbbda0aad74b6858c0473db96f968ffdfca93781e0e7` |
| `FloatScanProbe.HC` | 1,762 | `0f8baca7578760e49afe0e04e17c5c70e5d6f1bb98f4c586257030fdcb808458` |
| `DateScanProbe.HC` | 2,261 | `6a9a07286117eb7962670dddaa7f005a08a4c39bf686fd54806f143523e6c8bd` |

The five complete reference traces, including their final newline, have
these SHA-256 values:

| Trace | Bytes | SHA-256 |
| --- | ---: | --- |
| String scanner | 466 | `d43947b011d53ed71e4b772567edac217f8533a1f15e4cd00935dd297c2443a3` |
| Document scanner | 19 | `9b7a0c9eb5cf80c0e330572a2cfbde72dceb2aa18756da611769848961e99352` |
| String utilities | 181 | `35b88b4e06d9c55524d7227da96e3dd6afb8623296f77a5f1a1cec0fd0a0b3da` |
| Floating scanner | 1,204 | `d3ca3067b33c56ab8f5a2678852aa0c710fb0633435a5ee5c5500163dcd47048` |
| Date scanner | 348 | `63501b6560dfd85b726a9287c1606c58a93e5f1ca6796f3272fc0b8843a8624f` |

`Linux/TestStrings.hpp` contains the full expected traces. Native
exception diagnostics include task-dependent caller addresses. The
driver validates their exact tag, count, order, and address format before
removing those diagnostic lines from the comparison; the original serial
log is retained. The undefined-define error text remains in the compared
output. The reference driver checks the source and its terminating NUL
after upload, captures the text cells and screenshot, and requires a
return to the empty terminal prompt. Only blank lines and the reference
elapsed-time line may appear after a fixture's final marker.

The floating comparison exposed 15 differing cases in the previous
native converter: six infinity-lookahead/end-pointer cases, five
overflowing-exponent cases, and four cases requiring the signed integer
to remain exact in x87 until after decimal scaling. For example,
`9007199254740993e-1` produces `0x430999999999999B`; rounding the integer
to binary64 before multiplication instead produces
`0x430999999999999A`. The revised converter matches all 44 cases.

Rebuild and compare the focused image from the repository root:

```sh
make -j6 build/strings-test.iso build/Linux/TestBoot
build/Linux/TestBoot --strings qemu-system-x86_64 \
  build/strings-test.iso build/strings-native
build/Linux/TestBoot --strings-reference qemu-system-x86_64 \
  /path/to/TOS_Distro.ISO build/strings-reference
```

Both guests use QEMU 8.2.2, `pc`, `qemu64`, one CPU, 512 MiB, and a clean
CD boot. The QEMU arguments are `-machine pc -cpu qemu64 -m 512 -boot d
-cdrom <image.iso> -display none -monitor none -serial file:<serial.log>
-qmp stdio -no-reboot`. The reference also uses
`-debugcon file:<debugcon.bin> -gdb tcp:127.0.0.1:45362` for the fixture
transfer. Its `TOS_Distro.ISO` SHA-256 is
`3c68ca395d0b64f779f1ffa330e2685798b17862e3c36ae4e349537fd676b40b`.
The source comparison uses the separate, clean checkout at
`c26482bb6ad3f80106d28504ec5db3c6a360732c`.

The startup and guest-built ISO tests enforce the same source and module
traces. The fixture comparison covers the listed inputs under the
default x87 environment. Allocation exhaustion, invalid pointers,
malformed widths, nonfinite indentation factors, altered x87 exception
masks, and complete editor behavior remain outside this evidence.

## String comparison entries

`StringCompareProbe.HC` adds a sixth fixture to the focused strings
image and the startup and guest-built ISO checks. Its 1,894 source bytes
have SHA-256
`56f178a2ab9593f2417ecd5abb8632d7a66c7bc05bd7a75fbcf6d4652d06988a`.
The reference upload, including the terminating NUL, has SHA-256
`8da433b7530380f778ee15ee12cebfd0c856c16060ef5b1b6c708ad3afeb57ff`.
The complete 24-line, 489-byte reference trace has SHA-256
`118f7dc8bcf3dc896cc9fcd6528055d643a94486d03a683f915346e677bbe08e`.

The fixture checks 19 direct and function-pointer cases for `StrICmp`,
`StrNCmp`, and `StrNICmp`: empty strings, ASCII case boundaries,
prefixes, NUL termination, negative and minimum counts, overlapping
strings, and high bytes. It also checks zero-count null pointers and
the public `StrOcc` declaration. All 65,536 pairs of byte values feed
a fingerprint for each comparison routine. Every possible `AL` value
passes through the raw `TO_UPPER` entry with a fixed high-`RAX` sentinel.

| Operation | Reference fingerprint |
| --- | --- |
| `StrICmp` byte pairs | `562AF89B93749399` |
| `StrNCmp` byte pairs | `E94E7B76B0C5E825` |
| `StrNICmp` byte pairs | `562AF89B93749399` |
| Raw `TO_UPPER`, including preserved high bits | `ED25B86AE3E78265` |

Native source and guest-compiled module output match all 24 reference
lines. The previous five traces remain unchanged after replacing the
float and date scanners' manual prefix checks with `StrNCmp`. Together,
the six fixtures compare 95 lines in each execution mode. The reference
uses the same source verification, terminal capture, ISO, and QEMU
configuration described above.

`Linux/TestStrCompare.cpp` separately checks the C++ adapters by linking
the actual NASM `StrA` object into a host C++20 test. It verifies `RBX`
preservation and exact stack restoration for 196,667 comparator calls,
the full `RAX` result for all 256 uppercase inputs, exact edge-case
results, and the four reference fingerprints. The unrelated output
entries have explicit test-only link stubs. `make test` builds and runs
this check with the selected host compiler.

`FunctionPtrProbe` uses the shared source/module executor with its
existing paths and compile command. Removing that duplicate test code
keeps the image within the unchanged reference loader boundary at
`0x96600`. Clang 20.1.2 links `__bss_end` at `0x9652B`, leaving 213
bytes; GCC 13.3.0 links it at `0x8F36B`, leaving 29,333 bytes. The
separate compiler and Adam load path remains incomplete, as recorded
in [Source layout](SourceLayout.md#remaining-structural-work).

```sh
make -j4 build/strings-test.iso build/Linux/TestBoot build/Linux/TestStrCompare
build/Linux/TestStrCompare
build/Linux/TestBoot --strings qemu-system-x86_64 \
  build/strings-test.iso build/string-compare-native
build/Linux/TestBoot --strings-reference qemu-system-x86_64 \
  /path/to/TOS_Distro.ISO build/string-compare-reference
```

The guest probes cover valid terminated strings and the reference's
zero-count null-pointer case. They do not establish fault behavior for
other invalid pointers or strings extending beyond accessible memory.

## Subsystem map

| Area | Pinned TempleOS entry points | UnholyOS scope |
| --- | --- | --- |
| DVD boot and CPU startup | `BootDVD.HC`, `KStart16.HC` through `KStart64.HC`, `PCIBIOS.HC` | DVD entry, firmware discovery, segmentation, paging, long mode, and BIOS32 PCI reads |
| Interrupts and timers | `Kernel/KInts.HC`, `Kernel/KMain.HC`, `Kernel/KMisc.HC` | Both IDT stages, PIC setup, PIT ticks, HPET, the initial TSC calibration sample, and boot fault diagnostics. Debugger recovery is pending. |
| Memory | `Kernel/Mem/PageTables.HC`, `BlkPool.HC`, `MAllocFree.HC` | Initial identity map, code and data pools, and early heap allocation. Heap allocation and freeing, the 32-bit device range list, and the uncached device alias are active. |
| Tasking and CPUs | `Kernel/KTask.HC`, `Sched.HC`, `MultiProc.HC` | Core 0 runs Adam, the idle task, and spawned tasks with cooperative switching and PIT wakeups. Starting additional CPUs remains pending. |
| Storage | `Kernel/BlkDev/DskATA.HC`, `DskATAId.HC`, `DskCDDVD.HC`, `DskBlkDev.HC`, `FileSysRedSea.HC`, `FileSysFAT.HC`, `DskFile.HC`, `DskStrA.HC`, `Kernel/Compress.HC` | Boot DVD identification, task-relative reads, writable RedSea RAM drives, and `.Z` expansion and encoding. ATA block writes, explicitly enabled FAT32 file and directory mutations, and buffered streams persist across cold boots. Fragmented streams, recursive file operations, cluster reclamation, and quick and full formatting of mounted FAT32 primary partitions are tested. Unmounted partitions and full cache and error-path parity remain pending. |
| Graphics | `Kernel/Display.HC`, `Adam/Gr/GrScrn.HC` | VGA text and graphics composition, 16-color palette, cursor, zoom, and a scheduled refresh task |
| Input and sound | `Kernel/SerialDev/Keyboard.HC`, `Mouse.HC`, `Message.HC`, `Kernel/KMisc.HC`, `Adam/Gr/ScrnCast.HC`, `Adam/ASnd.HC` | PS/2 keyboard and mouse messages, PC speaker tones, queued sound recording, event shifting, packed AU output, and music notation playback. Full desktop input and remaining media APIs are pending. |
| Desktop and system tools | `StartOS.HC`, `Adam/MakeAdam.HC`, `Adam/WinMgr.HC`, `Adam/DolDoc`, `Apps` | The refresh task draws visible DolDoc text and borders. The full editor and interactive desktop remain pending. |
| HolyC | `Compiler/CMain.HC`, `Lex.HC`, `Prs*.HC`, `Back*.HC`, `Kernel/KLoad.HC` | Source execution with `ExeFile` and `ExePutS`, compile-time streams, plain and DolDoc tokenization, scalar expressions and functions, typed pointers, packed classes, unions, fixed arrays and their initializers, print calls, and TOSB module generation and loading for supported source. Full language support and native kernel compilation remain pending. |
