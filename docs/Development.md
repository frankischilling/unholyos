# Development milestones

UnholyOS aims to reproduce all of TempleOS in freestanding C++20 and x86
assembly, with native HolyC support. The scope includes the kernel, compiler,
runtime, desktop, editor, device support, system tools, applications, and games.
Original, unmodified HolyC programs must compile and run inside the guest with
the reference language semantics, runtime APIs, and calling conventions.

The source of truth is Terry A. Davis's
[TempleOS at c26482bb6ad3f80106d28504ec5db3c6a360732c](https://github.com/cia-foundation/TempleOS/tree/c26482bb6ad3f80106d28504ec5db3c6a360732c).
Keep that checkout separate and read-only. Verify its revision through the Git
Human Workflow helper before source or runtime comparisons.

Work proceeds in this order: full rewrite, full compatibility, bootstrap,
then self-hosting. Host GCC or Clang and NASM remain the development toolchain
during the rewrite and compatibility work. Building a development ISO now does
not complete the later bootstrap milestone. Existing guest compilation and ISO
tests also do not establish that the system can rebuild itself.

## Source and tooling rules

Read each reference implementation and its callers before translating it.
Preserve directory names, case, basenames, symbols, record fields, and their
order. Translate runtime `.HC` implementations to `.cpp`, `.HH` declarations
to `.hpp`, and assembly portions to `.asm` at the corresponding path. Keep
runtime responsibilities in their reference subsystems. Record necessary
differences by source path and symbol in [Source layout](SourceLayout.md).

Use procedural C++20 with the reference's two-space blocks, brace placement,
pointer spelling, and compact expressions where the language permits. Apply
`.clang-format` where it can express those rules, then compare with the source.
NASM uses Intel syntax and uppercase instructions and registers. Preserve label
order, data order, and alignment, and explain required directive translations.

Preserve integer widths, record sizes and offsets, argument order, return
values, and stack cleanup. Assert shared layouts at compile time. Keep C++ ABI
adapters explicit and preserve the exported HolyC names. Matching a declaration
or filename alone does not prove compatibility.

Write all maintained host tools, test drivers, generators, and other executable
tooling in C++20. Makefiles and CI YAML may orchestrate installed tools. Hosted
programs may use the standard library and host APIs; guest code stays
freestanding. Keep guest verification code identifiable as test support.
Original HolyC programs and fixtures retain their language, exact bytes,
provenance, and Terry A. Davis's credit. Add a separate fixture when different
test input is needed.

## 1. Full rewrite

Implement the complete reference scope in its corresponding source locations.
Track each subsystem's missing routines, partial implementations, temporary
adapters, and required assets. Complete the native HolyC compiler and runtime
alongside the kernel and desktop so the original software can use them.

This phase finishes when every reference subsystem and required runtime path
has an implementation, all applications and games are present and runnable, and no
known feature remains represented only by a stub or a demonstration. Source
review and focused runtime tests accompany each increment. The resulting
system must build with the host toolchain and boot from a clean VM start.

## 2. Full compatibility

Compare the completed rewrite with the pinned reference under the same VM
configuration. Cover normal operation, failures, boundary cases, observable
quirks, UI text, file and disk formats, and the HolyC ABI. Compile and execute
the original unmodified programs, including applications and games.

This phase finishes when the complete pinned reference scope has comparison
evidence and every known compatibility discrepancy is resolved. Record the exact
revision, inputs, commands, VM settings, and observed results. Source review
and reproduced runtime results remain separate evidence. Individual passing
probes do not establish compatibility for untested behavior.

## 3. Bootstrap

After the rewrite and compatibility phases are complete, use host GCC or Clang
and NASM to build the seed system for self-hosting. Supply the guest compiler,
assembler, linker, source tree, and build entry points needed to rebuild the
C++20, HolyC, and x86 assembly components. Record the seed tools and versions
and make the host build reproducible from a clean checkout.

This phase finishes when the seed ISO boots and its guest build environment can
begin the complete native rebuild. A host-built seed is an explicit input to
this milestone. The in-guest rebuild and its replacement ISO are verified in
the final phase.

## 4. Self-hosting

Build every UnholyOS C++20, HolyC, and x86 assembly component entirely inside
the guest. Rebuild the tools needed for that process and produce a bootable ISO
through the native workflow. No host compilation or host-generated replacement
objects may supply missing steps in the reported guest build.

Follow the reference's project and distribution workflow: `MakeAll` in
[BootDVDIns.HC](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Adam/Opt/Boot/BootDVDIns.HC)
compiles `Compiler/Compiler.PRJ` before `Kernel/Kernel.PRJ`, then `BootDVDIns`
places the kernel in `/0000Boot/0000Kernel.BIN.C`.
[Misc/DoDistro.HC](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Misc/DoDistro.HC)
builds the distribution tree and calls `RedSeaISO` from
[DskISORedSea.HC](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Adam/Opt/Boot/DskISORedSea.HC).
Preserve those responsibilities while adding the required C++ compilation.

This phase finishes when the ISO built inside the guest boots from a clean VM,
passes the applicable compatibility checks, and performs another complete
guest build. Record both generations and account for any nondeterministic
metadata when comparing their outputs. Current implementation and runtime
evidence are recorded in [Booting from DVD](Boot.md); the milestones above
define completion criteria rather than claims that those criteria are met.
