# Rewrite audit

Source reference: TempleOS `c26482bb6ad3f80106d28504ec5db3c6a360732c`.
This inventory is a path comparison, not an implementation percentage. It
counts tracked `.HC` and `.HH` files in the reference, then checks for the
same path or the same basename with a `.cpp`, `.hpp` or `.asm` extension
in UnholyOS. It includes include-only and project-organization files.

| Directory | Reference files | Corresponding paths | Missing paths |
| --- | ---: | ---: | ---: |
| `Kernel` | 76 | 69 | 7 |
| `Compiler` | 33 | 33 | 0 |
| `Adam` | 132 | 53 | 79 |
| `Apps` | 71 | 3 | 68 |
| `Demo` | 203 | 22 | 181 |

A corresponding path can contain a partial translation. Counts do not
establish that a file builds or that an original program runs. Asset,
documentation and distribution files outside this inventory are also
part of the full rewrite.

## Remaining kernel work

- `Kernel/BlkDev/FileSysISO.HC` is present in the reference tree, but is
  not included by `MakeBlkDev.HC`; `KernelA.HH` marks `FSt_ISO9660`
  unsupported. Preserve that active-build distinction rather than
  enabling a general ISO9660 filesystem. Fixed-size contiguous read-only
  and writable RedSea file-backed devices are implemented, including
  contiguous FAT32 files, nested images, optical backing pins, and
  the mounted-image guest ISO builder.
  `DskCache.cpp` now supplies the block cache and drive and device
  locking use task ownership. `DskClus.cpp` supplies cluster traversal, allocation,
  reads and writes, and `DirNew` publishes RedSea and FAT32 directory
  entries. RedSea retains the reference free-extent queue and uses its
  best-fit selection. Drive and device deletion, enabling/disabling, and initial
  disk-change handling are implemented; complete chain ownership,
  recovery after persistent hardware I/O errors, general open-stream lifetime,
  extended and unmounted partitions, persistent FSInfo ownership, removable-media
  behavior and multi-CPU locking still need work.
  `DskCluster.HC` is present in the reference tree but not included by
  its `MakeBlkDev.HC`; the active cluster implementation is `DskClus.HC`.
- `Kernel/KCfg.inc` and `Kernel/KCfg.hpp` implement host-build configuration
  with actual memory, heap, and variable initialization; boot and home drive
  selection; primary IDE discovery; and disk cache sizing. The five focused
  boot profiles in [KernelConfiguration.md](KernelConfiguration.md) cover
  default and alternate values, separate data heaps, and text-mode gating.
  The original interactive configuration, complete drive discovery, and text
  desktop remain outstanding. `Kernel/BlkDev/MakeBlkDev.HC`,
  `Kernel/Mem/MakeMem.HC`, `Kernel/SerialDev/MakeSerialDev.HC`,
  `Kernel/KEnd.HC` and `Kernel/KernelC.HH` still need their project and
  declaration responsibilities integrated without duplicating existing code.
- Review existing scheduler, interrupt, device, memory and exception
  routines against the reference. Single-CPU boot and selected device
  tests do not establish SMP, driver or scheduling parity.

## Remaining compiler and Adam work

- `UAsm.cpp` supplies the last missing corresponding compiler path,
  building immutable opcode-table views at compiler initialization and
  exposing `Ui`, `U` and `Un` through the loaded compiler module. It
  decodes selected integer, control-transfer and x87 forms; complete
  instruction coverage, matching selection and formatting in every
  operand/address-size mode, source links and debugger integration
  still need work. The split backend now has
  corresponding integer and floating instruction-selection files, a
  shared emitter, register-ABI symbol resolution and NASM templates.
  Continue translating the
  intermediate-code, optimization and code-generation pipeline as
  connected work, preserving the reference's representation and ABI.
  `OptPass012.cpp` implements typed simplification and immediate lowering
  in the current indexed expression IR. `OptPass3.cpp` assigns eligible
  frame members to four preserved registers and the backend emits their
  loads, stores and updates. `OptPass4.cpp` folds eligible addresses,
  `OptPass5.cpp` selects simple right operands, and `BackLib.cpp` encodes
  their memory and arithmetic instructions. `OptPass6.cpp` selects a single
  R9 temporary for compatible expression trees; `OptPass789A.cpp` relaxes
  ordinary local branches during repeated layout. Full operand-location
  analysis, register selection, stack layout, remaining backend optimizations
  and reference code-generation parity remain outstanding. R9 is spilled
  and restored around ordinary direct and indirect calls. The new intrinsic
  probes verify selected integer and x87 instructions, their typed metadata,
  exact constant folds and JIT/AOT execution; `SYS_POW` uses the reference
  register ABI and a named module relocation. Template variants, general
  clobber analysis and other intrinsic families remain incomplete. Locked
  and unlocked bit operations, memory swaps, unsigned quotient/remainder
  updates, and port I/O now have native instruction lowering. Source-level
  `_intern` declarations work for the supported opcode and arity set in
  JIT and AOT. Remaining intrinsic opcodes and complete operand-location
  selection are still needed for the original headers and startup.
- Original `Adam/AMath.HC` is included without source changes and runs
  in both native JIT and AOT probes. Kernel floating math entries and
  `Complex` metadata supply its dependencies. This is not yet execution
  of the complete original Adam startup sequence.
- Original `Adam/AMem.HC` also runs unchanged. The non-debug heap
  metadata now includes memory blocks, used/unused allocations, block
  pools, complete heap-controller fields and the free-list arrays.
  Single- and separate-heap checks exercise its allocation accounting,
  membership traversal and lock/interrupt restoration.
- Original `Adam/TaskRep.HC` traverses live CPU and task records with
  the native allocator's counters. The shared probe checks recursive
  task ordering, rendered report contents, interrupt restoration and
  child cleanup. CPU enumeration currently covers the initialized boot
  CPU. The original report's Kill command now executes through the
  public task API; SMP startup and graphical link activation remain pending.
- `Adam/DevInfo.cpp` restores the three public device reports. The
  standard DVD includes the pinned PCI database, and the memory reports
  read the kernel's live boot and page-table records.
- `Adam/CPURep.cpp` reports clock frequency and samples CPU yields and
  TSC cycles. Adam enters the server loop after terminal startup so it can
  service jobs. Application-processor startup remains outstanding.
- `Adam/MakeAdam.cpp` compiles the unchanged math, memory and task-report
  sources during ordinary boot. Child tasks inherit the resulting Adam
  namespace instead of rebuilding compiler types independently. Missing
  and malformed source tests verify rollback and stop before the desktop.
  The original `StartOS.HC`, `MakeAdam.HC`, header declarations and remaining
  startup dependencies still need native execution. `Option` and `GetOption`
  now operate on the current compiler context, including the no-register
  optimization gate, and `HashTablePurge` removes superseded entries in
  its own table. Complete option effects and source header support remain
  outstanding.
- `Adam/DolDoc/DocTree.cpp` implements nested branch lookup, creation,
  replacement, append, file persistence and execution from the actual
  document entries. `DocClipBoard.cpp` supplies inclusive range copy and
  cut. Both native toolchains execute the shared tree fixture in JIT and
  AOT mode, including a caught Break and read-only media. Registry startup,
  the remaining clipboard operations, and popup/menu integration remain
  outstanding.
  Complete the window manager, DolDoc, editor, graphics, audio, terminal
  and system tools in their respective reference subsystems.
- Include the remaining applications, demos, documentation and assets,
  retaining the original HolyC sources when they can run unchanged.
  Use original programs as integration tests rather than replacing
  them with smaller demonstrations.

See [Development milestones](Development.md) for the completion criteria,
[Source layout](SourceLayout.md) for implementation details and known
deviations, and [Booting from DVD](Boot.md) for runtime evidence.
