GLOBAL CPUStructInit,TaskInit,SET_GS_BASE,SET_FS_BASE,CallModuleMain,CallHolyC0,CallHolyC2
GLOBAL _MEMCPY,_MEMSET,_MEMSET_I64,_BEQU,_CALL,_CLAMP_I64
GLOBAL _MALLOC,_FREE,_MSIZE,_MSIZE2,_MHEAP_CTRL,CAllocHC,MAllocIdentHC
GLOBAL StrNewHC,AStrNewHC,_STRCPY,_STRCMP,_STRIMATCH,StrOccHC
GLOBAL _HASH_ADD,_HASH_ADD_AFTER,_HASH_FIND,_HASH_SINGLE_TABLE_FIND
GLOBAL _HASH_REM_DEL,HashValHC,HashDelHC,HashTableDelHC,HashSrcFileSetHC,SysSymImportsResolveHC
GLOBAL HashTableNewHC,HashTypeNumHC,CeilU64HC,Pow10I64HC
GLOBAL LinkedLstDelHC,LinkedLstSizeHC,QueDelHC
GLOBAL QueVectU8NewHC,QueVectU8PutHC,QueVectU8GetHC,QueVectU8DelHC
GLOBAL QSortHC,QSortI64HC,LstMatchHC
GLOBAL FileReadHC,FileNameAbsHC,DirNameAbsHC,DirCurHC,DirFileHC,ExtChgHC,ExtDftHC
GLOBAL FileWriteHC,BlkDevNextFreeSlotHC,BlkDevAddHC,Let2DrvHC,BlkReadHC,BlkWriteHC
GLOBAL RedSeaISOHC
GLOBAL SysUntryHC,PutExceptHC
GLOBAL StrPrintJoinHC,StrPrintHC,CatPrintHC,MStrPrintHC,PrintHC,PrintErrHC,PrintWarnHC
GLOBAL PutSHC,PutHexHC
GLOBAL ExePutSHC,ExeFileHC,StreamPrintHC,StreamExePrintHC
GLOBAL PutCharsHC,IsRawHC,DefineLstLoadHC,DefineSubHC,DefineMatchHC
GLOBAL DocNewHC,DocDelHC,DocLoadHC,DocSizeHC,DocSaveHC,DocWriteHC
GLOBAL DocPrintHC,Doc2PlainTextHC,DocBinsValidateHC,DocPutLineHC
GLOBAL RawHC,DbgModeHC,IsDbgModeHC,PutFileLinkHC,PutSrcLinkHC,FixSetHC,AdamErrHC
GLOBAL tSHC,BusyHC,SleepUntilHC,SleepHC,Ona2FreqHC,Freq2OnaHC,SndHC
GLOBAL ScrnCastHC,SndRstHC,BeepHC,MuteHC,IsMuteHC
EXTERN CPUStructInitCpp,TaskInitCpp
EXTERN MAllocCpp,FreeCpp,MSizeCpp,MSize2Cpp,MHeapCtrlCpp,CAllocCpp,MAllocIdentCpp
EXTERN StrNewCpp,AStrNewCpp,StrCpyCpp,StrCmpCpp,StrIMatchCpp,StrOccCpp
EXTERN HashAddCpp,HashAddAfterCpp,HashFindCpp,HashSingleTableFindCpp
EXTERN HashRemDelCpp,HashValCpp,HashDelCpp,HashTableDelCpp,HashSrcFileSetCpp,SysSymImportsResolveCpp
EXTERN HashTableNewCpp,HashTypeNumCpp,CeilU64Cpp,Pow10I64Cpp
EXTERN LinkedLstDelCpp,LinkedLstSizeCpp,QueDelCpp
EXTERN QueVectU8NewCpp,QueVectU8PutCpp,QueVectU8GetCpp,QueVectU8DelCpp
EXTERN QSortCpp,QSortI64Cpp,LstMatchCpp
EXTERN FileReadCpp,FileNameAbsCpp,DirNameAbsCpp,DirCurCpp,DirFileCpp,ExtChgCpp,ExtDftCpp
EXTERN FileWriteCpp,BlkDevNextFreeSlotCpp,BlkDevAddCpp,Let2DrvCpp,BlkReadCpp,BlkWriteCpp
EXTERN RedSeaISOCpp
EXTERN SysUntryCpp,PutExceptCpp
EXTERN StrPrintJoin,CatPrintCpp,MStrPrintCpp,PrintCpp,PrintErrCpp,PrintWarnCpp
GLOBAL CmpHC
EXTERN CmpCpp,ExePutSCpp,ExeFileCpp,StreamPrintCpp,StreamExePrintCpp
EXTERN PutCharsCpp,PutS,PutHexCpp,IsRawCpp,DefineLstLoadCpp,DefineSubCpp,DefineMatchCpp
EXTERN DocNewCpp,DocDelCpp,DocLoadCpp,DocSizeCpp,DocSaveCpp,DocWriteCpp
EXTERN DocPrintCpp,Doc2PlainTextCpp,DocBinsValidateCpp,DocPutLineCpp
EXTERN RawCpp,DbgModeCpp,IsDbgModeCpp,PutFileLinkCpp,PutSrcLinkCpp,FixSetCpp,AdamErrCpp
EXTERN tSCpp,BusyCpp,SleepUntilCpp,SleepCpp,Ona2FreqCpp,Freq2OnaCpp,SndCpp
EXTERN ScrnCastCpp,SndRstCpp,BeepCpp,MuteCpp,IsMuteCpp

SECTION .text
BITS 64

; Adapt HolyC stack arguments and callee cleanup to C++.
%macro HC_FRAME_ENTER 0
	PUSH	RBP
	MOV	RBP,RSP
	PUSH	RSI
	PUSH	RDI
	PUSH	R10
	PUSH	R11
%endmacro

%macro HC_FRAME_CALL 1
	AND	RSP,-16
	CALL	%1
	LEA	RSP,[RBP-32]
	POP	R11
	POP	R10
	POP	RDI
	POP	RSI
	POP	RBP
%endmacro

%macro HOLYC_CPP 3
%1:
	HC_FRAME_ENTER
%if %3>=1
	MOV	RDI,[RBP+16]
%endif
%if %3>=2
	MOV	RSI,[RBP+24]
%endif
%if %3>=3
	MOV	RDX,[RBP+32]
%endif
%if %3>=4
	MOV	RCX,[RBP+40]
%endif
%if %3>=5
	MOV	R8,[RBP+48]
%endif
	HC_FRAME_CALL %2
	RET	%3*8
%endmacro

; The caller pops fixed arguments, argc, and the inline argv for variadics.
%macro HOLYC_PRINT 3-4 0
%1:
	HC_FRAME_ENTER
	MOV	RDI,[RBP+16]
	MOV	RSI,[RBP+24]
%if %3=2
	MOV	RDX,[RBP+32]
	LEA	RCX,[RBP+40]
%else
	LEA	RDX,[RBP+32]
%if %4
	MOV	RCX,RBP
%endif
%endif
	HC_FRAME_CALL %2
	RET
%endmacro

HOLYC_CPP CPUStructInit,CPUStructInitCpp,3
HOLYC_CPP TaskInit,TaskInitCpp,2
HOLYC_CPP _MALLOC,MAllocCpp,2
HOLYC_CPP _FREE,FreeCpp,1
HOLYC_CPP _MSIZE,MSizeCpp,1
HOLYC_CPP _MSIZE2,MSize2Cpp,1
HOLYC_CPP _MHEAP_CTRL,MHeapCtrlCpp,1
HOLYC_CPP CAllocHC,CAllocCpp,2
HOLYC_CPP MAllocIdentHC,MAllocIdentCpp,2
HOLYC_CPP StrNewHC,StrNewCpp,2
HOLYC_CPP AStrNewHC,AStrNewCpp,1
HOLYC_CPP _STRCPY,StrCpyCpp,2
HOLYC_CPP _STRCMP,StrCmpCpp,2
HOLYC_CPP _STRIMATCH,StrIMatchCpp,2
HOLYC_CPP StrOccHC,StrOccCpp,2
HOLYC_CPP _HASH_ADD,HashAddCpp,2
HOLYC_CPP _HASH_ADD_AFTER,HashAddAfterCpp,3
HOLYC_CPP _HASH_FIND,HashFindCpp,4
HOLYC_CPP _HASH_SINGLE_TABLE_FIND,HashSingleTableFindCpp,4
HOLYC_CPP _HASH_REM_DEL,HashRemDelCpp,3
HOLYC_CPP HashValHC,HashValCpp,1
HOLYC_CPP HashDelHC,HashDelCpp,1
HOLYC_CPP HashTableDelHC,HashTableDelCpp,1
HOLYC_CPP HashSrcFileSetHC,HashSrcFileSetCpp,3
HOLYC_CPP SysSymImportsResolveHC,SysSymImportsResolveCpp,2
HOLYC_CPP HashTableNewHC,HashTableNewCpp,2
HOLYC_CPP HashTypeNumHC,HashTypeNumCpp,1
HOLYC_CPP CeilU64HC,CeilU64Cpp,2
HOLYC_CPP Pow10I64HC,Pow10I64Cpp,1
HOLYC_CPP LinkedLstDelHC,LinkedLstDelCpp,1
HOLYC_CPP LinkedLstSizeHC,LinkedLstSizeCpp,1
HOLYC_CPP QueDelHC,QueDelCpp,2
HOLYC_CPP QueVectU8NewHC,QueVectU8NewCpp,1
HOLYC_CPP QueVectU8PutHC,QueVectU8PutCpp,3
HOLYC_CPP QueVectU8GetHC,QueVectU8GetCpp,2
HOLYC_CPP QueVectU8DelHC,QueVectU8DelCpp,1
HOLYC_CPP QSortHC,QSortCpp,4
HOLYC_CPP QSortI64HC,QSortI64Cpp,3
HOLYC_CPP LstMatchHC,LstMatchCpp,3
HOLYC_CPP FileReadHC,FileReadCpp,3
HOLYC_CPP FileNameAbsHC,FileNameAbsCpp,2
HOLYC_CPP DirNameAbsHC,DirNameAbsCpp,1
HOLYC_CPP DirCurHC,DirCurCpp,2
HOLYC_CPP DirFileHC,DirFileCpp,3
HOLYC_CPP ExtChgHC,ExtChgCpp,2
HOLYC_CPP ExtDftHC,ExtDftCpp,2
HOLYC_CPP FileWriteHC,FileWriteCpp,5
HOLYC_CPP RedSeaISOHC,RedSeaISOCpp,3
HOLYC_CPP BlkDevNextFreeSlotHC,BlkDevNextFreeSlotCpp,2
HOLYC_CPP BlkDevAddHC,BlkDevAddCpp,4
HOLYC_CPP Let2DrvHC,Let2DrvCpp,2
HOLYC_CPP BlkReadHC,BlkReadCpp,4
HOLYC_CPP BlkWriteHC,BlkWriteCpp,4
HOLYC_CPP SysUntryHC,SysUntryCpp,0
HOLYC_CPP PutExceptHC,PutExceptCpp,1
HOLYC_CPP StrPrintJoinHC,StrPrintJoin,4
HOLYC_PRINT StrPrintHC,StrPrintJoin,2
HOLYC_PRINT CatPrintHC,CatPrintCpp,2
HOLYC_PRINT MStrPrintHC,MStrPrintCpp,1
HOLYC_PRINT PrintHC,PrintCpp,1
HOLYC_PRINT PrintErrHC,PrintErrCpp,1,1
HOLYC_PRINT PrintWarnHC,PrintWarnCpp,1,1
HOLYC_CPP PutHexHC,PutHexCpp,2
HOLYC_CPP PutCharsHC,PutCharsCpp,1
HOLYC_CPP PutSHC,PutS,1
HOLYC_CPP ExePutSHC,ExePutSCpp,4
HOLYC_CPP CmpHC,CmpCpp,4
HOLYC_CPP ExeFileHC,ExeFileCpp,2
HOLYC_PRINT StreamPrintHC,StreamPrintCpp,1,1
HOLYC_PRINT StreamExePrintHC,StreamExePrintCpp,1,1
HOLYC_CPP IsRawHC,IsRawCpp,0
HOLYC_CPP DefineSubHC,DefineSubCpp,2
HOLYC_CPP DefineMatchHC,DefineMatchCpp,3
HOLYC_CPP DocNewHC,DocNewCpp,2
HOLYC_CPP DocDelHC,DocDelCpp,1
HOLYC_CPP DocLoadHC,DocLoadCpp,3
HOLYC_CPP DocSizeHC,DocSizeCpp,1
HOLYC_CPP DocSaveHC,DocSaveCpp,2
HOLYC_CPP DocWriteHC,DocWriteCpp,2
HOLYC_CPP Doc2PlainTextHC,Doc2PlainTextCpp,2
HOLYC_CPP DocBinsValidateHC,DocBinsValidateCpp,1
HOLYC_CPP DocPutLineHC,DocPutLineCpp,2
HOLYC_PRINT DocPrintHC,DocPrintCpp,2
HOLYC_CPP RawHC,RawCpp,1
HOLYC_CPP DbgModeHC,DbgModeCpp,1
HOLYC_CPP IsDbgModeHC,IsDbgModeCpp,0
HOLYC_CPP tSHC,tSCpp,0
HOLYC_CPP BusyHC,BusyCpp,1
HOLYC_CPP SleepUntilHC,SleepUntilCpp,1
HOLYC_CPP SleepHC,SleepCpp,1
HOLYC_CPP Ona2FreqHC,Ona2FreqCpp,1
HOLYC_CPP Freq2OnaHC,Freq2OnaCpp,1
HOLYC_CPP SndHC,SndCpp,1
HOLYC_CPP ScrnCastHC,ScrnCastCpp,3
HOLYC_CPP SndRstHC,SndRstCpp,0
HOLYC_CPP BeepHC,BeepCpp,2
HOLYC_CPP MuteHC,MuteCpp,1
HOLYC_CPP IsMuteHC,IsMuteCpp,0
HOLYC_CPP PutFileLinkHC,PutFileLinkCpp,4
HOLYC_CPP PutSrcLinkHC,PutSrcLinkCpp,3
HOLYC_CPP FixSetHC,FixSetCpp,2
HOLYC_PRINT AdamErrHC,AdamErrCpp,1

DefineLstLoadHC:
	HC_FRAME_ENTER
	MOV	RDI,[RBP+16]
	MOV	RSI,[RBP+24]
	MOV	RDX,RBP
	HC_FRAME_CALL DefineLstLoadCpp
	RET	16

CallHolyC2:
	PUSH	RBX
	PUSH	RDX
	PUSH	RSI
	CALL	RDI
	CLD
	POP	RBX
	RET

CallModuleMain:
CallHolyC0:
	PUSH	RBP
	MOV	RBP,RSP
	PUSH	RBX
	PUSH	R12
	PUSH	R13
	PUSH	R14
	PUSH	R15
	SUB	RSP,8
	CALL	RDI
	CLD
	ADD	RSP,8
	POP	R15
	POP	R14
	POP	R13
	POP	R12
	POP	RBX
	POP	RBP
	RET

_MEMCPY:
	PUSH	RBP
	MOV	RBP,RSP
	PUSH	RSI
	PUSH	RDI
	CLD
	MOV	RDI,[RBP+16]
	MOV	RSI,[RBP+24]
	MOV	RCX,[RBP+32]
	REP MOVSB
	MOV	RAX,RDI
	POP	RDI
	POP	RSI
	POP	RBP
	RET	24

%macro MEMSET 2
%1:
	PUSH	RBP
	MOV	RBP,RSP
	PUSH	RDI
	CLD
	MOV	RDI,[RBP+16]
	MOV	RAX,[RBP+24]
	MOV	RCX,[RBP+32]
	REP %2
	MOV	RAX,RDI
	POP	RDI
	POP	RBP
	RET	24
%endmacro

MEMSET _MEMSET,STOSB
MEMSET _MEMSET_I64,STOSQ

_BEQU:
	PUSH	RBP
	MOV	RBP,RSP
	XOR	EAX,EAX
	MOV	RDX,[RBP+32]
	MOV	RBX,[RBP+16]
	MOV	RCX,[RBP+24]
	TEST	RDX,RDX
	JZ	.clear
	BTS	QWORD [RBX],RCX
	JMP	.old
.clear:
	BTR	QWORD [RBX],RCX
.old:
	ADC	AL,0
	POP	RBP
	RET	24

_CALL:
	PUSH	RBP
	MOV	RBP,RSP
	MOV	RAX,[RBP+16]
	TEST	RAX,RAX
	JZ	.done
	CALL	RAX
.done:
	POP	RBP
	RET	8

_CLAMP_I64:
	PUSH	RBP
	MOV	RBP,RSP
	MOV	RAX,[RBP+16]
	MOV	RCX,[RBP+24]
	MOV	RDX,[RBP+32]
	CMP	RAX,RCX
	CMOVL	RAX,RCX
	CMP	RAX,RDX
	CMOVG	RAX,RDX
	POP	RBP
	RET	24

SET_GS_BASE:
	PUSH	RCX
	MOV	ECX,0xC0000101
	JMP	SHORT SET_SEG_BASE
SET_FS_BASE:
	PUSH	RCX
	MOV	ECX,0xC0000100
SET_SEG_BASE:
	PUSH	RDX
	MOV	RDX,RAX
	SHR	RDX,32
	WRMSR
	POP	RDX
	POP	RCX
	RET

SECTION .note.GNU-stack noalloc noexec nowrite progbits
