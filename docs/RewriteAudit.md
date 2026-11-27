# Rewrite audit

Source reference: TempleOS `c26482bb6ad3f80106d28504ec5db3c6a360732c`.
This inventory is a path comparison, not an implementation percentage. It
counts tracked `.HC` and `.HH` files in the reference, then checks for the
same path or the same basename with a `.cpp`, `.hpp` or `.asm` extension
in UnholyOS. It includes include-only and project-organization files.

| Directory | Reference files | Corresponding paths | Missing paths |
| --- | ---: | ---: | ---: |
| `Kernel` | 76 | 68 | 8 |
| `Compiler` | 33 | 18 | 15 |
| `Adam` | 132 | 44 | 88 |
| `Apps` | 71 | 3 | 68 |
| `Demo` | 203 | 22 | 181 |

A corresponding path can contain a partial translation. Counts do not
establish that a file builds or that an original program runs. Asset,
documentation and distribution files outside this inventory are also
part of the full rewrite.

## Remaining kernel work

- `Kernel/BlkDev/FileSysISO.HC`: restore the ISO filesystem and its
  mount and file operations. `DskCache.cpp` now supplies the block cache,
  and drive and device locking use task ownership. The per-drive FAT
  sector cache, RedSea free lists and complete media lifecycle remain
  separate work. `DskClus.cpp` supplies cluster traversal, allocation,
  reads and writes, and `DirNew` publishes RedSea and FAT32 directory
  entries. Complete chain ownership and error recovery, mount lifecycle,
  removable media and multi-CPU locking still need work.
  `DskCluster.HC` is present in the reference tree but not included by
  its `MakeBlkDev.HC`; the active cluster implementation is `DskClus.HC`.
- `Kernel/BlkDev/MakeBlkDev.HC`, `Kernel/Mem/MakeMem.HC`,
  `Kernel/SerialDev/MakeSerialDev.HC`, `Kernel/KCfg.HC`, `Kernel/KEnd.HC`
  and `Kernel/KernelC.HH`: reproduce their configuration, declaration
  and build responsibilities without duplicating existing code.
- Review existing scheduler, interrupt, device, memory and exception
  routines against the reference. Single-CPU boot and selected device
  tests do not establish SMP, driver or scheduling parity.

## Remaining compiler and Adam work

- The unmatched compiler files are `BackA.HC`, `BackB.HC`, `BackFA.HC`,
  `BackFB.HC`, `BackLib.HC`, `CExts.HC`, `CompilerB.HH`, `OptPass012.HC`,
  `OptPass3.HC`, `OptPass4.HC`, `OptPass5.HC`, `OptPass6.HC`,
  `OptPass789A.HC`, `Templates.HC` and `UAsm.HC`. Translate the
  intermediate-code, optimization and code-generation pipeline as
  connected work, preserving the reference's representation and ABI.
- Move remaining Adam initialization to the guest startup path after
  compiler support permits the original project and startup files.
  Complete the window manager, DolDoc, editor, graphics, audio, terminal
  and system tools in their respective reference subsystems.
- Include the remaining applications, demos, documentation and assets,
  retaining the original HolyC sources when they can run unchanged.
  Use original programs as integration tests rather than replacing
  them with smaller demonstrations.

See [Development milestones](Development.md) for the completion criteria,
[Source layout](SourceLayout.md) for implementation details and known
deviations, and [Booting from DVD](Boot.md) for runtime evidence.
