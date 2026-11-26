# Booting from DVD

Build on Linux with GNU Make, a C++20 compiler with an x86-64 ELF target, GNU binutils, and NASM. Use QEMU to run the disc. On Windows, run the build in Ubuntu under WSL and use native QEMU to open the VM window.

```sh
make -j2 test
qemu-system-x86_64 -machine pc -cpu qemu64 -m 512 -boot d -cdrom build/unholyos.iso -serial stdio
```

The C++20 host tools build the RedSea image and check it against the kernel built by Make. The test starts fresh PC VMs with 512 MiB, 3 GiB, and 4.5 GiB of RAM. It checks the RedSea boot records, the BIOS handoff, the long-mode registers, the code and data pools, the bootstrap CPU record, Adam's heap and stack, the 32-bit device ranges, the 4 GiB uncached alias, the second-stage interrupt gates, PIT ticks, and the 640×480 framebuffer. It checks PCI reads and boot clock setup on PC, Q35, and PC with HPET disabled. On PC, the guest mounts the DVD, locates `/0000Boot/0000Kernel.BIN.C`, and reports the file's checksum. The test checks the checksum against the build and runs with the DVD attached to either unit on the secondary IDE channel. When Q35 boots from its default AHCI controller, the kernel reports that it could not mount the DVD. With Clang installed, select it with `Makefile` overrides `CXX=clang++ HOSTCXX=clang++`. The window displays `UnholyOS kernel entered in 64-bit mode`.

`make test` also builds `build/unholyos-startup-test.iso` with the original `StartOS.HC.Z`, compiler fixtures, and eleven TOSB module fixtures. The guest asks for `/StartOS.HC` through `FileRead` and reports the length and checksum of the expanded bytes. At 512 MiB, 3 GiB, and 4.5 GiB, the test scans the lexer fixtures, compiles and runs scalar HolyC expressions and functions, compiles and runs the original `Demo/Print.HC` from source, and runs the compiled modules from the RedSea DVD. `build/unholyos.iso` is the standard boot image.

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
their original paths. The probe reads the original documents and
checks the attachment without running their sprite drawing routines.
[Pinned TempleOS graphics demos](https://github.com/cia-foundation/TempleOS/tree/c26482bb6ad3f80106d28504ec5db3c6a360732c/Demo/Graphics)

[`Doc/Start.DD`](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Doc/Start.DD)
is Terry A. Davis's public-domain startup document, copied byte for
byte from the pinned source. Its SHA-256 is
`0f5f7dc1ebd091d2acee39b81b4211a8ffcd0febc423e6b8d5075e9854017530`.
The title uses the runtime `DD_OS_NAME_VERSION` definition, currently
`UnholyOS`. Help navigation, the directory chooser, border controls,
menus, autocomplete, and the full editor remain pending.

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
3,072, and 4,608 MiB. It reads the two output files from guest RAM, checks
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
- `ISOInit` in `Kernel/BlkDev/DskCDDVD.HC` looks for the RedSea publisher in the supplementary descriptor and advances the volume offset for a bootable disc. `RedSeaInit` and `RedSeaFileFind` in `Kernel/BlkDev/FileSysRedSea.HC` read the boot record and the 64-byte directory records. UnholyOS attaches the read-only boot volume to Adam after mounting it. Its file reader can read a byte range from a contiguous file. In a separate test, GDB set the DVD handoff to the pinned TempleOS disc on the second IDE unit, and the guest checksum matched the full 189,760-byte kernel file on that disc. Disk caching and hard-disk support remain pending.
- `FileRead` in `Kernel/BlkDev/DskFile.HC` checks Adam's resident file table, tries the requested and alternate names in the target directory, then searches its parents. The native RedSea entry point follows that order and returns the file size and attributes with a zero-terminated buffer. `ArcExpandBuf`, `ArcCompressBuf`, and `ArcEntryGet` in `Kernel/Compress.HC` define the stored, 7-bit, and 8-bit streams, width changes, and dictionary reuse at 12 bits. The host test reproduces both pinned compressed archive streams byte for byte.
- `Cmp` in `Compiler/CMain.HC` emits the `CBinFile` header and patch records read by `LoadPass1` and `LoadPass2` in `Kernel/KLoad.HC`. UnholyOS validates the records, aligns the executable body, and installs symbols in Adam's hash table using the bucket order from `Kernel/KHashA.HC`. It resolves relative imports when a later module publishes an export. A module with unresolved imports can be loaded for linking, and its initializers wait for an explicit call after resolution. Fixed-address modules and the full kernel symbol set remain pending.
- Kernel exports use HolyC stack arguments and callee cleanup. `Kernel/Mem/MAllocFree.HC` supplies the heap and string allocation contracts, including the negative offset before an aligned pointer. `Kernel/KHashA.HC` and `Kernel/KHashB.HC` supply the hash lookup and insertion rules, including matching across parent tables. `Kernel/StrA.HC`, `Kernel/KUtils.HC`, and `Kernel/KMathB.HC` supply the string, bit, memory, and numeric calls. Allocation exceptions and configurable variable initialization at boot are pending.
- The pinned TempleOS compiler built `Linux/Fixtures/LoadProbe.BIN` and `LoadCaller.BIN` from the adjacent HolyC source files using `Cmp`. The two guests produced 40 and 28, preserved a relocated function address, and loaded the probe at its requested 128-byte alignment. The probe also imported `SYS_COMPILE_TIME` and called `_MEMCPY` with HolyC stack arguments. The copy returned the address just past its destination, as implemented in `Kernel/KUtils.HC`. The 4.5 GiB test checks that a data-heap relocation reaches an allocation above 4 GiB.
- The pinned compiler also built `RuntimeProbe.BIN` from `RuntimeProbe.HC`. Both guests reported `0xff` with QEMU PC, `qemu64`, and 512 MiB. The probe checks allocation and aligned heap headers, string calls, hash instances and use counts, numeric and bit calls, and imported globals. It checks the exact `Pow10I64(2)` return bits (`0x4058fffffffffffe`).
- The same probe checks all 512 bits in each character map from `Kernel/StrA.HC`, including the high bytes and the special whitespace used by DolDoc. It checks `LstMatch` aliases and ambiguous prefixes, list sizing and queue cleanup from `Kernel/KDataTypes.HC`, and zero-filled gaps across 512-byte queue-vector nodes. It also checks the callback arguments, call count, and equal-key order from `Kernel/QSort.HC`. A separate link-only run of the pinned `Compiler.BIN.Z` left 49 unresolved import groups, down from 68 before these routines and maps were exported. Initializers stayed disabled.
- The pinned compiler built `FileReadProbe.BIN` from `FileReadProbe.HC`. Both guests reported `0x7f` under QEMU PC with `qemu64` and 512 MiB. It checks `DirNameAbs`, `FileNameAbs`, `DirFile`, `ExtChg`, and `ExtDft` from `Kernel/BlkDev/DskStrA.HC`, including the first-dot extension rule. It reads the expanded `StartOS.HC` by relative name, by its archive name, and through parent search. It also checks the plain kernel file attributes, a missing file, and a resident file table hit. The path helpers use the current task directory and the boot and home aliases for the mounted DVD.
- The same compiler built `FileWriteProbe.BIN`, which mounts a fresh `B:` RAM drive in each guest. `BlkDevInit` in `Kernel/BlkDev/DskBlkDev.HC` supplies the allocation and formatting rules. The probe checks the packed `CBlkDev` layout, disk geometry, bitmap, and root entries. It tests `RedSeaFileWrite` and `RedSeaDirNew` in `Kernel/BlkDev/FileSysRedSea.HC` through block reads and writes, nested directory creation and growth, file replacement, empty files, and resident cache updates. The resulting `StartOS.HC.Z` stream has the pinned 889-byte size and checksum. Hard-disk writes and FAT32 remain pending.
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

`Linux/Fixtures/Grid.HC` contains the original 672 bytes of Terry A.
Davis's [Grid demo](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Demo/Graphics/Grid.HC).
QEMU sends PS/2 motion to move the 5×5 cursor to a new grid cell. A
left click ends the demo, and the test checks that the prior callback
and grid settings are restored. The first and moved cursor patches
matched pinned TempleOS across 162 RGB pixels with the same VM settings
and black background. The boot driver runs the probes again from the
guest-built ISO.

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
`B:/Tmp/%X.GR` in audio-only mode. Video capture and writing the queued
events to an AU file remain pending.

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

## Subsystem map

| Area | Pinned TempleOS entry points | UnholyOS scope |
| --- | --- | --- |
| DVD boot and CPU startup | `BootDVD.HC`, `KStart16.HC` through `KStart64.HC`, `PCIBIOS.HC` | DVD entry, firmware discovery, segmentation, paging, long mode, and BIOS32 PCI reads |
| Interrupts and timers | `Kernel/KInts.HC`, `Kernel/KMain.HC`, `Kernel/KMisc.HC` | Both IDT stages, PIC setup, PIT ticks, HPET, the initial TSC calibration sample, and boot fault diagnostics. Debugger recovery is pending. |
| Memory | `Kernel/Mem/PageTables.HC`, `BlkPool.HC`, `MAllocFree.HC` | Initial identity map, code and data pools, and early heap allocation. Heap allocation and freeing, the 32-bit device range list, and the uncached device alias are active. |
| Tasking and CPUs | `Kernel/KTask.HC`, `Sched.HC`, `MultiProc.HC` | Core 0 runs Adam, the idle task, and spawned tasks with cooperative switching and PIT wakeups. Starting additional CPUs remains pending. |
| Storage | `Kernel/BlkDev/DskATA.HC`, `DskATAId.HC`, `DskCDDVD.HC`, `DskBlkDev.HC`, `FileSysRedSea.HC`, `DskFile.HC`, `DskStrA.HC`, `Kernel/Compress.HC` | Boot DVD identification, task-relative reads, writable RedSea RAM drives, and `.Z` expansion and encoding. Persistent writes and FAT32 are pending. |
| Graphics | `Kernel/Display.HC`, `Adam/Gr/GrScrn.HC` | VGA text and graphics composition, 16-color palette, cursor, zoom, and a scheduled refresh task |
| Input and sound | `Kernel/SerialDev/Keyboard.HC`, `Mouse.HC`, `Message.HC`, `Kernel/KMisc.HC` | PS/2 keyboard and mouse messages, PC speaker tones, and queued sound recording. Desktop input controls and AU output remain pending. |
| Desktop and system tools | `StartOS.HC`, `Adam/MakeAdam.HC`, `Adam/WinMgr.HC`, `Adam/DolDoc`, `Apps` | The refresh task draws visible DolDoc text and borders. The full editor and interactive desktop remain pending. |
| HolyC | `Compiler/CMain.HC`, `Lex.HC`, `Prs*.HC`, `Back*.HC`, `Kernel/KLoad.HC` | Source execution with `ExeFile` and `ExePutS`, compile-time streams, plain and DolDoc tokenization, scalar expressions and functions, typed pointers, packed classes, unions, fixed arrays and their initializers, print calls, and TOSB module generation and loading for supported source. Full language support and native kernel compilation remain pending. |
