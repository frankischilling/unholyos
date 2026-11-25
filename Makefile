BUILD := build
CXX := g++
NASM := nasm
LD := ld
OBJCOPY := objcopy
HOSTCXX := g++
QEMU := qemu-system-x86_64
HOSTCXXFLAGS := -std=c++20 -O2 -Wall -Wextra -Werror

CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -Werror -ffreestanding -fno-pie \
	-fno-exceptions -fno-rtti -fno-stack-protector -fno-asynchronous-unwind-tables \
	-mno-red-zone -mgeneral-regs-only -MMD -MP

ASM_SRC := Kernel/KStart16.asm Kernel/KStart32.asm Kernel/KStart64.asm Kernel/KUtils.asm \
	Kernel/KMathA.asm Kernel/StrA.asm Kernel/KInts.asm Kernel/KExcept.asm Kernel/Sched.asm Kernel/PCIBIOS.asm \
	Kernel/Mem/PageTables.asm Kernel/SerialDev/SerialBoot.asm Adam/Opt/Boot/DskISORedSea.asm \
	Compiler/CExcept.asm
CXX_SRC := Kernel/KMain.cpp Kernel/KGlbls.cpp Kernel/KInts.cpp Kernel/KMisc.cpp Kernel/KDbg.cpp Kernel/MultiProc.cpp Kernel/Display.cpp Kernel/SerialDev/Serial.cpp \
	Kernel/KTask.cpp Kernel/KHashA.cpp Kernel/KHashB.cpp Kernel/KLoad.cpp Kernel/KMathB.cpp Kernel/StrA.cpp \
	Kernel/KDataTypes.cpp Kernel/KDate.cpp Kernel/KExcept.cpp Kernel/QSort.cpp \
	Kernel/StrPrint.cpp Kernel/FunSeg.cpp Kernel/KDefine.cpp Kernel/KeyDev.cpp \
	Kernel/EdLite.cpp Kernel/Job.cpp \
	Kernel/PCIBIOS.cpp Kernel/Mem/MemPag.cpp Kernel/Mem/MAllocFree.cpp \
	Kernel/Mem/BlkPool.cpp Kernel/Mem/HeapCtrl.cpp Kernel/Mem/MemPhysical.cpp Kernel/Mem/PageTables.cpp \
	Kernel/BlkDev/DskATA.cpp Kernel/BlkDev/DskATAId.cpp Kernel/BlkDev/DskAddDev.cpp \
	Kernel/BlkDev/DskCDDVD.cpp Kernel/BlkDev/FileSysRedSea.cpp Kernel/BlkDev/DskFile.cpp \
	Kernel/BlkDev/DskStrA.cpp Kernel/BlkDev/DskDirA.cpp Kernel/BlkDev/DskBlk.cpp Kernel/Compress.cpp \
	Compiler/CMain.cpp Compiler/Asm.cpp Compiler/CHash.cpp Compiler/CExcept.cpp Compiler/CMisc.cpp Compiler/Lex.cpp Compiler/LexLib.cpp Compiler/AsmInit.cpp Compiler/PrsLib.cpp \
	Compiler/PrsExp.cpp Compiler/PrsStmt.cpp Compiler/PrsVar.cpp Compiler/OptLib.cpp Compiler/BackC.cpp \
	Adam/DolDoc/DocNew.cpp Adam/DolDoc/DocRecalc.cpp \
	Adam/DolDoc/DocPlain.cpp Adam/DolDoc/DocBin.cpp Adam/DolDoc/DocPutS.cpp Adam/DolDoc/DocFile.cpp \
	Adam/Opt/Boot/DskISORedSea.cpp
ASM_OBJ := $(ASM_SRC:%.asm=$(BUILD)/%.asm.o)
CXX_OBJ := $(CXX_SRC:%.cpp=$(BUILD)/%.o)
KERNEL := $(BUILD)/0000Boot/0000Kernel.BIN.C
LOADER := $(BUILD)/Adam/Opt/Boot/BootDVD.BIN
ISO := $(BUILD)/unholyos.iso
ISO_TOOL := $(BUILD)/Linux/MakeISO
BOOT_TEST := $(BUILD)/Linux/TestBoot
COMPRESS_TEST := $(BUILD)/Linux/TestCompress
STARTUP_FIXTURE := Linux/Fixtures/StartOS.HC.Z
MODULE_PROBE := Linux/Fixtures/LoadProbe.BIN
MODULE_CALLER := Linux/Fixtures/LoadCaller.BIN
RUNTIME_PROBE := Linux/Fixtures/RuntimeProbe.BIN
FILE_READ_PROBE := Linux/Fixtures/FileReadProbe.BIN
FILE_WRITE_PROBE := Linux/Fixtures/FileWriteProbe.BIN
EXCEPT_PROBE := Linux/Fixtures/ExceptProbe.BIN
FORMAT_PROBE := Linux/Fixtures/FormatProbe.BIN
HASH_CALLER := Linux/Fixtures/HashCaller.BIN
HASH_PROBE := Linux/Fixtures/HashProbe.BIN
DOC_PROBE := Linux/Fixtures/DocProbe.BIN
COMPILER_RUNTIME_PROBE := Linux/Fixtures/CompilerRuntimeProbe.BIN
LEX_PROBES := $(addprefix Linux/Fixtures/,LexProbe.HC LexDirectives.HC LexInput.HC LexNested.HH ExpressionProbe.HC FunctionProbe.HC)
DOC_SOURCE_PROBES := $(addprefix Linux/Fixtures/,TextDemo.HC SpritePlot.HC)
PRINT_SOURCE_PROBES := $(addprefix Linux/Fixtures/,Print.HC PrintSourceProbe.HC)
STREAM_PROBES := $(addprefix Linux/Fixtures/,StreamProbe.HC StreamNested.HC)
MEMORY_PROBES := $(addprefix Linux/Fixtures/,MemoryProbe.HC SubIntAccess.HC AggregateProbe.HC CompoundProbe.HC)
ISO_PROBE := Linux/Fixtures/RedSeaISOProbe.HC
CMP_PROBES := $(addprefix Linux/Fixtures/,CmpProbeRunner.HC CmpProbe.PRJ CmpProbe.HH CmpInvalid.HC \
	ImportCaller.HC ImportProvider.HC ImportBuild.HC ImportExtern.HC ImportLate.HC \
	AsmBuild.HC AsmProbe.HC DivByHand.HC AsmHelloWorld.HC MulByHand.HC PutDec.HC \
	AsmAndC3.HC InlineAsmProbe.HC)
STARTUP_TEST_ISO := $(BUILD)/unholyos-startup-test.iso

.PHONY: all run test
all: $(ISO)

$(LOADER): Adam/Opt/Boot/BootDVD.asm Kernel/KernelA.inc
	mkdir -p $(@D)
	$(NASM) -f bin -o $@ $<

$(BUILD)/Adam/Opt/Boot/DskISORedSea.asm.o: NASMFLAGS += -D'BDVD_FILE="$(LOADER)"'
$(BUILD)/Adam/Opt/Boot/DskISORedSea.asm.o: $(LOADER)

$(ASM_OBJ): $(BUILD)/%.asm.o: %.asm Kernel/KernelA.inc
	mkdir -p $(@D)
	$(NASM) $(NASMFLAGS) -f elf64 -o $@ $<

$(CXX_OBJ): $(BUILD)/%.o: %.cpp
	mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD)/Kernel/Kernel.elf: $(ASM_OBJ) $(CXX_OBJ) Kernel/Kernel.ld
	$(LD) -m elf_x86_64 --build-id=none -T Kernel/Kernel.ld -o $@ $(filter %.o,$^)

$(KERNEL): $(BUILD)/Kernel/Kernel.elf
	mkdir -p $(@D)
	$(OBJCOPY) -O binary $< $@

$(ISO_TOOL): Linux/MakeISO.cpp Adam/Opt/Boot/DskISORedSea.cpp \
	Adam/Opt/Boot/DskISORedSea.hpp Linux/BootMedia.hpp Kernel/KernelA.hpp
	mkdir -p $(@D)
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ $(filter %.cpp,$^)

$(BOOT_TEST): Linux/TestBoot.cpp Linux/BootMedia.hpp
	mkdir -p $(@D)
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ $<

$(COMPRESS_TEST): Linux/TestCompress.cpp Linux/BootMedia.hpp Kernel/Compress.cpp \
	Kernel/Compress.hpp Kernel/KernelA.hpp Kernel/KernelB.hpp
	mkdir -p $(@D)
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ Linux/TestCompress.cpp Kernel/Compress.cpp

$(ISO): $(LOADER) $(KERNEL) $(ISO_TOOL)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@

TEST_MODULES := $(MODULE_PROBE) $(MODULE_CALLER) $(RUNTIME_PROBE) $(FILE_READ_PROBE) \
	$(FILE_WRITE_PROBE) $(EXCEPT_PROBE) $(FORMAT_PROBE) $(HASH_CALLER) $(HASH_PROBE) \
	$(DOC_PROBE) $(COMPILER_RUNTIME_PROBE)

STARTUP_FILES := $(TEST_MODULES) $(LEX_PROBES) $(DOC_SOURCE_PROBES) \
	$(PRINT_SOURCE_PROBES) $(STREAM_PROBES) $(ISO_PROBE) $(MEMORY_PROBES) $(CMP_PROBES)

$(STARTUP_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_FIXTURE) $(STARTUP_FILES)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(STARTUP_FIXTURE) $(addprefix --file ,$(STARTUP_FILES))

run: $(ISO)
	$(QEMU) -machine pc -cpu qemu64 -m 512 -boot d -cdrom $< -serial stdio

test: $(ISO) $(BOOT_TEST) $(COMPRESS_TEST) $(STARTUP_TEST_ISO)
	$(COMPRESS_TEST) $(STARTUP_FIXTURE) Linux/Fixtures/KDbg.HC.Z
	$(BOOT_TEST) "$(QEMU)" $< $(KERNEL) $(STARTUP_TEST_ISO)

-include $(CXX_OBJ:.o=.d)
