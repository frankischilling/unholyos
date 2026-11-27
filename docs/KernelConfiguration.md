# Kernel configuration

The host bootstrap builds the DVD kernel with the options in
`Kernel/KCfg.inc`. The reference collects its configuration through
`KCfgNew` in `Kernel/Kernel.PRJ`, then emits the resulting values and code
into the kernel during compilation. UnholyOS currently supplies those
values to NASM when building the kernel.

| Setting | DVD default | Consumer |
| --- | --- | --- |
| `KCFG_MEM_INIT` | `-1` | Fill newly admitted free physical memory. |
| `KCFG_HEAP_INIT` | `-1` | Fill the usable bytes of each MAlloc allocation. |
| `KCFG_VAR_INIT` | `-1` | Fill otherwise uninitialized HolyC global variable storage. |
| `KCFG_TEXT_MODE` | `0` | Skip the BIOS graphics mode call and stop after compiler initialization when set to 1. |
| `KCFG_BOOT_DRV_LET` | `'T'` | Mount the boot DVD at the chosen letter (T through Z). |
| `KCFG_HOME_DIR` | `"::/Home"` | Set the account directory for path resolution and new tasks. |
| `KCFG_FIRST_HD_DRV_LET` | `'C'` | First drive letter for the discovered primary ATA disk (C through L). |
| `KCFG_MOUNT_PRIMARY_HD` | `1` | Discover and mount supported partitions of the primary IDE disk. |
| `KCFG_CACHE_MIN` | `2048` | Minimum disk cache request in bytes. |
| `KCFG_CACHE_MAX` | `0x40000` | Maximum disk cache request in bytes. |
| `KCFG_CACHE_LIMIT` | `0x80000000` | Pool size at which the cache reaches its maximum. |

The three initialization settings accept -1 (disabled) or a byte from 0
through 255. Zero is a valid fill value. The compiler can also change
the variable initialization flag and value at runtime. CAlloc still
zeros the requested bytes after MAlloc, as in the reference.

Use an absolute account directory with a drive prefix: `::/Home` resolves
to the configured boot drive, and `D:/Account` selects an explicit drive.
The selected drive and directory must be available when tasks use them.
Set both cache bounds to the same value to request a fixed cache size.
The cache allocator still determines its actual size and can decline
requests below its minimum.

Edit `Kernel/KCfg.inc` and rebuild to change the configuration. NASM
defines in `KCFG_NASMFLAGS` can also override the settings; use a fresh
`BUILD` directory when changing command-line defines so all affected
objects are rebuilt.

Run `make test-kernel-cfg` for a host check of the assembled configuration
data, the memory scaling calculation, drive prefix handling and mutable
initialization flags.

The focused boot test uses a separate ISO with a small HolyC fixture.
It tests the actual boot drive, home aliases and idle task directory,
disk cache allocation, small and large heap fills, CAlloc, sampled free
memory and compiled HolyC globals. The host checks the expected
configuration against the serial report. For the text configuration
it also checks VGA text memory and the halted CPU's interrupt flag.
Run the following profiles in separate build directories:

```sh
make BUILD=build/cfg-default test-kernel-cfg-boot
make BUILD=build/cfg-alt CXX=clang++-20 HOSTCXX=clang++-20 \
    KCFG_NASMFLAGS='-DKCFG_MEM_INIT=107 -DKCFG_HEAP_INIT=165 -DKCFG_VAR_INIT=60 -DKCFG_BOOT_DRV_LET=85 -DKCFG_FIRST_HD_DRV_LET=68 -DKCFG_HOME_DIR=58,58,47,65,99,99,111,117,110,116 -DKCFG_CACHE_MIN=32768 -DKCFG_CACHE_MAX=32768' \
    KCFG_TEST_PROFILE=alt test-kernel-cfg-boot
make BUILD=build/cfg-alt CXX=clang++-20 HOSTCXX=clang++-20 \
    KCFG_NASMFLAGS='-DKCFG_MEM_INIT=107 -DKCFG_HEAP_INIT=165 -DKCFG_VAR_INIT=60 -DKCFG_BOOT_DRV_LET=85 -DKCFG_FIRST_HD_DRV_LET=68 -DKCFG_HOME_DIR=58,58,47,65,99,99,111,117,110,116 -DKCFG_CACHE_MIN=32768 -DKCFG_CACHE_MAX=32768' \
    KCFG_TEST_PROFILE=alt KCFG_TEST_MEMORY=3072 test-kernel-cfg-boot
make BUILD=build/cfg-text CXX=clang++-20 HOSTCXX=clang++-20 \
    KCFG_NASMFLAGS='-DKCFG_TEXT_MODE=1 -DKCFG_MEM_INIT=0 -DKCFG_HEAP_INIT=0 -DKCFG_VAR_INIT=0 -DKCFG_BOOT_DRV_LET=86 -DKCFG_FIRST_HD_DRV_LET=68 -DKCFG_MOUNT_PRIMARY_HD=0 -DKCFG_CACHE_MIN=0 -DKCFG_CACHE_MAX=0' \
    KCFG_TEST_PROFILE=text test-kernel-cfg-boot
```

The alternate profile encodes `::/Account` as a sequence of NASM byte
values to avoid shell quoting of a string definition. It mounts the DVD
as U: and the primary disk as D:. The text profile selects V: and
disables primary disk discovery and the disk cache.

The same alternate build can be tested with `KCFG_TEST_MEMORY=4608`.
On QEMU 8.2.2 with machine pc, CPU qemu64 and a snapshot of the primary
IDE FAT disk, the GCC default profile passed at 512 MiB and the
Clang 20 alternate profile passed at 512, 3072 and 4608 MiB. The
4608 MiB run used data allocations above 4 GiB. The Clang 20 text profile
passed at 512 MiB. Each run reported the complete eight-bit guest
check mask (0xFF); the GUI runs also reached the existing startup
completion marker. These were focused tests using the configuration
fixture, not the full regression suite.

## Compatibility limits

The reference's `KCfg.HC` interactive prompts, generated Mount2 script,
arbitrary HolyC cache expressions, StaffMode task exit logging, NoMP
startup branch, DontProbe port prompts and DbgDistro image embedding are
not yet translated. The current primary disk discovery is a bootstrap
option, separate from the reference's full MountIDEAuto.

For MemInit, the low memory fill begins at the free heap base after the
fixed area and page tables, using the same contiguous E801 range as the
early allocator. Above 16 MiB the fill intersects each newly admitted
pool with the usable BIOS E820 ranges, or the E801 extended memory range
if E820 is unavailable. This preserves the existing allocator's memory
admission assumptions. The defaults leave physical and heap
initialization disabled.

Text mode has no graphical Adam desktop or command prompt yet. It
initializes the kernel and compiler, displays a status message in VGA
text memory, and halts with interrupts disabled. It does not initialize
the graphics contexts, window manager or user tasks, which currently
assume graphics-mode geometry. This is a bootstrap limitation rather
than full text-mode compatibility.
