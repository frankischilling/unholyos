# UnholyOS

UnholyOS is a freestanding C++20 and x86 assembly rewrite of TempleOS. The goal is to reproduce its architecture, desktop, file formats, and runtime behavior, including the ability to compile and run unmodified HolyC programs in the guest.

Terry A. Davis wrote TempleOS. The rewrite uses the pinned TempleOS source and matching runtime behavior as its reference. [Reference source](https://github.com/cia-foundation/TempleOS/tree/c26482bb6ad3f80106d28504ec5db3c6a360732c)

See [Booting from DVD](docs/Boot.md) for the build, QEMU command, and subsystem status. The [DolDoc boot guide](Doc/Boot.DD) uses the native document format.
