# Building TOSB modules from host objects

`Linux/MakeBIN.cpp` converts a combined x86-64 ELF relocatable object into
the TOSB format used by TempleOS. It provides a host build path for C++20
and NASM components without changing the guest loader's records. The
compiler and Adam are still part of the host-linked kernel image.

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
already has a public data export. A future compiler manifest must map
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

## Compiler ABI placement and remaining loading work

The compiler's nine public HolyC adapters live beside their C++
implementations, in the order of Terry A. Davis's corresponding
`Compiler/CMain.HC`, `Compiler/CMisc.HC`, and `Compiler/LexLib.HC`
routines:

| Assembly file | Adapter symbols |
| --- | --- |
| `Compiler/CMain.asm` | `CmpHC`, `ExePutSHC`, `ExeFileHC`, `StreamExePrintHC` |
| `Compiler/CMisc.asm` | `StreamPrintHC`, `StreamDirHC` |
| `Compiler/LexLib.asm` | `MemberMetaDataHC`, `MemberMetaFindHC`, `MemberFindHC` |

These C++ ABI additions have moved out of `Kernel/KUtils.asm`.
Their arguments, saved registers, cleanup, and public names are
unchanged. `HC_FRAME_ENTER`, `HC_FRAME_CALL`, `HOLYC_CPP`, and
`HOLYC_PRINT` are defined once in `Kernel/KernelA.inc` and used by
both the kernel and compiler adapters. Including these definitions
emits no instructions. The adapter move preserves the assembled
instruction bytes and relocation records.

The reference intentionally imports compiler routines back into the
kernel. `ExeCmdLine` belongs to
[`Kernel/KTask.HC`](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Kernel/KTask.HC)
and calls `LexStmt2Bin`; `Compiler/CMain.HC` calls `ExeCmdLine`.
[`Kernel/KHashB.HC`](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Kernel/KHashB.HC)
also identifies `ClassMemberLstDel` as an import to the kernel.
Those responsibilities stay in their reference subsystems. The native
`CmpCall` helper in `Kernel/KTask.cpp` adapts checked HolyC execution;
the reference uses `Call(machine_code)` at that point.

The current kernel still links the compiler and Adam statically.
Separating the compiler requires explicit bindings for the calls back
into it, a HolyC entry that initializes its native implementation,
packaging `Compiler/Compiler.BIN`, and loading it before any compiler
caller runs. The reference sequence in `Kernel/KMain.HC` loads the
compiler from `/Compiler`, runs `CInit` from `Compiler/CMain.HC`,
sets the `RLf_COMPILER` bit in `sys_run_level`, and then executes
`StartOS.HC`.
Current boot probes can initialize and call the compiler before
`Kernel/KMain.cpp` checks its opcode dictionary. That ordering and
the probes' first-load coverage must be addressed during the cutover.

Later separation of Adam also needs the reference's late imports.
`Compiler/CExts.HC` imports DolDoc functions before Adam is loaded.
The native `LoadPass2` currently skips module entry points while
any imports remain unresolved,
whereas the reference executes the compiler's initialization with
those late imports still pending. Publishing native addresses and
moving the adapters does not complete this initialization work.

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
[Booting from DVD](Boot.md). This tool alone does not establish separate
compiler loading, complete C++20 support inside the guest, or self-hosting.
