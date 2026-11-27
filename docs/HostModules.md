# Building TOSB modules from host objects

`Linux/MakeBIN.cpp` converts a combined x86-64 ELF relocatable object into
the TOSB format used by TempleOS. It provides a host build path for C++20
and NASM components without changing the guest loader's records. The
Makefile now builds the compiler as `build/Compiler/Compiler.BIN` and
packages it at `/Compiler/Compiler.BIN`. Adam and the boot probes remain
in the host-linked kernel image. Both GCC and Clang builds passed the
boot and startup matrices, including execution through the separate
compiler and a cold boot of the ISO built inside the guest.

The format follows Terry A. Davis's pinned
[`Compiler/CMain.HC`](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Compiler/CMain.HC)
and [`Kernel/KLoad.HC`](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Kernel/KLoad.HC).
The output has the native 32-byte `CBinFile` header, `INVALID_PTR` origin,
module alignment, export/import records, absolute-address table, entry
record, and zero padding to a 16-byte file boundary.

## Build inputs

The focused Make target builds the tools, C++/NASM fixture, and dedicated
ISO, then runs the conversion checks and native guest probe:

```sh
make -j4 test-host-modules
make -j4 BUILD=build/clang CXX=clang++ HOSTCXX=clang++ test-host-modules
```

`make test` includes this target. The fixture binary is packaged only in
`build/host-modules-test.iso`, leaving the ordinary boot tests' allocations
unchanged.

Compile module C++ with the existing freestanding flags followed by
`-fPIC -fcf-protection=none`. The kernel's compilation flags are unchanged.
NASM produces an ELF64 object. Combine the component objects before
conversion:

```sh
ld -m elf_x86_64 -r --no-relax --force-group-allocation \
  -o component.o component.cpp.o component.asm.o
build/Linux/MakeBIN component.o component.bindings component.BIN
```

The input must be a little-endian ELF64 `ET_REL` object for x86-64, with
one static symbol table and RELA relocations. The partial link resolves
section groups. Disabling linker relaxation preserves the GOT references
that the producer handles explicitly. Disabling control-flow protection
avoids an allocated GNU property note whose execution requirements are
outside this format.

Allocated code, read-only data, initialized data, and zero-filled storage
retain their section order and alignment in the payload. Each GOT symbol
gets an eight-byte slot. The producer validates the headers, section
extents, symbol names and ranges, bindings, relocation widths, overlapping
patches, and complete output size before writing the destination.

TLS, constructor and destructor arrays, implicit initialization sections,
unwinding data, IFUNC symbols, unsupported allocated section semantics,
and unsupported relocations are rejected. The tool does not silently
drop runtime requirements that TOSB cannot represent.

## Explicit bindings and calling conventions

The manifest has one required entry and explicit exports and imports:

```text
entry HostModuleMain
export HostModuleResult HostModuleResult
export HostModuleAnchor HostModuleAnchor
import holyc _STRCMP _STRCMP
import data char_bmp_alpha char_bmp_alpha
```

The first name after `entry`, `export`, or the import ABI is the exact ELF
symbol. An export or import's final name is the guest hash-table name.
Blank lines and `#` comments are accepted. Missing, duplicate, conflicting,
ambiguous, or unused bindings are errors.

Import ABIs are `native`, `holyc`, and `data`. Native function imports must
use the private `__unholyos_sysv.` guest namespace. A C++ function and a
HolyC stack-call adapter can share a conceptual API while requiring
different entry points; the manifest must make that choice explicit.
The tool does not generate an ABI adapter or infer one from a symbol's
spelling.

`Kernel/KLoad.cpp` publishes 44 native function addresses under
`__unholyos_sysv.` for the current compiler's host-built dependencies.
The suffix names the source entry point: `__unholyos_sysv.HashFind`
refers to the C++ `HashFind`, while `__unholyos_sysv.StrCmpCpp`
retains that entry point's `Cpp` suffix. The ELF import name can still
be mangled. For example, these manifest entries select native addresses
without changing the public HolyC exports:

```text
import native PutS __unholyos_sysv.PutS
import native _Z8HashFindPKcP10CHashTablejm __unholyos_sysv.HashFind
import native StrCmpCpp __unholyos_sysv.StrCmpCpp
```

Public `PutS` resolves to `PutSHC`, which receives HolyC stack
arguments. The private `__unholyos_sysv.PutS` resolves directly to
the native `PutS(const char *)`. Likewise, public `HashFind` uses
`_HASH_FIND`; its private entry uses the C++ signature. A module must
choose the entry that matches its caller, including argument types,
return convention, and stack cleanup.

The loader also exports the existing `winmgr` and
`sys_task_being_scrn_updated` storage. `sys_cur_submenu_entry`
already has a public data export. `Compiler/Compiler.bindings` maps
the two lowercase ELF names to their existing uppercase guest names:

```text
import data sys_var_init_flag SYS_VAR_INIT_FLAG
import data sys_var_init_val SYS_VAR_INIT_VAL
```

`IET_MAIN` uses the HolyC calling convention. The fixture's
`HostModuleMain` assembly entry preserves the HolyC registers and aligns
the stack before calling its System V C++ body. Its two `_STRCMP` adapters
push arguments in HolyC order, let the callee remove them, preserve the
System V callee-saved `RBX`, and clear the direction flag on return.

## Public HolyC loader

HolyC programs can call the declaration from `Kernel/KernelC.HH`:

```c
U8 *Load(U8 *filename,I64 ld_flags=0,CBinFile *bfh_addr=INVALID_PTR);
```

The result points to the 32-byte `CBinFile` header. Its payload starts
32 bytes later. `Compiler/AsmInit.cpp` supplies the packed class members,
signed `I64` header fields, constants, and argument defaults. Compile-time
assertions check every native header offset. `Kernel/KLoad.asm::LoadHC`
accepts the three HolyC stack arguments, calls `LoadCpp`, returns the
header address in `RAX`, and removes 24 argument bytes. The internal C++
loader still returns its own `CLoadedModule` descriptor.

`LoadCpp` and that internal loader share image validation, placement,
patch preparation, symbol registration, and entry execution in
`Kernel/KLoad.cpp`. Public `Load` follows `Kernel/KLoad.HC::Load` through
`ExtDft(filename,"BIN.Z")`, task-relative file lookup, and the existing
compressed-file fallback. An omitted extension prefers `Name.BIN.Z` and
can fall back to `Name.BIN`; an explicit `Name.BIN` selects that file.
The module hash entry has `HTT_MODULE | HTF_PUBLIC` and stores the header
address in `user_data0`. Its name follows `FileExtDot`, including the
reference quirk that a qualifying dot in a directory can truncate the
module name before the final basename. Each load registers its own image.

| Flag | Loader behavior |
| --- | --- |
| `LDF_NO_ABSS` (`1`) | Skip `IET_ABS_ADDR`; other fixups still run |
| `LDF_JUST_LOAD` (`2`) | Perform the first pass without running `IET_MAIN` |
| `LDF_SILENT` (`4`) | Suppress the unresolved-import report |

`INVALID_PTR` is `I64_MAX`. A supplied header address overrides the
file's `org`; otherwise a non-sentinel `org` selects the destination.
These destinations remain caller-owned, retain the file's original
`org`, and are used without additional alignment. Automatic placement
aligns the payload according to `module_align_bits`. If the current code
and data heaps are the same and the requested alignment is at most eight
bytes, the loader retains the file-read buffer. Other automatic loads
allocate from the code heap with the header offset accounted for.

The loader prepares its metadata before replacing caller-owned bytes.
A missing file returns `NULL`; a rejected binary raises `BinModul`.
The native parser retains its bounded record and extent validation, which
rejects malformed inputs beyond the reference's checks. Allocation
failure returns `NULL`. Invalid destination addresses, extreme alignment,
and allocation-failure behavior have not been compared with TempleOS.

`make test-load-api` executes the same `LoadApiProbe.HC` source and NASM
consumer/provider bytes used by the pinned guest. GCC and Clang native
builds match its path selection, header and hash identity, flags, low
32-bit absolute relocation, explicit destinations, tested errors, and
retained earlier images. The small-alignment test checks the resulting
allocation's owner and capacity; retention of the exact temporary input
pointer is established by source review. [Boot.md](Boot.md#public-holyc-load)
records the commands, hashes, full trace, and limits.

## Raw HolyC second pass

`Kernel/KLoad.HC::LoadPass2` is available through a raw export rather than
a declaration in the reference kernel headers. A HolyC program can declare
it explicitly:

```c
extern U0 LoadPass2(U8 *src,U8 *module_base,I64 flags);
```

`src` points directly to the serialized patch table. The call needs no
binary header or native descriptor. It reads a nonzero record type,
zero-extends the following unaligned `U32` value, and skips the
NUL-terminated name. Its remaining work depends on the record:

| Record | Second-pass action |
| --- | --- |
| `IET_END` | Stop immediately, before reading a value or name |
| `IET_MAIN` | Call `module_base+value` if the resulting address is nonzero |
| `IET_ABS_ADDR` | Skip `4*value` bytes |
| `IET_CODE_HEAP`, `IET_ZEROED_CODE_HEAP` | Skip the four-byte size and `4*value` offset bytes |
| `IET_DATA_HEAP`, `IET_ZEROED_DATA_HEAP` | Skip the eight-byte size and `4*value` offset bytes |
| Other nonzero types | Continue after the common value and name |

Every explicit pass runs its entries in table order. The third argument
is ignored, including `LDF_JUST_LOAD`; that flag controls whether `Load`
invokes the second pass. An entry's HolyC exception propagates to the
surrounding handler and prevents later entries from running. The null
target check preserves `Kernel/KUtils.HC::Call`.

`Kernel/KLoad.asm::LoadPass2HC` adapts the three HolyC stack arguments
and removes 24 argument bytes. The shared C++ scanner uses the existing
unchecked `CallModuleMain` entry so exceptions keep their guest behavior.
The internal descriptor overload marks its module started, invokes this
scanner, and returns true after normal completion. The raw interface
does not search for or update native descriptors.

The raw API trusts its table, as the reference does: it has no length
argument or bounded-image validation. Normal `Load` still validates
the complete image before it reaches this scanner. The compiler's
reverse-import binder keeps its separate validation and initialization
guards.

`make test-load-pass2` runs the common `LoadPass2Probe.HC` source with
the unchanged late-import consumer and provider. The pinned guest and
native GCC and Clang builds produced the same 134-byte trace and `FFFF`
check mask. The probe covers all tail kinds, unsigned offsets with their
high bit set, repeated passes, ignored flags, null targets, exceptions,
and an entry that runs while its unrelated function import remains
pending. [Boot.md](Boot.md#raw-holyc-loadpass2) records the inputs and
observed results. Malformed pointers and unterminated raw tables are
outside this comparison.

## Compiler module ownership and startup

The Makefile lists the compiler's 16 C++ sources and four assembly
sources separately in `COMPILER_CXX_SRC` and `COMPILER_ASM_SRC`.
Those objects form `build/Compiler/Compiler.o`; they are absent from
the kernel ELF link. `MakeBIN` converts that combined object with
`Compiler/Compiler.bindings`. Every ISO target packages the result
beside the unchanged `Compiler/OpCodes.DD` resource.

The compiler manifest states each side of the boundary:

| Record | Count | Purpose |
| --- | ---: | --- |
| Native imports | 44 | Exact ELF names mapped to the kernel's `__unholyos_sysv.` entries |
| Data imports | 30 | Existing runtime storage, including the two uppercase aliases above |
| Public exports | 9 | Existing HolyC compiler adapters |
| Private native exports | 16 | Native compiler bodies called by kernel and Adam code |
| Status export | 1 | `CInitStatus` as `__unholyos_compiler.CInitStatus` |
| Entry | 1 | `CInitHC`, the sole `IET_MAIN` target |

This manifest is a C++ build addition. The pinned
`Compiler/Compiler.PRJ` instead turns declarations in the kernel
headers into HolyC imports while compiling the project.

The compiler's nine public HolyC adapters live beside their C++
implementations, in the order of Terry A. Davis's corresponding
`Compiler/CMain.HC`, `Compiler/CMisc.HC`, and `Compiler/LexLib.HC`
routines:

| Assembly file | Adapter symbols |
| --- | --- |
| `Compiler/CMain.asm` | `CmpHC`, `ExePutSHC`, `ExeFileHC`, `StreamExePrintHC` |
| `Compiler/CMisc.asm` | `StreamPrintHC`, `StreamDirHC` |
| `Compiler/LexLib.asm` | `MemberMetaDataHC`, `MemberMetaFindHC`, `MemberFindHC` |

These C++ ABI additions retain their arguments, saved registers,
cleanup, and public names. `Kernel/KLoad.cpp` no longer registers
them as kernel exports; loading the compiler publishes them.
`CInitHC` is appended in `Compiler/CMain.asm` and adapts its zero-argument
HolyC entry to `CInitCpp`. `HC_FRAME_ENTER`, `HC_FRAME_CALL`, `HOLYC_CPP`, and
`HOLYC_PRINT` are defined once in `Kernel/KernelA.inc` and used by
both the kernel and compiler adapters. Including these definitions
emits no instructions.

The reference intentionally imports compiler routines back into the
kernel. `ExeCmdLine` belongs to
[`Kernel/KTask.HC`](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Kernel/KTask.HC)
and calls `LexStmt2Bin`; `Compiler/CMain.HC` calls `ExeCmdLine`.
[`Kernel/KHashB.HC`](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Kernel/KHashB.HC)
also identifies `ClassMemberLstDel` as an import to the kernel.
Those responsibilities stay in their reference subsystems. The native
`CmpCall` helper in `Kernel/KTask.cpp` adapts checked HolyC execution;
the reference uses `Call(machine_code)` at that point.

`Kernel/KExts.hpp`, `KExts.cpp`, and `KExts.asm` implement the native
calls back into the compiler. The 16-field `CCompilerImports` record
uses the declared C++ function-pointer types, with assertions for its
128-byte size and every slot offset. Each assembly entry checks the
published table and its target using `R11`, then tail-jumps without
changing arguments or the stack. An unavailable entry raises the
existing `UndefExt` exception. This is a C++ adaptation of the compiler
imports enabled by `Kernel/Kernel.PRJ`; it follows the advice to check
function pointers at the start of `Kernel/KExts.HC`.

The private `ExePutS` and `ExeFile` exports name their actual native
bodies, which return the 24-byte `CExeResult`. Its field offsets and
size are asserted. The reverse-import entries preserve the hidden
System V return-storage argument. Public HolyC calls still use
`ExePutSHC` and `ExeFileHC`, whose adapters return the reference's
integer result. Binding the private names to `ExePutSCpp` or
`ExeFileCpp` would cross incompatible return conventions.

`LoadCompilerImports` in `Kernel/KLoad.cpp` inspects the loaded
module's own records. It requires all 16 targets to be unique relative
exports inside that module's payload. The status export must contain
eight aligned bytes and initially hold zero. Unresolved imports,
an already-started module, heap or immediate exports under those names,
duplicate names, and invalid bounds fail before table publication.
After validation, `CompilerImportsPublish` copies the complete typed
table under `CMemLock` and publishes its pointer with a release store.
A later publication is rejected.

`Kernel/KMain.cpp` loads the compiler after PS/2 setup and before
color definitions, DolDoc definitions, graphics contexts, the window
manager, or interactive tasks. The Adam task and its heaps already
exist at this point. The loader uses `LDF_JUST_LOAD | LDF_SILENT`,
binds the native reverse imports, and then calls `LoadPass2`.
`CInitCpp` records initialization in module-owned storage:

| `CInitStatus` | Meaning |
| ---: | --- |
| `0` | The module entry has not run |
| `-1` | The entry has begun; initialization has not succeeded |
| `1` | `AsmHashLoad` returned success |

KMain requires both successful `LoadPass2` and status `1` before setting
`RLF_COMPILER` and continuing. Missing module, binding failure, or
initialization failure produces a display and serial diagnostic and
returns to the existing idle path. The controlled failure and status
export are native implementation additions.

The reference changes to `/Compiler` and calls `Load("Compiler")`.
The native path names `/Compiler/Compiler.BIN` on the selected boot
drive, preserving the caller's directory. `AsmHashLoad` similarly
reads `::/Compiler/OpCodes.DD` from the boot volume. These absolute
paths are explicit differences from `Kernel/KMain.HC` and
`Compiler/AsmInit.HC`.

The implemented `CInitCpp` calls only `AsmHashLoad`. It does not
reproduce the full `Compiler/CMain.HC` initialization sequence:
`CmpLoadDefines`, `CmpFillTables`, the `cmp.ic_nop` setup, and
`UAsmHashLoad` are not performed there. The startup-test ISO's
opcode-resource probe runs between reverse-import binding and the
module entry so it can still test a missing first read, successful
retry, and cached reuse. That probe remains in `Linux/BootProbe.cpp`.

Adam remains statically linked. Its DolDoc addresses are therefore
available when compiler imports are resolved. Later separation of
Adam must address the late imports declared in `Compiler/CExts.HC`.
Generic `LoadPass2` permits those pending imports and executes entries
on every explicit call, matching the reference. The compiler-specific
binder still requires all of its imports before publishing its native
dispatch table. The complete `StartOS.HC` and
`Adam/MakeAdam.HC` startup sequence also remains unfinished.

At the compiler-separation revision
`2e9052c46dcd99dc2fe4c45527882dc3a631db74` (PR #81), both production
compiler conversions passed with the explicit manifest. These hashes
identify that revision; later compiler metadata changes produce different
module bytes:

| Build | `Compiler.BIN` bytes | SHA256 |
| --- | ---: | --- |
| GCC | 246,208 | `19324dd7315e5625dcaffdb2bfabd0975a50371edae0d081539e6b86aa3a71fe` |
| Clang | 254,976 | `c6130db6ba6979231359b1518c08df24603eb0a4239c834d6eda35b9ddd81f67` |

Both GCC and Clang guest builds passed `TestBoot --compiler-failures`.
The seven cases cover a missing or corrupt compiler module, a missing
required export, a zero-valued immediate export in place of a native
target, an unresolved kernel import, and missing or malformed opcode
data. The driver checks relocated module bytes, retained patch records,
import targets, status, and heap ownership. Binding failures leave the
dispatch table unpublished and status at zero; opcode failures retain
the published table and leave status at `-1`. Every case stops before
`RLF_COMPILER` and later Adam initialization. Both builds also passed the
complete boot and startup matrices at 512, 3,072, and 4,608 MiB, plus
guest ISO generation and cold boot. [Boot.md](Boot.md) records the commands,
VM configuration, and observed results.

## Relocation mapping

`S` is a symbol's payload offset, `A` the ELF addend, and `P` the
relocation field's payload offset. A defined symbol must belong to an
allocated section.

| ELF relocation | Defined symbol | Imported symbol |
| --- | --- | --- |
| `R_X86_64_PC32`, `R_X86_64_PLT32` | Write `S+A-P` after a signed 32-bit range check | Require `A=-4`; emit `IET_REL_I32` |
| `R_X86_64_64` | Write a nonnegative payload offset and add its low 32 bits to `IET_ABS_ADDR` | Require `A=0`; emit `IET_IMM_I64` |
| `R_X86_64_GOTPCREL`, `R_X86_64_GOTPCRELX`, `R_X86_64_REX_GOTPCRELX` | Resolve the instruction's displacement to an internal GOT slot; relocate that slot through `IET_ABS_ADDR` | Resolve the displacement to an internal GOT slot; patch that slot with `IET_IMM_I64` |

The reference loader adjusts only four bytes for `IET_ABS_ADDR`. The
producer initializes each internal pointer as a full eight-byte value
with zero upper 32 bits. The normal code-heap load must place the complete
allocation below 2 GiB, so adding the module base to the low 32 bits preserves
the pointer. Caller-supplied fixed load addresses are outside this
producer's contract.

Native import patches replace their destination with the resolved
address or displacement; they do not preserve an existing addend.
Consequently, an external absolute pointer such as `external_data+8`
is rejected even when PIC compilation emits it. The diagnostic identifies
the object, section, field offset, symbol, relocation type, and addend.
The first implementation also rejects `R_X86_64_32` and `R_X86_64_32S`.

The destination is replaced only after validation and complete output
generation. The writer uses a temporary file in the destination directory
and renames it after a successful write and close.

## Verification fixture

`Linux/Fixtures/HostModuleProbe.cpp` and its assembly companion exercise
zeroed storage, initialized data, an internal pointer with an addend, a
local function pointer, GOT access to imported data, an external data
pointer, direct and indirect HolyC calls, 64-byte data alignment, and
System V entry stack alignment. The complete result is `0x1FF`.

The module exports `HostModuleResult` and five `HostModuleAnchor` words:
the anchor's own address, the initialized data address, an address 16
bytes into that data, a local function address, and the imported character
bitmap address. Loading the same file twice checks that internal pointers
move with their respective allocations while the imported pointer stays
the same.

`Linux/TestMakeBIN.cpp` invokes the actual producer, checks deterministic
output and native record bounds, and supplies malformed ELF and manifest
inputs. Rejected inputs must leave an existing destination byte for byte
intact. A crash is a test failure. GCC and Clang compile both the producer
and the freestanding fixture independently.

The focused native-import coverage uses
`Linux/Fixtures/NativeImportProbe.cpp`, its assembly entry, and its
explicit binding manifest. It checks direct and indirect calls to
`__unholyos_sysv.PutS` and `__unholyos_sysv.StrCmpCpp`, alongside a
call to the public HolyC `PutS`. Exported addresses let the harness
check that the private native entries and public HolyC entry remain
distinct. GCC and Clang builds passed the native fixture and the
pinned-guest comparison, with matching result and binding masks.

For the pinned-guest comparison,
`Linux/Fixtures/NativeImportReference.asm` supplies test-only System V
adapters around TempleOS's HolyC `PutS` and `_STRCMP`.
`NativeImportLoadProbe.HC` checks that the private names are initially
absent, loads that bridge, and checks the fixture's bindings.
The bridge is verification scaffolding; it does not establish native
C++ support in TempleOS.

The guest harness and its reproduced results are recorded in
[Booting from DVD](Boot.md). The pinned-guest module-fixture comparisons
predate the compiler load change; the separate compiler's boot results
are recorded independently. Neither establishes complete C++20 support
inside the guest or self-hosting.
