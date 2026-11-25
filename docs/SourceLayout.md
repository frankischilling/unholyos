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
| `Kernel/BlkDev/*.HC` | Corresponding `.cpp` files under `Kernel/BlkDev/` | Block devices, paths, files, and filesystems as implemented |
| `Compiler/*.HC` | Corresponding `.cpp` files under `Compiler/` | HolyC scanning, parsing, code generation, and execution as implemented |
| `Adam/DolDoc/*.HC` | Corresponding `.cpp` files under `Adam/DolDoc/` | Native document records and operations as implemented |
| `Adam/Opt/Boot/BootDVD.HC` | `Adam/Opt/Boot/BootDVD.asm` | DVD boot loader |
| `Adam/Opt/Boot/DskISORedSea.HC` | `Adam/Opt/Boot/DskISORedSea.cpp`, `DskISORedSea.asm` | RedSea ISO construction and the guest's stage-one image |

Use `Kernel`, including its capital K, in the filesystem, include paths, and build definitions. An existing Windows checkout can retain an older lowercase directory name even when Git tracks the correct case. Check actual directory names when preparing a Linux build.

`Linux/` holds C++ host support and verification programs. `Linux/Fixtures/` holds HolyC source and binary inputs. `Kernel/SerialDev/Serial.cpp` and `SerialBoot.asm` provide serial diagnostics without a matching reference file. `Adam/DolDoc/DolDoc.hpp` and `Kernel/Compress.hpp` are additional C++ declarations. These additions support the translation and its tests; they do not represent completed TempleOS subsystems.

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

## HolyC assembler

`Compiler/Asm.cpp` follows `PrsAsmBlk` and `PrsAsmInst` in
`Compiler/Asm.HC`. The current implementation also resolves assembly
references there. The pinned compiler splits that work into
`AOTLocalsResolve` and `AOTGlblsResolve` in `Compiler/AsmResolve.HC`.
JIT assembly publishes its labels after resolving the block.
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

## Remaining structural work

`Kernel/KMain.cpp` contains guest verification routines for boot records, file access, compiler behavior, and runtime modules. The reference `Kernel/KMain.HC` concentrates on initialization and startup. Those probes remain a structural difference and should move into a separate verification component as the runtime startup path develops.

The guest's `RedSeaISO` follows the sizing and directory-copy steps from
`Adam/Opt/Boot/DskISORedSea.HC` and `Adam/ABlkDev/ADskA.HC`. The current
`CopyTree2` works on the ISO's private RedSea RAM volume. The reference
uses `BDT_ISO_FILE_WRITE` and the general `CopyTree` path. UnholyOS
reserves a spare DVD sector when a stage-two file is supplied because the
shifted read can extend beyond that file's last RedSea block. The loader
is the NASM `BootDVD.BIN` embedded by `DskISORedSea.asm`.

The native compiler, runtime, desktop, and device subsystems remain incomplete. Source correspondence is a review aid. [Boot.md](Boot.md) records the implemented scope and runtime comparisons; new changes need their own validation before those claims expand.
