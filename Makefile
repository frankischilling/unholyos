BUILD := build
CXX := g++
NASM := nasm
LD := ld
OBJCOPY := objcopy
HOSTCXX := g++
QEMU := qemu-system-x86_64
HOSTCXXFLAGS := -std=c++20 -O2 -Wall -Wextra -Werror

CXXFLAGS := -std=c++20 -Os -Wall -Wextra -Werror -ffreestanding -fno-pie \
	-fno-exceptions -fno-rtti -fno-stack-protector -fno-asynchronous-unwind-tables \
	-mno-red-zone -mgeneral-regs-only -MMD -MP

ASM_SRC := Kernel/KStart16.asm Kernel/KStart32.asm Kernel/KStart64.asm Kernel/KUtils.asm \
	Kernel/KMathA.asm Kernel/StrA.asm Kernel/KInts.asm Kernel/KExcept.asm Kernel/KLoad.asm Kernel/KExts.asm Kernel/Sched.asm Kernel/PCIBIOS.asm \
	Kernel/Mem/PageTables.asm Kernel/SerialDev/SerialBoot.asm Adam/Opt/Boot/DskISORedSea.asm \
	Adam/Gr/GrAsm.asm Adam/Gr/GrTextBase.asm
CXX_SRC := Kernel/KMain.cpp Kernel/KGlbls.cpp Kernel/KInts.cpp Kernel/KMisc.cpp Kernel/KDbg.cpp Kernel/MultiProc.cpp Kernel/Display.cpp Kernel/FontStd.cpp Kernel/FontCyrillic.cpp Kernel/SerialDev/Serial.cpp \
	Kernel/KTask.cpp Kernel/Sched.cpp Kernel/KHashA.cpp Kernel/KHashB.cpp Kernel/KLoad.cpp Kernel/KMathB.cpp Kernel/StrA.cpp \
	Kernel/KDataTypes.cpp Kernel/KDate.cpp Kernel/KExcept.cpp Kernel/KExts.cpp Kernel/QSort.cpp \
	Kernel/StrPrint.cpp Kernel/FunSeg.cpp Kernel/KDefine.cpp Kernel/KeyDev.cpp Kernel/StrB.cpp Kernel/StrScan.cpp \
	Kernel/EdLite.cpp Kernel/Job.cpp Kernel/SerialDev/Message.cpp Kernel/SerialDev/Keyboard.cpp Kernel/SerialDev/Mouse.cpp \
	Kernel/PCIBIOS.cpp Kernel/Mem/MemPag.cpp Kernel/Mem/MAllocFree.cpp \
	Kernel/Mem/BlkPool.cpp Kernel/Mem/HeapCtrl.cpp Kernel/Mem/MemPhysical.cpp Kernel/Mem/PageTables.cpp \
	Kernel/BlkDev/DskATA.cpp Kernel/BlkDev/DskATAId.cpp Kernel/BlkDev/DskAddDev.cpp \
	Kernel/BlkDev/DskCDDVD.cpp Kernel/BlkDev/DskDrv.cpp Kernel/BlkDev/DskBlkDev.cpp Kernel/BlkDev/FileSysRedSea.cpp Kernel/BlkDev/FileSysFAT.cpp Kernel/BlkDev/DskFile.cpp Kernel/BlkDev/DskCFile.cpp Kernel/BlkDev/DskCopy.cpp \
	Kernel/BlkDev/DskStrA.cpp Kernel/BlkDev/DskStrB.cpp Kernel/BlkDev/DskDirA.cpp Kernel/BlkDev/DskDirB.cpp Kernel/BlkDev/DskDirContext.cpp Kernel/BlkDev/DskFind.cpp Kernel/BlkDev/DskBlk.cpp Kernel/BlkDev/DskClus.cpp Kernel/BlkDev/DskCache.cpp Kernel/BlkDev/DskFmt.cpp Kernel/Compress.cpp \
	Adam/ADefine.cpp Adam/ADbg.cpp Adam/ASnd.cpp Adam/TaskSettings.cpp Adam/CPURep.cpp Adam/DevInfo.cpp Adam/AutoComplete/ACTask.cpp Adam/Ctrls/CtrlsA.cpp Adam/Menu.cpp Adam/Win.cpp Adam/WinMgr.cpp Adam/Gr/GrGlbls.cpp Adam/Gr/GrPalette.cpp Adam/Gr/GrDC.cpp Adam/Gr/GrInitB.cpp Adam/Gr/GrMath.cpp \
	Adam/Gr/GrBitMap.cpp Adam/Gr/GrPrimatives.cpp Adam/Gr/GrComposites.cpp Adam/Gr/GrScrn.cpp Adam/Gr/GrEnd.cpp Adam/Gr/GrTextBase.cpp \
	Adam/Gr/SpriteNew.cpp Adam/Gr/GrSpritePlot.cpp Adam/Gr/ScrnCast.cpp \
	Adam/DolDoc/DocNew.cpp Adam/DolDoc/DocDblBuf.cpp Adam/DolDoc/DocRecalc.cpp Adam/DolDoc/DocRecalcLib.cpp \
	Adam/DolDoc/DocPlain.cpp Adam/DolDoc/DocBin.cpp Adam/DolDoc/DocPutS.cpp Adam/DolDoc/DocFile.cpp Adam/DolDoc/DocClipBoard.cpp Adam/DolDoc/DocTree.cpp \
	Adam/DolDoc/DocChar.cpp Adam/DolDoc/DocPutKey.cpp Adam/DolDoc/DocRun.cpp \
	Adam/DolDoc/DocGet.cpp Adam/DolDoc/DocForm.cpp Adam/DolDoc/DocEd.cpp Adam/DolDoc/DocTerm.cpp HomeSys.cpp \
	Adam/ABlkDev/ADskA.cpp Adam/ABlkDev/Mount.cpp Adam/Opt/Boot/DskISORedSea.cpp Adam/MakeAdam.cpp
GUEST_TEST_SRC := Linux/BootProbe.cpp
COMPILER_ASM_SRC := Compiler/CExcept.asm Compiler/CMain.asm Compiler/CMisc.asm Compiler/LexLib.asm Compiler/Templates.asm
COMPILER_CXX_SRC := Compiler/CInit.cpp Compiler/CMain.cpp Compiler/Asm.cpp Compiler/AsmResolve.cpp Compiler/AsmLib.cpp Compiler/CHash.cpp Compiler/CExcept.cpp Compiler/CMisc.cpp Compiler/Lex.cpp Compiler/LexLib.cpp Compiler/AsmInit.cpp Compiler/PrsLib.cpp \
	Compiler/PrsExp.cpp Compiler/PrsStmt.cpp Compiler/PrsVar.cpp Compiler/OptLib.cpp Compiler/OptPass012.cpp Compiler/OptPass3.cpp Compiler/OptPass4.cpp Compiler/OptPass5.cpp Compiler/OptPass6.cpp Compiler/OptPass789A.cpp Compiler/CExts.cpp Compiler/BackLib.cpp Compiler/BackA.cpp Compiler/BackB.cpp Compiler/BackFA.cpp Compiler/BackFB.cpp Compiler/BackC.cpp Compiler/UAsm.cpp
ASM_OBJ := $(ASM_SRC:%.asm=$(BUILD)/%.asm.o)
CXX_OBJ := $(CXX_SRC:%.cpp=$(BUILD)/%.o) $(GUEST_TEST_SRC:%.cpp=$(BUILD)/%.o)
COMPILER_ASM_OBJ := $(COMPILER_ASM_SRC:%.asm=$(BUILD)/%.asm.o)
COMPILER_CXX_OBJ := $(COMPILER_CXX_SRC:%.cpp=$(BUILD)/%.o)
COMPILER_OBJECT := $(BUILD)/Compiler/Compiler.o
COMPILER_MODULE := $(BUILD)/Compiler/Compiler.BIN
COMPILER_BINDINGS := Compiler/Compiler.bindings
KERNEL := $(BUILD)/0000Boot/0000Kernel.BIN.C
LOADER := $(BUILD)/Adam/Opt/Boot/BootDVD.BIN
ISO := $(BUILD)/unholyos.iso
ISO_TOOL := $(BUILD)/Linux/MakeISO
BOOT_TEST := $(BUILD)/Linux/TestBoot
COMPRESS_TEST := $(BUILD)/Linux/TestCompress
STR_COMPARE_TEST := $(BUILD)/Linux/TestStrCompare
OPT_PASS_TEST := $(BUILD)/Linux/TestOptPass012
OPT_PASS3_TEST := $(BUILD)/Linux/TestOptPass3
OPERAND_LOWERING_TEST := $(BUILD)/Linux/TestOperandLowering
OPT_PASS6_TEST := $(BUILD)/Linux/TestOptPass6
BRANCH_LAYOUT_TEST := $(BUILD)/Linux/TestBranchLayout
BIN_TOOL := $(BUILD)/Linux/MakeBIN
MAKE_BIN_TEST := $(BUILD)/Linux/TestMakeBIN
HOST_MODULE_CPP := $(BUILD)/Linux/Fixtures/HostModuleProbe.cpp.o
HOST_MODULE_ASM := $(BUILD)/Linux/Fixtures/HostModuleProbe.asm.o
HOST_MODULE_OBJECT := $(BUILD)/Linux/Fixtures/HostModuleProbe.o
HOST_MODULE := $(BUILD)/Linux/Fixtures/HostModuleProbe.BIN
HOST_MODULE_BINDINGS := Linux/Fixtures/HostModuleProbe.bindings
HOST_MODULE_TEST_ISO := $(BUILD)/host-modules-test.iso
NATIVE_IMPORT_CPP := $(BUILD)/Linux/Fixtures/NativeImportProbe.cpp.o
NATIVE_IMPORT_ASM := $(BUILD)/Linux/Fixtures/NativeImportProbe.asm.o
NATIVE_IMPORT_OBJECT := $(BUILD)/Linux/Fixtures/NativeImportProbe.o
NATIVE_IMPORT := $(BUILD)/Linux/Fixtures/NativeImportProbe.BIN
NATIVE_IMPORT_BINDINGS := Linux/Fixtures/NativeImportProbe.bindings
NATIVE_IMPORT_REFERENCE_OBJECT := $(BUILD)/Linux/Fixtures/NativeImportReference.asm.o
NATIVE_IMPORT_REFERENCE := $(BUILD)/Linux/Fixtures/NativeImportReference.BIN
NATIVE_IMPORT_REFERENCE_BINDINGS := Linux/Fixtures/NativeImportReference.bindings
NATIVE_IMPORT_TEST_ISO := $(BUILD)/native-import-test.iso
LATE_IMPORT_CONSUMER_OBJECT := $(BUILD)/Linux/Fixtures/LateImportConsumer.asm.o
LATE_IMPORT_PROVIDER_OBJECT := $(BUILD)/Linux/Fixtures/LateImportProvider.asm.o
LATE_IMPORT_CONSUMER := $(BUILD)/Linux/Fixtures/LateImportConsumer.BIN
LATE_IMPORT_PROVIDER := $(BUILD)/Linux/Fixtures/LateImportProvider.BIN
LATE_IMPORT_TEST_ISO := $(BUILD)/late-import-test.iso
LOAD_API_CONSUMER_OBJECT := $(BUILD)/Linux/Fixtures/LoadApiConsumer.asm.o
LOAD_API_CONSUMER := $(BUILD)/Linux/Fixtures/LoadApiConsumer.BIN
LOAD_API_PROBE := Linux/Fixtures/LoadApiProbe.HC
LOAD_API_TEST_ISO := $(BUILD)/load-api-test.iso
LOAD_PASS2_PROBE := Linux/Fixtures/LoadPass2Probe.HC
LOAD_PASS2_TEST_ISO := $(BUILD)/load-pass2-test.iso
COMPILER_DEFINES_PROBE := Linux/Fixtures/CompilerDefinesProbe.HC
COMPILER_DEFINES_TEST_ISO := $(BUILD)/compiler-defines-test.iso
STRING_DEFINES_PROBE := Linux/Fixtures/StringDefinesProbe.HC
STRING_DEFINES_TEST_ISO := $(BUILD)/string-defines-test.iso
SYSTEM_DEFINES_PROBE := Linux/Fixtures/SystemDefinesProbe.HC
SYSTEM_DEFINES_TEST_ISO := $(BUILD)/system-defines-test.iso
COLOR_CONVERSIONS_PROBE := Linux/Fixtures/ColorConversionsProbe.HC
COLOR_CONVERSIONS_TEST_ISO := $(BUILD)/color-conversions-test.iso
FORMAT_MISSING_ARGS_PROBE := Linux/Fixtures/FormatMissingArgsProbe.HC
FORMAT_MISSING_ARGS_TEST_ISO := $(BUILD)/format-missing-args-test.iso
INTERNAL_TYPES_PROBE := Linux/Fixtures/InternalTypesProbe.HC
INTERNAL_TYPES_TEST_ISO := $(BUILD)/internal-types-test.iso
CLASS_RECORDS_PROBE := Linux/Fixtures/ClassRecordsProbe.HC
CLASS_RECORDS_TEST_ISO := $(BUILD)/class-records-test.iso
LIST_UTILITIES_PROBE := Linux/Fixtures/ListUtilitiesProbe.HC
LIST_UTILITIES_TEST_ISO := $(BUILD)/list-utilities-test.iso
STORAGE_SERVICES_PROBES := $(addprefix Linux/Fixtures/,DriveServicesProbe.HC PathNamesProbe.HC DirectoryServicesProbe.HC)
STORAGE_SERVICES_TEST_ISO := $(BUILD)/storage-services-test.iso
CLUSTER_ENTRY_PROBE := Linux/Fixtures/ClusterEntryProbe.HC
CLUSTER_ENTRY_TEST_ISO := $(BUILD)/cluster-entry-test.iso
FAT_CLUSTER_PROBE := Linux/Fixtures/Fat32ClusterProbe.HC
FAT_CLUSTER_TEST_ISO := $(BUILD)/fat32-cluster-test.iso
DISK_CACHE_PROBE := Linux/Fixtures/DiskCacheProbe.HC
DISK_CACHE_TEST_ISO := $(BUILD)/disk-cache-test.iso
FAT_SECTOR_CACHE_PROBE := $(addprefix Linux/Fixtures/,FatSectorCacheProbe.HC FatSectorCacheNativeProbe.HC)
FAT_SECTOR_CACHE_TEST_ISO := $(BUILD)/fat-sector-cache-test.iso
STORAGE_LIFECYCLE_PROBE := $(addprefix Linux/Fixtures/,StorageLifecycleProbe.HC StorageLifecycleNativeProbe.HC)
STORAGE_LIFECYCLE_TEST_ISO := $(BUILD)/storage-lifecycle-test.iso
FILE_BACKED_REDSEA_PROBES := $(addprefix Linux/Fixtures/,FileBackedRedSeaProbe.HC FileBackedRedSeaWriteProbe.HC FileBackedRedSeaNativeProbe.HC)
FILE_BACKED_REDSEA_TEST_ISO := $(BUILD)/file-backed-redsea-test.iso
OPTIMIZER_PROBE := Linux/Fixtures/OptimizerProbe.HC
OPTIMIZER_AOT_PROBE := Linux/Fixtures/OptimizerAOTProbe.HC
OPTIMIZER_TEST_ISO := $(BUILD)/optimizer-test.iso
OPTIMIZER_AOT_TEST_ISO := $(BUILD)/optimizer-aot-test.iso
REGISTER_ALLOCATION_PROBES := $(addprefix Linux/Fixtures/,RegisterAllocationProbe.HC RegisterAllocationNativeProbe.HC)
REGISTER_ALLOCATION_TEST_ISO := $(BUILD)/register-allocation-test.iso
REGISTER_ALLOCATION_AOT_PROBE := Linux/Fixtures/RegisterAllocationAOTProbe.HC
REGISTER_ALLOCATION_AOT_TEST_ISO := $(BUILD)/register-allocation-aot-test.iso
ADDRESS_FOLD_PROBES := $(addprefix Linux/Fixtures/,AddressFoldProbe.HC AddressFoldNativeProbe.HC)
ADDRESS_FOLD_TEST_ISO := $(BUILD)/address-fold-test.iso
ADDRESS_FOLD_AOT_PROBE := Linux/Fixtures/AddressFoldAOTProbe.HC
ADDRESS_FOLD_AOT_TEST_ISO := $(BUILD)/address-fold-aot-test.iso
BACKEND_PROBES := $(addprefix Linux/Fixtures/,BackendProbe.HC BackendNativeProbe.HC)
BACKEND_TEST_ISO := $(BUILD)/backend-test.iso
BACKEND_AOT_PROBE := Linux/Fixtures/BackendAOTProbe.HC
BACKEND_AOT_TEST_ISO := $(BUILD)/backend-aot-test.iso
ADAM_MATH_SOURCE := Adam/AMath.HC
ADAM_MATH_PROBE := Linux/Fixtures/AdamMathProbe.HC
ADAM_MATH_AOT_PROBE := Linux/Fixtures/AdamMathAOTProbe.HC
ADAM_MATH_TEST_ISO := $(BUILD)/adam-math-test.iso
ADAM_MATH_AOT_TEST_ISO := $(BUILD)/adam-math-aot-test.iso
ADAM_MEM_SOURCE := Adam/AMem.HC
ADAM_MEM_PROBE := Linux/Fixtures/AdamMemProbe.HC
ADAM_MEM_AOT_PROBE := Linux/Fixtures/AdamMemAOTProbe.HC
ADAM_MEM_TEST_ISO := $(BUILD)/adam-mem-test.iso
ADAM_MEM_AOT_TEST_ISO := $(BUILD)/adam-mem-aot-test.iso
ADAM_TASK_SOURCE := Adam/TaskRep.HC
ADAM_TASK_PROBE := Linux/Fixtures/AdamTaskProbe.HC
ADAM_TASK_AOT_PROBE := Linux/Fixtures/AdamTaskAOTProbe.HC
ADAM_TASK_TEST_ISO := $(BUILD)/adam-task-test.iso
ADAM_TASK_AOT_TEST_ISO := $(BUILD)/adam-task-aot-test.iso
ADAM_STARTUP_PROBE := Linux/Fixtures/AdamStartupProbe.HC
DEV_INFO_TEST_ISO := $(BUILD)/dev-info-test.iso
DEV_INFO_PROBE := Linux/Fixtures/DevInfoProbe.HC
CPU_REP_TEST_ISO := $(BUILD)/cpu-rep-test.iso
CPU_REP_PROBE := Linux/Fixtures/CPURepProbe.HC
ADAM_STARTUP_TEST_ISO := $(BUILD)/adam-startup-test.iso
DOC_TREE_PROBE := Linux/Fixtures/DocTreeProbe.HC
DOC_TREE_AOT_PROBE := Linux/Fixtures/DocTreeAOTProbe.HC
DOC_TREE_TEST_ISO := $(BUILD)/doc-tree-test.iso
UASM_PROBES := $(addprefix Linux/Fixtures/,UAsmProbe.HC UAsmNativeProbe.HC)
UASM_TEST_ISO := $(BUILD)/uasm-test.iso
UASM_AOT_PROBE := Linux/Fixtures/UAsmAOTProbe.HC
UASM_AOT_TEST_ISO := $(BUILD)/uasm-aot-test.iso
NESTED_REDSEA_PROBES := $(addprefix Linux/Fixtures/,NestedRedSeaProbe.HC RedSeaFreeListProbe.HC RedSeaISOImageProbe.HC NestedRedSeaNativeProbe.HC)
NESTED_REDSEA_TEST_ISO := $(BUILD)/nested-redsea-test.iso
FAT32_IMAGE_PROBE := Linux/Fixtures/Fat32ImageProbe.HC
FAT32_IMAGE_TEST_ISO := $(BUILD)/fat32-image-test.iso
OPTICAL_IMAGE_PROBE := Linux/Fixtures/OpticalImageProbe.HC
OPTICAL_IMAGE_TEST_ISO := $(BUILD)/optical-image-test.iso
CHARACTER_GRAPHICS_PROBE := Linux/Fixtures/CharPanObserver.HC
CHARACTER_GRAPHICS_TEST_ISO := $(BUILD)/character-graphics-test.iso
FAT_TOOL := $(BUILD)/Linux/MakeFAT32
FAT_DISK := $(BUILD)/fat32.img
FAT_INVALID_DISK := $(BUILD)/fat32-invalid.img
FAT_TEST_ISO := $(BUILD)/fat32-test.iso
FAT_PROBES := Linux/Fixtures/Fat32ReadProbe.HC Linux/Fixtures/Fat32RejectProbe.HC
FAT_WRITE_DISK := $(BUILD)/fat32-writable.img
FAT_WRITE_ISO := $(BUILD)/fat32-write-test.iso
FAT_WRITE_PROBE := Linux/Fixtures/Fat32WriteProbe.HC
FAT_FORMAT_ISO := $(BUILD)/fat32-format-test.iso
FAT_FORMAT_PROBE := Linux/Fixtures/Fat32FormatProbe.HC
FAT_STREAM_ISO := $(BUILD)/fat32-stream-test.iso
FAT_STREAM_PROBE := Linux/Fixtures/Fat32StreamProbe.HC
FAT_TREE_ISO := $(BUILD)/fat32-tree-test.iso
FAT_TREE_PROBE := Linux/Fixtures/Fat32TreeProbe.HC
ATA_TEST_ISO := $(BUILD)/ata-test.iso
ATA_PROBES := Linux/Fixtures/AtaWriteProbe.HC Linux/Fixtures/AtaWriteRejectProbe.HC
BREAK_TEST_ISO := $(BUILD)/break-test.iso
BREAK_PROBE := Linux/Fixtures/BreakProbe.HC
MATH_TEST_ISO := $(BUILD)/math-test.iso
MATH_PROBE := Linux/Fixtures/MathHelpersProbe.HC
MUSIC_PRECISION_ISO := $(BUILD)/music-precision-test.iso
MUSIC_PRECISION_PROBE := Linux/Fixtures/MusicPrecision.HC
STRINGS_TEST_ISO := $(BUILD)/strings-test.iso
STRINGS_PROBES := Linux/Fixtures/StrScanProbe.HC Linux/Fixtures/DocScanProbe.HC Linux/Fixtures/StringUtilitiesProbe.HC \
	Linux/Fixtures/FloatScanProbe.HC Linux/Fixtures/DateScanProbe.HC Linux/Fixtures/StringCompareProbe.HC
STARTUP_FIXTURE := Linux/Fixtures/StartOS.HC.Z
STARTUP_DOC := Doc/Start.DD
PCI_DEV_FILE := Misc/PCIDevices.DD
COMPILER_RESOURCE := Compiler/OpCodes.DD
ADAM_STARTUP_SOURCES := Adam/AMath.HC Adam/AMem.HC Adam/TaskRep.HC
COMPILER_ISO_ARGS := --file-at $(COMPILER_RESOURCE) $(COMPILER_RESOURCE) \
	--file-at Compiler/Compiler.BIN $(COMPILER_MODULE) \
	$(foreach source,$(ADAM_STARTUP_SOURCES),--file-at $(source) $(source))
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
DOC_SOURCE_PROBES := $(addprefix Linux/Fixtures/,TextDemo.HC SpritePlot.HC MemSetProbe.HC)
PRINT_SOURCE_PROBES := $(addprefix Linux/Fixtures/,Print.HC PrintSourceProbe.HC)
STREAM_PROBES := $(addprefix Linux/Fixtures/,StreamProbe.HC StreamNested.HC)
MEMORY_PROBES := $(addprefix Linux/Fixtures/,MemoryProbe.HC SubIntAccess.HC AggregateProbe.HC CompoundProbe.HC)
ISO_PROBE := Linux/Fixtures/RedSeaISOProbe.HC
CMP_PROBES := $(addprefix Linux/Fixtures/,CmpProbeRunner.HC CmpProbe.PRJ CmpProbe.HH CmpInvalid.HC AssertWarningProbe.HC \
	ImportCaller.HC ImportProvider.HC ImportBuild.HC ImportExtern.HC ImportLate.HC \
	AsmBuild.HC AsmProbe.HC DivByHand.HC AsmHelloWorld.HC MulByHand.HC PutDec.HC \
	AsmAndC3.HC InlineAsmProbe.HC GotoProbe.HC \
	NullCase.HC SubSwitch.HC SwitchProbe.HC SwitchBuild.HC OpcodeProbe.HC \
	FixedAsm.HC FixedImage.HC AsmBranchProbe.HC AsmBranchImport.HC BuzzFizz.HC \
	AsmListingProbe.HC AsmListingScope.HC SchedulerProbe.HC JobProbe.HC SoundProbe.HC SoundRecord.HC AUProbe.HC MusicProbe.HC \
	KeyboardProbe.HC MsgLoop.HC MouseProbe.HC PaletteProbe.HC DCProbe.HC BitmapDiffProbe.HC CaptureProbe.HC RasterProbe.HC Blot3Probe.HC DepthSpansProbe.HC MatrixProbe.HC PenProbe.HC ArrowProbe.HC ArcProbe.HC SplineProbe.HC TaskSettingsProbe.HC WinScrollProbe.HC ScrollControlsProbe.HC DocRefreshProbe.HC ViewAnglesProbe.HC MenuProbe.HC MenuProbe.DD Lines.HC \
	ScreenProbe.HC FontTextProbe.HC PromptProbe.HC DateOffsetProbe.HC CursorProbe.HC CursorExcept.HC Grid.HC FunctionPtrProbe.HC VariadicProbe.HC VariadicPtrProbe.HC DocFrame.HC DocButton.HC DocCallback.HC TryProbe.HC Exceptions.HC DirectoryProbe.HC DirContextProbe.HC FileStreamProbe.HC FileCopyProbe.HC FileOperationsProbe.HC DocBinLinkProbe.HC SpriteProbe.HC MemberMetaProbe.HC LastClassProbe.HC)
GRAPHICS_DEMOS := Demo/Graphics/3DPoly.HC Demo/Graphics/Box.HC \
	Demo/Graphics/Symmetry.HC Demo/Graphics/CommonAncestor.HC Demo/Graphics/Shading.HC Demo/Graphics/ScrnCapture.HC Demo/Graphics/BSpline.HC \
	Demo/PullDownMenu.HC Demo/ExtChars.HC Demo/Prompt.HC Demo/Graphics/Blot.HC
GAME_DEMOS := Demo/Games/Digits.HC Demo/Games/Maze.HC Demo/Games/CharDemo.HC Demo/Games/TicTacToe.HC
APP_FILES := Apps/TimeClock/Load.HC Apps/TimeClock/Install.HC Apps/TimeClock/TimeClk.HC
CLASS_META_DEMO := Demo/ClassMeta.HC Demo/LastClass.HC
FORM_DEMOS := Demo/Dsk/DataBase.HC Demo/DolDoc/Form.HC
SOUND_DEMOS := Demo/Snd/OhGreat.HC
DOC_BIN_DEMOS := Demo/Graphics/SpritePut.HC Demo/Graphics/SpritePutExt.HC
DOC_BIN_LINK := Linux/Fixtures/DocBinLink.DD
QUEUE_PROBE := Linux/Fixtures/QueueProbe.HC
TIME_CLOCK_PROBE := Linux/Fixtures/TimeClockProbe.HC Linux/Fixtures/TimeClockInputProbe.HC
STARTUP_TEST_ISO := $(BUILD)/unholyos-startup-test.iso

# Every bootable test image uses the same Adam source initialization.
$(ISO) $(foreach target,$(filter %_ISO,$(.VARIABLES)),$($(target))): $(ADAM_STARTUP_SOURCES)

.PHONY: all run test test-host-modules test-native-imports test-late-imports test-load-api test-load-pass2 test-compiler-defines test-string-defines test-system-defines test-color-conversions test-format-missing-args test-internal-types test-class-records
all: $(ISO)

$(LOADER): Adam/Opt/Boot/BootDVD.asm Kernel/KernelA.inc
	mkdir -p $(@D)
	$(NASM) -f bin -o $@ $<

$(BUILD)/Adam/Opt/Boot/DskISORedSea.asm.o: NASMFLAGS += -D'BDVD_FILE="$(LOADER)"'
$(BUILD)/Adam/Opt/Boot/DskISORedSea.asm.o: $(LOADER)

$(BUILD)/Kernel/KStart16.asm.o $(BUILD)/Kernel/KStart32.asm.o: Kernel/KCfg.inc
$(BUILD)/Kernel/KStart16.asm.o $(BUILD)/Kernel/KStart32.asm.o: NASMFLAGS += $(KCFG_NASMFLAGS)

$(BUILD)/Linux/KernelCfgTest.asm.o: Kernel/KStart32.asm Kernel/KernelA.inc Kernel/KCfg.inc
	mkdir -p $(@D)
	$(NASM) -DKCFG_MEM_INIT=0xA5 -DKCFG_HEAP_INIT=0 -DKCFG_VAR_INIT=-1 \
		-DKCFG_BOOT_DRV_LET=85 -DKCFG_FIRST_HD_DRV_LET=68 -DKCFG_MOUNT_PRIMARY_HD=0 \
		-D'KCFG_HOME_DIR="::/Account"' -DKCFG_CACHE_MIN=32768 -DKCFG_CACHE_MAX=32768 \
		-DKCFG_CACHE_LIMIT=0x80000000 -f elf64 -o $@ $<

$(BUILD)/Linux/TestKernelCfg: Linux/TestKernelCfg.cpp Kernel/KCfg.hpp $(BUILD)/Linux/KernelCfgTest.asm.o
	$(HOSTCXX) $(HOSTCXXFLAGS) -no-pie -Wl,--gc-sections -o $@ $< $(BUILD)/Linux/KernelCfgTest.asm.o

.PHONY: test-kernel-cfg
test-kernel-cfg: $(BUILD)/Linux/TestKernelCfg
	$<

test: test-kernel-cfg

$(BUILD)/kernel-cfg-test.iso: $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) Linux/Fixtures/KernelCfgProbe.HC $(COMPILER_MODULE) $(COMPILER_RESOURCE) $(ADAM_STARTUP_SOURCES)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --dir Account --file Linux/Fixtures/KernelCfgProbe.HC

KCFG_TEST_PROFILE ?= default
KCFG_TEST_MEMORY ?= 512
.PHONY: test-kernel-cfg-boot
test-kernel-cfg-boot: $(BUILD)/kernel-cfg-test.iso $(FAT_WRITE_DISK) $(BOOT_TEST)
	$(BOOT_TEST) --kernel-cfg "$(QEMU)" $(BUILD)/kernel-cfg-test.iso $(FAT_WRITE_DISK) \
		$(KCFG_TEST_PROFILE) $(KCFG_TEST_MEMORY) $(BUILD)/kernel-cfg-$(KCFG_TEST_MEMORY)

$(ASM_OBJ) $(COMPILER_ASM_OBJ): $(BUILD)/%.asm.o: %.asm Kernel/KernelA.inc
	mkdir -p $(@D)
	$(NASM) $(NASMFLAGS) -f elf64 -o $@ $<

$(CXX_OBJ): $(BUILD)/%.o: %.cpp
	mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(COMPILER_CXX_OBJ): $(BUILD)/%.o: %.cpp
	mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -fPIC -fcf-protection=none -c -o $@ $<

$(COMPILER_OBJECT): $(COMPILER_CXX_OBJ) $(COMPILER_ASM_OBJ)
	$(LD) -m elf_x86_64 -r --no-relax --force-group-allocation -o $@ $^

$(COMPILER_MODULE): $(COMPILER_OBJECT) $(COMPILER_BINDINGS) $(BIN_TOOL)
	$(BIN_TOOL) $(COMPILER_OBJECT) $(COMPILER_BINDINGS) $@

$(BUILD)/Kernel/Kernel.elf: $(ASM_OBJ) $(CXX_OBJ) Kernel/Kernel.ld
	$(LD) -m elf_x86_64 --build-id=none -T Kernel/Kernel.ld -o $@ $(filter %.o,$^)

$(KERNEL): $(BUILD)/Kernel/Kernel.elf
	mkdir -p $(@D)
	$(OBJCOPY) -O binary $< $@

$(ISO_TOOL): Linux/MakeISO.cpp Adam/Opt/Boot/DskISORedSea.cpp Kernel/Compress.cpp \
	Adam/Opt/Boot/DskISORedSea.hpp Linux/BootMedia.hpp Kernel/Compress.hpp Kernel/KernelA.hpp Kernel/KernelB.hpp
	mkdir -p $(@D)
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ $(filter %.cpp,$^)

$(BOOT_TEST): Linux/TestBoot.cpp Linux/BootMedia.hpp Linux/TestCharacterGraphics.hpp Linux/TestStrings.hpp Linux/TestFileBackedRedSea.hpp
	mkdir -p $(@D)
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ $<

$(COMPRESS_TEST): Linux/TestCompress.cpp Linux/BootMedia.hpp Kernel/Compress.cpp \
	Kernel/Compress.hpp Kernel/KernelA.hpp Kernel/KernelB.hpp
	mkdir -p $(@D)
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ Linux/TestCompress.cpp Kernel/Compress.cpp

$(STR_COMPARE_TEST): Linux/TestStrCompare.cpp $(BUILD)/Kernel/StrA.asm.o
	mkdir -p $(@D)
	$(HOSTCXX) $(HOSTCXXFLAGS) -fno-pie -no-pie -mno-red-zone -o $@ $^

$(OPT_PASS_TEST): Linux/Fixtures/OptPass012Probe.cpp Compiler/OptPass012.cpp Compiler/CompilerA.hpp Kernel/KernelA.hpp
	mkdir -p $(@D)
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ $(filter %.cpp,$^)

.PHONY: test-opt-pass012
test-opt-pass012: $(OPT_PASS_TEST)
	$(OPT_PASS_TEST)

test: test-opt-pass012

$(OPT_PASS3_TEST): Linux/Fixtures/OptPass3Probe.cpp Compiler/OptPass3.cpp Compiler/CompilerA.hpp Kernel/KernelA.hpp
	mkdir -p $(@D)
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ $(filter %.cpp,$^)

.PHONY: test-opt-pass3
test-opt-pass3: $(OPT_PASS3_TEST)
	$(OPT_PASS3_TEST)

test: test-opt-pass3

$(OPERAND_LOWERING_TEST): Linux/Fixtures/OperandLoweringProbe.cpp Compiler/BackLib.cpp Compiler/OptPass5.cpp Compiler/OptPass4.cpp Compiler/CompilerA.hpp Kernel/KernelA.hpp
	mkdir -p $(@D)
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ $(filter %.cpp,$^)

.PHONY: test-operand-lowering
test-operand-lowering: $(OPERAND_LOWERING_TEST)
	$(OPERAND_LOWERING_TEST)

test: test-operand-lowering

$(OPT_PASS6_TEST): Linux/Fixtures/OptPass6Probe.cpp Compiler/OptPass6.cpp Compiler/OptPass3.cpp Compiler/CompilerA.hpp Kernel/KernelA.hpp
	mkdir -p $(@D)
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ $(filter %.cpp,$^)

.PHONY: test-opt-pass6
test-opt-pass6: $(OPT_PASS6_TEST)
	$(OPT_PASS6_TEST)

test: test-opt-pass6

$(BRANCH_LAYOUT_TEST): Linux/Fixtures/BranchLayoutProbe.cpp Compiler/OptPass789A.cpp Compiler/CompilerA.hpp Kernel/KernelA.hpp
	mkdir -p $(@D)
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ $(filter %.cpp,$^)

.PHONY: test-branch-layout
test-branch-layout: $(BRANCH_LAYOUT_TEST)
	$(BRANCH_LAYOUT_TEST)

test: test-branch-layout

$(BIN_TOOL): Linux/MakeBIN.cpp Linux/BootMedia.hpp Kernel/KernelA.hpp
	mkdir -p $(@D)
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ $<

$(MAKE_BIN_TEST): Linux/TestMakeBIN.cpp Linux/BootMedia.hpp Kernel/KernelA.hpp
	mkdir -p $(@D)
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ $<

$(HOST_MODULE_CPP) $(NATIVE_IMPORT_CPP): $(BUILD)/Linux/Fixtures/%.cpp.o: Linux/Fixtures/%.cpp
	mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -fPIC -fcf-protection=none -c -o $@ $<

$(HOST_MODULE_ASM) $(NATIVE_IMPORT_ASM) $(NATIVE_IMPORT_REFERENCE_OBJECT) $(LATE_IMPORT_CONSUMER_OBJECT) $(LATE_IMPORT_PROVIDER_OBJECT) $(LOAD_API_CONSUMER_OBJECT): $(BUILD)/Linux/Fixtures/%.asm.o: Linux/Fixtures/%.asm
	mkdir -p $(@D)
	$(NASM) $(NASMFLAGS) -f elf64 -o $@ $<

$(HOST_MODULE_OBJECT): $(HOST_MODULE_CPP) $(HOST_MODULE_ASM)
	$(LD) -m elf_x86_64 -r --no-relax --force-group-allocation -o $@ $^

$(HOST_MODULE): $(HOST_MODULE_OBJECT) $(HOST_MODULE_BINDINGS) $(BIN_TOOL)
	$(BIN_TOOL) $(HOST_MODULE_OBJECT) $(HOST_MODULE_BINDINGS) $@

$(NATIVE_IMPORT_OBJECT): $(NATIVE_IMPORT_CPP) $(NATIVE_IMPORT_ASM)
	$(LD) -m elf_x86_64 -r --no-relax --force-group-allocation -o $@ $^

$(NATIVE_IMPORT): $(NATIVE_IMPORT_OBJECT) $(NATIVE_IMPORT_BINDINGS) $(BIN_TOOL)
	$(BIN_TOOL) $(NATIVE_IMPORT_OBJECT) $(NATIVE_IMPORT_BINDINGS) $@

$(NATIVE_IMPORT_REFERENCE): $(NATIVE_IMPORT_REFERENCE_OBJECT) $(NATIVE_IMPORT_REFERENCE_BINDINGS) $(BIN_TOOL)
	$(BIN_TOOL) $(NATIVE_IMPORT_REFERENCE_OBJECT) $(NATIVE_IMPORT_REFERENCE_BINDINGS) $@

$(LATE_IMPORT_CONSUMER): $(LATE_IMPORT_CONSUMER_OBJECT) Linux/Fixtures/LateImportConsumer.bindings $(BIN_TOOL)
	$(BIN_TOOL) $(LATE_IMPORT_CONSUMER_OBJECT) Linux/Fixtures/LateImportConsumer.bindings $@

$(LATE_IMPORT_PROVIDER): $(LATE_IMPORT_PROVIDER_OBJECT) Linux/Fixtures/LateImportProvider.bindings $(BIN_TOOL)
	$(BIN_TOOL) $(LATE_IMPORT_PROVIDER_OBJECT) Linux/Fixtures/LateImportProvider.bindings $@

$(LOAD_API_CONSUMER): $(LOAD_API_CONSUMER_OBJECT) Linux/Fixtures/LoadApiConsumer.bindings $(BIN_TOOL)
	$(BIN_TOOL) $(LOAD_API_CONSUMER_OBJECT) Linux/Fixtures/LoadApiConsumer.bindings $@

$(FAT_TOOL): Linux/MakeFAT32.cpp
	mkdir -p $(@D)
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ $<

$(FAT_DISK): $(FAT_TOOL)
	$(FAT_TOOL) $@

$(FAT_INVALID_DISK): $(FAT_TOOL)
	$(FAT_TOOL) $@ invalid

$(FAT_WRITE_DISK): $(FAT_TOOL)
	$(FAT_TOOL) $@ writable

$(ISO) $(STARTUP_TEST_ISO) $(FAT_TEST_ISO) $(ATA_TEST_ISO) $(FAT_WRITE_ISO) \
$(FAT_FORMAT_ISO) $(FAT_STREAM_ISO) $(FAT_TREE_ISO) $(BREAK_TEST_ISO) \
$(MATH_TEST_ISO) $(MUSIC_PRECISION_ISO) $(STRINGS_TEST_ISO) \
$(HOST_MODULE_TEST_ISO) $(NATIVE_IMPORT_TEST_ISO) $(LATE_IMPORT_TEST_ISO) $(LOAD_API_TEST_ISO) $(LOAD_PASS2_TEST_ISO) $(COMPILER_DEFINES_TEST_ISO) $(STRING_DEFINES_TEST_ISO) $(SYSTEM_DEFINES_TEST_ISO) $(COLOR_CONVERSIONS_TEST_ISO) $(FORMAT_MISSING_ARGS_TEST_ISO) $(INTERNAL_TYPES_TEST_ISO) $(CLASS_RECORDS_TEST_ISO): $(COMPILER_RESOURCE) $(COMPILER_MODULE)

$(HOST_MODULE_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(HOST_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(HOST_MODULE)

$(NATIVE_IMPORT_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(NATIVE_IMPORT)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(NATIVE_IMPORT)

$(LATE_IMPORT_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(LATE_IMPORT_CONSUMER) $(LATE_IMPORT_PROVIDER)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(LATE_IMPORT_CONSUMER) --file $(LATE_IMPORT_PROVIDER)

$(LOAD_API_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(LOAD_API_CONSUMER) $(LATE_IMPORT_PROVIDER) $(LOAD_API_PROBE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(LOAD_API_CONSUMER) --file $(LATE_IMPORT_PROVIDER) --file $(LOAD_API_PROBE)

$(LOAD_PASS2_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(LATE_IMPORT_CONSUMER) $(LATE_IMPORT_PROVIDER) $(LOAD_PASS2_PROBE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(LATE_IMPORT_CONSUMER) --file $(LATE_IMPORT_PROVIDER) --file $(LOAD_PASS2_PROBE)

$(COMPILER_DEFINES_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(COMPILER_DEFINES_PROBE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(COMPILER_DEFINES_PROBE)

$(STRING_DEFINES_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(STRING_DEFINES_PROBE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(STRING_DEFINES_PROBE)

$(SYSTEM_DEFINES_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(SYSTEM_DEFINES_PROBE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(SYSTEM_DEFINES_PROBE)

$(COLOR_CONVERSIONS_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(COLOR_CONVERSIONS_PROBE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(COLOR_CONVERSIONS_PROBE)

$(FORMAT_MISSING_ARGS_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(FORMAT_MISSING_ARGS_PROBE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(FORMAT_MISSING_ARGS_PROBE)

$(INTERNAL_TYPES_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(INTERNAL_TYPES_PROBE) $(FILE_WRITE_PROBE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(INTERNAL_TYPES_PROBE) --file $(FILE_WRITE_PROBE)

$(CLASS_RECORDS_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(CLASS_RECORDS_PROBE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(CLASS_RECORDS_PROBE)

$(LIST_UTILITIES_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(LIST_UTILITIES_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(LIST_UTILITIES_PROBE)

$(STORAGE_SERVICES_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(STORAGE_SERVICES_PROBES) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home $(addprefix --file ,$(STORAGE_SERVICES_PROBES))

$(CLUSTER_ENTRY_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(CLUSTER_ENTRY_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(CLUSTER_ENTRY_PROBE)

$(FAT_CLUSTER_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(FAT_CLUSTER_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(FAT_CLUSTER_PROBE)

$(DISK_CACHE_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(DISK_CACHE_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(DISK_CACHE_PROBE)

$(FAT_SECTOR_CACHE_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(FAT_SECTOR_CACHE_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home $(addprefix --file ,$(FAT_SECTOR_CACHE_PROBE))

$(STORAGE_LIFECYCLE_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(STORAGE_LIFECYCLE_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home $(addprefix --file ,$(STORAGE_LIFECYCLE_PROBE))

$(FILE_BACKED_REDSEA_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(FILE_BACKED_REDSEA_PROBES) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home $(addprefix --file ,$(FILE_BACKED_REDSEA_PROBES))

$(OPTIMIZER_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(OPTIMIZER_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(OPTIMIZER_PROBE)

$(OPTIMIZER_AOT_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(OPTIMIZER_PROBE) $(OPTIMIZER_AOT_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(OPTIMIZER_PROBE) --file $(OPTIMIZER_AOT_PROBE)

$(REGISTER_ALLOCATION_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(REGISTER_ALLOCATION_PROBES) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home $(addprefix --file ,$(REGISTER_ALLOCATION_PROBES))

$(REGISTER_ALLOCATION_AOT_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(firstword $(REGISTER_ALLOCATION_PROBES)) $(REGISTER_ALLOCATION_AOT_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(firstword $(REGISTER_ALLOCATION_PROBES)) --file $(REGISTER_ALLOCATION_AOT_PROBE)

$(ADDRESS_FOLD_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(ADDRESS_FOLD_PROBES) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home $(addprefix --file ,$(ADDRESS_FOLD_PROBES))

$(ADDRESS_FOLD_AOT_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(firstword $(ADDRESS_FOLD_PROBES)) $(ADDRESS_FOLD_AOT_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(firstword $(ADDRESS_FOLD_PROBES)) --file $(ADDRESS_FOLD_AOT_PROBE)

$(BACKEND_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(BACKEND_PROBES) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home $(addprefix --file ,$(BACKEND_PROBES))

$(BACKEND_AOT_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(firstword $(BACKEND_PROBES)) $(BACKEND_AOT_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(firstword $(BACKEND_PROBES)) --file $(BACKEND_AOT_PROBE)

$(ADAM_MATH_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(ADAM_MATH_SOURCE) $(ADAM_MATH_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(ADAM_MATH_PROBE)

$(ADAM_MATH_AOT_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(ADAM_MATH_SOURCE) $(ADAM_MATH_PROBE) $(ADAM_MATH_AOT_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(ADAM_MATH_PROBE) --file $(ADAM_MATH_AOT_PROBE)

$(ADAM_MEM_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(ADAM_MEM_SOURCE) $(ADAM_MEM_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(ADAM_MEM_PROBE)

$(ADAM_MEM_AOT_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(ADAM_MEM_SOURCE) $(ADAM_MEM_PROBE) $(ADAM_MEM_AOT_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(ADAM_MEM_PROBE) --file $(ADAM_MEM_AOT_PROBE)

$(ADAM_TASK_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(ADAM_MEM_SOURCE) $(ADAM_TASK_SOURCE) $(ADAM_TASK_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(ADAM_TASK_PROBE)

$(ADAM_TASK_AOT_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(ADAM_MEM_SOURCE) $(ADAM_TASK_SOURCE) $(ADAM_TASK_PROBE) $(ADAM_TASK_AOT_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(ADAM_TASK_PROBE) --file $(ADAM_TASK_AOT_PROBE)

$(ADAM_STARTUP_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(ADAM_STARTUP_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(ADAM_STARTUP_PROBE)

$(DOC_TREE_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(DOC_TREE_PROBE) $(DOC_TREE_AOT_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(DOC_TREE_PROBE) --file $(DOC_TREE_AOT_PROBE)

$(DEV_INFO_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(PCI_DEV_FILE) $(DEV_INFO_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --compress-at $(PCI_DEV_FILE).Z $(PCI_DEV_FILE) --dir Home --file $(DEV_INFO_PROBE)

$(CPU_REP_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(CPU_REP_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(CPU_REP_PROBE)

$(UASM_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(UASM_PROBES) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home $(addprefix --file ,$(UASM_PROBES))

$(UASM_AOT_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(UASM_PROBES) $(UASM_AOT_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file Linux/Fixtures/UAsmProbe.HC --file $(UASM_AOT_PROBE)

$(NESTED_REDSEA_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(NESTED_REDSEA_PROBES) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home $(addprefix --file ,$(NESTED_REDSEA_PROBES))

$(FAT32_IMAGE_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(FAT32_IMAGE_PROBE) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(FAT32_IMAGE_PROBE)

$(OPTICAL_IMAGE_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(OPTICAL_IMAGE_PROBE) $(FILE_BACKED_REDSEA_TEST_ISO) $(COMPILER_RESOURCE) $(COMPILER_MODULE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(OPTICAL_IMAGE_PROBE) --file-at Embedded.ISO.C $(FILE_BACKED_REDSEA_TEST_ISO)

$(FAT_WRITE_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(FAT_WRITE_PROBE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(FAT_WRITE_PROBE)

$(FAT_FORMAT_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(FAT_FORMAT_PROBE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(FAT_FORMAT_PROBE)

$(FAT_STREAM_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(FAT_STREAM_PROBE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(FAT_STREAM_PROBE)

$(FAT_TREE_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(FAT_TREE_PROBE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(FAT_TREE_PROBE)

$(FAT_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(FAT_PROBES)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home $(addprefix --file ,$(FAT_PROBES))

$(ATA_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(FAT_PROBES) $(ATA_PROBES)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home $(addprefix --file ,$(FAT_PROBES) $(ATA_PROBES))

$(BREAK_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(BREAK_PROBE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(BREAK_PROBE)

$(MATH_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(MATH_PROBE) $(FILE_WRITE_PROBE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(MATH_PROBE) --file $(FILE_WRITE_PROBE)

$(MUSIC_PRECISION_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(MUSIC_PRECISION_PROBE) $(FILE_WRITE_PROBE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(MUSIC_PRECISION_PROBE) --file $(FILE_WRITE_PROBE)

$(STRINGS_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(STRINGS_PROBES) $(FILE_WRITE_PROBE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home $(addprefix --file ,$(STRINGS_PROBES) $(FILE_WRITE_PROBE))

$(ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(PCI_DEV_FILE) $(GRAPHICS_DEMOS) $(GAME_DEMOS) $(CLASS_META_DEMO) $(APP_FILES) $(FORM_DEMOS) $(SOUND_DEMOS)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --compress-at $(PCI_DEV_FILE).Z $(PCI_DEV_FILE) --dir Home \
		$(foreach demo,$(GRAPHICS_DEMOS) $(GAME_DEMOS) $(CLASS_META_DEMO) $(APP_FILES) $(FORM_DEMOS) $(SOUND_DEMOS),--file-at $(demo) $(demo))

$(CHARACTER_GRAPHICS_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_DOC) $(COMPILER_RESOURCE) $(COMPILER_MODULE) $(GAME_DEMOS) $(CHARACTER_GRAPHICS_PROBE)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home --file $(CHARACTER_GRAPHICS_PROBE) \
		$(foreach demo,$(GAME_DEMOS),--file-at $(demo) $(demo))

TEST_MODULES := $(MODULE_PROBE) $(MODULE_CALLER) $(RUNTIME_PROBE) $(FILE_READ_PROBE) \
	$(FILE_WRITE_PROBE) $(EXCEPT_PROBE) $(FORMAT_PROBE) $(HASH_CALLER) $(HASH_PROBE) \
	$(DOC_PROBE) $(COMPILER_RUNTIME_PROBE)

STARTUP_FILES := $(TEST_MODULES) $(LEX_PROBES) $(DOC_SOURCE_PROBES) $(CLASS_META_DEMO) Linux/Fixtures/ClassReportProbe.HC Linux/Fixtures/LastClassRun.HC \
	Linux/Fixtures/FloodFillProbe.HC Linux/Fixtures/SpriteFloodProbe.HC Linux/Fixtures/FifoProbe.HC $(MATH_PROBE) $(MUSIC_PRECISION_PROBE) $(STRINGS_PROBES) \
	$(PRINT_SOURCE_PROBES) $(STREAM_PROBES) $(ISO_PROBE) $(MEMORY_PROBES) $(CMP_PROBES) $(TIME_CLOCK_PROBE)

$(STARTUP_TEST_ISO): $(LOADER) $(KERNEL) $(ISO_TOOL) $(STARTUP_FIXTURE) $(STARTUP_FILES) $(STARTUP_DOC) $(DOC_BIN_DEMOS) $(DOC_BIN_LINK) $(GRAPHICS_DEMOS) $(GAME_DEMOS) $(QUEUE_PROBE) $(APP_FILES) $(FORM_DEMOS) $(SOUND_DEMOS)
	$(ISO_TOOL) $(LOADER) $(KERNEL) $@ $(STARTUP_FIXTURE) $(COMPILER_ISO_ARGS) --compress-at $(STARTUP_DOC).Z $(STARTUP_DOC) --dir Home $(addprefix --file ,$(STARTUP_FILES)) \
			$(foreach demo,$(DOC_BIN_DEMOS) $(GRAPHICS_DEMOS) $(GAME_DEMOS) $(CLASS_META_DEMO) $(APP_FILES) $(FORM_DEMOS) $(SOUND_DEMOS),--file-at $(demo) $(demo)) --file-at Demo/Graphics/DocBinLink.DD $(DOC_BIN_LINK) --file-at Doc/QueueProbe.HC $(QUEUE_PROBE)

test-host-modules: $(HOST_MODULE_TEST_ISO) $(BOOT_TEST) $(MAKE_BIN_TEST)
	$(MAKE_BIN_TEST) $(BIN_TOOL) $(HOST_MODULE_OBJECT) $(HOST_MODULE_BINDINGS) $(BUILD)/make-bin-test
	$(BOOT_TEST) --host-module "$(QEMU)" $(HOST_MODULE_TEST_ISO) $(HOST_MODULE)

test-native-imports: $(NATIVE_IMPORT_TEST_ISO) $(NATIVE_IMPORT_REFERENCE) $(BOOT_TEST)
	$(BOOT_TEST) --native-import "$(QEMU)" $(NATIVE_IMPORT_TEST_ISO) $(NATIVE_IMPORT)

test-late-imports: $(LATE_IMPORT_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --late-import "$(QEMU)" $(LATE_IMPORT_TEST_ISO) $(LATE_IMPORT_CONSUMER) $(LATE_IMPORT_PROVIDER)

test-load-api: $(LOAD_API_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --load-api "$(QEMU)" $(LOAD_API_TEST_ISO) $(LOAD_API_CONSUMER) $(LATE_IMPORT_PROVIDER)

test-load-pass2: $(LOAD_PASS2_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --load-pass2 "$(QEMU)" $(LOAD_PASS2_TEST_ISO) $(LATE_IMPORT_CONSUMER) $(LATE_IMPORT_PROVIDER)

test-compiler-defines: $(COMPILER_DEFINES_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --compiler-defines "$(QEMU)" $(COMPILER_DEFINES_TEST_ISO)

test-string-defines: $(STRING_DEFINES_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --string-defines "$(QEMU)" $(STRING_DEFINES_TEST_ISO)

test-system-defines: $(SYSTEM_DEFINES_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --system-defines "$(QEMU)" $(SYSTEM_DEFINES_TEST_ISO)

test-color-conversions: $(COLOR_CONVERSIONS_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --color-conversions "$(QEMU)" $(COLOR_CONVERSIONS_TEST_ISO)

test-format-missing-args: $(FORMAT_MISSING_ARGS_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --format-missing-args "$(QEMU)" $(FORMAT_MISSING_ARGS_TEST_ISO)

test-internal-types: $(INTERNAL_TYPES_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --internal-types "$(QEMU)" $(INTERNAL_TYPES_TEST_ISO)

test-class-records: $(CLASS_RECORDS_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --class-records "$(QEMU)" $(CLASS_RECORDS_TEST_ISO)

.PHONY: test-list-utilities
test-list-utilities: $(LIST_UTILITIES_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --list-utilities "$(QEMU)" $(LIST_UTILITIES_TEST_ISO)

.PHONY: test-storage-services
test-storage-services: $(STORAGE_SERVICES_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --storage-services "$(QEMU)" $(STORAGE_SERVICES_TEST_ISO)

test: test-list-utilities test-storage-services

.PHONY: test-cluster-entry
test-cluster-entry: $(CLUSTER_ENTRY_TEST_ISO) $(FAT_CLUSTER_TEST_ISO) $(FAT_WRITE_DISK) $(BOOT_TEST)
	$(BOOT_TEST) --cluster-entry "$(QEMU)" $(CLUSTER_ENTRY_TEST_ISO)
	$(BOOT_TEST) --fat32-cluster "$(QEMU)" $(FAT_CLUSTER_TEST_ISO) $(FAT_WRITE_DISK)

test: test-cluster-entry

.PHONY: test-disk-cache
test-disk-cache: $(DISK_CACHE_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --disk-cache "$(QEMU)" $(DISK_CACHE_TEST_ISO)

test: test-disk-cache

.PHONY: test-fat-sector-cache
test-fat-sector-cache: $(FAT_SECTOR_CACHE_TEST_ISO) $(FAT_WRITE_DISK) $(BOOT_TEST)
	$(BOOT_TEST) --fat-sector-cache "$(QEMU)" $(FAT_SECTOR_CACHE_TEST_ISO) $(FAT_WRITE_DISK)

test: test-fat-sector-cache

.PHONY: test-storage-lifecycle
test-storage-lifecycle: $(STORAGE_LIFECYCLE_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --storage-lifecycle "$(QEMU)" $(STORAGE_LIFECYCLE_TEST_ISO)

test: test-storage-lifecycle

.PHONY: test-file-backed-redsea
test-file-backed-redsea: $(FILE_BACKED_REDSEA_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --file-backed-redsea "$(QEMU)" $(FILE_BACKED_REDSEA_TEST_ISO)

test: test-file-backed-redsea

.PHONY: test-optimizer
test-optimizer: $(OPTIMIZER_TEST_ISO) $(OPTIMIZER_AOT_TEST_ISO) $(BOOT_TEST) test-opt-pass012
	$(BOOT_TEST) --optimizer "$(QEMU)" $(OPTIMIZER_TEST_ISO)
	$(BOOT_TEST) --optimizer-aot "$(QEMU)" $(OPTIMIZER_AOT_TEST_ISO)

test: test-optimizer

.PHONY: test-register-allocation
test-register-allocation: $(REGISTER_ALLOCATION_TEST_ISO) $(REGISTER_ALLOCATION_AOT_TEST_ISO) $(BOOT_TEST) test-opt-pass3
	$(BOOT_TEST) --register-allocation "$(QEMU)" $(REGISTER_ALLOCATION_TEST_ISO)
	$(BOOT_TEST) --register-allocation-aot "$(QEMU)" $(REGISTER_ALLOCATION_AOT_TEST_ISO)

test: test-register-allocation

.PHONY: test-address-fold
test-address-fold: $(ADDRESS_FOLD_TEST_ISO) $(ADDRESS_FOLD_AOT_TEST_ISO) $(BOOT_TEST) test-operand-lowering
	$(BOOT_TEST) --address-fold "$(QEMU)" $(ADDRESS_FOLD_TEST_ISO)
	$(BOOT_TEST) --address-fold-aot "$(QEMU)" $(ADDRESS_FOLD_AOT_TEST_ISO)

test: test-address-fold

.PHONY: test-backend
test-backend: $(BACKEND_TEST_ISO) $(BACKEND_AOT_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --backend "$(QEMU)" $(BACKEND_TEST_ISO)
	$(BOOT_TEST) --backend-aot "$(QEMU)" $(BACKEND_AOT_TEST_ISO)

test: test-backend

.PHONY: test-adam-math
test-adam-math: $(ADAM_MATH_TEST_ISO) $(ADAM_MATH_AOT_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --adam-math "$(QEMU)" $(ADAM_MATH_TEST_ISO)
	$(BOOT_TEST) --adam-math-aot "$(QEMU)" $(ADAM_MATH_AOT_TEST_ISO)

test: test-adam-math

.PHONY: test-adam-mem
test-adam-mem: $(ADAM_MEM_TEST_ISO) $(ADAM_MEM_AOT_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --adam-mem "$(QEMU)" $(ADAM_MEM_TEST_ISO)
	$(BOOT_TEST) --adam-mem-aot "$(QEMU)" $(ADAM_MEM_AOT_TEST_ISO)
	$(BOOT_TEST) --adam-mem-large "$(QEMU)" $(ADAM_MEM_TEST_ISO)
	$(BOOT_TEST) --adam-mem-large-aot "$(QEMU)" $(ADAM_MEM_AOT_TEST_ISO)

test: test-adam-mem

.PHONY: test-adam-task
test-adam-task: $(ADAM_TASK_TEST_ISO) $(ADAM_TASK_AOT_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --adam-task "$(QEMU)" $(ADAM_TASK_TEST_ISO)
	$(BOOT_TEST) --adam-task-aot "$(QEMU)" $(ADAM_TASK_AOT_TEST_ISO)

test: test-adam-task

.PHONY: test-adam-startup
test-adam-startup: $(ADAM_STARTUP_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --adam-startup "$(QEMU)" $(ADAM_STARTUP_TEST_ISO)

test: test-adam-startup

.PHONY: test-doc-tree
test-doc-tree: $(DOC_TREE_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --doc-tree "$(QEMU)" $(DOC_TREE_TEST_ISO)
	$(BOOT_TEST) --doc-tree-aot "$(QEMU)" $(DOC_TREE_TEST_ISO)

test: test-doc-tree

.PHONY: test-dev-info
test-dev-info: $(DEV_INFO_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --dev-info "$(QEMU)" $(DEV_INFO_TEST_ISO)

test: test-dev-info

.PHONY: test-cpu-rep
test-cpu-rep: $(CPU_REP_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --cpu-rep "$(QEMU)" $(CPU_REP_TEST_ISO)

test: test-cpu-rep

.PHONY: test-uasm
test-uasm: $(UASM_TEST_ISO) $(UASM_AOT_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --uasm "$(QEMU)" $(UASM_TEST_ISO)
	$(BOOT_TEST) --uasm-aot "$(QEMU)" $(UASM_AOT_TEST_ISO)

test: test-uasm

.PHONY: test-nested-redsea test-fat32-image test-optical-image
test-nested-redsea: $(NESTED_REDSEA_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --nested-redsea "$(QEMU)" $(NESTED_REDSEA_TEST_ISO)

test-fat32-image: $(FAT32_IMAGE_TEST_ISO) $(FAT_WRITE_DISK) $(BOOT_TEST)
	$(BOOT_TEST) --fat32-image "$(QEMU)" $(FAT32_IMAGE_TEST_ISO) $(FAT_WRITE_DISK)

test-optical-image: $(OPTICAL_IMAGE_TEST_ISO) $(BOOT_TEST)
	$(BOOT_TEST) --optical-image "$(QEMU)" $(OPTICAL_IMAGE_TEST_ISO)

test: test-nested-redsea test-fat32-image test-optical-image

run: $(ISO)
	$(QEMU) -machine pc -cpu qemu64 -m 512 -boot d -cdrom $< -serial stdio

test: $(ISO) $(BOOT_TEST) $(COMPRESS_TEST) $(STR_COMPARE_TEST) $(STARTUP_TEST_ISO) $(FAT_TEST_ISO) $(FAT_DISK) $(FAT_INVALID_DISK) $(ATA_TEST_ISO) $(FAT_WRITE_DISK) $(FAT_WRITE_ISO) $(FAT_STREAM_ISO) $(FAT_TREE_ISO) $(FAT_FORMAT_ISO) $(BREAK_TEST_ISO) $(CHARACTER_GRAPHICS_TEST_ISO) test-host-modules test-native-imports test-late-imports test-load-api test-load-pass2 test-compiler-defines test-string-defines test-system-defines test-color-conversions test-format-missing-args test-internal-types test-class-records
	$(COMPRESS_TEST) $(STARTUP_FIXTURE) Linux/Fixtures/KDbg.HC.Z
	$(STR_COMPARE_TEST)
	$(BOOT_TEST) --compiler-failures "$(QEMU)" $(ISO) $(KERNEL)
	$(BOOT_TEST) --fat32 "$(QEMU)" $(FAT_TEST_ISO) $(FAT_DISK) valid
	$(BOOT_TEST) --fat32 "$(QEMU)" $(FAT_TEST_ISO) $(FAT_INVALID_DISK) invalid
	$(BOOT_TEST) --ata-write "$(QEMU)" $(ATA_TEST_ISO) $(FAT_DISK)
	$(BOOT_TEST) --fat32-write "$(QEMU)" $(FAT_WRITE_ISO) $(FAT_WRITE_DISK)
	$(BOOT_TEST) --fat32-format "$(QEMU)" $(FAT_FORMAT_ISO) $(FAT_WRITE_DISK)
	$(BOOT_TEST) --fat32-stream "$(QEMU)" $(FAT_STREAM_ISO) $(FAT_WRITE_DISK)
	$(BOOT_TEST) --fat32-tree "$(QEMU)" $(FAT_TREE_ISO) $(FAT_WRITE_DISK)
	$(BOOT_TEST) "$(QEMU)" $< $(KERNEL) $(STARTUP_TEST_ISO)
	$(BOOT_TEST) --character-graphics "$(QEMU)" $(CHARACTER_GRAPHICS_TEST_ISO)
	$(BOOT_TEST) --music "$(QEMU)" $<
	$(BOOT_TEST) --tic-tac-toe "$(QEMU)" $<
	$(BOOT_TEST) --screen-capture "$(QEMU)" $<
	$(BOOT_TEST) --spline "$(QEMU)" $<
	$(BOOT_TEST) --blot "$(QEMU)" $<
	$(BOOT_TEST) --break "$(QEMU)" $(BREAK_TEST_ISO)
	$(BOOT_TEST) --database "$(QEMU)" $<
	$(BOOT_TEST) --database-persistent "$(QEMU)" $< $(FAT_WRITE_DISK)

-include $(CXX_OBJ:.o=.d) $(COMPILER_CXX_OBJ:.o=.d) $(HOST_MODULE_CPP:.o=.d) $(NATIVE_IMPORT_CPP:.o=.d)
