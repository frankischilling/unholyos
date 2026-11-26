GLOBAL CPUStructInit,TaskInit,SET_GS_BASE,SET_FS_BASE,CallModuleMain,CallHolyC0,CallHolyC2
GLOBAL _MEMCPY,_MEMSET,_MEMSET_I64,_BEQU,_CALL,_CLAMP_I64
GLOBAL _MALLOC,_FREE,_MSIZE,_MSIZE2,_MHEAP_CTRL,CAllocHC,MAllocIdentHC
GLOBAL StrNewHC,AStrNewHC,_STRCPY,_STRCMP,_STRIMATCH,StrOccHC
GLOBAL Str2I64HC
GLOBAL _HASH_ADD,_HASH_ADD_AFTER,_HASH_FIND,_HASH_SINGLE_TABLE_FIND
GLOBAL _HASH_REM_DEL,HashValHC,HashDelHC,HashTableDelHC,HashSrcFileSetHC,SysSymImportsResolveHC
GLOBAL HashTableNewHC,HashTypeNumHC,CeilU64HC,Pow10I64HC
GLOBAL LinkedLstDelHC,LinkedLstSizeHC,QueDelHC
GLOBAL QueVectU8NewHC,QueVectU8PutHC,QueVectU8GetHC,QueVectU8DelHC
GLOBAL QSortHC,QSortI64HC,LstMatchHC
GLOBAL FileReadHC,FileNameAbsHC,DirNameAbsHC,DirCurHC,DirFileHC,ExtChgHC,ExtDftHC
GLOBAL CdHC,IsDirHC,DirHC,PutDirLinkHC,FilesFindHC,FileFindHC,FilesFindMatchHC
GLOBAL DirEntryDelHC,DirEntryDel2HC,DirTreeDelHC,DirTreeDel2HC
GLOBAL DirContextNewHC,DirContextDelHC
GLOBAL FileWriteHC,BlkDevNextFreeSlotHC,BlkDevAddHC,Let2DrvHC,BlkReadHC,BlkWriteHC
GLOBAL RedSeaISOHC
GLOBAL SysUntryHC,PutExceptHC,YorNHC
GLOBAL StrPrintJoinHC,StrPrintHC,CatPrintHC,MStrPrintHC,PrintHC,PrintErrHC,PrintWarnHC
GLOBAL PutSHC,PutHexHC
GLOBAL ExePutSHC,ExeFileHC,StreamPrintHC,StreamExePrintHC
GLOBAL PutCharsHC,IsRawHC,DefineLstLoadHC,DefineSubHC,DefineMatchHC
GLOBAL DocNewHC,DocDelHC,DocLoadHC,DocSizeHC,DocSaveHC,DocWriteHC
GLOBAL DocReadHC,DocTypeHC,TypeHC
GLOBAL DocPrintHC,Doc2PlainTextHC,DocBinsValidateHC,DocPutLineHC,DocPutKeyHC
GLOBAL DocPutHC,DocDisplayHC,DocBorderHC,DocRecalcHC,WinHorzHC,WinVertHC
GLOBAL WinRefocusHC,WinToTopHC,WinFocusHC,WinTileHorzHC,WinTileVertHC,WinMaxHC
GLOBAL DocLockHC,DocUnlockHC,DocEntryRunHC,DocBottomHC,DocClearHC,NowHC
GLOBAL DocTermNewHC,DocGetKeyHC,DocScanLineHC,GetStrHC,GetSHC,UserCmdLineHC
GLOBAL TextCharHC,TextLenStrHC,TextLenAttrHC,TextRectHC,TextBorderHC,WinZBufUpdateHC
GLOBAL RawHC,DbgModeHC,IsDbgModeHC,PutFileLinkHC,PutSrcLinkHC,FixSetHC,AdamErrHC
GLOBAL tSHC,BusyHC,SleepUntilHC,SleepHC,Ona2FreqHC,Freq2OnaHC,SndHC
GLOBAL ScrnCastHC,SndRstHC,BeepHC,MuteHC,IsMuteHC
GLOBAL SpawnHC,TaskValidateHC,SuspendHC,IsSuspendedHC,TaskQueInsHC,TaskQueRemHC
GLOBAL SingleUserHC,IsSingleUserHC
GLOBAL JobDelHC,TaskRstAwaitingMsgHC,TaskExeHC,TaskTextHC
GLOBAL TaskMsgHC,JobResScanHC,JobResGetHC,TaskWaitHC
GLOBAL PostMsgHC,PostMsgWaitHC,MsgHC,JobsHndlrHC
GLOBAL SrvTaskContHC,ScanMsgHC,FlushMsgsHC,GetMsgHC
GLOBAL InputFilterTaskHC
GLOBAL Char2ScanCodeHC,ScanCode2CharHC,KbdTypeMaticHC,PutKeyHC
GLOBAL GridInitHC,MsSetHC
GLOBAL GrPaletteColorSetHC,GrPaletteColorGetHC
GLOBAL GrPaletteGetHC,GrPaletteSetHC,PaletteSetStdHC,WinMgrTaskEntry
GLOBAL CallHolyC1,CallHolyC3,CallHolyC4
GLOBAL GrScaleZoomHC,DrawStdMsHC,WinFinalUpdateHC
GLOBAL SignI64HC,RandI16HC,RandU16HC,RandI32HC
GLOBAL RandU32HC,RandI64HC,RandU64HC,RandHC
GLOBAL SeedHC,IsPixCovered0HC,GrPlot0HC,GrPeek0HC
GLOBAL GrBlotHC,GrRectHC,GrRectBHC,GrClampHC
GLOBAL GrPlotHC,GrPeekHC,GrLineHC,ClipLineHC
GLOBAL GrFillCircleHC,GrPlot3BHC,GrPlot3HC,GrLine3HC,DCThickScaleHC
GLOBAL SpriteSizeHC,Sprite3HC,Sprite3BHC,DocSpriteHC,SpriteHC
GLOBAL GrBlot3HC,DCSymmetry3SetHC,DCReflectHC
GLOBAL GrHLineHC,GrVLineHC,GrFillTri0HC,GrFillPoly3HC,GrRect3HC
GLOBAL Mat4x4MulXYZHC,DCTransformHC,Mat4x4IdentEquHC,Mat4x4IdentNewHC
GLOBAL Mat4x4NormSqr65536HC,DCMat4x4SetHC,DCLightingHC,DCFillHC
GLOBAL DCClearHC,DCRstHC,DCExtentsInitHC,DCAliasHC
GLOBAL DCNewHC,DCDelHC,DCSizeHC,DCDepthBufRstHC
GLOBAL DCDepthBufAllocHC,DCCopyHC,DCMonoHC,DCColorChgHC
GLOBAL DCSaveHC,DCLoadHC,GRWriteHC,GRReadHC
GLOBAL Mat4x4EquHC,Mat4x4NewHC
GLOBAL Mat4x4MulMat4x4EquHC,Mat4x4MulMat4x4NewHC,Mat4x4RotXHC,Mat4x4RotYHC
GLOBAL Mat4x4RotZHC,Mat4x4ScaleHC,Mat4x4TranslationEquHC,Mat4x4TranslationAddHC
GLOBAL DCSymmetrySetHC,RefreshHC
GLOBAL ScanCharHC,ScanKeyHC,GetKeyHC,GetCharHC
EXTERN CPUStructInitCpp,TaskInitCpp
EXTERN SpawnCpp,TaskValidateCpp,SuspendCpp,IsSuspendedCpp,TaskQueInsCpp,TaskQueRemCpp
EXTERN SingleUserCpp,IsSingleUserCpp
EXTERN JobDelCpp,TaskRstAwaitingMsgCpp,TaskExeCpp,TaskTextCpp
EXTERN TaskMsgCpp,JobResScanCpp,JobResGetCpp,TaskWaitCpp
EXTERN PostMsgCpp,PostMsgWaitCpp,MsgCpp,JobsHndlrCpp
EXTERN SrvTaskContCpp,ScanMsgCpp,FlushMsgsCpp,GetMsgCpp
EXTERN InputFilterTaskCpp
EXTERN Char2ScanCodeCpp,ScanCode2CharCpp,KbdTypeMaticCpp,PutKeyCpp
EXTERN GridInitCpp,MsSetCpp
EXTERN GrPaletteColorSetCpp,GrPaletteColorGetCpp
EXTERN GrPaletteGetCpp,GrPaletteSetCpp,PaletteSetStdCpp,WinMgrTask
EXTERN GrScaleZoomCpp,DrawStdMsCpp,WinFinalUpdateCpp
EXTERN SignI64Cpp,RandI16Cpp,RandU16Cpp,RandI32Cpp
EXTERN RandU32Cpp,RandI64Cpp,RandU64Cpp,RandCpp
EXTERN SeedCpp,IsPixCovered0Cpp,GrPlot0Cpp,GrPeek0Cpp
EXTERN GrBlotCpp,GrRectCpp,GrRectBCpp,GrClampCpp
EXTERN GrPlotCpp,GrPeekCpp,GrLineCpp,ClipLineCpp
EXTERN GrFillCircleCpp,GrPlot3BCpp,GrPlot3Cpp,GrLine3Cpp,DCThickScaleCpp
EXTERN SpriteSizeCpp,Sprite3Cpp,Sprite3BCpp,DocSpriteCpp,SpriteCpp
EXTERN GrBlot3Cpp,DCSymmetry3SetCpp,DCReflectCpp
EXTERN GrHLineCpp,GrVLineCpp,GrFillTri0Cpp,GrFillPoly3Cpp,GrRect3Cpp
EXTERN Mat4x4MulXYZCpp,DCTransformCpp,Mat4x4IdentEquCpp,Mat4x4IdentNewCpp
EXTERN Mat4x4NormSqr65536Cpp,DCMat4x4SetCpp,DCLightingCpp,DCFillCpp
EXTERN DCClearCpp,DCRstCpp,DCExtentsInitCpp,DCAliasCpp
EXTERN DCNewCpp,DCDelCpp,DCSizeCpp,DCDepthBufRstCpp
EXTERN DCDepthBufAllocCpp,DCCopyCpp,DCMonoCpp,DCColorChgCpp
EXTERN DCSaveCpp,DCLoadCpp,GRWriteCpp,GRReadCpp
EXTERN Mat4x4EquCpp,Mat4x4NewCpp
EXTERN Mat4x4MulMat4x4EquCpp,Mat4x4MulMat4x4NewCpp,Mat4x4RotXCpp,Mat4x4RotYCpp
EXTERN Mat4x4RotZCpp,Mat4x4ScaleCpp,Mat4x4TranslationEquCpp,Mat4x4TranslationAddCpp
EXTERN DCSymmetrySetCpp,RefreshCpp
EXTERN ScanCharCpp,ScanKeyCpp,GetKeyCpp,GetCharCpp
EXTERN MAllocCpp,FreeCpp,MSizeCpp,MSize2Cpp,MHeapCtrlCpp,CAllocCpp,MAllocIdentCpp
EXTERN StrNewCpp,AStrNewCpp,StrCpyCpp,StrCmpCpp,StrIMatchCpp,StrOccCpp
EXTERN HashAddCpp,HashAddAfterCpp,HashFindCpp,HashSingleTableFindCpp
EXTERN HashRemDelCpp,HashValCpp,HashDelCpp,HashTableDelCpp,HashSrcFileSetCpp,SysSymImportsResolveCpp
EXTERN HashTableNewCpp,HashTypeNumCpp,CeilU64Cpp,Pow10I64Cpp
EXTERN LinkedLstDelCpp,LinkedLstSizeCpp,QueDelCpp
EXTERN QueVectU8NewCpp,QueVectU8PutCpp,QueVectU8GetCpp,QueVectU8DelCpp
EXTERN QSortCpp,QSortI64Cpp,LstMatchCpp
EXTERN FileReadCpp,FileNameAbsCpp,DirNameAbsCpp,DirCurCpp,DirFileCpp,ExtChgCpp,ExtDftCpp
EXTERN CdCpp,IsDirCpp,DirCpp,PutDirLinkCpp,FilesFindCpp,FileFindCpp,FilesFindMatchCpp
EXTERN DirEntryDelCpp,DirEntryDel2Cpp,DirTreeDelCpp,DirTreeDel2Cpp
EXTERN DirContextNewCpp,DirContextDelCpp,Str2I64Cpp
EXTERN FileWriteCpp,BlkDevNextFreeSlotCpp,BlkDevAddCpp,Let2DrvCpp,BlkReadCpp,BlkWriteCpp
EXTERN RedSeaISOCpp
EXTERN SysUntryCpp,PutExceptCpp,YorNCpp
EXTERN StrPrintJoin,CatPrintCpp,MStrPrintCpp,PrintCpp,PrintErrCpp,PrintWarnCpp
GLOBAL CmpHC
EXTERN CmpCpp,ExePutSCpp,ExeFileCpp,StreamPrintCpp,StreamExePrintCpp
EXTERN PutCharsCpp,PutS,PutHexCpp,IsRawCpp,DefineLstLoadCpp,DefineSubCpp,DefineMatchCpp
EXTERN DocNewCpp,DocDelCpp,DocLoadCpp,DocSizeCpp,DocSaveCpp,DocWriteCpp
EXTERN DocReadCpp,DocTypeCpp,TypeCpp
EXTERN DocPrintCpp,Doc2PlainTextCpp,DocBinsValidateCpp,DocPutLineCpp,DocPutKeyCpp
EXTERN DocPutCpp,DocDisplayCpp,DocBorderCpp,DocRecalcCpp,WinHorzCpp,WinVertCpp
EXTERN WinRefocusCpp,WinToTopCpp,WinFocusCpp,WinTileHorzCpp,WinTileVertCpp,WinMaxCpp
EXTERN DocLockCpp,DocUnlockCpp,DocEntryRunCpp,DocBottomCpp,DocClearCpp,NowCpp
EXTERN DocTermNewCpp,DocGetKeyCpp,DocScanLineCpp,GetStrCpp,GetSCpp,UserCmdLineCpp
EXTERN TextCharCpp,TextLenStrCpp,TextLenAttrCpp,TextRectCpp,TextBorderCpp,WinZBufUpdateCpp
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

%macro HC_FRAME_CALL 1-2 0
	AND	RSP,-16
%if %2>=9
	SUB	RSP,32
%elif %2>=7
	SUB	RSP,16
%endif
%if %2>=7
	MOV	RAX,[RBP+64]
	MOV	[RSP],RAX
%endif
%if %2>=8
	MOV	RAX,[RBP+72]
	MOV	[RSP+8],RAX
%endif
%if %2>=9
	MOV	RAX,[RBP+80]
	MOV	[RSP+16],RAX
%endif
%if %2>=10
	MOV	RAX,[RBP+88]
	MOV	[RSP+24],RAX
%endif
	CALL	%1
	LEA	RSP,[RBP-32]
	POP	R11
	POP	R10
	POP	RDI
	POP	RSI
	POP	RBP
%endmacro

; The optional fourth argument widens a SysV Bool return for HolyC.
%macro HOLYC_CPP 3-4 0
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
%if %3>=6
	MOV	R9,[RBP+56]
%endif
	HC_FRAME_CALL %2,%3
%if %4
	MOVZX	EAX,AL
%endif
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
HOLYC_CPP TaskValidateHC,TaskValidateCpp,1
HOLYC_CPP SuspendHC,SuspendCpp,2
HOLYC_CPP IsSuspendedHC,IsSuspendedCpp,1
HOLYC_CPP TaskQueInsHC,TaskQueInsCpp,2
HOLYC_CPP TaskQueRemHC,TaskQueRemCpp,1
HOLYC_CPP SingleUserHC,SingleUserCpp,1
HOLYC_CPP IsSingleUserHC,IsSingleUserCpp,0
HOLYC_CPP JobDelHC,JobDelCpp,1
HOLYC_CPP TaskRstAwaitingMsgHC,TaskRstAwaitingMsgCpp,1
HOLYC_CPP TaskExeHC,TaskExeCpp,4
HOLYC_CPP TaskTextHC,TaskTextCpp,4
HOLYC_CPP TaskMsgHC,TaskMsgCpp,6
HOLYC_CPP JobResScanHC,JobResScanCpp,2
HOLYC_CPP JobResGetHC,JobResGetCpp,1
HOLYC_CPP TaskWaitHC,TaskWaitCpp,2
HOLYC_CPP PostMsgHC,PostMsgCpp,5
HOLYC_CPP PostMsgWaitHC,PostMsgWaitCpp,5
HOLYC_CPP MsgHC,MsgCpp,4
HOLYC_CPP JobsHndlrHC,JobsHndlrCpp,2
HOLYC_CPP SrvTaskContHC,SrvTaskContCpp,0
HOLYC_CPP UserCmdLineHC,UserCmdLineCpp,1
HOLYC_CPP ScanMsgHC,ScanMsgCpp,4
HOLYC_CPP FlushMsgsHC,FlushMsgsCpp,1
HOLYC_CPP GetMsgHC,GetMsgCpp,4
HOLYC_CPP InputFilterTaskHC,InputFilterTaskCpp,0
HOLYC_CPP Char2ScanCodeHC,Char2ScanCodeCpp,2
HOLYC_CPP ScanCode2CharHC,ScanCode2CharCpp,1
HOLYC_CPP KbdTypeMaticHC,KbdTypeMaticCpp,1
HOLYC_CPP GridInitHC,GridInitCpp,0
HOLYC_CPP MsSetHC,MsSetCpp,5
HOLYC_CPP GrPaletteColorSetHC,GrPaletteColorSetCpp,2
HOLYC_CPP GrPaletteColorGetHC,GrPaletteColorGetCpp,1
HOLYC_CPP GrPaletteGetHC,GrPaletteGetCpp,1
HOLYC_CPP GrPaletteSetHC,GrPaletteSetCpp,1
HOLYC_CPP PaletteSetStdHC,PaletteSetStdCpp,0
HOLYC_CPP WinMgrTaskEntry,WinMgrTask,1
HOLYC_CPP GrScaleZoomHC,GrScaleZoomCpp,1
HOLYC_CPP DrawStdMsHC,DrawStdMsCpp,3
HOLYC_CPP WinFinalUpdateHC,WinFinalUpdateCpp,1
HOLYC_CPP SignI64HC,SignI64Cpp,1
HOLYC_CPP RandI16HC,RandI16Cpp,0
HOLYC_CPP RandU16HC,RandU16Cpp,0
HOLYC_CPP RandI32HC,RandI32Cpp,0
HOLYC_CPP RandU32HC,RandU32Cpp,0
HOLYC_CPP RandI64HC,RandI64Cpp,0
HOLYC_CPP RandU64HC,RandU64Cpp,0
HOLYC_CPP RandHC,RandCpp,0
HOLYC_CPP SeedHC,SeedCpp,2
HOLYC_CPP IsPixCovered0HC,IsPixCovered0Cpp,3
HOLYC_CPP GrPlot0HC,GrPlot0Cpp,3
HOLYC_CPP GrPeek0HC,GrPeek0Cpp,3
HOLYC_CPP GrBlotHC,GrBlotCpp,4
HOLYC_CPP GrRectHC,GrRectCpp,5
HOLYC_CPP GrRectBHC,GrRectBCpp,5
HOLYC_CPP GrClampHC,GrClampCpp,7
HOLYC_CPP GrPlotHC,GrPlotCpp,3
HOLYC_CPP GrFillCircleHC,GrFillCircleCpp,5
HOLYC_CPP GrPlot3BHC,GrPlot3BCpp,4,1
HOLYC_CPP GrPlot3HC,GrPlot3Cpp,4,1
HOLYC_CPP SpriteSizeHC,SpriteSizeCpp,1
HOLYC_CPP Sprite3HC,Sprite3Cpp,6
HOLYC_CPP Sprite3BHC,Sprite3BCpp,5
HOLYC_CPP GrBlot3HC,GrBlot3Cpp,5
HOLYC_CPP GrHLineHC,GrHLineCpp,6
HOLYC_CPP GrVLineHC,GrVLineCpp,6
HOLYC_CPP GrFillTri0HC,GrFillTri0Cpp,4
HOLYC_CPP GrFillPoly3HC,GrFillPoly3Cpp,3
HOLYC_CPP GrRect3HC,GrRect3Cpp,6
HOLYC_CPP DCSymmetry3SetHC,DCSymmetry3SetCpp,10
HOLYC_CPP DCReflectHC,DCReflectCpp,4
HOLYC_CPP DocSpriteHC,DocSpriteCpp,3
HOLYC_CPP SpriteHC,SpriteCpp,2
HOLYC_CPP GrPeekHC,GrPeekCpp,3
HOLYC_CPP GrLineHC,GrLineCpp,7
HOLYC_CPP GrLine3HC,GrLine3Cpp,9,1
HOLYC_CPP DCThickScaleHC,DCThickScaleCpp,1
HOLYC_CPP ClipLineHC,ClipLineCpp,8
HOLYC_CPP Mat4x4MulXYZHC,Mat4x4MulXYZCpp,4
HOLYC_CPP DCTransformHC,DCTransformCpp,4
HOLYC_CPP Mat4x4IdentEquHC,Mat4x4IdentEquCpp,1
HOLYC_CPP Mat4x4IdentNewHC,Mat4x4IdentNewCpp,1
HOLYC_CPP Mat4x4NormSqr65536HC,Mat4x4NormSqr65536Cpp,1
HOLYC_CPP DCMat4x4SetHC,DCMat4x4SetCpp,2
HOLYC_CPP DCLightingHC,DCLightingCpp,5
HOLYC_CPP DCFillHC,DCFillCpp,2
HOLYC_CPP DCClearHC,DCClearCpp,1
HOLYC_CPP DCRstHC,DCRstCpp,1
HOLYC_CPP DCExtentsInitHC,DCExtentsInitCpp,1
HOLYC_CPP DCAliasHC,DCAliasCpp,2
HOLYC_CPP DCNewHC,DCNewCpp,4
HOLYC_CPP DCDelHC,DCDelCpp,1
HOLYC_CPP DCSizeHC,DCSizeCpp,1
HOLYC_CPP DCDepthBufRstHC,DCDepthBufRstCpp,1
HOLYC_CPP DCDepthBufAllocHC,DCDepthBufAllocCpp,1
HOLYC_CPP DCCopyHC,DCCopyCpp,2
HOLYC_CPP DCMonoHC,DCMonoCpp,4
HOLYC_CPP DCColorChgHC,DCColorChgCpp,3
HOLYC_CPP DCSaveHC,DCSaveCpp,3
HOLYC_CPP DCLoadHC,DCLoadCpp,3
HOLYC_CPP GRWriteHC,GRWriteCpp,3
HOLYC_CPP GRReadHC,GRReadCpp,2
HOLYC_CPP Mat4x4MulMat4x4EquHC,Mat4x4MulMat4x4EquCpp,3
HOLYC_CPP Mat4x4MulMat4x4NewHC,Mat4x4MulMat4x4NewCpp,3
HOLYC_CPP Mat4x4RotXHC,Mat4x4RotXCpp,2
HOLYC_CPP Mat4x4RotYHC,Mat4x4RotYCpp,2
HOLYC_CPP Mat4x4RotZHC,Mat4x4RotZCpp,2
HOLYC_CPP Mat4x4ScaleHC,Mat4x4ScaleCpp,2
HOLYC_CPP Mat4x4TranslationEquHC,Mat4x4TranslationEquCpp,4
HOLYC_CPP Mat4x4TranslationAddHC,Mat4x4TranslationAddCpp,4
HOLYC_CPP DCSymmetrySetHC,DCSymmetrySetCpp,5
HOLYC_CPP RefreshHC,RefreshCpp,2
HOLYC_CPP Mat4x4EquHC,Mat4x4EquCpp,2
HOLYC_CPP Mat4x4NewHC,Mat4x4NewCpp,2
HOLYC_CPP PutKeyHC,PutKeyCpp,2
HOLYC_CPP ScanCharHC,ScanCharCpp,0
HOLYC_CPP ScanKeyHC,ScanKeyCpp,3
HOLYC_CPP GetKeyHC,GetKeyCpp,3
HOLYC_CPP GetCharHC,GetCharCpp,3

SpawnHC:
	HC_FRAME_ENTER
	MOV	RDI,[RBP+16]
	MOV	RSI,[RBP+24]
	MOV	RDX,[RBP+32]
	MOV	RCX,[RBP+40]
	MOV	R8,[RBP+48]
	MOV	R9,[RBP+56]
	AND	RSP,-16
	SUB	RSP,16
	MOV	RAX,[RBP+64]
	MOV	[RSP],RAX
	CALL	SpawnCpp
	LEA	RSP,[RBP-32]
	POP	R11
	POP	R10
	POP	RDI
	POP	RSI
	POP	RBP
	RET	56
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
HOLYC_CPP CdHC,CdCpp,2
HOLYC_CPP IsDirHC,IsDirCpp,1
HOLYC_CPP DirHC,DirCpp,2
HOLYC_CPP PutDirLinkHC,PutDirLinkCpp,2
HOLYC_CPP FilesFindHC,FilesFindCpp,2
HOLYC_CPP FileFindHC,FileFindCpp,3
HOLYC_CPP FilesFindMatchHC,FilesFindMatchCpp,3
HOLYC_CPP DirEntryDelHC,DirEntryDelCpp,1
HOLYC_CPP DirEntryDel2HC,DirEntryDel2Cpp,1
HOLYC_CPP DirTreeDelHC,DirTreeDelCpp,1
HOLYC_CPP DirTreeDel2HC,DirTreeDel2Cpp,1
HOLYC_CPP DirContextNewHC,DirContextNewCpp,4
HOLYC_CPP DirContextDelHC,DirContextDelCpp,2
HOLYC_CPP Str2I64HC,Str2I64Cpp,3
HOLYC_CPP FileWriteHC,FileWriteCpp,5
HOLYC_CPP RedSeaISOHC,RedSeaISOCpp,3
HOLYC_CPP BlkDevNextFreeSlotHC,BlkDevNextFreeSlotCpp,2
HOLYC_CPP BlkDevAddHC,BlkDevAddCpp,4
HOLYC_CPP Let2DrvHC,Let2DrvCpp,2
HOLYC_CPP BlkReadHC,BlkReadCpp,4
HOLYC_CPP BlkWriteHC,BlkWriteCpp,4
HOLYC_CPP SysUntryHC,SysUntryCpp,0
HOLYC_CPP YorNHC,YorNCpp,0
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
HOLYC_CPP WinHorzHC,WinHorzCpp,3
HOLYC_CPP WinVertHC,WinVertCpp,3
HOLYC_CPP WinRefocusHC,WinRefocusCpp,1
HOLYC_CPP WinToTopHC,WinToTopCpp,2
HOLYC_CPP WinFocusHC,WinFocusCpp,1
HOLYC_CPP WinTileHorzHC,WinTileHorzCpp,0
HOLYC_CPP WinTileVertHC,WinTileVertCpp,0
HOLYC_CPP WinMaxHC,WinMaxCpp,1
HOLYC_CPP WinZBufUpdateHC,WinZBufUpdateCpp,0
HOLYC_CPP TextCharHC,TextCharCpp,5
HOLYC_CPP TextLenStrHC,TextLenStrCpp,6
HOLYC_CPP TextLenAttrHC,TextLenAttrCpp,5
HOLYC_CPP TextRectHC,TextRectCpp,5
HOLYC_CPP TextBorderHC,TextBorderCpp,7
HOLYC_CPP DocPutHC,DocPutCpp,1
HOLYC_CPP DocDisplayHC,DocDisplayCpp,1
HOLYC_CPP DocBorderHC,DocBorderCpp,1
HOLYC_CPP DocRecalcHC,DocRecalcCpp,2
HOLYC_CPP DocPutKeyHC,DocPutKeyCpp,3
HOLYC_CPP DocTermNewHC,DocTermNewCpp,0
HOLYC_CPP DocGetKeyHC,DocGetKeyCpp,1
HOLYC_CPP DocScanLineHC,DocScanLineCpp,4
HOLYC_CPP GetStrHC,GetStrCpp,3
HOLYC_CPP GetSHC,GetSCpp,3
HOLYC_CPP DocLockHC,DocLockCpp,1
HOLYC_CPP DocUnlockHC,DocUnlockCpp,1
HOLYC_CPP DocEntryRunHC,DocEntryRunCpp,4
HOLYC_CPP DocBottomHC,DocBottomCpp,1
HOLYC_CPP DocClearHC,DocClearCpp,2
HOLYC_CPP NowHC,NowCpp,0
HOLYC_CPP DocNewHC,DocNewCpp,2
HOLYC_CPP DocDelHC,DocDelCpp,1
HOLYC_CPP DocLoadHC,DocLoadCpp,3
HOLYC_CPP DocSizeHC,DocSizeCpp,1
HOLYC_CPP DocSaveHC,DocSaveCpp,2
HOLYC_CPP DocWriteHC,DocWriteCpp,2
HOLYC_CPP DocReadHC,DocReadCpp,2
HOLYC_CPP DocTypeHC,DocTypeCpp,3
HOLYC_CPP TypeHC,TypeCpp,2
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

%macro CALL_HOLYC_CALLBACK 2
%1:
	PUSH	RBP
	MOV	RBP,RSP
	PUSH	RBX
	PUSH	R12
	PUSH	R13
	PUSH	R14
	PUSH	R15
%if %2 = 2 || %2 = 4
	SUB	RSP,8
%endif
%if %2 >= 4
	PUSH	R8
%endif
%if %2 >= 3
	PUSH	RCX
%endif
%if %2 >= 2
	PUSH	RDX
%endif
	PUSH	RSI
	CALL	RDI
	CLD
%if %2 = 2 || %2 = 4
	ADD	RSP,8
%endif
	POP	R15
	POP	R14
	POP	R13
	POP	R12
	POP	RBX
	POP	RBP
	RET
%endmacro

CALL_HOLYC_CALLBACK CallHolyC1,1
CALL_HOLYC_CALLBACK CallHolyC2,2
CALL_HOLYC_CALLBACK CallHolyC3,3
CALL_HOLYC_CALLBACK CallHolyC4,4

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
