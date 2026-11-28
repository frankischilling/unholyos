# Source layout and translation

The reference is [TempleOS commit c26482bb6ad3f80106d28504ec5db3c6a360732c](https://github.com/cia-foundation/TempleOS/tree/c26482bb6ad3f80106d28504ec5db3c6a360732c). Each subsystem should remain recognizable by its path, symbols, data records, and division of work. The current tree implements part of that design; matching a filename does not establish behavior or complete structural parity.

Keep the reference in a separate checkout outside the UnholyOS source tree. Verify its revision with the Git Human Workflow helper before comparing it, and leave its files unchanged. Reference source paths in this document are relative to that checkout.

## File mapping

Preserve the spelling and case of the reference directories and basenames. Use `.cpp` for C++ implementations, `.hpp` for declarations, and `.asm` for extracted assembly. A mixed HolyC source file can map to both a C++ file and an assembly file when it contains both kinds of implementation.

| TempleOS source | UnholyOS location | Responsibility |
| --- | --- | --- |
| `Kernel/KernelA.HH` | `Kernel/KernelA.hpp`, `Kernel/KernelA.inc` | Runtime records and constants, with assembly declarations where needed |
| `Kernel/KStart16.HC`, `KStart32.HC`, `KStart64.HC` | Corresponding `.asm` files under `Kernel/` | Firmware entry and CPU mode transitions |
| `Kernel/Mem/PageTables.HC` | `Kernel/Mem/PageTables.asm`, `PageTables.cpp` | Early page tables and memory mapping |
| `Kernel/KMain.HC` | `Kernel/KMain.cpp` | Kernel initialization and startup |
| `Kernel/KLoad.HC` | `Kernel/KLoad.cpp`, `KLoad.asm` | TOSB loading, patch records, module registration, and the public HolyC `Load` adapter |
| `Kernel/KDefine.HC` | `Kernel/KDefine.cpp` | String and list definitions, lookup, formatting, undefined-name exceptions, and packed color conversion as implemented |
| Compiler imports in `Kernel/Kernel.PRJ`, guarded-import guidance in `Kernel/KExts.HC` | `Kernel/KExts.hpp`, `KExts.cpp`, `KExts.asm` | Typed native reverse imports and guards for the separately loaded compiler |
| `Kernel/BlkDev/*.HC` | Corresponding `.cpp` files under `Kernel/BlkDev/` | Block devices, paths, files, and filesystems as implemented |
| `Compiler/*.HC` | Corresponding `.cpp` and `.asm` files under `Compiler/` | HolyC scanning, parsing, code generation, execution, and explicit C++ ABI adapters as implemented |
| `Adam/DolDoc/*.HC` | Corresponding `.cpp` files under `Adam/DolDoc/` | Native document records and operations as implemented |
| `Adam/ADefine.HC` | `Adam/ADefine.cpp` | Startup document name definition |
| `Adam/Opt/Boot/BootDVD.HC` | `Adam/Opt/Boot/BootDVD.asm` | DVD boot loader |
| `Adam/Opt/Boot/DskISORedSea.HC` | `Adam/Opt/Boot/DskISORedSea.cpp`, `DskISORedSea.asm` | RedSea ISO construction and the guest's stage-one image |

Use `Kernel`, including its capital K, in the filesystem, include paths, and build definitions. An existing Windows checkout can retain an older lowercase directory name even when Git tracks the correct case. Check actual directory names when preparing a Linux build.

`Linux/` holds C++ host support and verification programs. `Linux/BootProbe.cpp` runs inside the guest and contains the boot reports and fixture checks consumed by the host's `TestBoot.cpp`. Its declaration is in `Linux/BootProbe.hpp`. The Makefile lists it separately in `GUEST_TEST_SRC` and compiles it with the freestanding guest flags. `Linux/Fixtures/` holds HolyC source and binary inputs. `Kernel/SerialDev/Serial.cpp` and `SerialBoot.asm` provide serial diagnostics without a matching reference file. `Adam/DolDoc/DolDoc.hpp` and `Kernel/Compress.hpp` are additional C++ declarations. These additions support the translation and its tests; they do not represent completed TempleOS subsystems.

`Compiler/Compiler.bindings` is an explicit ELF-to-guest symbol manifest
for the host build. The Makefile builds the compiler's 17 C++ and four
assembly objects separately from the kernel and converts their combined
object to `build/Compiler/Compiler.BIN`. Every ISO includes that file at
`/Compiler/Compiler.BIN`. The pinned `Compiler/Compiler.PRJ` instead
uses HolyC's project compilation and import directives.

## Structure and style

Keep each source function's responsibility with its corresponding subsystem. Retain the reference's function and record names and relative order where possible. Use explicit adapters for C++ entry points that must cross the HolyC ABI. Some current internal entry points have a `Cpp` suffix while the guest exports retain the reference names.

C++ requires declarations, casts, and calling-convention adapters that HolyC does not. Use the closest valid C++20 form while preserving the reference behavior and binary layouts. Keep the implementation procedural. Hosted tools can use the C++ standard library for filesystem and process access.

The reference uses two-space blocks, opening function and class braces on the next line, compact operator and comma spacing, and tabs for alignment. NASM files preserve uppercase Intel instructions and registers. The formatter expresses only part of this style. Existing C++ still uses standard type spellings such as `int64_t`, `uint8_t`, and `double`; these have not been converted throughout the tree to TempleOS-style names. Type spelling changes need explicit size and layout checks where records cross the guest ABI.

## ISO records

The ISO writer maps to `Adam/Opt/Boot/DskISORedSea.HC`. Its reference symbols include `FillU16Palindrome`, `FillU32Palindrome`, `CElTorito`, `RedSeaISO9660Stage1`, and `RedSeaISO9660`; the shared descriptor records come from `Kernel/KernelA.HH`. The host tool in `Linux/MakeISO.cpp` and the guest use the same descriptor and loader patch functions.

The `PRS1B_CLASS` case in `Compiler/PrsVar.HC` assigns consecutive member offsets without inserting alignment padding. As a result, `CISODirEntry` occupies 34 bytes and `CISOPriDesc` occupies 2,044 bytes. Its `root_dir_record` starts at byte 152, `publisher_id` at byte 314, and `file_structure_version` at byte 877. C++ representations need packed fields and assertions for these sizes and offsets.

`RedSeaISO9660` passes the address of the entire `root_dir_record` to `FillU32Palindrome`, so the root-cluster palindrome starts at byte 152. The writer and its media checks must use that location. The reference allocates and writes a full 2,048-byte sector with `CAlloc(DVD_BLK_SIZE)`, leaving four zero bytes after the descriptor. These details follow from source review; boot and compatibility evidence belongs in [Boot.md](Boot.md).

## Ahead-of-time HolyC

`CmpJoin` and `Cmp` in `Compiler/CMain.cpp` follow the reference's
isolated compilation and TOSB patch-table stages. The current compiler
uses RIP-relative references within the module. It emits
`IET_REL32_EXPORT` for public definitions, `IET_IMM_I64` for typed
runtime calls, and `IET_MAIN` for top-level statements. The existing
loader consumes those records. The map writer follows
`MapFileWrite` in `Compiler/CHash.HC` and emits DolDoc source links.
It has no binary debug line tables yet.

`Compiler/PrsStmt.cpp` follows `PrsFunJoin` and `PrsGlblVarLst` for
typed declarations. The expression parser and back end retain the
declared types at call and data access sites. The AOT writer uses
`IET_IMM_I64` for `import` and `_import` fixups. A forward JIT call
reads the function address from its declaration, and the later
definition updates that slot. `Compiler/CExcept.cpp` and its assembly
adapter implement the reference's `UndefinedExtern` path.

## Public HolyC module loading

`Kernel/KLoad.cpp::LoadCpp` follows `Kernel/KLoad.HC::Load`, sharing the
native image-loading core with the existing C++ boot callers.
`Kernel/KLoad.asm::LoadHC` adapts its three HolyC stack arguments and
24-byte cleanup. The exported `Load` returns the binary header address.
The compiler declaration preserves `Kernel/KernelC.HH`'s `U8 *` return,
default flags, and `CBinFile *` destination default of `INVALID_PTR`.
`Compiler/AsmInit.cpp` installs the 32-byte packed `CBinFile` class,
including signed `I64` origin and size fields, and the loader constants.
Native size and offset assertions cover every header member.

The public path uses the reference's `.BIN.Z` default, existing compressed
file lookup, `FileExtDot` module naming, three load flags, explicit
destination precedence, and header ownership rules. A missing file
returns `NULL`; rejected binaries raise `BinModul`. The C++ parser keeps
its stronger bounded validation. `Linux/Fixtures/LoadApiProbe.HC` runs
unchanged in both guests against the same NASM consumer and provider.
The native boot probe stages and verifies its RAM-drive inputs; it does
not implement the public API. [Host modules](HostModules.md#public-holyc-loader)
describes the contract and [Boot.md](Boot.md#public-holyc-load) records the
matching GCC, Clang, and pinned-guest results. Invalid destination handling
and allocation-failure parity remain unverified.

`Kernel/KLoad.cpp::LoadPass2Cpp` scans the serialized patch table in the
order of `Kernel/KLoad.HC::LoadPass2`. Its raw HolyC export uses a
three-argument adapter in `Kernel/KLoad.asm`, with no additional built-in
compiler declaration. The existing C++ descriptor overload calls the
same scanner. Direct calls trust the supplied table, ignore their third
argument, preserve `Call`'s null-target check, and propagate MAIN
exceptions through the unchecked HolyC call entry. These responsibilities
remain in the loader; the descriptor and ABI adapter are C++ additions.
`Linux/Fixtures/LoadPass2Probe.HC` verifies the same raw tables and loaded
module behavior in the pinned guest and native GCC and Clang builds.
Its local hash-record views remain test support. The matching trace and
input hashes are recorded in [Boot.md](Boot.md#raw-holyc-loadpass2).

## Compiler ABI ownership

The public adapters for `Cmp`, `ExePutS`, `ExeFile`, and
`StreamExePrint` are in `Compiler/CMain.asm`. `StreamPrint` and
`StreamDir` use `Compiler/CMisc.asm`; `MemberMetaData`,
`MemberMetaFind`, and `MemberFind` use `Compiler/LexLib.asm`.
These nine adapters are C++ ABI additions beside the matching
translations of Terry A. Davis's reference routines. They retain
their existing instruction sequences, HolyC names, arguments,
saved registers, and stack cleanup.

The shared `HC_FRAME_ENTER`, `HC_FRAME_CALL`, `HOLYC_CPP`, and
`HOLYC_PRINT` macros are in `Kernel/KernelA.inc`.
`Kernel/KUtils.asm` includes them for its remaining adapters;
it no longer owns the compiler adapters. The definitions emit
instructions only where an adapter invokes them.

`Kernel/KLoad.cpp` publishes 45 explicit native function addresses
under `__unholyos_sysv.`. Their suffixes follow the source entry
names, such as `HashFind` and `StrCmpCpp`; a binding manifest still
names the exact ELF symbol. These System V entries coexist with the
public HolyC adapters. The loader also exposes `winmgr` and
`sys_task_being_scrn_updated` as data. The existing
`sys_cur_submenu_entry` export remains available. The compiler manifest
maps `sys_var_init_flag` and `sys_var_init_val` to
`SYS_VAR_INIT_FLAG` and `SYS_VAR_INIT_VAL`.

`Compiler/Compiler.bindings` imports 44 of those native functions and
32 data symbols. It exports twelve public adapters, 16 native
compiler bodies under `__unholyos_sysv.`, and the module-owned
`__unholyos_compiler.CInitStatus`. The twelve public names are published
by the module rather than `LoadKernel`. `CInitHC` in
`Compiler/CMain.asm` is the sole `IET_MAIN` target.

The reference has calls in both directions across this module boundary.
`ExeCmdLine` remains in `Kernel/KTask.cpp`, matching
`Kernel/KTask.HC`, while it calls the compiler's `LexStmt2Bin`.
`Compiler/CMain.HC` calls the kernel's `ExeCmdLine`.
`HashDel` in `Kernel/KHashB.HC` explicitly imports
`ClassMemberLstDel` from the compiler. Those responsibilities remain
in their original subsystems. `Kernel/KExts.hpp` declares the
16-field native dispatch record using the actual function-pointer
types and checks its 128-byte layout. `KExts.asm` uses only `R11` to
check the table and target before tail-jumping. An unbound call raises
`UndefExt`; a bound call preserves arguments and the stack.

`LoadCompilerImports` in `Kernel/KLoad.cpp` accepts only unique,
module-owned relative exports within the payload. It validates the
full aligned status word and its initial zero value, along with all
16 targets, before `KExts.cpp` publishes a complete table once under
`CMemLock`. An unresolved or already-started module is rejected.
The private `ExePutS` and `ExeFile` exports use the actual native
bodies returning `CExeResult`, whose size is 24 bytes with fields at
offsets 0, 8, and 16. The assembly dispatch preserves the hidden
return-storage argument. Their public HolyC adapters retain the
reference's integer return convention.

After PS/2 setup, `Kernel/KMain.cpp` loads `/Compiler/Compiler.BIN`
from the selected boot drive with `LDF_JUST_LOAD | LDF_SILENT`, binds
the reverse imports, and invokes `LoadPass2`. The Adam task, its heaps,
and all ten kernel system definition lists already exist. Adam's palette
and DolDoc definitions, graphics contexts, the window manager, and user
tasks have not been initialized yet.
`CInitCpp` sets `CInitStatus` from zero to `-1` on entry and to `1`
only if both `CmpLoadDefines` and `AsmHashLoad` succeed. KMain sets
`RLF_COMPILER` only after the entry completes successfully and reports
status `1`.

This C++ entry does not reproduce all of pinned `Compiler/CMain.HC`'s
`CInit`: it omits that routine's `CmpFillTables` and
`cmp.ic_nop` setup. `AsmHashLoad` now calls `UAsmHashLoad` before publishing
the opcode dictionary. GCC and Clang have built
the separate module successfully, and both guest builds passed all seven
focused compiler failure cases and the boot and startup matrices,
including a cold boot of the ISO built in the guest.
[Host modules](HostModules.md) describes the binding rules and
native-import fixture. Its pinned-guest bridge is test
scaffolding, not native C++ support in TempleOS.

## Compiler initialization lists

`Compiler/CInit.cpp` retains `Compiler/CInit.HC::CmpLoadDefines` in its
reference subsystem and preserves all 14 list names, entries, and call
order. `CInitCpp` invokes it before `AsmHashLoad`, following the first
step of pinned `Compiler/CMain.HC::CInit`. The function's boolean failure
return is a native adaptation for the existing initialization status.

An explicit private native binding calls the three-argument
`Kernel/KDefine.cpp::DefineLstLoadCpp`. Its records, names, copied data,
indices, and source links belong to Adam's data heap, while the indexed
strings remain in the resident compiler module. A null caller frame
records `AD:0x0`, consistent with other native boot definitions and
different from the reference's caller-address metadata. Registration
failure leaves completed lists in place; a later opcode failure retains
all 14. [Host modules](HostModules.md#compiler-definition-lists) records
the list counts, ownership, initialization order, and remaining steps.
`Compiler/AsmInit.cpp` supplies the missing typed `DefineMatch`
declaration from `Kernel/KernelC.HH`; its existing public adapter and
runtime implementation retain their three-argument ABI.
The common source and native failure-phase evidence are recorded in
[Boot.md](Boot.md#compiler-definition-lists).

## Compiler primitive types

`Compiler/CInit.cpp::internal_types_table` preserves the 17 entries at
the start of pinned `Compiler/CInit.HC`, including `I0i` and `I0` with
raw type 2. `Compiler/CompilerA.hpp` declares the 10-byte
`CInternalType` record and checks its field offsets. The compiler
module exports the actual writable 170-byte array under its reference
name. `Compiler/AsmInit.cpp::CmpFillHash` reads that array directly;
its former private primitive list has been removed.

C++ string initialization zeroes the unused bytes of each `name[8]`.
Pinned `Compiler/PrsVar.HC::PrsVarInit2` instead copies the full fixed
array length from a temporary string. The checked-in `Compiler.BIN`
therefore contains nonzero bytes after some name terminators. This
translation retains zero-filled tails; it does not reproduce those
bytes from temporary storage. The common probe checks the declared
fields and terminated names, while the native host check verifies the
entire deterministic C++ table before and after module initialization.

The six integer-view classes remain separate and follow their
`Kernel/KernelA.HH` order: `U16`, `I16`, `U32`, `I32`, `U64`, and `I64`.
Their base records retain raw type `RT_PTR`, as in `PrsClassNew`; a
`fwd_class` link identifies the scalar primitive. `ClassRawType` follows
the complete forwarding chain, matching `OptLib.HC::OptClassFwd`.
Each primitive uses `PrsClassNew`'s existing allocation for one base
record and four pointer records. `PrsClass` accepts both zero-width
raw types, and the explicit cast path in `PrsExp.cpp` accepts them as
source or destination types. It still rejects unsupported aggregate
casts and other invalid zero-size classes. `BackC.cpp::CCodeOut::Convert`
leaves floating-point results unconverted when either side is `I0` or
`U0`.

The reference registers these primitives once in `AsmHashLoad` and
stores their addresses in `cmp.internal_types`. The native compiler
still initializes separate child hash tables on first use. The table
migration restores the listed fields and registration inputs; shared
compiler ownership, `CCmpGlbls`, `CmpFillTables` and `cmp.ic_nop`
remain unfinished.

`Compiler/UAsm.cpp` builds separate 16/32-bit and 64-bit instruction
views from the non-alias entries of the original opcode dictionary.
`Ui` resolves prefixes, opcode extensions, registers, ModR/M and SIB
operands, immediate values and relative destinations for the supported
forms. `U` walks an instruction count and `Un` walks a byte range;
unsupported or truncated encodings advance by one byte. Their typed
headers and stack adapters use the existing compiler module ABI. These
are partial translations of the reference disassembler: complete
selection rules, operand families, 16/32-bit behavior, source links
and debugger presentation remain to be verified or implemented.

The primitive probe checks selected record fields and pointer
companions. Its original comparison at `24030d55` preceded the class
allocation and list-initialization changes below; it does not establish
complete class-record or allocation parity.
The common source and compiled-module comparison is recorded in
[Boot.md](Boot.md#compiler-primitive-types).

## Compiler class records

`Compiler/PrsLib.cpp::PrsClassNew` follows the constructor in
`Compiler/PrsLib.HC`: five adjacent 128-byte records come from the
current task's code heap. Each starts with the class hash type, pointer
raw type, eight-byte size, and its pointer-star count. Only the base
record then receives size zero and a `last_in_member_lst` pointer to
its own `member_lst_and_root` slot. The four pointer companions retain
their zeroed list fields.

`MemberAdd` recognizes that empty-list sentinel. Its tree insertion
already assigns the root slot, so it skips the duplicate store through
the sentinel and appends normally through later real members. This
preserves the reference's layout without treating a pointer field as
a complete C++ `CMemberLst` object. Offset assertions cover the leading
`next` field and the class's tail field. Null tails remain supported for
the separate native function constructors.

After failed completion of a forward class, `PrsClassDef` retains the
sentinel restored by `ClassMemberLstDel`; it no longer overwrites it
with null. That retry path is checked as native recovery behavior.
The constructor still returns null if allocation fails, whereas the
pinned constructor assumes allocation succeeds. Function-record
construction and lazy per-task type registration remain separate gaps.
The common lifecycle comparison and native retry checks are recorded in
[Boot.md](Boot.md#compiler-class-records).

## HolyC assembler

`Compiler/Asm.cpp` follows `PrsAsmBlk` and `PrsAsmInst` in
`Compiler/Asm.HC`. `Compiler/AsmResolve.cpp` holds `AsmResolve` and
`AsmUnresolvedAdd`, corresponding to the routines in
`Compiler/AsmResolve.HC`. The current compiler resolves references
when emitting each assembly block; the reference has separate
`AOTLocalsResolve` and `AOTGlblsResolve` passes. JIT assembly publishes
its labels after resolving the block.
For assembly inside a function, `Compiler/PrsStmt.cpp` records
`IC_ASM` as in `PrsStmt` in `Compiler/PrsStmt.HC`. The assembler
retains the references until `Compiler/BackC.cpp` assigns code
offsets, following the `IC_ASM` case in `Compiler/OptPass789A.HC`.
The statement compiler and assembler share the function's frame offsets.
Their named labels use a single function-local table, following
`COCGoToLabelFind` in `Compiler/PrsLib.HC`. The back end assigns offsets
to HolyC and assembly labels before patching branches in either direction.
`Compiler/CMain.cpp` emits TOSB exports at assembly label offsets,
`IET_REL_I32` and `IET_IMM_I64` for named imports, and
`IET_ABS_ADDR` for image-base fixups.

The raw `PUT_HEX_U64`, `PUT_HEX_U32`, `PUT_HEX_U16`,
`PUT_HEX_U8`, `PUT_CHARS`, and `PUT_STR` entry points from
`Kernel/StrA.HC` live in `Kernel/StrA.asm`. They retain register
arguments and call the C++ text routines through explicit adapters.
`StrPrintHex` and `PutHex` live in `Kernel/StrA.cpp`. `PutHex` returns
early for a negative width to keep its 17-byte buffer in bounds.
`Compiler/AsmInit.cpp` installs the `PUSH_C_REGS` and `POP_C_REGS`
definitions from `Kernel/KernelA.HH` in the compiler's bootstrap hash.
The source lexer expands them through the same path as `#define`.
`AsmHashLoad`, `AsmPrsInsFlags`, and `AsmPrsArgType` in the same
file read Terry A. Davis's unchanged `Compiler/OpCodes.DD` from the
pinned tree. `AsmPrsArgType` uses the operand list from
`Compiler/CInit.HC`. Each Makefile ISO target includes the dictionary at
`Compiler/OpCodes.DD`; `AsmHashLoad` reads it through `FileReadCpp` and
releases the source buffer after parsing. The old assembly embedding
has been removed.
`Compiler/Asm.cpp` uses its opcode, alias, and register records for
instruction selection and emission. The current encoder covers integer,
segment, and x87 forms used by the tests. Far and `MOFFS` operand forms
remain pending. [Pinned opcode dictionary](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Compiler/OpCodes.DD)

The reference's `Kernel/KMain.HC` changes to `/Compiler` before loading
the compiler. `CInit`, called by `Compiler/CMain.HC`, then invokes
`AsmHashLoad`, whose filename is relative to that directory. The native
module load uses `/Compiler/Compiler.BIN` on the selected boot drive,
and the dictionary read uses `::/Compiler/OpCodes.DD`; both preserve
the caller's directory. These absolute paths are explicit C++
adaptations. A failed dictionary read returns `false` without
publishing the shared table, permitting a later retry. Non-null input
retains the existing parser behavior.

The startup-test ISO's opcode-resource probe runs after reverse-import
binding and before `CInitHC` so it can exercise the missing first read,
retry, and cache through the loaded compiler. Its temporary drive and
task state are restored before the module entry. Normal startup lets
`CInitCpp` load the dictionary. KMain rejects a failed entry or a
status other than `1` before setting `RLF_COMPILER` or initializing
the later Adam subsystems. The controlled diagnostic and return to
the idle path use the C++ status contract; the reference's startup
failure after a missing dictionary has not been reproduced.

`ORG` and `PrsBinFile` in `Compiler/Asm.cpp` follow `Compiler/Asm.HC`.
`BINFILE` uses `ExtDft` and `FileRead` with the native `.BIN` default.
`Compiler/CMain.cpp` writes the requested origin into `CBinFile.org`.
`Kernel/KLoad.cpp` places the module header at that address and uses
the following bytes as the relocation base. The loader validates the
patch table and allocates patch records before copying to caller-owned
storage.

Relative instruction forms follow `PrsAsmInst` and `AsmStoreNum` in
`Compiler/Asm.HC`. `AsmResolve` checks the chosen displacement width
after labels are resolved, following `Compiler/AsmResolve.HC`.
Literal targets and local labels use offsets in the assembled image.
Named imports keep the chosen width in their TOSB relocation records.
The remaining assembler diagnostics include the reference's
suggestion to use an I8 displacement when a wider branch would fit.

`Compiler/AsmLib.cpp` implements `AsmLineLst` from
`Compiler/AsmLib.HC`. It keeps offsets into the active source so a
popped include cannot leave a stale line pointer. `LexPutLine` in
`Compiler/CExcept.cpp` handles plain source and calls `DocPutLine`
for DolDoc. Listing output uses provisional instruction bytes from
`AsmStoreNum` before symbols are fixed up.

The compiler also registers the 65-byte `CCntsGlbls` guest class
from `Kernel/KernelA.HH`. Its typed `cnts` symbol aliases the
kernel storage defined in `Kernel/KGlbls.cpp` and uses the export
from `Kernel/KLoad.cpp` when linked in a TOSB module.

## HolyC switch statements

`PrsSwitch` in `Compiler/PrsStmt.cpp` follows `PrsSwitch` in
`Compiler/PrsStmt.HC` for case ranges, implicit case values, and
`start`/`end` groups. Each group has its own `break` target. Dispatch
and fallthrough share the group entry code through `IC_SUB_CALL`.

`Compiler/BackC.cpp` emits the jump table and the bounds check for
`switch ()`, following `ICSwitch` in `Compiler/BackC.HC`. The current
back end stores fixed 32-bit relative offsets. TempleOS also selects
smaller entries when the destinations fit.

## Cooperative task scheduling

`Kernel/Sched.asm` follows `_YIELD` and `RESTORE_SETH_TASK_IF_READY` in
`Kernel/Sched.HC`. `Kernel/Sched.cpp` maintains the runnable and child
queues. `IRQ_TIMER` in `Kernel/KInts.asm` rechecks the queue when a tick
interrupts the idle task. An interrupt on a running task restores that
same task.

`Spawn` and the task exit path in `Kernel/KTask.cpp` follow
`Kernel/KTask.HC`. A new task starts with its HolyC data argument above
its return address to `Exit`. `HeapCtrlDel` in `Kernel/Mem/HeapCtrl.cpp`
returns the task's pages after the dying queue delay. The bootstrap
scheduler checks that queue from `Yield`. TempleOS uses the window
manager on Core 0 and the Seth loop on other cores for this check.

`Compiler/PrsExp.cpp` and `Compiler/BackC.cpp` emit an inline FS read for
`Fs` and relocate a function address taken with `&`. The bootstrap
headers expose the task fields used by the scheduler. Full function
pointer declarations and the rest of the HolyC `CTask` members remain
pending.

## Task jobs and messages

`Kernel/Job.cpp` follows `Kernel/Job.HC` for source execution requests,
text input, messages, and completion queues. `SrvTaskCont` in
`Kernel/KTask.cpp` puts a servant to sleep on an empty queue.
`Kernel/SerialDev/Message.cpp` follows `ScanMsg`, `GetMsg`, `FlushMsgs`,
and `InputFilterTask` in `Kernel/SerialDev/Message.HC`. The compiler
inlines `GetRFlags` and exposes the queue fields and message constants
used by these APIs.

`ScanMsg` returns an empty result when a foreign task has an execution or
text job at the head of its queue. It leaves that job for its owning task
to run. The reference can keep retrying that entry. The port also reads
the keyboard description flag before releasing the message allocation.

Cross-core dispatch through `JobQue` and the popup and terminal
wrappers remain pending.

## PS/2 keyboard

`Kernel/SerialDev/Keyboard.cpp` follows `Kernel/SerialDev/Keyboard.HC`
for controller setup, the translated scan tables, and the
`KbdPktRead`, `KbdHndlr`, and `KbdMsgsQue` stages. `IRQKbd` in
`Kernel/KInts.asm` acknowledges IRQ1 and buffers bytes. Queue delivery
runs from the scheduler and message scanner until the window manager
takes over. `KbdMsInit` allocates the five input FIFOs from the task heap.

`Kernel/KDataTypes.cpp` and the shared ring operations in
`Kernel/KernelB.hpp` implement both FIFO types from `Kernel/KDataTypes.HC`.
`Kernel/KernelA.hpp` checks both 32-byte layouts against `KernelA.HH`.
The compiler registers the classes, their typed buffer and index fields,
and the declarations from `KernelC.HH`; the HolyC stack adapters in
`Kernel/KUtils.asm` are published by `Kernel/KLoad.cpp`.
The keyboard and mouse records expose their FIFO pointers with those types.

`Kernel/SerialDev/Message.cpp` carries the reference `ScanChar`,
`ScanKey`, `GetKey`, and `GetChar` paths. Echo and the raw cursor use
the current text output in `Kernel/KeyDev.cpp`. Key name formatting
and the system hotkey callbacks remain pending.

## PS/2 mouse

`Kernel/SerialDev/Mouse.cpp` follows `Kernel/SerialDev/Mouse.HC` for
auxiliary port commands, packet assembly, wheel negotiation, position
scaling, and grid snapping. The shared controller read path routes each
byte using the auxiliary bit. `IRQMsHard` acknowledges the cascaded PIC
interrupt and buffers bytes. A failed reset leaves IRQ12 masked.

`WinMsInput` in `Adam/Win.cpp` transfers decoded positions to `ms`
as the bootstrap scheduler drains input. `WinQueIPMsgs` in
`Adam/WinMgr.cpp` delivers motion and button messages relative to the
focused task. Wheel changes are available through `ms.pos.z`.
`WinMsUpdate` runs on the frame loop to decay mouse speed and recenter
zoom. Control hit-testing remains pending.

## PC speaker

`Kernel/KMisc.cpp` follows `Kernel/KMisc.HC` for note conversion, PIT
channel 2 programming, mute, beep, and sound event capture.
`F64Log2Bits` in `Kernel/KMathA.asm` uses the x87 instruction sequence
from `_LOG2` in `Kernel/KMathA.HC`. The packed `CAUData`, `CSndData`,
and `CScrnCastGlbls` records in `Kernel/KernelA.hpp` follow
`Kernel/KernelA.HH`. `Kernel/KUtils.asm` adapts the exported HolyC
stack calls to the C++ ABI.

`SleepUntilCpp` sets the idle bit and wake jiffy before yielding to the
scheduler. `Adam/Gr/ScrnCast.cpp` follows the `SndShift` and `AUWrite`
functions in `Adam/Gr/ScrnCast.HC`. The compiler exposes the packed
sound types and the live `scrncast` global. The AU writer includes the
queue sentinel and stores timestamps as raw CDate values, followed by
one signed note byte. Music notation playback is described below;
video capture and the remaining desktop sound APIs are pending.

## VGA palette

`Adam/Gr/GrPalette.cpp` follows `GrPalette.HC` for attribute-controller
identity mapping and the 16-color standard and grayscale palettes.
Color writes use the upper six bits of each component. Reads expand those
bits back to the reference 16-bit range. `CBGR48` preserves the blue,
green, red, and padding words and crosses the HolyC call boundary in one
64-bit argument slot or return register.

`GrInit2` runs the palette setup during the bootstrap. The reference
calls it when loading the Adam graphics stack. The task palette reset callback remains pending.

## Graphics device contexts

`Adam/Gr/GrDC.cpp` carries the reference `CDC` allocation and ownership
rules, fixed-point transforms, depth buffers, lighting, and GR
serialization. `GrMath.cpp` supplies the matrix copy used by `DCCopy`.
`CDC` retains its 32-byte on-disk header and the optional palette that
precedes the pixel body. `DCSave` and `DCLoad` use the kernel's TempleOS
LZW codec. `GRRead` also checks that the serialized data fits in the file.

`DCExt` and `DCDiff` in `GrBitMap.cpp` follow the reference's inclusive
rectangle, clipping, and difference bounds. The vertical difference
scan advances by the logical width even when the bitmap is padded, as
in `GrBitMap.HC`. `DC2Sprite` in `SpriteNew.cpp` writes the packed
bitmap element and terminator, and `DocGR` in `GrDC.cpp` inserts that
sprite into a document. Invalid geometry and failed allocations return
without dereferencing a missing buffer.

`GrBitMap.cpp` expands the four screen bitplanes into indexed pixels in
the same order as `GrBitMap4ToBitMap8` in `GrBitMap.HC`. `DCScrnCapture`
in `GrDC.cpp` copies either the displayed (possibly zoomed) screen or the
unzoomed composite. `GrComposites.cpp` carries `GRScrnCaptureWrite` and
`GRScrnCaptureRead`, including the current VGA palette on write and the
equivalent raster operation on read. Its HolyC adapter widens the SysV
Boolean return to preserve the reference's full-register TRUE and FALSE.
Movie capture and playback remain pending.

`GrInit2` allocates the six screen contexts. The persistent `gr.dc`
starts transparent with `DCF_SCRN_BITMAP | DCF_ON_TOP`. The context
holds HolyC ABI addresses for the transform and lighting callbacks.
HolyC can call the two functions by name and assign the typed
`CDC.transform` callback. Typed access to `CDC.lighting` remains
pending.

## 2D raster drawing

`Adam/Gr/GrBitMap.cpp`, `GrPrimatives.cpp`, `GrComposites.cpp`, and
`GrMath.cpp` follow the reference for points, clipping, stepped lines,
blits, and filled rectangles. `GrAsm.asm` reads the text-cell window
depth buffer. The bootstrap gives Adam the full screen and initializes
the depth buffer to zero until the window manager owns it.

The blit and rectangle loops work on bytes in the padded bitmap. Opaque
blits read a word's source pixels before writing the corresponding
destination pixels, including when the two contexts share storage.
The unclipped plot tests depth. Clipped 2D drawing bypasses that depth
test. Probability dithering uses the task RNG from `Kernel/KMathB.cpp`.
The HolyC adapters extend narrow RNG returns through RAX. `Line` shares
the same walker with the typed HolyC callback adapter.

## Graphics transforms and refresh

`Adam/Gr/GrMath.cpp` follows `Mat4x4MulMat4x4Equ`, the three
`Mat4x4Rot` routines, the scale and translation routines, and
`DCSymmetrySet` in `GrMath.HC`. Rotations multiply in the reference
order and store 32.32 matrix elements. `Kernel/KMathA.asm` uses the
x87 sine and cosine instructions from the HolyC templates. The
compiler registers both names for the reference binary64 value of pi.
`Compiler/PrsExp.cpp` and `Compiler/BackC.cpp` translate the math intrinsics
`Sin`, `Cos`, `Sqrt`, `SqrI64`, `MinI64`, and `MaxI64` using the opcodes
declared in `Kernel/KernelB.HH` and emitted by `Compiler/OptPass789A.HC`.
`D3I32Norm` follows `Adam/Gr/GrMath.HC` and returns binary64 through the
HolyC ABI.

`Refresh` in `Adam/WinMgr.cpp` waits on the window manager's completed
frame count. It follows `Refresh` in `WinMgr.HC` for forced wakes,
message pumping, and restoring the caller's idle and full-refresh
flags. An early call before the window manager starts can return
immediately. The window manager also returns immediately if one of
its callbacks calls `Refresh` while painting.

## 3D bitmap drawing

`GrBlot3` in `Adam/Gr/GrPrimatives.cpp` follows the three-corner
transformation and 32.32 sampling in `GrPrimatives.HC`. It walks pixels
in column order, skips consecutive duplicate destination coordinates,
and uses the device context's depth buffer through `GrPlot1`.
`DCSymmetry3Set` and `DCReflect` follow `Adam/Gr/GrMath.HC`. The former
passes ten fixed arguments through the HolyC adapter. Transform
callbacks run through the HolyC stack convention, including callbacks
assigned by guest programs.

`GrHLine` and `GrVLine` in `GrBitMap.cpp` follow the depth-buffered
spans in `GrBitMap.HC`. They interpolate 32.32 depth from the original
endpoints, apply window scroll and clipping, and keep the reference
dither sample order even for rejected pixels. `GrFillTri0` in
`GrPrimatives.cpp` preserves the scan direction and inclusive edges
from `GrPrimatives.HC`, including the shared row in opposing sweeps.
`GrFillPoly3` and `GrRect3` in `GrComposites.cpp` follow the triangle
fan in `GrComposites.HC`. They call the device context's transform
callback and reflect vertices when symmetry is enabled.

`GrRayLenMinus` and `GrRayLen` in `Adam/Gr/GrBitMap.cpp` preserve
the leftward scan and two-direction drawing pass from `GrBitMap.HC`.
They check window coverage at character-cell boundaries and update
explicit depth values before testing the boundary color. `GrFloodFill`
and `GrFloodFill3` in `GrPrimatives.cpp` follow the reference's
`CFFRay` state machine and restore the brush, secondary color and flags.
The mirrored pass in `GrFloodFill3` runs first; the normal pass replaces
its returned count. The C++ traversal stops at the reference's fixed
stack allocation limit rather than writing outside it.

## Round pens and 3D lines

`GrSetUpTables` in `Adam/Gr/GrInitB.cpp` builds the circle spans and
the solid, collision, even, and odd pen bitmaps in `GrInitB.HC`.
The original array dimensions are visible to HolyC code through
`gr`. The bootstrap releases the allocated pens if graphics setup
runs out of memory.

`GrFillCircle`, `GrPlot3B`, `GrPlot3`, and `GrLine3` in
`GrPrimatives.cpp` follow `GrPrimatives.HC`. Small pens use the
precomputed bitmaps and spans. Larger circles compute the edges
before converting them to integer coordinates. Thick lines use 32.32
stepping in three axes and draw their body and caps with the
depth-buffered spans. A custom brush or nonunit step uses the point
callback, including the original endpoint rules.

`DCThickScale` in `GrMath.cpp` applies the matrix norm in
`GrMath.HC`. `Sprite3` in `GrSpritePlot.cpp` applies it when reading
`SPT_THICK` and restores the caller's width. Point and line elements
use the 3D primitives, including their packed variants.

## Circle and ellipse walkers

`Adam/Gr/GrMath.cpp` follows `Circle` and `Ellipse` in
`GrMath.HC`. Rotation uses a 24-bit fixed-point coefficient and
an 8-bit point fraction. The circle calls the plotter at each
step; the ellipse skips consecutive duplicate coordinates.
The callback adapters use the HolyC stack convention and
preserve the Boolean return. The public compiler headers describe
the four-argument plot callback rather than an untyped address.

`GrCircle`, `GrEllipse`, `GrCircle3` and `GrEllipse3` in
`GrPrimatives.cpp` follow their counterparts in `GrPrimatives.HC`.
The 2D variants temporarily bypass the depth buffer. The transformed
3D variants project the two ellipse axes and restore the transform
flag after plotting. `F64ArgBits` in `Kernel/KMathA.asm` follows
the reference `_ARG` x87 instruction sequence.

## Regular polygons and splines

`Adam/Gr/GrMath.cpp` follows `RegPoly`, `Bezier2`, `Bezier3`, `BSpline2`
and `BSpline3` in `GrMath.HC`. The polygon walks each edge with `Line`.
The curve walkers keep the reference's adaptive step, coefficient
expressions, duplicate suppression and endpoint rules. Closed splines
construct the additional control segment before walking the curves.
`D3I32Dist`, `D3I32DistSqr` and `D3I32NormSqr` expose their native
return types through the HolyC ABI. The compiler accepts all twelve
arguments of `RegPoly` and the typed plot callback.

`Adam/Gr/GrPrimatives.cpp` implements the corresponding 2D and 3D
device-context wrappers from `GrPrimatives.HC`. `GrLinePlot` sets the
depth and calls `GrPlot1` before returning TRUE. `GrRegPoly` temporarily
bypasses the depth buffer; the flat Bezier and spline wrappers retain
it. The 3D wrappers transform and reflect the control points before
plotting, and restore the context flags. `GrRegPoly3` shares the
ellipse-axis projection. `BSpline2` and `BSpline3` release their scratch
allocation on callback refusal in UnholyOS; the reference returns
before `Free(c)` on that path. Allocation failure and extreme control
coordinates have not been compared. Additional sprite elements remain
pending.

## Arrows and raster text

`GrBorder` and `GrArrow3` in `Adam/Gr/GrComposites.cpp` follow
`GrComposites.HC`. Each arrow uses the native line walker for the shaft
and its two wings. The arrow reflects the endpoints before calculating
the wings, and the shaft alone uses the caller's start offset.
`DrawStdMs` and packed `SPT_ARROW` elements use the same routine.

`GrPutChar` and `GrPrint` in `Adam/Gr/GrBitMap.cpp` follow
`GrBitMap.HC` for clipping, extent recording, collision, XOR, tab
stops, and `SX` and `SY` offsets. The leading cell keeps the reference's
preceding-byte read when a glyph is unaligned. Both raster and screen
renderers read glyphs through the writable `text.font` pointer.

`PressAKey` in `Kernel/StrB.cpp` prints the blinking prompt and
reads a nonzero character without echo, as in `StrB.HC`.

## Window input masks

The HolyC compiler exposes the `WIf_` bit indices, `WIF_` masks, and
`WIG_` groups from `Kernel/KernelA.HH`. `TaskInit` sets `WIG_TASK_DFT`,
and `UserCmdLine` switches to `WIG_USER_TASK_DFT`, as in `KTask.HC`.
`Demo/Graphics/Symmetry.HC` uses the same mask expression as the
reference to receive left and right mouse drags while drawing.

The border handling from `Adam/WinMgr.HC`'s `WinMgrTask` lives in
`Adam/WinMgr.cpp`. It uses the same hit order for title, left, lower
right, bottom, and right edges. The held drag keeps its task through
successive refreshes, using `WinHorz` and `WinVert` for bounds and
raising the task with `WinToTop` at the start. `WIF_SELF_BORDER`
and `WIF_FOCUS_TASK_BORDER` gate the gestures and title actions.
The control dispatch in `Adam/WinMgr.cpp` checks control rectangles or
their HolyC hit-test callbacks before the border hit zones. It keeps a
captured control until release and applies wheel changes to the
focused window under the pointer. During a capture the local
thumb rectangle moves while the screen hit bounds stay cached
until the control is released.

`Adam/Ctrls/CtrlsA.cpp` follows the reference `CCtrl` queue, its
coordinate updates, and the horizontal and vertical scrollbar
callbacks. The HolyC compiler sees both `CCtrl` and `CWinScroll`
with their reference layouts and callback signatures. DolDoc
reads pending positions and publishes the document range in its
full screen recalc pass. The scrollbar cursor search follows the
visible entry start, as in `Adam/DolDoc/DocRecalc.HC`.

The same control queue hosts the X, Y, and Z sliders from `CtrlsA.HC`.
`ViewAnglesNew` creates one per task and `ViewAnglesDel` unlinks it.
`CViewAngles` follows the 72-byte record in `Kernel/KernelA.HH`. The
slider rounds to a multiple of two before clamping, and its drawing
preserves HolyC's precedence in the scale tick expression.

`Adam/DolDoc/DocTerm.cpp` puts `MENU` and `[X]` in the border
document at the reference positions. The rest of the standard
border document from `Adam/DolDoc/DocTerm.HC`'s `DocBorderNew`
still needs its title, task address, and editor indicators.

## Task settings and drawing callbacks

`Adam/TaskSettings.cpp` follows `SettingsPush`, `SettingsPop2`, and
`SettingsPop` in `TaskSettings.HC`. The 392-byte `CTaskSettings` layout
and its callback types come from `Kernel/KernelA.HH`. Settings form a
stack for each task and cover the directory, draw and end callbacks,
song and animation tasks, window bounds and attributes, document
flags, and palette. The pop applies the screen settings again after a
forced refresh. It saves whether autocomplete has a live task and
passes that setting to `AutoComplete` on pop. `Adam/AutoComplete/ACTask.cpp`
starts and stops the pane task and raises it without changing keyboard focus.

`WinBorder` in `Adam/Win.cpp` preserves the one-cell geometry changes
and previous-state return from `Win.HC`. `GrUpdateTaskWin` in
`Adam/Gr/GrScrn.cpp` calls the visible task's `draw_it` with a task-owned
alias of `gr.dc2`, using the HolyC stack convention. `TaskEnd` in
`Kernel/KTask.cpp` defers deletion while the window manager uses that
task and schedules the task's end callback before teardown.

## Screen composition and VGA

`Adam/Gr/GrScrn.cpp` builds the text background and glyph layer in
`gr.dc2`, then overlays nontransparent pixels from `gr.dc`.
`Kernel/FontStd.cpp` and `Kernel/FontCyrillic.cpp` preserve Terry A.
Davis's two 256-glyph fonts from the corresponding HolyC sources.
`SysGrInit` in `Kernel/KMain.cpp` initializes `CTextGlbls`, the VGA
aliases, border characters and raw-screen buffer. `CtrlAltF` in
`Kernel/KeyDev.cpp` exchanges the active and auxiliary fonts. The
keyboard handler invokes it directly; the general shortcut callback
registry and raw console rendering remain pending.
`TextLenAttrStr` in `Adam/Gr/GrTextBase.asm` clips packed character
and attribute cells. `TextPrint` in `GrTextBase.cpp` formats through
the native variadic bridge and uses the clipped text-layer writer.
The typed memory fills in `Kernel/KUtils.asm` use the reference
`REP STOSB/W/D/Q` sequences and return the end pointer. The native
compiler exposes their typed signatures and packed text attribute
constants. `Adam/DolDoc/DocRecalcLib.cpp` implements the cursor,
cursor highlighting, and scrollbar flag setters and returns the
previous visibility state. The compiler also exposes the task's
window width and height used by the original `CharDemo.HC` and
`Maze.HC`. These demos run their own HolyC code and draw callbacks;
the host driver supplies keyboard and mouse input, not game logic.
The bootstrap puts its first status line in
`gr.text_base`. `GrUpdateScrn` calls `gr.fp_final_scrn_update` on an
on-top alias of the composite. `WinFinalUpdate` invokes `DrawMs` before
packing the frame. Cursor callbacks receive their original HolyC stack
arguments. A callback exception drops the current frame and releases
the alias. The compiler attaches the HolyC signatures to the callback
slots. Terry A. Davis's unchanged `Demo/Graphics/Grid.HC` saves and
replaces the cursor callback through its original typed declaration.

`DCBlotColor4` in `Adam/Gr/GrAsm.asm` caches eight indexed pixels at a
time and packs their low four bits into VGA planes. The screen update
can zoom those planes before copying them to VGA. `GrUpdateLine64`
reverses bits for VGA scan order and writes changed words. The
flush path also rewrites unchanged words.

`WinMgrTask` schedules refreshes at the reference 1001/30000-second
period using rational jiffy deadlines. It has its own task and stack.
The manager owns the persistent screen context's window geometry.
`DrawStdMs` calls `GrArrow3` with the reference cursor angle and pen
width. In VGA text mode the bootstrap inverts the cursor cell while encoding the frame so the stored text
attribute stays stable between refreshes.

`GrScaleZoom` updates the mouse scale and offset on all three axes.
`CtrlAltZ` in `Adam/Gr/GrEnd.cpp` doubles the scale, or halves it with
Shift. The keyboard decoder dispatches this shortcut while the rest of
the reference callback table is pending.

`Adam/WinMgr.cpp` implements Ctrl grab scrolling from `WinMgrTask` in
`Adam/WinMgr.HC`. It captures the eligible window at the pointer,
snaps horizontal and vertical drag offsets to eight pixels, and resets
all three offsets on Ctrl-right. The PS/2 input pump captures the grab
before queuing task messages. The window manager applies the offsets
before drawing the next frame. `WinScrollNull` and `WinScrollRestore`
in `Adam/Win.cpp` expose the reference HolyC ABI and keep the border
document in place during a pan. The autocomplete pane exception remains pending.

`Adam/Menu.cpp` follows the menu records, parser, allocation, and drawing
code in `Adam/Menu.HC`. Menu arguments use the HolyC lexer and expression
evaluator, including lexical includes for menu files. `CMenu` and
`CMenuEntry` keep the reference record layouts and `CTask.cur_menu`
points to the top of the menu stack. Menu parse failures throw the
`Compiler` exception. `LexExceptReport` follows the source-position report
in `Compiler/CExcept.HC`. It prints the token, file link and source line,
and passes the location to `FixSet`. `MenuNew` reports before freeing the
lexer context and raises the exception after cleanup. The boot-phase
debugger entry, compiler error counter and other parser callers remain pending.

`Adam/WinMgr.cpp` follows `WinKeyNavMenu` and the mouse menu loop in
`Adam/WinMgr.HC`. Mouse selection starts at the screen's top edge.
The GUI key opens keyboard selection, the arrow keys position the
highlight, and releasing the GUI key sends the highlighted command
to the focused task. UnholyOS tracks that virtual cursor during
drawing so the input pump can keep sampling the physical mouse.

## Wallpaper progress bars

`Kernel/KStart32.asm` holds the four contiguous 80-byte `CProgress`
records declared by `Kernel/KernelA.HH`. The zero-initialized storage is
in the kernel BSS. The `SYS_PROGRESS*` labels and the typed HolyC
`progress*` globals address the same fields as `sys_progresses`.
The C++ view stores `t0` and `tf` as IEEE-754 bits for the freestanding
floating-point helpers. HolyC sees both fields as `F64`.

`Adam/Win.cpp` follows `DrawProgressBars`, `ProgressBarsRegTf`, and
`WinFinalUpdate` from `Adam/Win.HC`. The bars draw before the menu and
mouse. `Kernel/KMisc.cpp` calls the registry writer directly from
`ProgressBarsRst` and then clears all four records. The reference
resolves the same writer through `CallExtStr`. Dictionary installation
uses the first bar through `ACProgressTask`.

## DolDoc windows

`Adam/Gr/GrInitB.cpp` follows `WinZBufFill` and `WinZBufUpdate` to
assign depth in task queue order and track which windows remain visible.
The bootstrap keeps its depth buffer empty until the first window update.
The map clears cells before walking the queue, so a removed window
leaves no stale depth even when the desktop does not fill the screen.
`Adam/Gr/GrScrn.cpp` draws visible windows over the boot banner and
background, then expands that text layer on the next graphics refresh.
`Adam/Gr/GrTextBase.asm` clips and writes text cells.
`Adam/Gr/GrTextBase.cpp` fills rectangles and draws the original CP437 frame.

`Adam/DolDoc/DocDblBuf.cpp` provides the task's put, display, and border
document accessors. `DocRecalcLib.cpp` draws the display and border
documents using `RECALCt_TO_SCRN` in `DocRecalc.cpp`. This path handles
text cells, attributes, tab stops, scrolling offsets, and cursor placement.
As in `DocRecalc.HC`, partial screen passes use cached entry coordinates to
stop at the bottom of the visible region. Full passes update entry positions,
cursor coordinates, and scroll ranges. They also consume pending
scrollbar requests. `WinMgrSleep` measures the frame rate for the full-pass
interval. `DOCF_DO_FULL_REFRESH` and `DOCF_BWD_MOVEMENT` override that
interval. A document locked by another task is retried on a later refresh
in the cooperative bootstrap.

`RECALCt_FIND_CURSOR` finds the closest cell in the laid-out text and
updates the document cursor. `Adam/WinMgr.cpp` finds the top visible
window at the pointer, selects the entry on mouse-down, and draws a
double-line border while the button is pressed. Releasing a button with
a left expression sends Space to the document task. `DocPutKey.cpp`, `DocChar.cpp`,
and `DocRun.cpp` handle numeric commands, the Escape flags, and entry
and link callbacks. Right-click selection sends Enter to the document
task. Entry callbacks run with the document unlocked. Live tags in
`DocRecalc.cpp` receive the locked document, entry, and owning memory
task. A tag callback that throws falls back to an empty tag during
that refresh. `DocRecalcLib.cpp` supplies `DocBottom` and the hold-aware
`DocClear` pass. Complex wrapping, data widgets, and the full
editor remain pending.

`Adam/DolDoc/DocTerm.cpp` creates the put, display, and border documents
from `DocTermNew` in `Adam/DolDoc/DocTerm.HC`.
`DocGet.cpp` follows `DocScanLine` and `DocGetStr2` to read input from the
edited document. `Kernel/SerialDev/Message.cpp` dispatches `GetStr` and
`GetS` through DolDoc or the raw keyboard path.
`Kernel/KTask.cpp` starts `UserCmdLine` and runs the native compiler using
the line stream from `Compiler/CMisc.cpp` and `Compiler/Lex.cpp`.
`CmdLinePmt` adds the trailing semicolon used by the reference's lexer
lookahead. Input skips the original filter and form actions while those
editor subsystems are pending. `HomeSys.cpp` loads the original
`::/Doc/Start.DD` in `UserStartUp` and waits until each user task
reaches its prompt before raising the first and calling `WinTileVert`.
`Adam/ADefine.cpp` supplies the `DD_OS_NAME_VERSION` title definition
used by the document. `Adam/Win.cpp` follows `WinToTop`, `WinFocus`,
`WinRefocus`, and the reference tiling functions. The window manager
restores focus on a refresh after the focused task exits. The terminals
use generic frames below the boot banner. `UserStartUp` calls `Dir`
after showing each terminal. `XTalk` input and autocomplete remain pending.

`Adam/DolDoc/DocFile.cpp` follows `DocRead`, `DocInsDoc`, `DocType`
and `Type`. It reads through the task's file path and inserts copies
of the parsed entries at the cursor. `DocNew.cpp` duplicates the
entry-owned strings and attachments. In raw mode `DocPutS.cpp` scans
the source document into lines for output. `DocRead` enters the
document's directory while it parses the file and restores the caller's
directory afterward. `DocPlain.cpp` and `DocBin.cpp` resolve
`DOCEF_BIN_PTR_LINK` by attachment number or tag. A cyclic link produces
an error entry. Saving preserves the `BP` reference and omits the
external binary from the appended data. Graphics-file dispatch through
`DocGR` and file-link navigation remain pending.

`Adam/DolDoc/DocRecalc.cpp` follows `DocWordWrapDel`, `DocSplitTag`, and
`DocWordWrapAdd` in `DocRecalc.HC` for ordinary editable text. `$WW,1$`
enables word wrapping. Text splits at the preceding space when one is
available within the reference's search range, and the inserted soft
newline is omitted from serialization. A change in window bounds or
cursor position permits rejoining the fragments before reflow. Complex
tag editing and the complete DolDoc wrapping and highlighting rules
remain pending.

`Compiler/PrsExp.cpp` handles `TK_INS_BIN` as in `PrsExp.HC`. It retains
the attachment's exact byte length in the generated image and gives
HolyC a `U8 *` to the bytes. `Adam/Gr/Gr.hpp` maps the packed element
records from `Gr.HH`. `SpriteNew.cpp` follows `SpriteElemSize` and
`SpriteSize` in `SpriteNew.HC`. It bounds reads when the caller supplies
the attachment length. `GrSpritePlot.cpp` follows `DocSprite` and
`Sprite` from `GrSpritePlot.HC` and gives the document its own copy of
the sprite data. `Sprite3` draws points, packed point paths, lines,
and polylines with the active pen width. `DocRecalc.cpp` draws sprite
entries through a task alias of `gr.dc2` during screen refresh.
Bitmap and rectangle elements pass through `GrBlot3` and `GrRect3`.
The two flood-fill elements pass through `GrFloodFill3`; the boundary-color
variant clears the secondary color for that element and restores it before
the next one. These elements support depth, transforms, and planar symmetry.
`Sprite3B` applies its
temporary translation and restores the caller's context. The remaining
3D drawing elements, sprite hit testing, and the sprite editor remain
pending.

## Directory listing

`Kernel/BlkDev/DskFind.cpp` follows `FilesFind` and `FileFind` in
`DskFind.HC`. `FileSysRedSea.cpp` reads a bounded directory extent,
exposes the first record as `.`, resolves the `..` cluster, and skips
deleted records. `DskStrA.cpp` applies the case-sensitive wildcard,
exclusion, and file-class masks used by the reference. `DskDirA.cpp`
sorts the returned `CDirEntry` records by the reference keys and emits
the `PutDirLink` macro. Flattened file-only lists clear parent pointers
when the corresponding parent records are freed.

`Kernel/BlkDev/DskDirB.cpp` follows `Dir`, `Cd`, and `IsDir` in
`DskDirB.HC`. `KDate.cpp` supplies the `Date2Struct` conversion used
by the short listing. `Cd` visits each path component and retains the
valid prefix when a component fails. `DskDirContext.cpp` ports
`DirContextNew` and `DirContextDel`, using the native `CDirContext`
record to save the task's drive, directory, and mask. `FilesFind`,
`FileFind`, and `IsDir` use those contexts to resolve RedSea DVD,
RAM, and mounted read-only FAT32 paths and restore the caller's working
directory. `PopUpCd` and full drive error reporting remain pending.

## FAT32 reads and writes

`Kernel/BlkDev/FileSysFAT.cpp` follows `FileSysFAT.HC` for `FAT32Init`,
`FATFromName`, `FATNameXSum`, `DirLongNameFill`, `FAT32CDirFill`,
`FAT32FileFind`, `FAT32FileRead`, and `FAT32FilesFind`. The disk structures
in `KernelA.hpp` retain the reference's 512-byte boot and FSInfo sectors
and 32-byte directory records. Mounting checks the partition extent,
sector and cluster sizes, FAT capacity, root cluster, and FSInfo
signatures before exposing the volume. Cluster numbers map to the data
area starting at cluster 2. Reads reject invalid, cyclic, and short
allocation chains. DOS modification timestamps convert to `CDate`.

`DskATA.cpp` now reads ATA IDENTIFY capacity and LBA28 sectors using
PIO; `DskAddDev.cpp` recognizes validated primary MBR FAT32 partitions
on ATA devices. DVD startup probes the primary IDE master for these
partitions and assigns free drive letters starting at `C:`. The ATA
device is marked read-only. `DskBlk.cpp` dispatches reads and protected
block writes to that device. `DskATA.cpp` implements `ATABlkSel` and
`ATAWriteBlks` using LBA28 PIO sector transfers, with channel-wide
serialization, bounded status polling, and a cache flush. The byte-range
helper preserves untouched bytes of partial sectors. Raw writes require
the device to be writable or `BDF_READ_ONLY_OVERRIDE` to be set; the
default read-only mount remains in effect for filesystem mutations.
Extended partitions, general automatic IDE discovery, LBA48, multiple
mode, disk caches, and physical-sector sizes other than 512 bytes remain
pending.

The generic path and lookup helpers in `DskFile.cpp` dispatch to either
filesystem; directory contexts, `Cd`, masks, recursive listings, whole-file
reads, and parent search use the same native `CDirEntry` ABI. Names follow
the reference's case-sensitive lookup and limited long-name support:
ASCII VFAT names of up to 25 characters, validated sequence and checksum,
or the short name when no supported valid long name is present. The
short alias of a valid long name is not an alternate lookup, matching
the pinned reader. Unicode conversion, longer VFAT names, active FAT
selection other than FAT 0, and complete error diagnostics remain pending.

`FAT32AllocClus`, `FAT32FreeClus`, `FAT32DirFill`, `FAT32DirNew`,
`FAT32MkDir`, `FAT32FilesDel`, and `FAT32FileWrite` follow the corresponding
reference allocation, directory, and whole-file operations. Allocation
checks the available chain before changing either FAT, retains the
reserved high bits and groups link updates by FAT sector. Each group
updates both copies and restores the primary sector if the mirror fails.
This follows the sector granularity of `DrvFATBlkClean` in TempleOS.
Noncontiguous allocation
starts after the FSInfo recent-allocation hint. Whole-file replacement
writes a new chain before replacing the directory entry and reclaiming
the old chain. Large file writes group contiguous clusters and yield
between PIO batches while the drive stays locked, so the desktop can
display write progress. Directory
growth allocates and zeroes a cluster before
linking it; deletion tracks the short entry and its validated long-name
records, including names that cross a cluster boundary.

The writer bounds short aliases to eleven bytes and chooses a numbered
alias for a collision instead of emitting duplicate aliases. Nonempty
directory deletion is refused to avoid orphaning children. These are
intentional safety differences from the pinned code. Allocation and
replacement have best-effort cleanup on write failure, not a transaction
or general corruption-repair facility. There is no persistent FAT cache,
full filesystem exception parity, or Unicode support. FAT32 mutations
require an explicitly writable ATA device; the normal mount policy is
unchanged.

`DskFmt.cpp` follows `DskFmt.HC` for `Fmt` and partition type updates.
`FAT32Fmt` in `FileSysFAT.cpp` uses the pinned sector-per-cluster
thresholds, boot record, FSInfo signatures, two FAT copies, and root
cluster allocation. `BlkWriteZero` in `DskBlk.cpp` clears the metadata
region for a quick format or the entire partition for a full format.
The current ATA formatter accepts an existing mounted FAT32 primary
partition only, checks its extent and excludes overlapping partitions.
It preserves unrelated MBR bytes. `Fmt` honors confirmation and refuses
read-only devices; it also supports the existing RAM RedSea formatting
path. Formatting unmounted partitions, extended partitions, ATA RedSea,
progress indicators, and complete error and cache behavior remain pending.
Formatting is destructive and is not transactional.

## File streams

`Kernel/BlkDev/DskCFile.cpp` follows `DskCFile.HC` for `FOpen`,
`FSize`, `FBlkRead`, `FBlkWrite`, and `FClose`. The native `CFile`
record and HolyC class share the reference's 168-byte layout. RedSea
streams use contiguous blocks, including compressed files whose stored
bytes and final block padding are accessed directly. RAM streams support
preallocation, `w` replacement, `w+` updates, and metadata changes on
close, including preservation of the original timestamp. The containing
directory must already exist.

DVD reads, RedSea RAM streams, and FAT32 ATA streams share this interface.
RedSea writes still require a preallocated extent. FAT32 noncontiguous
streams can allocate and extend their cluster chains on demand; preallocated
and contiguous streams also support bounded writes. `FSetClus` tracks the
current cluster and flushes a dirty buffer before changing clusters or
closing. Reads from the current dirty cluster use the buffered data.
Close commits size and timestamp changes to the directory entry. An
explicit contiguous FAT32 stream requires the `.C` extension and checks
the chain before accessing its blocks. FAT32 mutations remain subject to
the explicitly writable device policy. Concurrent open-stream coherence,
general truncation, transactional recovery, and full error-path parity
remain pending.

`Kernel/BlkDev/DskCopy.cpp` follows `CopySingleZ` and `CopySingle` in
`DskCopy.HC`. Equal filename attributes select the raw block-stream path
with a 128-block buffer. Attribute changes select `FileRead` and `FileWrite`
to expand or encode data. Both paths preserve the source timestamp; the
stream path also preserves the logical file size and attributes. `FileAttr`
is in `DskStrA.cpp`, and native HolyC receives the typed `FileRead`,
`FileWrite`, `FileAttr`, and `CopySingle` signatures.

The current copy path checks block I/O return values that the reference
ignores. It can leave a partially written destination on failure and does
not guarantee transactional replacement. General error diagnostics and
resident-cache coherence for raw stream writes remain pending.

`Del` in `DskCopy.cpp` resolves a directory context and calls
`RedSeaFilesDel` in `FileSysRedSea.cpp`. The latter marks matching directory
entries deleted and releases their contiguous extents in the allocation
bitmap. It excludes dot names, counts files only, and deletes directories
only when requested. `DirMk` in `DskDirB.cpp` supports preallocating directory
blocks for a specified entry count. As in TempleOS, `Del` on a nonempty
directory with `del_dir` set is not a recursive deletion; use `DelTree`
to reclaim the descendant extents as well.

`Adam/ABlkDev/ADskA.cpp` follows `ADskA.HC` for `Copy`, `Move`, `CopyTree`,
and `DelTree`. It traverses the actual `FilesFind` lists, preserves their
ordering, and builds target paths from the directory context. `CopyTree2`
counts one for each visited directory, including the destination root, as
the reference does despite its files-only API comment. A same-drive
destination inside the resolved source directory is rejected. `ScanFlags`
in `Kernel/StrB.cpp` uses the file utility flag list from `KDefine.cpp`.
Read-only devices are refused before mutation. Writable RAM RedSea and
explicitly writable FAT32 ATA destinations have recursive file-tree
coverage. The FAT32 probe checks masked copy, move, nested trees, deletion,
fragmented stream growth, and reclamation across cold boots. Error paths,
resident-cache deletion behavior, and restoring a deleted current
directory are not yet fully matched to TempleOS.

## HolyC function pointers

`Compiler/PrsVar.cpp` follows `PrsType` in `Compiler/PrsVar.HC` for
parenthesized function-pointer declarations. Local variables, globals,
record members, and parameters carry their signatures in the existing
`CHashFun` records. An indirect call uses the same argument conversions
and defaults as a direct call. `Compiler/BackC.cpp` saves the target
before evaluating arguments and applies the signature's stack cleanup
rule. The original backend also uses `IC_CALL_INDIRECT` for this path.

The probe covers fixed signatures, pointer-to-pointer and array access,
floating-point arguments and results, and caller-pop and callee-pop
calls. It also rebinds a pointer while evaluating an argument.
Pointer arithmetic loses the callable signature, as in the pinned
compiler. Function-pointer casts and relocatable initializers are still pending.

`PrsDotDotDot` in `Compiler/PrsVar.cpp` follows the corresponding HolyC
routine for native variadic functions and pointer signatures. The hidden
`argc` and inline `argv` members follow the fixed parameters at positive RBP
offsets. Their count is excluded from `arg_cnt` and `member_cnt`; `argv`
retains the reference's 127-element array metadata. Variadic functions use
caller cleanup, including indirect calls. Explicit `argpop` variadic calls
remain unsupported.

## Class member metadata

`PrsMemberMeta` in `Compiler/PrsVar.cpp` follows the class-member branch
of `PrsVarLst` in `Compiler/PrsVar.HC`. A declaration can attach named
strings and evaluated expressions to each member, including `format`
strings. Each `CMemberLstMeta` is prepended to the member's list, so
repeated names resolve to the last declaration. `MemberMetaData` and
`MemberMetaFind` in `Compiler/LexLib.cpp` return the value and record,
respectively; the latter distinguishes a missing value from zero.
`ClassMemberLstDel` frees the metadata and owned strings.

`PrsFunArgs` and `PrsFunCall` handle `lastclass` defaults. The default
comes from the last explicitly supplied argument's class, before
parameter conversion. Scalar aliases forward to their internal class,
and pointer levels are stripped. JIT code uses the live class name;
AOT code copies it into the module's string data. Explicit arguments
override the default. Direct string defaults use the same ownership and
AOT storage rules. Constant metadata expressions work in AOT; expressions
that require executing or relocating unfinished AOT code remain pending.

`AsmInit.cpp` exposes the compiler's member and class records for HolyC
reflection, including `Fs->hash_table`, `HashFind`, and `MemberFind`.
The records retain their ABI sizes; debugger and external-usage pointers
are exposed as opaque addresses. The original `Demo/ClassMeta.HC` runs
unchanged.

`Adam/ADbg.cpp` follows `Adam/ADbg.HC` for `ClassRep2`, `ClassRep`,
and `ClassRepD`, using the compiler's actual class and member records.
It reports base classes, nested records, bounded arrays, scalar values,
and strings. The dynamic report uses bound DolDoc data entries; scalar
entries refresh from and write back to the reported memory. `Kernel/KDbg.cpp`
provides pointer validation, including the native fixed boot records for
the CPU and Adam task. `Gs` and the native `CCPU` member declarations
allow the original `Demo/LastClass.HC` to run unchanged. `CTask` reflection
still covers only the implemented member declarations; native reports
therefore need not have the reference's number of nested records.

`Adam/DolDoc/DocForm.cpp` follows `DocForm.HC` for metadata-driven form
construction, navigation, bound data formatting and scanning, list fields,
checkboxes, and reallocated string fields. `DocEd.cpp` handles modal editing
and restores the caller's document and window state. `DocPlain`, `DocPutKey`,
and `DocRecalc` connect the entries to parsing, input, and rendering.
The original `Demo/Dsk/DataBase.HC` and `Demo/DolDoc/Form.HC` run unchanged.
`DocDataScan` uses the shared `Kernel/StrScan.cpp` scanner for bound
fields. The full editor status bar, menu, undo, and task-end handling
remain incomplete. List selection uses a modal
document in the current task rather than a separate popup task.

`Kernel/BlkDev/DskDirB.cpp` exposes `HomeSet`, and the compiler headers expose
the existing RAM-drive creation and lookup APIs so the database can write
to a writable home directory. The same unchanged database also writes to
FAT32 ATA storage through the buffered file-stream APIs, with record
creation, editing, cancellation, and cold-reboot retrieval tested against
the pinned guest. `Compiler/PrsExp.cpp` accepts address-of on
array expressions used by the original form's `StrCpy(&fds.name, ...)`.
`Kernel/KLoad.cpp` exposes the existing `_STRCPY` adapter under its HolyC
name; the `ToUpper` adapter in `Kernel/StrA.cpp` supplies the reference
character conversion without the reference compiler's intrinsic lowering.

The kernel header table stores numeric and string defaults in a tagged
union. This saves space in the boot image while preserving the same
runtime argument records and the pinned DVD loader address.

## Date and numeric prompts

`Adam/DolDoc/DocGet.cpp` follows `GetI64`, `GetF64`, and `GetDate` in
`DocGet.HC`. Numeric expressions run through the native compiler and
retry on failure or an out-of-range result. Blank input returns the
default even if it lies outside the range. Date input uses the reference
calendar expression syntax in `Kernel/StrScan.cpp`.

`Kernel/KDate.cpp` shares the reference month tables between
`Struct2Date`, `Date2Struct`, and the calendar boundary functions.
`local_time_offset` starts at zero and is exposed as a native `CDate`.
`Now` converts the RTC's local time to UTC; date expressions calculate
local calendar boundaries before subtracting the offset. Date/time
formatting and directory listings add it back before displaying the
date. The public calendar records and routines are available to HolyC.

The date scanner splits fields before converting numeric prefixes and
narrows them to the reference record widths. Invalid month indices
return zero instead of reading outside the month tables. The pinned
`LastDayOfMon` advances December to month zero; the resulting date is
reproduced explicitly for common and leap years without an out-of-bounds
read. CPU fault recovery, full compiler diagnostics, and unordered
floating-point comparisons remain outside this prompt implementation.

## Queue primitives and source paths

`Compiler/BackC.cpp` emits the queue operations from `BackC.HC` and
the string-length operation from `Templates.HC`. The compiler keeps the
reference opcodes for `QueInit`, `QueIns`, `QueInsRev`, `QueRem`, and
`StrLen`. `CQue` exposes its two actual pointers. `StrCpy` uses the
existing native `_STRCPY` entry point and HolyC calling convention.

`StreamDir` in `Compiler/CMisc.cpp` obtains the active source directory
and writes its quoted name to the compile-time stream, as in `CMisc.HC`.
It skips define-expansion frames to reach the source file. The `__DIR__`
define invokes it through `#exe`. This is needed by the unchanged
application loaders rather than a fixed path for any particular app.

`Compiler/AsmInit.cpp` describes the implemented block-device globals
using the actual `CBlkDevGlbls` fields and array offsets, including
`home_dir`. Cache pointers remain outside the exposed record members
until the cache subsystem is implemented. The original `Apps/TimeClock`
sources use the native queue, date, prompt, and file APIs directly.
The probes set a temporary RAM home directory and restore it afterward.

## HolyC exception statements

`Compiler/PrsStmt.cpp` follows `PrsTryBlk` in `Compiler/PrsStmt.HC`.
The generated code installs the catch and cleanup labels with
`SysTry` from `Kernel/KExcept.HC`. A handler can accept by setting
`Fs->catch_except`. Declining leaves the original exception for the next
handler. Cleanup removes the installed handler on the normal path and
before evaluating a return from the try body.

The backend emits RIP-relative label addresses and uses the existing
external symbol machinery for the runtime calls in `.BIN.Z`. The
bootstrap's `SysTryHC` aligns its internal C++ call and saves the
unpadded guest stack for resumption. The original demo cautions against
returning from a catch body. That path is outside the tested scope.

`Kernel/KExcept.cpp` follows `Break`, `BreakLock`, and
`BreakUnlock` in `KExcept.HC`. `KeyDev.cpp` records the
Ctrl-Alt-C request and `Sched.asm` handles it after saving the
task context, following `BREAK_FOCUS_USER` in `Sched.HC`.
Locked tasks receive a pending flag; unlocking delivers the
exception. The Shift-Esc path posts a keyboard message without
replacing the saved return address. Native drive and block locks
do not yield while held. Full drive-lock release, general
Ctrl-Alt callback registration and preemptive scheduling remain
pending.

## Kernel math helpers

`Kernel/KMathB.cpp` follows the function order and branches in
`Kernel/KMathB.HC` for `Clamp`, `Min`, `Max`, `Pow10I64`, `FloorU64`,
`CeilU64`, `RoundI64`, `FloorI64`, and `CeilI64`. Unsigned intermediate
addition and subtraction preserve 64-bit wraparound; signed remainders
retain the reference's negative-divisor behavior. `RoundI64` removes the
signed remainder. `CeilI64` keeps its separate negative-input branch,
including the result of -10 for `CeilI64(-1,10)`.

The floating-point C++ entry points pass binary64 bits through integer
registers. `F64CompareBits` in `Kernel/KMathA.asm` uses `FCOMIP`, as emitted
by `Compiler/BackFB.HC`, and restores the x87 stack depth. The wrappers
preserve the pinned runtime's unordered comparisons: `Min` selects the
second operand, `Max` selects the first, and `Clamp` selects its upper
bound when the upper comparison is unordered. These choices also retain
the selected operand's signed-zero and NaN payload bits.

`Compiler/AsmInit.cpp` supplies the signatures from `Kernel/KernelC.HH`.
`Kernel/KLoad.cpp` and the explicit `KUtils.asm` adapters expose the same
HolyC names, argument order, return register, and callee stack cleanup.
The focused fixture runs through both source execution and a guest-built
TOSB module; the host driver compares them with the pinned runtime trace.
Fault delivery for zero divisors and signed division overflow, and
unmasked floating-point exceptions, remain outside this comparison.

## String scanning and transformations

`StrScan` in `Kernel/StrScan.cpp` follows `Kernel/StrScan.HC`'s format
loop and conversion order. The variadic adapter in `Kernel/KUtils.asm`
passes the two fixed arguments, argument count, and inline argument
vector to C++. The caller retains responsibility for the variadic stack.
`Kernel/KLoad.cpp` and `Compiler/AsmInit.cpp` expose the original name
and typed signature.

Literal format characters advance the source without testing equality.
An unspecified width ends at the following format character; a fixed
width consumes its full byte count, including embedded NUL bytes.
Justification and precision values do not change conversion, but runtime
widths and precision still consume their arguments. The zero-width
precision branch retains its separate reference control flow. `%s`
takes a pointer to a destination pointer, `%c` and `%C` write one byte,
and numeric, date, list, and define conversions write eight bytes.
Missing arguments and a null format raise `Scan`. Argument checks that
follow allocation retain that ordering on the exception path.

`Str2F64` follows the byte reads and branches in `Kernel/StrScan.HC`,
including its lookahead for the CP437 infinity byte and its returned end
pointer. Unsigned accumulators preserve the reference's signed 64-bit
wraparound in the significand, fractional digit count, and exponent.
The decimal scaling step loads the signed integer directly into x87,
multiplies by `Pow10I64`, and stores the final binary64 result. Converting
the integer to binary64 before multiplication loses a bit for inputs
such as `9007199254740993e-1`. The existing null-input guard remains.

`Str2Date` uses `MStrUtil`, `StrNew`, and `StrFirstRem` in the reference
allocation and parsing order. Relative day offsets are parsed before
reading the clock; calendar boundaries retain the fractional day and
32-bit day wraparound. Each local-time adjustment reads the exposed
`local_time_offset`. The documented calendar table guards still apply.

`DocDataScan` in `Adam/DolDoc/DocForm.cpp` uses an eight-byte temporary
for small scalar fields and passes larger destinations directly to
`StrScan`. It retains tag truncation, terminator restoration, and
reallocated string ownership from `DocForm.HC`. Like the reference,
this routine does not acquire an inner document lock; a `Scan`
exception can propagate without leaving that extra lock held. A list
with no tag stores zero, and a checkbox with no tag still stores its
checked state. The
compiler exposes the existing `CDocEntry.define_str` and `my_fmt_data`
members with offset assertions. Native pointer and metadata validation
still restrict invalid document entries.

The compiler also exposes the existing `MSize` and `MSize2` allocator
entry points, `CTask.code_heap` and `data_heap`, and the signature,
locking, accounting, and owner fields of `CHeapCtrl`. These declarations
use the original HolyC types and the existing `_MSIZE` and `_MSIZE2`
adapters. Assertions pin the shared record size and offsets; the rest
of the heap-control record's metadata remains incomplete.

`Kernel/StrA.cpp` implements `Spaces2Tabs`, `StrUtil`, the first and last
marker operations, and `StrFind` in reference order. `Kernel/StrA.asm`
retains the `_STRMATCH` instruction sequence, local label order, and
`RET 16`, with a separate C++ argument adapter. `Kernel/StrB.cpp`
implements `Tabs2Spaces`, `ScaleIndent`, and `MStrUtil`. The public
`SUF_*` and `SFF_*` flags come from `Kernel/KernelA.HH`.

Whitespace transformations precede case and safe-dollar conversion.
Trailing-only trimming preserves the first whitespace byte, and tab
columns continue across newline bytes. `StrFind` applies its leading
and trailing label checks in sequence. Allocated transformations keep
the reference allocation and freeing order; indentation uses the x87
rounding helper. Invalid pointers, malformed widths, allocation
exhaustion, and extreme indentation factors require further comparison.

## String comparison entries

`Kernel/StrA.asm` retains the instruction and label order of `TO_UPPER`,
`_STRICMP`, `_STRNCMP`, and `_STRNICMP` in `Kernel/StrA.HC`. The counted
comparisons test the count before reading either string, so a zero count
returns zero even for null pointers. Negative counts continue until a
mismatch or NUL. Results are exactly -1, 0, or 1, with unsigned byte
ordering and ASCII-only case folding.

The raw `TO_UPPER` entry reads and writes `AL` while preserving the other
bits of `RAX`. The comparison entries retain their HolyC stack arguments
and `RET 16` or `RET 24` cleanup. Their explicit C++ adapters preserve
`RBX`, which the reference routines use as byte scratch, and translate
the SysV argument registers into that stack layout. `KLoad.cpp` exports
the original raw symbols and public names; `AsmInit.cpp` supplies the
signatures from `Kernel/KernelC.HH`, including required count arguments.

`Str2F64` now calls `StrNCmp` for its infinity prefix, and `Str2Date`
uses it for the four calendar prefixes. The date scanner calls the
existing `StrOcc` for its slash test. Its public declaration retains
the reference header's `U8` character argument, while the internal C++
entry accepts the existing integer-width argument.

## Kernel string definitions

`Kernel/KDefine.cpp` implements `DefineLoad`, `Define`, `DefineCnt`, and
`DefinePrint` alongside the existing list functions from Terry A. Davis's
`Kernel/KDefine.HC`. `DefineLoad` allocates the record in the current
task's data heap, copies its name and string, sets `cnt` to -1, and leaves
`sub_idx` null. A null source string becomes an allocated empty string.
Adding another definition with the same name prepends a new record;
removing it reveals the older definition.

`Define` returns the record's copied data for either a string or a list.
`DefineCnt` returns its signed count. A null lookup name returns null or
-1, respectively; a missing non-null name uses the existing `UndefDef`
exception and diagnostic. The formatter retains its deferred-error
helpers for undefined definitions so it can release temporary output.
`DefinePrint` formats first, calls `DefineLoad`, then frees the temporary
string. A formatting exception therefore occurs before publication.

The public adapters remain in `Kernel/KUtils.asm`, with exports in
`Kernel/KLoad.cpp` and declarations from `Kernel/KernelC.HH` registered
by `Compiler/AsmInit.cpp`. `Define` and `DefineCnt` pop one stack argument;
`DefineLoad` pops two and passes its real HolyC frame. `DefinePrint` has
two fixed arguments followed by `argc` and inline `argv`, with caller
cleanup. Its return type is `U0`.

Direct `DefineLoad` calls record their HolyC caller's address.
`DefinePrint` records its internal call site, as the reference does.
The C++ implementation keeps `DefineLoadCpp` out of line and uses the
actual native return address for that internal call; ordinary HolyC
calls use the supplied frame. Existing native `DefineLstLoad` boot calls
retain their separate null-frame `AD:0x0` convention.

Native allocation failure frees unpublished string-definition storage
and returns null. The reference does not check those allocations, so
exhaustion behavior is not claimed to match. `StrPrintJoin` raises at the
read of a missing argument, before formatting a replacement value or
appending the completion NUL. Bytes written by earlier conversions or
literal text remain intact. The pinned formatter checks arguments at the
same point. Its null-destination allocation behavior still differs:
TempleOS allocates `STR_LEN` bytes before reading the format, while native
`CPrintOut` allocates lazily. A missing first argument therefore retains
the reference's allocation but creates none natively; native output
already allocated before a later missing argument remains retained.
Existing list index arrays
still point to the original list strings, and `HashDel` still leaves
their `sub_idx` allocation alone, following the pinned implementation.
The common fixture and runtime evidence are recorded in
[Boot.md](Boot.md#kernel-string-definitions). Full Adam definition
initialization requires further translation.

## Kernel system definitions

`Kernel/KDefine.cpp::SysDefinesLoad` preserves the ten lists in pinned
`Kernel/KDefine.HC::SysDefinesLoad`, in their original order. Their 170
entries occupy 980 literal bytes, including the final empty strings.
This replaces the separate native color and file-flag initialization
helpers. The record, name, copied data, index, and source link for each
list belong to Adam's data heap; the index pointers retain the original
strings in the kernel image.

| List | Entries |
| --- | ---: |
| `ST_OFF_ON` | 2 |
| `ST_HTT_TYPES` | 32 |
| `ST_DAYS_OF_WEEK` | 7 |
| `ST_MONTHS` | 12 |
| `ST_FILE_ATTRS` | 13 |
| `ST_FILE_UTIL_FLAGS` | 25 |
| `ST_BLKDEV_TYPES` | 6 |
| `ST_DRV_TYPES` | 6 |
| `ST_COLORS` | 16 |
| `ST_INT_NAMES` | 51 |

The intervening space entries remain intact. `ST_INT_NAMES` places
`MP Crash`, `Wake`, and `Dbg` at indices 48, 49, and 50. The reference's
HolyC `$$` spelling in the file-utility list becomes one literal `$` byte
in C++.

`Kernel/KMain.cpp` follows the reference's relative sequence through
`UncachedAliasAlloc`, `LoadKernel`, `SysGrInit`, `SysDefinesLoad`, and
`IntInit1`. Adam's task record, hash table, and heaps already exist when
the kernel symbols and definitions are registered. The new boolean
initialization gate returns before interrupt setup on failure and leaves
earlier successful registrations in place. Pinned HolyC does not perform
those allocation checks. Other boot responsibilities still differ;
native `Core0TaskInit` creates the idle task and does not replace the
reference's APIC and CPU-array initialization in `Core0Init`.

The system lists therefore precede compiler loading, while
`Compiler/CInit.cpp::CmpLoadDefines` supplies its fourteen lists when the
module entry starts. Adam's `ST_RAINBOW_10` and `DD_OS_NAME_VERSION`
definitions remain in their reference subsystems and initialize later.
`GrPaletteInit` retains its existing position after successful compiler
initialization.

The ten system lists use 50 native allocations totaling 4,296 bytes with
the existing null-frame `AD:0x0` source links. That convention differs
from the reference's internal HolyC caller addresses. Compiler failure
checks distinguish these kernel-owned literals from the compiler's
literals and Adam's later definitions. The common fixture, phase checks,
and remaining evidence limits are recorded in
[Boot.md](Boot.md#kernel-system-definitions).

## Color conversion

`Kernel/KDefine.cpp` follows the remaining declarations in Terry A. Davis's
pinned `Kernel/KDefine.HC`: `Color2Str`, `str2color_lst`, `Str2ColorU16`, and
`Str2ColorU32`. The color records retain their two-byte and four-byte layouts.
The native bridges pass packed integers, matching the reference's
`public U16 class` and `public U32 class` declarations. Explicit HolyC
adapters and typed compiler metadata preserve the argument and result types.
The mutable `str2color_lst` pointer also has a raw data export, matching the
reference's ordinary AOT global export. It has no added typed header
declaration; both parsers read its current pointer value.

| Function | HolyC arguments | Result | Stack cleanup |
| --- | --- | --- | ---: |
| `Color2Str` | Buffer, packed `CColorROPU32` | Original buffer pointer | 16 bytes |
| `Str2ColorU16` | String pointer | Packed `CColorROPU16` | 8 bytes |
| `Str2ColorU32` | Mutable string pointer | Packed `CColorROPU32` | 8 bytes |

`Color2Str` clears the destination, writes `TWO|` before `HALF|`, then writes
the palette name, `TRANSPARENT`, or `INVALID`. It writes the second color
only when `ROPF_DITHER` is set; probability-dither alone does not select that
path. Other raster-operation bits are omitted. Palette names are passed
to `CatPrint` as format strings, as in the reference. A shadowed `ST_COLORS`
definition can therefore expand percent escapes or raise a formatting
exception.

`Str2ColorU16` does not change its input. It stops at NUL or any character
in `str2color_lst`, initially `/,)}>`. Alphabetic tokens accumulate `TWO`
and `HALF` flags or replace the current color. Matching uses the existing
case-insensitive definition lookup, including unique abbreviations and
ambiguous-prefix rejection. Numeric parsing delegates to `Str2I64` with
radix 10, retaining its explicit radix-prefix behavior. Values from 0
through 255 fit the color byte; 16 is the invalid sentinel. An invalid
token or out-of-range number returns that sentinel and discards flags.
The parser allocates and frees alphabetic tokens in the reference order.

`Str2ColorU32` allocates a segment buffer for each non-null call and uses
`StrFirstRem` to remove the first segment and delimiter from the caller's
string. A single color leaves an empty string. Remaining text supplies
the second color and enables dither; the second parser stops at its next
delimiter. The reference never frees the segment buffer, including on
failure, and the translation retains that allocation. A null input returns
the invalid sentinel without allocating. This lifetime is intentional
compatibility behavior, not ownership transferred to the caller.
The allocation paths retain the reference's unchecked behavior; allocation
exhaustion is outside the reproduced cases.

The reference Sprite code uses these routines when formatting and parsing
color elements and building its bitmap menu. Those callers still require
translation; the common public-API fixture does not establish Sprite
subsystem parity. Exact comparisons and their limits are recorded in
[Boot.md](Boot.md#kernel-color-conversions). The color-name formatting
error case also exposed the shared formatter's deferred missing-argument
error. `CPrintArgs::Take` now raises immediately; a separate supplied-buffer
fixture checks partial output, dynamic arguments, and exception restoration.
`Compiler/AsmInit.cpp` also registers the existing `CatPrint` export with
the reference's two `U8 *` fixed arguments, variadic caller cleanup, and
`U8 *` result.

## Music playback

`Adam/ASnd.cpp` follows the note conversion, timing, notation parsing,
and playback routines in `Adam/ASnd.HC`. `Adam/ASnd.hpp` retains the
packed `CMusicGlbls` layout, including its seven-entry note map.
The native compiler exposes those fields and the typed runtime
functions. Playback uses the existing speaker and scheduler routines.
`MusicSetNoteLen` retains the pinned HolyC literal's binary64 value for
`0.25`, `0x3FD0000000000001`. C++ parses the same decimal spelling as
`0x3FD0000000000000`, so the translated sixteenth-note case uses the
observed bits explicitly. Triplet and dotted modifiers operate on that
value. The default tempo, staccato factor, and other note lengths use
matching C++ constants.
The single-CPU implementation protects beat updates with interrupts
disabled. `Play` also checks the lower bound of the note index before
accessing `music.note_map`; the reference only checks the upper bound.
Characters below `A` outside the notation modifiers therefore become
rests instead of reading outside that array. SMP synchronization,
`CurSongTask`, the waveform functions, `Noise`, and `Sweep` remain
pending. Terry A. Davis's original song stays in `Demo/Snd/OhGreat.HC`.

## Linked lists and queues

`Kernel/KDataTypes.cpp` follows the order of Terry A. Davis's
`Kernel/KDataTypes.HC` list and queue routines. `LinkedLstCopy` copies the
entire allocated payload of each node with `MAllocIdent` and replaces
its first pointer with the next copied node. `LinkedLstCnt` counts nodes
through that pointer. `QueCopy` copies the allocated head and each entry,
initializes the new head, and inserts entries in the original order.
`QueCnt` and `QueSize` exclude the sentinel head; both size routines
include the allocator overhead through `MSize2`.

The native list implementation reads and writes link fields with
`MemCpy` so packed records do not require C++ pointer alignment.
The copy routines use the existing allocator's task or heap selection.
If allocation fails, they free the partially copied list or queue and
return null. The reference assumes allocation succeeds. The public
HolyC adapters follow the existing `KUtils.asm` convention with one
or two stack arguments and the original names exported by `KLoad.cpp`.
`Compiler/AsmInit.cpp` registers all eight list and queue functions
with the declarations and defaults in `Kernel/KernelC.HH`.
The matching guest test covers empty lists, allocated payloads, link
order, ownership and cleanup. See [Boot.md](Boot.md#linked-lists-and-queues).

## Drive, filename and directory services

`Kernel/BlkDev/DskDrv.cpp` and `DskBlkDev.cpp` hold the drive and device
slot lookup, signature checks, drive letter aliases, writable queries,
drive selection, remapping and identity reporting from the corresponding
reference files. `DskAddDev.cpp` retains mounting and initialization.
The ATA mount keeps its identify record on Adam's heap for the model and
serial routines. ATAPI devices leave this record unset, as the reference
does. The drive color tables retain the reference names and contents and
are exposed as data exports. The compiler registers their array metadata,
the drive and device members, and the typed HolyC declarations.

`Kernel/BlkDev/DskStrA.cpp` supplies extension handling, filename checks,
temporary names and the directory-entry lookup. `DskStrB.cpp` translates
`CFileNameTo`, `Name2DirClus` and `Name2ParentDirClus`; the filesystem's
bounded internal traversal shares that implementation. `DskDirA.cpp`
provides extension-first sorting, cluster sorting, recursive flattening
and entry/tree disposal. The flattening return value is the final link
slot, which aliases the last entry because `next` is its first field.
As in TempleOS, a flattened files-only result can retain parent pointer
values belonging to freed directory entries; callers must not dereference
them. `DskFind.cpp` exposes `FilesFind2` for the current directory and
`DskDirB.cpp` emits the directory-creation diagnostic before dispatch.

The native mount path still requires canonical drive letters, supported
device types and validated partition geometry. Its slot locking is not
the reference's task-owned lock protocol, and its writable filesystem
guard deliberately rejects writes to read-only disks. The reference's
removable-media initialization sequence remains partial. Directory resolution uses the existing bounded
filesystem lookup and lacks the reference drive lock; failure and
allocation-exhaustion paths are not fully matched. See [Rewrite audit](RewriteAudit.md)
for their dependencies.

## Cluster and directory-entry services

`Kernel/BlkDev/DskClus.cpp` translates `ClusNumNext`, `Clus2Blk`,
`ClusBlkRead`, `ClusRead`, `ClusBlkWrite`, `ClusWrite` and `ClusAlloc`
in reference order. The APIs retain the reference's RedSea arithmetic,
FAT32 next-link traversal, cluster-to-block conversion, partial-block
counts and return conventions. `DskDirA.cpp` dispatches `DirNew` to
the filesystem implementations. Native exports and HolyC declarations
include the public cluster calls, directory publication and private
filesystem allocation and free routines.

The RedSea RAM implementation copies the supplied 64-byte disk record,
sets the contiguous attribute, preserves or releases the prior extent
according to `free_old_chain`, and updates the parent entry or boot root
when a directory grows. FAT32 publishes the supplied cluster chain and
metadata, retains the long-name alias on replacement, and reclaims the
old chain when requested. Both `DirNew` implementations return false
even after successful publication, as in the reference.

The native implementation rejects malformed geometry and invalid cluster
links, protects the root and overlapping or shared chains, and refuses
nonterminal append, freeing an overlapping extent, or replacing a
nonempty directory with a different chain. Same-chain metadata changes
preserve the chain; FAT32 replacement may stay in place instead of using
an earlier deleted slot. Allocation uses the per-drive FAT sector cache
and RedSea free-extent queue described below.
Low-level freeing still relies on caller ownership outside the checked
root, parent and directory entries. Rollback is best effort on hardware
write failures, and full reference locking, break handling and exception
semantics remain pending. RedSea mutation supports RAM drives and writable
file-backed images; FAT32 mutation supports explicitly writable ATA devices.

## Block cache and drive ownership

`Kernel/BlkDev/DskCache.cpp` restores the 560-byte `CCacheBlk` record,
LRU queue, compact hash sentinels, initialization, insertion and
invalidation, and `RCache`. The cache key is the drive pointer and
absolute 512-byte block number. Cache hits do not promote entries;
`RCache` consumes cached prefixes and suffixes. DVD startup selects
the pinned distribution's `Scale2Mem(2048,0x40000)` configuration.
This is not yet a general implementation of the kernel configuration
language.

`DskDrv.cpp` and `DskBlkDev.cpp` implement task ownership, same-task
reentry, contention, reset and release. The native filesystem scopes
retain interrupt exclusion after acquiring ownership, without yielding
while unwinding a C++ scope. Task exit and Break release owned drives
and devices. Controller-sharing IDE devices forward to the common
device lock. Native release clears the drive before the device and
does not reproduce every intermediate reference yield. SMP and
contention across multiple partitions sharing a controller remain
untested.

`DskBlk.cpp` routes filesystem reads of published drives through the
cache, including partial-sector reads; mount-time reads bypass it.
Raw writes update overlapping cached bytes for the same device or
invalidate affected blocks after a failed ATA write. RedSea mutations
use bounded device I/O and update or invalidate cached blocks.
Device reset and RAM replacement invalidate cached state.
Reinitialization covers RAM, ATA, ATAPI and supported file-backed devices; the
reference's removable-media sequence and full
exception paths remain incomplete.

## Per-drive FAT and bitmap cache

`DskDrv.cpp` also maintains the Adam-owned 512-byte `cur_fat_blk`
buffer for mounted FAT32 and RedSea drives. `DrvFATBlkAlloc`,
`DrvFATBlkClean` and `DrvFATBlkSet` use the reference sector indices
and selector defaults. A same-sector set does not reload the other
FAT copy; for two copies, selector 0 writes the first and leaves the
dirty bit set, selector 1 writes the second and clears it, and
selector 3 writes both. Single-copy flushes clear the dirty bit.

Native allocation and free operations retain immediate persistence,
independent FAT mirror updates and best-effort rollback. Normal FAT
link reads and RedSea bitmap reads consume the persistent buffer.
Raw writes update its clean, selected sector; conflicting overlapping
writes are rejected while a guest's dirty sector is pending. Native
allocation and mutation likewise reject a pending dirty buffer rather
than overwriting it. A private copy selector preserves the public
`CDrv` layout. Native reads of another sector bypass pending guest
data, and clean native primary reads may reload FAT1. Explicit cache
allocation reuses the existing buffer if possible; failed flushes
retain pending data. These are deliberate differences from the
reference's less guarded behavior.

Mounting publishes a stable buffer only after successful geometry and
initial sector reads. Temporary drives are not retained in the cache.
Normal replacement refuses dirty state; explicit reset and format
discard it. Direct writes through an arbitrary guest RAM pointer
cannot be intercepted. Persistent FSInfo ownership and full
media-change/error paths remain outstanding.

`FileSysRedSea.cpp` implements `RedSeaFreeFreeLst`, `RedSeaFreeLstBuild`,
`RedSeaAllocClus`, and `RedSeaFreeClus` with the circular `CFreeLst` records
anchored in `CDrv`. Allocation selects the smallest sufficient extent;
freeing merges the first adjacent extent in queue order. Successful native
bitmap writes maintain the queue and sector cache; external bitmap writes
and device resets invalidate the queue so the next allocation rebuilds it.
Mount-time allocation for an unpublished drive still scans the bitmap.
`RedSeaValidate` and `RedSeaFmt` expose validation and formatting through the
HolyC ABI. The native formatter rejects pending dirty allocation metadata.

## Drive and device lifecycle

`DrvEnable` changes bit 15 of the filesystem type and returns the
previous enabled state. Disabled drives retain their geometry and raw
block access. The enabled state survives device reinitialization.
`DrvDel`, `DrvBlkDevDel` and `BlkDevDel` invalidate cached blocks and
resident files, release drive metadata, and clear the reference-sized
records. Drive aliases are restored to their canonical slots. RAM
replacement uses the same teardown path and tracks the actual owned
allocation separately from caller-provided memory.

Deletion refuses locked or dirty drives and devices with forwarding
or backing-file dependents. Native-owned RAM is freed; caller-owned
RAM is retained. Disabling a drive with pending allocation-sector
writes is also rejected. These guards deliberately differ from the
reference's less restricted deletion and enable operations. Ordinary
open `CFile` handles and task current-directory references are not
globally tracked, so callers must close files and change drives before
teardown. A live file-backed device prevents deletion of its backing device.

`DskCacheInvalidate` and `DskChg` ensure initialization, clear applicable
cached state, refresh removable ATAPI metadata, and reset the current
directory to the root on disk change. The read-only override is respected.
Complete removable-media failure/retry behavior remains unverified.

## File-backed RedSea devices

`Adam/ABlkDev/Mount.cpp` translates `MountFile` and `Unmount`, including
the default `.ISO.C` extension. `Kernel/BlkDev/DskAddDev.cpp` initializes
read-only image devices on M–P and writable image devices on Q–S. Both
use the 88-sector RedSea offset and a fixed contiguous extent. The
writable path creates the backing file, formats its RedSea volume and
writes the ISO descriptor and El Torito prefix before publishing the device.
The backing stream, name and cluster buffer belong to Adam; device locking
forwards to the backing device. `DskCFile.cpp`, `DskBlk.cpp` and
`FileSysRedSea.cpp` share bounded device I/O for RAM and writable images.

The implementation accepts contiguous RedSea and FAT32 backing files on
RAM, ATA, and nested image devices. FAT32 requires the contiguous `.C`
attribute. Read-only optical backing uses ATAPI medium-removal prevention
until the last direct image dependency is detached. A bounded dependency
walk checks every ancestor's extent, access mode and forwarded lock before
performing I/O. Nested writes pass authorization through the immediate
child at each hop; unrelated backing mutations and teardown are rejected.
Multiple read-only mounts may coexist, while a writable child requires
exclusive access to its immediate backing device. Closing its owned stream
directly raises `File`. These restrictions are broader than the reference's
lifetime rules. Invalid mount requests return zero and invalid raw I/O
returns false where the reference may throw. Occupied-slot replacement
remains restricted.

Writable mounting validates geometry before allocating and saves an existing
backing file before replacement. A failed staging operation attempts to
remove its new extent or restore the saved file; restoration requires enough
memory and working storage. Persistent hardware I/O errors have no guaranteed
transactional rollback.

`Adam/Opt/Boot/DskISORedSea.cpp` now follows the mounted writable-image
workflow: calculate the tree's extent, mount the image, call `CopyTree`,
refresh its descriptors, patch stage-one boot data in place, and detach it.
The native `RedSeaISO9660` ABI also updates descriptors on supported
writable media. The builder retains padding for the shifted stage-two
read. Root relocation outside the builder and explicit formatting does not
yet update the ISO descriptor. General nonlocal exception recovery during
copying, direct RAM mutation, arbitrary open streams, SMP, and failure to
release or externally forced replacement of optical media remain limitations.

## Initial optimizer passes

`Compiler/OptPass012.cpp` implements the constant simplification and
immediate-operation parts of `OptPass012.HC`. The parser already constructs
typed, indexed expression trees, so the pass operates on their links instead
of reconstructing the reference's postfix queue. `BackC.cpp::COCCompile`
runs the optimizer before sizing and emitting either JIT or AOT code.
The emitter handles `IC_ADD_CONST`, `IC_SUB_CONST`, `IC_SHL_CONST`, and
`IC_SHR_CONST` using the reference opcode identifiers.

Integer operations preserve the existing 64-bit intermediate semantics;
constant division faults remain executable. Immediate multiplication and
unsigned division by powers of two use shifts. Constant branches retain
evaluation order and comparison-chain links. Binary64 folding is limited to
supported comparisons, bitwise operations, conversions, and arithmetic that
does not depend on rounding precision; other operations still use the existing
x87 emitter. Dynamic signed division strength reduction still needs
reference validation.

## Frame operand allocation

`PrsVar.cpp` and `PrsStmt.cpp` recognize `reg`, `noreg`, and named
register hints on local and parameter declarations. The parser retains the
hint in `CMemberLst.reg`. Static variables remain in memory.

`OptPass3.cpp` scores direct accesses to eight-byte integer and pointer
frame members. It gives explicit R12 through R15 requests priority, then
allocates hinted and frequently used members. R12 and R13 are preferred
for nonpointer members. The selected registers appear in the member
records and `CHashFun.used_reg_mask`; the dereference nodes carry the
actual register operands for the emitter.

`BackC.cpp` keeps existing frame offsets and saves the assigned registers
in the prologue. It emits direct register loads, assignments, compound
assignments and prefix/postfix updates. Calls synchronize the frame
slots after argument evaluation and reload them after returning. The
epilogue restores the saved registers in reverse order; the call
alignment calculation includes the extra stack slots.

The allocation is conservative. Narrow values, floating-point values,
aggregates, arrays, static storage, `noreg` members and unsupported named
registers stay in memory. A frame address that escapes a direct typed
member access disables allocation for the entire function, since HolyC
pointer arithmetic can reach neighboring frame slots. Functions with
inline assembly, try/catch, known context transfers, subroutine control
flow, label addresses, explicit flag reads or variadic arguments use
the existing memory lowering. The pass does not
yet reproduce the reference's full register pool, stack layout,
operand-location tracking or clobber metadata. Arbitrary frame aliases
fabricated through externally supplied addresses remain outside the
verified behavior.

## Address and right-operand lowering

`OptPass4.cpp` annotates scalar dereferences with an x86-64 base, optional
index, scale and signed displacement. It folds the indexed expression
tree without removing or duplicating evaluation of its dynamic operands.
`BackLib.cpp` selects ModR/M, SIB, REX and displacement fields, including
the special RBP, R13, RSP and R12 cases. Typed loads sign- or zero-extend
narrow integers, and stores write only the destination width. Floating
loads and stores retain the raw binary64 representation.

`BackC.cpp` consumes these annotations for direct frame loads, stores,
compound assignments and increments. For a dynamic destination it captures
the address before evaluating the right operand, then reads the current
destination value for a compound update. Ordinary pointer increments use
the full pointee size rather than truncating the immediate to eight bits.
For a class pointer directly updated inside an arrow expression,
`PrsExp.cpp` preserves the reference's one-byte stride: the reference's
`PrsUnaryModifier` changes the class on the update before its emission.
The prefix and postfix forms of both increment and decrement were checked
for packed three-byte and 136-byte classes. Standalone updates and direct
scalar and pointer-pointer dereferences retain their full stride.

`OptPass5.cpp` selects sign-extended immediate, promoted register and simple
64-bit frame-memory operands for integer addition, subtraction,
multiplication, bitwise operations and comparisons. Frame-memory operands
are read after the left subtree. Comparison chains and flag-sensitive
functions retain the existing lowering. These are selected portions of
the reference `OptPass4.HC`, `OptPass5.HC` and `BackLib.HC`, adapted to
the indexed IR. Full operand allocation, variable shifts and division
operand selection, complex memory operands, instruction scheduling and
reference machine-code parity remain incomplete.

## Stack temporaries and branch layout

`OptPass6.cpp` implements the single R9 temporary from the reference
`OptPass6Lag` for selected integer binary expressions. It checks the
evaluated right subtree, including folded address operands. The emitter
tracks whether R9 is live: nested expressions use the stack while the
slot is occupied. Before evaluating an ordinary call's arguments or
indirect target, it spills a live R9 and includes the spill in stack
alignment; it restores the slot after call cleanup. Inline assembly,
variadic functions, flag observation and nonlocal control flow disable
selection. Function metadata records the additional used and clobbered
register bit without clearing other bits. The full register-lifetime
model and selection of more reference operand forms remain incomplete.

`OptPass789A.cpp` selects rel8 branches from recorded branch ends and
label offsets. The backend first measures all branches in their near
forms, then repeats layout for up to 32 rounds, monotonically selecting
short jumps and conditions. Forward targets move with the shortened
branch; backward candidates account for the branch's own size reduction.
The final emitter checks signed displacement ranges and total code size
before publication. Inline assembly and opaque code addresses retain the
near layout; subroutine calls and switch-table branches are not relaxed.
The reference's other late optimization and instruction selection work
remains incomplete.

## Backend instruction selection and templates

`CompilerB.hpp` contains the shared code emitter. `BackA.cpp` and
`BackB.cpp` implement integer binary operations, compound assignments,
unary instructions and min/max selection; `BackFA.cpp` and `BackFB.cpp`
implement floating binary operations and template selection. `BackC.cpp`
coordinates expression and statement emission. The module copies bounded,
position-independent instruction sequences from `Templates.asm`, derived
from Terry A. Davis's public-domain `Compiler/Templates.HC`. The base
template table includes floating increment, decrement, remainder,
comparisons, string length, TSC, sign and unary math; not every entry or
reference template variant is selected by the current backend.

Fourteen additional typed internal functions cover sign, absolute value,
unsigned square and min/max, bit scans, Boolean conversion, uppercase,
TSC, floating square/absolute value, tangent and arctangent. The parser
resolves their signatures and inserts needed numeric conversions.
Exact constant folding leaves rounding-sensitive and exceptional floating
cases for execution. `CExts.cpp` resolves register-ABI exports, including
`SYS_POW`, whose RDX/RAX calling convention is implemented in
`Kernel/KMathA.asm`. AOT uses a named import rather than embedding a host
address; the compiler no longer imports `__unholyos_sysv.F64PowerBits`.

The bit-operation, swap, unsigned remainder and port-I/O families add
eighteen typed intrinsic signatures from `Kernel/KernelB.HH`.
`BackB.cpp` emits memory bit instructions with the reference's immediate
operand widths and lock prefix, width-specific swaps, `DIV` with quotient
writeback, and immediate or DX-based port accesses. `BackC.cpp` preserves
right-to-left operand evaluation and stack alignment while lowering these
operations. `PrsStmt.cpp` evaluates `_intern` opcode expressions and creates
typed function records for implemented intrinsic opcodes. These records
hold an opcode rather than a machine-code address, so `AOTDel` does not
free it. Unsupported opcodes, wrong arities and variadic or array arguments
are rejected. Full reference intrinsic declarations and operand-selection
parity remain incomplete.

## Original Adam math

`Adam/AMath.HC` is an unchanged reference source. `Kernel/KMathA.asm`
provides its missing exponential and logarithmic operations and related
rounding, sign, power and x87 control entries using the HolyC stack-call
ABI. `Kernel/KLoad.cpp` exports the assembly labels and public aliases;
`Compiler/AsmInit.cpp` registers their typed headers and the two-member
`Complex` class. The class occupies 16 bytes with binary64 members at
offsets 0 and 8. The native compiler processes the original math file in
the JIT and AOT test media and in the ordinary Adam startup namespace;
the full original startup sequence remains pending. The tested scope
and reference comparison are in [Boot.md](Boot.md).

## Original Adam task reporting

`Adam/TaskRep.HC` uses the unchanged recursive report from the pinned
reference. `Kernel/MultiProc.cpp` initializes the CPU-record pointer and
active CPU count for core zero. `Kernel/KLoad.cpp` exports both public
names and their reference system-symbol names. The compiler resolves
their typed global aliases through the kernel export table. Typed
`Raw` and `IsRaw` declarations and `DOCF_NO_CURSOR` allow a guest to
capture the original report with its DolDoc formatting intact.
The public `Kill` API executes the command generated by the original
report, preserves protected tasks, and uses the existing scheduler's
task-deletion path. Additional processor initialization, activation of
graphical Kill links and the complete original Adam startup remain pending.

## Adam source namespace

`Adam/MakeAdam.cpp` compiles the three unchanged original source files
before graphics contexts and user tasks are created. It owns their JIT
functions and typed symbols in Adam's heaps. The shared compiler class
table is chained above the kernel export table, preserving the raw
exports needed by module relocations. The idle task and new child tasks
inherit the published namespace. If an include is absent or compilation
fails, the private tables and generated code are released and the
startup gate prevents publication of the desktop. This is a limited
native bridge, not execution of the original startup project.

`Compiler/CMisc.cpp` implements `Option` and `GetOption` on the active
compiler context. `OPTf_NO_REG_VAR` disables automatic local-register
and expression-stack register selection in the current backend. Other
option bits are stored, but not all their reference effects are implemented.
`Kernel/KHashB.cpp` implements the same-table purge of superseded raw
exports and invalid entries without modifying inherited tables.

## Document trees

`Adam/DolDoc/DocTree.cpp` follows the reference tree paths and relative
indent records. It finds existing branches, creates nested missing branches,
rewrites and appends formatted text, and reads and writes the DolDoc files
through the ordinary filesystem. `DocClipBoard.cpp` copies inclusive
entry ranges with owned payloads and cuts ranges without deleting the
document sentinel. The native `ExeDoc` attaches a root document to a
compiler context, executes it through the normal statement compiler and
restores the context and document lock on failure. Branch execution copies
the selected range before compiling it and releases the original document
lock before executing the copy.

## Registry

`Adam/ARegistry.cpp` follows `Adam/ARegistry.HC` for the cached
`~/Registry.HC.Z` document, branch defaults, execution, counts, writes, and
appends. The document belongs to Adam's heap. `Kernel/KMain.cpp` initializes
it after DolDoc is available and before spawning the user terminals, at
the registry stage of the reference's `StartOS.HC`. The compiler exposes
the registry document pointer, message flags array, and version fields with
their HolyC types. The guest stores the two F64 version values as their
64-bit representations on the C++ side.

`RegDft` releases the document lock before dispatching an Adam entry and
reacquires it to save. A caller waiting for Adam then allows Adam to enter
the registry. `Kernel/Job.cpp` uses its existing execute job and
`JobResGet` to wait and reap the result. `RegWrite` and `RegAppend` share
the DolDoc tree writers.

## Once startup

`Adam/ARegistry.cpp` follows the `AOnce`, `Once`, drive variants, flush
functions, and `OnceExe` in `Adam/ARegistry.HC`. The drive variants hold
the document lock through the temporary drive change and restore the
original drive after writing. `OnceExe` runs the Adam branch through
Adam's job queue before running the user branch in the caller. When a
user entry throws, it clears both branches, sets both run-level bits,
and propagates the same exception to the caller.

`HomeSys.cpp` queues `Cd;#include "Once";` after tiling the user
terminals when a Once script exists. As in `HomeSys.HC`, this goes
through `XTalk` in `Kernel/Job.cpp`. `Kernel/KeyDev.cpp` forwards the
input filter's output as key messages to the terminal, where HolyC
compiles and executes the command. Adam posts the input without waiting
and can service the resulting job as soon as it enters server mode.
The startup lookup uses the first terminal's directory and the same
file search as the compiler, preserving Adam's working directory.
The native stock startup test DVD includes Terry A. Davis's original
`Once.HC` at the root and all thirteen `Misc/Tour/*.IN` files. The live
DVD prompt and the tour entry menu run through the original scripts.
The first chapter runs through the help pane and modifier-key holds.
The key-description pane, installer wizard, and remaining chapters
still need work.

`Compiler/CMain.cpp` runs the compiled file before forwarding the caller's
arguments to its last declared function. `Compiler/CMisc.asm` bridges
the reference `_LAST_FUN` stack call to the current compiler module's
native code. It copies the argument slots in order and restores the
native frame after the HolyC function returns. The `2` entry points
propagate compiler and runtime exceptions.

`Kernel/Job.cpp` follows `InStr`, `InFile`, `In`, and the `XTalk` variants
in `Kernel/Job.HC`. `InFile` carries the caller's current drive and
directory into the input filter before including the script. A call
from inside the filter executes there, so nested scripts and input text
reach the same terminal.

`Adam/InFile.cpp` follows the key, character, print, view, and mouse
procedures in `Adam/InFile.HC`. It sends accepted keys back through
`Msg` and uses the existing DolDoc menu for the wrong-key prompt.
`InGetChar` sizes its prompt for the caller's argument count so a long
list fits. It receives the HolyC inline `argv` without a fixed
argument, as does `InUntilChar`. `InSetMs` uses the graphics line
walker and the mouse state from `Kernel/SerialDev/Mouse.HC`.
`Kernel/SerialDev/Keyboard.cpp` shares `Char2KeyName` between
the character prompt and `ScanCode2KeyName`. `Kernel/KUtils.asm`
exports TempleOS's locked bit assignment as `_LBEQU` and returns
the previous bit.

`Kernel/KLoad.cpp` exports `SYS_BOOT_SRC` for the typed `U32 sys_boot_src`
alias used by `Once.HC`. `Adam/Training.cpp` follows `TipOfDay` in
`Adam/Training.HC`. It uses the same random draw and selects the same
star-delimited DolDoc section. Counting the sections first avoids the
reference's unbounded traversal for a file with no tips. The copy skips
error entries and preserves the remaining DolDoc entries and links.

## Keyboard output hooks and key maps

`Kernel/KeyDev.cpp` follows `Kernel/KeyDev.HC` for the priority-ordered
output chain and the `KeyDescSet` and `CtrlAltCBSet` tables. A hook can
replace itself at the same priority. The output path looks up the next
priority again after a callback so a removed entry is never used after
the callback returns. The input-filter and raw hooks are registered at
kernel startup. DolDoc joins the chain from `Adam/DolDoc/MakeDoc.cpp`.

`Adam/Training.cpp` builds `KeyMap` by querying the live hooks with
`SCF_KEY_DESC`, sorting the results, and linking to guest callback
source when it is available. `DocGet` temporarily enables description
messages while the editor waits for ordinary keys. `DocPutKey`
describes the editor actions currently implemented and leaves the
document untouched during those requests.

`Kernel/SerialDev/Keyboard.cpp` dispatches registered Ctrl-Alt handlers
from the staged PS/2 decoder. Native interrupt-time dispatch remains
pending. The current built-ins register Break, the auxiliary font, and
both zoom directions. The rest of the reference default registrations,
source descriptions for additional editor commands, and the editor's
key-map popup macro still need work.

`Adam/AutoComplete/ACTask.cpp` follows `ACTask`, `ACTaskCtrl`, and
`ACTaskAlt` in `Adam/AutoComplete/ACTask.HC`. The task waits for the
reference keyboard and mouse idle interval, follows input filters to the
underlying task, and queries the focused task's descriptions through
`KeyMapFamily`. It renders registered Ctrl-Alt actions through
`KeyMapCtrlAltFamily`. The regular pane displays Terry A. Davis's
unchanged `Doc/StandBy.DD`. A task-end callback clears the shared task
pointer so `AutoComplete(ON)` can start another pane after shutdown.

`ACInit.cpp` follows `ACMainFileLstTraverse`, `ACSingleFileAdd`, and
`ACHashAdd` in `ACInit.HC`. It walks the requested text-file mask in
cluster order, indexes case-sensitive words from expanded file contents,
and counts repeated words in the 32-byte `CHashAC` records.
`HomeSys.cpp` runs the reference startup mask after queuing `Once` on
media that include `Doc/StandBy.DD.Z`. The regular boot image lets the
user start the pane and choose a file mask with `ACInit(mask)`.

`ACPutChoices` scans the active DolDoc line, finds the prefix before
the cursor, and ranks up to ten matching words by hit count.
`ACFill.cpp` follows `ACFillIn` and `ACMan` in `ACFill.HC`.
`DocPutKey` uses Ctrl-F1 through Ctrl-F10 to insert the selected
suffix through the task input queue. Ctrl-Shift with those keys
looks up the selected symbol's source link.

The regular and stock DVDs carry the pinned `ACWords.DATA` and
`ACDefs.DATA` byte for byte before packaging. Terry A. Davis credits
Project Gutenberg for the Webster dictionary in [Credits](../Doc/Credits.DD).
`ACDWordsLoad` expands the word list and installs its case-folded hash
entries. The loader accepts entries without a marker, as the original
does. `ACDWordPtAt` searches the original word bytes and distinguishes the
word marker from the same byte in a block index. Definition lookup reads
up to two 16 KiB blocks from the uncompressed definition file and clips
the final read to the file extent.

The source checkout's word list has 1,238,691 bytes. The pinned DVD
expands its word list to 1,253,930 bytes. Both load 95,293 words.
The UnholyOS DVD packages definitions uncompressed so they are available
in a live session. The reference test expands the original DVD archive
to a scratch RAM drive and sets that drive as the popup tasks' home.

`ACFill.cpp` provides the HolyC definition and popup calls. A typed
`acd` global exposes the original packed 114-byte record. Ctrl-0 through Ctrl-9
insert the dictionary suffixes and Ctrl-Shift with those keys opens
definitions. `EdLinkCvt` and `Ed` handle `DN:` links with an optional
definition number. Writable-media preparation moves to the uncompressed
name as in TempleOS. `ACProgressTask` shows `Uncompressing Dictionary`
on writable drives other than RAM. It measures the change in
`blkdev.write_cnt` against the archive's expanded block count.
`ACInit` reads the header on the caller's drive before starting the task
and joins the task after `Move` so the bar clears if the write ends early.

`ACDictGen.cpp` follows the standalone `Adam/AutoComplete/ACDictGen.HC`
program. The HolyC command `ACDictGen` runs in the current directory so it
can use `DICTIONARY.DD` on a writable drive. The original script changes to
its own source directory first. Both stages retain the original tag lists,
case-insensitive duplicate handling, definition markers and little-endian
16 KiB block indexes. The port bounds each parsed entry before writing it.
It reports a failed output write before printing that file's size.
The compiler exposes `CDoc.head`, `CDocEntryBase` queue fields, `LstMatch`
with its default flags, and `QSortI64` with its HolyC callback signature.
The unchanged script also runs in UnholyOS. See the paired small-fixture
and full-corpus results in [Boot.md](Boot.md#generating-the-webster-dictionary).

`ACTask.cpp` follows the link activation in `Adam/AutoComplete/ACTask.HC`.
After the 333 ms sleep and the Escape check, the task sends a space to
the selected link and resets the document cursor to the entry-list head.
`Adam/WinMgr.cpp` retries cursor placement for a press held inside the
document while preserving the button transitions used by window controls.
`Adam/DolDoc/DocRun.cpp` dispatches the left and right macros through
`PopUp`, `InStr`, or `In`, following `Adam/DolDoc/DocRun.HC`.

The pane clears its document with `DocClear` and fits the content when shown.
TempleOS calls `DocRst` and handles minimum-window sizing during
every document recalculation.

## Popup tasks and document menus

`Adam/DolDoc/DocPopUp.cpp` follows `PopUp1`, `PopUp2`, `PopUpOk`,
and `PopUpCancelOk` from `Adam/DolDoc/DocPopUp.HC`. The buttons and
optional header and footer are placed in a document passed to `PopUpMenu`.

`Adam/DolDoc/DocPutKey.cpp` dispatches F1 and Shift-F1 to `Ed` using
the same Help Index and About TempleOS paths as `DocPutKey.HC`.
`Ed` and `PopUpEd` live in `DocEd.cpp`. `DocLink.cpp` resolves
file, plain-text, in-memory, and symbol-source links and restores the
caller's working directory when the editor returns. `DocFind.cpp`
supplies the one-based line, tag search, and named-anchor operations.
`DocTerm.cpp` opens file links through the reference left-click
callback. Raw-mode editing, the right-click terminal file menu,
undo, generated help indexes, address, and Bible links
remain pending.

`Kernel/Job.cpp` follows `Kernel/Job.HC` for `PopUp` and `PopUpPrint`.
The servant enters `SrvCmdLine` and runs the standard execute job.
A modal call records its child in `popup_task` and suspends the caller
until the servant returns the job result. The asynchronous form exits
the servant when the job completes. `HomeSys.cpp` establishes the
servant's viewport before the window manager draws it.

`Adam/DolDoc/DocForm.cpp` runs the document editor, returns the
selected entry's action result, and restarts the menu when an accepted
entry has no action. It follows `DocMenu`, `PopUpMenu` and `PopUpForm`
in `DocForm.HC`. The popup restores the caller's document window
association after the servant is reaped. `DocEd.cpp` uses a checked
HolyC call around `View` so it can restore the editor's state when a
task-end callback unwinds the input loop.

`DocPlain.cpp` keeps literal numeric fields on its direct parser path
and uses the HolyC compiler for integer expressions in other fields.
This follows the expression evaluation of `PrsDollarCmd` and lets
button actions reference the calling task's defines.
`Adam/ARegistry.cpp` uses the original checkbox and OKAY button text.
`RegOneTimePopUp` saves the system message flags when the user elects
to suppress the message.

## Install registration

`Adam/InsReg.cpp` follows `Adam/InsReg.HC`. The three registration calls
use the original form, drive override, and `Adam/InsReg` registry branch.
`Kernel/KernelA.hpp` preserves the packed `CInsReg` layout from
`Kernel/KernelA.HH`. The compiler's native class registration supplies
the same four DolDoc format strings. `Kernel/KHashB.cpp` marks the
generated defines public and updates their help metadata when the
registry code invokes `HashPublic`.

`Kernel/KMain.cpp` loads the two choice lists after `MakeAdam` and writes
the unregistered defaults only if the registry replay leaves the record
unregistered. These calls stand in for the `MakeAdam.HC` include and the
`InsUnreg` call in `StartOS.HC` during the current native boot sequence.

## Original Adam memory inspection

`Adam/AMem.HC` is preserved from the pinned reference. `CmpHeapData`
registers the non-debug `CMemUnused`, `CMemUsed`, `CMemBlk`, `CBlkPool`
and complete `CHeapCtrl` layouts, including the zero-width aliases and
typed free-list arrays. The inspection functions operate on the native
allocator's actual queues and counters. Their original bare assembly
and locked bit operations preserve interrupt state and caller-held
locks. `test-adam-mem` runs JIT and AOT probes at 512 and 3072 MiB.
The debug heap layout and concurrent SMP behavior remain unimplemented.

## Adam CPU report and server loop

`Adam/CPURep.cpp` follows `CCPURep`, `MPCPURep` and `CPURep` in
Terry A. Davis's `Adam/CPURep.HC`. The short report reads the calibrated
TSC frequency. The detailed report starts a worker for each initialized
CPU, samples yields and TSC cycles while the caller sleeps for two
seconds, and calculates rates and averages from those samples.
`Kernel/KUtils.asm` adapts the public Boolean argument and the worker
entry to the HolyC stack ABI.

The report uses the reference's packed completion record. It explicitly
parents the workers to the reporting task so task teardown stops the
workers before their caller's stack and heap are reclaimed. The reference
uses the scheduler's default parent. The completion word holds 64 worker
bits. UnholyOS initializes only CPU 0, and multicore reporting remains
unverified.

`Kernel/KMain.cpp` enters `SrvTaskContCpp` after starting the terminals
and sets `RLF_ADAM_SERVER` at the point corresponding to `KMain` in
`Kernel/KMain.HC`. Adam can now accept execution jobs and lets
`Kernel/Sched.asm` decide when no runnable task needs the CPU. Earlier
boot exits still use the fallback halt loop.
The compiler publishes the typed `adam_task` pointer as declared in
`Kernel/KernelC.HH`, using the existing kernel address.

## Adam device reports

`Adam/DevInfo.cpp` follows the active `PCILookUpSingle`,
`PCILookUpDevs`, `PCIRep`, `MemBIOSRep` and `MemPageRep` functions
in Terry A. Davis's `Adam/DevInfo.HC`. The database conversion
utility is commented out in the reference.

PCI enumeration uses the BIOS bus count and the byte and word calls
from `Kernel/PCIBIOS.HC`. The report reads the pinned
`Misc/PCIDevices.DD` as plain DolDoc, with tab entries preserved.
The reference identifies pcidatabase.com as the database source.
The reference inserts records as it finds them. UnholyOS allocates the
records and names in Adam's data heap and publishes the queue after the
scan succeeds. The host ISO tool compresses the unchanged database to
the reference disc path `::/Misc/PCIDevices.DD.Z`.

`MemBIOSRep` reads the 32-bit device range queue and the BIOS E801 and
E820 handoff records. `MemPageRep` reads the mapping size, page counts,
table bases and heap bounds recorded by the kernel. NASM keeps the
current 32-bit page-table base fields used by the bootstrap, and the
report widens them for display. The boot path uses 2 MiB pages. The
three public report names use the HolyC calling convention through
`Kernel/KUtils.asm`.

## Remaining structural work

`Kernel/KMain.cpp` contains `SysGlblsInit`, `TimersInit`, and `KMainCpp`, in the relative definition order of their reference counterparts. `KMainCpp` calls `BootProbe` after the window manager starts and before user startup. The standard and startup-test ISOs use the same kernel, so both retain that verification call. Earlier reports now cover the kernel state before compiler loading and the loaded compiler image; the opcode-resource probe runs before the module entry. These reports remain in `Linux/BootProbe.cpp`. [Source structure verification](SourceStructureVerification.md) records the earlier layout review; the compiler load cutover requires its own runtime evidence in [Boot.md](Boot.md).

Startup remains partial. The current kernel initializes several Adam subsystems directly after the separate compiler's initialization gate. The reference loads the compiler and executes `StartOS.HC`, which includes `Adam/MakeAdam.HC`. Matching that startup path requires further compiler and runtime work. GCC and Clang builds passed the seven focused failure cases and the boot and startup matrices, including guest ISO generation and cold boot. [Boot.md](Boot.md) records the tested scope.

The host-linked image retains Adam and the boot probes in low memory;
the compiler objects now form the separate TOSB module.
`Kernel/Kernel.ld` keeps the kernel below the
reference DVD loader at `0x96600`; the temporary boot stack occupies
`0x96E00` through `0x96FFF`. Separating Adam remains structural work.
The reference initializes the compiler with late DolDoc imports from
`Compiler/CExts.HC` unresolved. Generic native `LoadPass2` now runs entries
with pending imports and on repeated calls; the compiler-specific binder
still requires all its imports before publishing dispatch targets.
Shared source/module probe execution
in `Linux/BootProbe.cpp` avoids duplicating the same compile-and-load
code within this limited boot image.

Separating Adam also requires changing the compiler's eager typed
metadata registration. Seven current data imports belong to Adam in
both trees: `gr`, `gr_palette_std`, `gr_palette_gray`, `gr_rainbow_10`,
`music`, `winmgr`, and `sys_cur_submenu_entry`. `CmpFillHash` copies their
current addresses into global aliases, while `CmpRuntimeHeaders` skips
unavailable public function exports. `CmpAsmHashLoad` then treats the
presence of `I64i` as completed initialization. Later module relocations
do not update those copied aliases or add the skipped headers. Metadata
for these providers must support later registration while retaining the
globals' reference ownership. The kernel's direct DolDoc input calls
also need the reference `fp_getstr2` callback before Adam is separated.

`Linux/MakeBIN.cpp` supplies a C++20 host producer for the native TOSB
module format. It converts explicitly bound ELF64 objects and retains
the patch semantics from `Compiler/CMain.HC` and `Kernel/KLoad.HC`.
The compiler build now uses this producer and its explicit binding
manifest. Its freestanding C++/NASM fixtures provide earlier runtime
evidence for relocation and ABI behavior. The separate compiler's
startup results are recorded independently in [Boot.md](Boot.md).
A guest C++ compiler and the complete native build workflow remain
pending. [Host modules](HostModules.md)
records the ABI choices, supported relocations, rejected inputs,
and reproduced native-import coverage.

The [development milestones](Development.md) define completion in the required order: full rewrite, full compatibility, bootstrap, then self-hosting. Host GCC or Clang and NASM support the current development builds.

The guest's `RedSeaISO` follows the sizing and directory-copy steps from
`Adam/Opt/Boot/DskISORedSea.HC` and `Adam/ABlkDev/ADskA.HC`, using
`BDT_ISO_FILE_WRITE` and the general `CopyTree` path. UnholyOS
reserves a spare DVD sector when a stage-two file is supplied because the
shifted read can extend beyond that file's last RedSea block. The loader
is the NASM `BootDVD.BIN` embedded by `DskISORedSea.asm`.

The native compiler, runtime, desktop, and device subsystems remain incomplete. Source correspondence is a review aid. [Boot.md](Boot.md) records the implemented scope and runtime comparisons; new changes need their own validation before those claims expand.
