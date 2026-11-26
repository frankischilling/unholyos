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
| `Adam/ADefine.HC` | `Adam/ADefine.cpp` | Startup document name definition |
| `Adam/Opt/Boot/BootDVD.HC` | `Adam/Opt/Boot/BootDVD.asm` | DVD boot loader |
| `Adam/Opt/Boot/DskISORedSea.HC` | `Adam/Opt/Boot/DskISORedSea.cpp`, `DskISORedSea.asm` | RedSea ISO construction and the guest's stage-one image |

Use `Kernel`, including its capital K, in the filesystem, include paths, and build definitions. An existing Windows checkout can retain an older lowercase directory name even when Git tracks the correct case. Check actual directory names when preparing a Linux build.

`Linux/` holds C++ host support and verification programs. `Linux/BootProbe.cpp` runs inside the guest and contains the boot reports and fixture checks consumed by the host's `TestBoot.cpp`. Its declaration is in `Linux/BootProbe.hpp`. The Makefile lists it separately in `GUEST_TEST_SRC` and compiles it with the freestanding guest flags. `Linux/Fixtures/` holds HolyC source and binary inputs. `Kernel/SerialDev/Serial.cpp` and `SerialBoot.asm` provide serial diagnostics without a matching reference file. `Adam/DolDoc/DolDoc.hpp` and `Kernel/Compress.hpp` are additional C++ declarations. These additions support the translation and its tests; they do not represent completed TempleOS subsystems.

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
`Compiler/CInit.HC`. `Compiler/AsmInit.asm` embeds the dictionary
while the compiler directory is absent from the guest boot media.
`Compiler/Asm.cpp` uses its opcode, alias, and register records for
instruction selection and emission. The current encoder covers integer,
segment, and x87 forms used by the tests. Far and `MOFFS` operand forms
remain pending. [Pinned opcode dictionary](https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Compiler/OpCodes.DD)

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
scheduler. The desktop playback in `Adam/ASnd.HC`, the AU writer in
`Adam/Snd/SndFile.HC`, and video capture remain pending.

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
can turn off an existing task and raise an enabled one. Dictionary
initialization, task creation, and completion results remain pending.

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
`Compiler` exception. The source-position report in
`Compiler/CExcept.HC`'s `LexExcept` remains pending.

`Adam/WinMgr.cpp` follows `WinKeyNavMenu` and the mouse menu loop in
`Adam/WinMgr.HC`. Mouse selection starts at the screen's top edge.
The GUI key opens keyboard selection, the arrow keys position the
highlight, and releasing the GUI key sends the highlighted command
to the focused task. UnholyOS tracks that virtual cursor during
drawing so the input pump can keep sampling the physical mouse.

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
`DocClear` pass. Macro dispatch, wrapping, data widgets, and the full
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
reserved high bits and updates both copies. Noncontiguous allocation
starts after the FSInfo recent-allocation hint. Whole-file replacement
writes a new chain before replacing the directory entry and reclaiming
the old chain. Directory growth allocates and zeroes a cluster before
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
The scan implementation supports the field formats exercised by the demos
and class reports; general `StrScan` and the full editor status bar, menu,
undo, and task-end handling remain incomplete. List selection uses a modal
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

## Remaining structural work

`Kernel/KMain.cpp` contains `SysGlblsInit`, `TimersInit`, and `KMainCpp`, in the relative definition order of their reference counterparts. `KMainCpp` calls `BootProbe` after the window manager starts and before user startup. The standard and startup-test ISOs use the same kernel, so both retain that verification call. The probes and their serial output belong to `Linux/BootProbe.cpp`; their bodies and execution order are preserved. [Verification evidence](SourceStructureVerification.md) records the source comparison, build, artifact hashes, and QEMU results.

Startup remains partial. The current kernel initializes several Adam subsystems directly. The reference loads the compiler and executes `StartOS.HC`, which includes `Adam/MakeAdam.HC`. Matching that startup path requires further compiler and runtime work. The probe extraction does not establish startup parity.

The [development milestones](Development.md) define completion in the required order: full rewrite, full compatibility, bootstrap, then self-hosting. Host GCC or Clang and NASM support the current development builds.

The guest's `RedSeaISO` follows the sizing and directory-copy steps from
`Adam/Opt/Boot/DskISORedSea.HC` and `Adam/ABlkDev/ADskA.HC`. The current
`CopyTree2` works on the ISO's private RedSea RAM volume. The reference
uses `BDT_ISO_FILE_WRITE` and the general `CopyTree` path. UnholyOS
reserves a spare DVD sector when a stage-two file is supplied because the
shifted read can extend beyond that file's last RedSea block. The loader
is the NASM `BootDVD.BIN` embedded by `DskISORedSea.asm`.

The native compiler, runtime, desktop, and device subsystems remain incomplete. Source correspondence is a review aid. [Boot.md](Boot.md) records the implemented scope and runtime comparisons; new changes need their own validation before those claims expand.
