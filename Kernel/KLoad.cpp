#include "../Adam/DolDoc/DolDoc.hpp"
#include "../Adam/God/GodExt.hpp"
#include "../Adam/Gr/GrGlbls.hpp"
#include "../Adam/Menu.hpp"
#include "../Compiler/CompilerA.hpp"
#include "KExts.hpp"
#include "KernelB.hpp"

struct CLoadImport
{
  CHashImport entry;
  CLoadedModule *module;
  size_t first, past;
};

struct CLoadRecord
{
  const uint8_t *raw, *offsets;
  const char *name;
  uint64_t bytes;
  uint32_t value;
  uint8_t type;
  uint8_t *storage;
  CHashExport *exported;
  CLoadImport *imported;
};

struct CLoadedModule
{
  uint8_t *image;
  CLoadRecord *records;
  size_t count, unresolved;
  bool owns_image, started;
};

static_assert(sizeof(CLoadRecord) == 64);
static_assert(offsetof(CLoadRecord, raw) == 0 &&
              offsetof(CLoadRecord, offsets) == 8 &&
              offsetof(CLoadRecord, name) == 16 &&
              offsetof(CLoadRecord, bytes) == 24 &&
              offsetof(CLoadRecord, value) == 32 &&
              offsetof(CLoadRecord, type) == 36 &&
              offsetof(CLoadRecord, storage) == 40 &&
              offsetof(CLoadRecord, exported) == 48 &&
              offsetof(CLoadRecord, imported) == 56);
static_assert(sizeof(CLoadedModule) == 40);
static_assert(offsetof(CLoadedModule, image) == 0 &&
              offsetof(CLoadedModule, records) == 8 &&
              offsetof(CLoadedModule, count) == 16 &&
              offsetof(CLoadedModule, unresolved) == 24 &&
              offsetof(CLoadedModule, owns_image) == 32 &&
              offsetof(CLoadedModule, started) == 33);

extern "C" {
void CallModuleMain(const uint8_t *entry);
void ACDictGenHC();
char *DirFileCpp(const char *dirname, const char *name, const char *extension);
CDocEntryBase *DocPutLineCpp(CDoc *doc, CDocEntryBase *entry);
void AdamErrCpp(const char *fmt, int64_t argc, const uint64_t *argv);
void FixSetCpp(const char *filename, int64_t line);
char *ExtChgCpp(const char *name, const char *ext);
uint64_t F64PowerBits(uint64_t base, uint64_t exponent);
void SYS_POW();
void _SIGN(),_ROUND(),_TRUNC(),_FLOOR(),_CEIL(),_POW(),_POW10(),_EXP();
void _LOG10(),_LOG2(),_LN(),_FLDCW(),_FSTCW(),_FSTSW(),_FCLEX();
uint64_t I64MulF64Bits(int64_t value, uint64_t scale);
void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);
void PrintErrCpp(const char *fmt, int64_t argc, const uint64_t *argv,
                 const uintptr_t *frame);
void _BEQU(), _LBEQU(), _CALL(), _CLAMP_I64();
void _BIT_FIELD_EXT_U32(), _BIT_FIELD_OR_U32();
void _MEMCPY(), _MEMSET(), _MEMSET_U16(), _MEMSET_U32(), _MEMSET_I64(), _MEMCMP();
void _MALLOC(), _FREE(), _MHEAP_CTRL(), _MSIZE(), _MSIZE2();
void CAllocHC(), MAllocIdentHC(), StrNewHC(), AStrNewHC();
void _STRCMP(), _STRCPY(), _STRIMATCH(), StrOccHC();
void TO_UPPER(), _STRICMP(), _STRNCMP(), _STRNICMP();
void _STRMATCH(), Spaces2TabsHC(), StrUtilHC(), StrFirstOccHC(), StrFirstRemHC();
void StrLastOccHC(), StrLastRemHC(), StrFindHC(), Tabs2SpacesHC(), ScaleIndentHC();
void MStrUtilHC();
void _HASH_ADD(), _HASH_ADD_AFTER(), _HASH_FIND(), _HASH_SINGLE_TABLE_FIND();
void HashTableNewHC(), HashTypeNumHC(), CeilU64HC(), Pow10I64HC();
void HashGenericAddHC(), FramePtrHC(), FramePtrAddHC(), FramePtrSetHC(), FramePtrDelHC();
void ClampHC(), MinHC(), MaxHC(), FloorU64HC(), RoundI64HC(), FloorI64HC(), CeilI64HC();
void LinkedLstDelHC(), LinkedLstCopyHC(), LinkedLstCntHC(), LinkedLstSizeHC();
void QueDelHC(), QueCopyHC(), QueCntHC(), QueSizeHC();
void QueVectU8NewHC(), QueVectU8PutHC(), QueVectU8GetHC(), QueVectU8DelHC();
void FifoU8NewHC(), FifoU8DelHC(), FifoU8InsHC(), FifoU8RemHC();
void FifoU8PeekHC(), FifoU8FlushHC(), FifoU8CntHC();
void FifoI64NewHC(), FifoI64DelHC(), FifoI64InsHC(), FifoI64RemHC();
void FifoI64PeekHC(), FifoI64FlushHC(), FifoI64CntHC();
void QSortHC(), QSortI64HC(), LstMatchHC();
void FileReadHC(), FileNameAbsHC(), DirNameAbsHC(), DirCurHC(), DirFileHC();
void CdHC(), IsDirHC(), DirHC(), PutDirLinkHC(), FilesFindHC(), FileFindHC(),
    FilesFindMatchHC(), DirEntryDelHC(), DirEntryDel2HC(), DirTreeDelHC(), DirTreeDel2HC();
void DirContextNewHC(), DirContextDelHC(), Str2I64HC(), StrScanHC();
void Str2DateHC(), GetI64HC(), GetF64HC(), GetDateHC();
void Str2F64HC(), ChkPtrHC(), ClassRep2HC(), ClassRepHC(), ClassRepDHC();
void DHC(), DmHC(), DaHC();
void DocMaxHC(), DocDataFmtHC(), DocDataScanHC();
void DocFormHC(), DocEdHC(), DocFormFwdHC(), DocFormBwdHC(), ViewHC(),
    HomeSetHC(), ToUpperHC();
void ExtChgHC(), ExtDftHC();
void FileExtDotHC();
void FileExtRemHC();
void IsDotZHC();
void IsDotCHC();
void FileNameChkHC();
void ToggleZorNotZHC();
void FileNameTmpTxtHC();
void CFileNameToHC();
void Name2DirClusHC();
void Name2ParentDirClusHC();
void Cd2DirEntryHC();
void DirEntryCompareNameHC();
void DirEntryCompareClusHC();
void DirFilesSortHC();
void DirFilesFlattenHC();
void FilesFind2HC();
void DrvLockHC();
void DrvUnlockHC();
void DrvsReleaseHC();
void BlkDevLockHC();
void BlkDevUnlockHC();
void BlkDevInitHC();
void BlkDevsReleaseHC();
void DskCacheInitHC();
void DskCacheAddHC();
void DskCacheInvalidate2HC();
void RCacheHC();
void DrvMakeFreeSlotHC();
void DrvNextFreeLetHC();
void DrvChkHC();
void DrvFATBlkAllocHC();
void DrvEnableHC();
void DrvDelHC();
void DrvBlkDevDelHC();
void BlkDevDelHC();
void DskCacheInvalidateHC();
void DskChgHC();
void DrvFATBlkCleanHC();
void DrvFATBlkSetHC();
void Drv2LetHC();
void Let2LetHC();
void Let2BlkDevTypeHC();
void DrvIsWritableHC();
void DrvMapHC();
void DrvHC();
void DrvSerialNumHC();
void DrvModelNumHC();
void DrvTextAttrGetHC();
void DrvRepHC();
void BlkDevChkHC();
void Let2BlkDevHC();
void UnmountHC(),MountFileHC();
void RedSeaISOHC(),RedSeaISO9660HC();
void FileWriteHC(), BlkDevNextFreeSlotHC(), BlkDevAddHC(), Let2DrvHC();
void FSizeHC(), FOpenHC(), FCloseHC(), FBlkReadHC(), FBlkWriteHC();
void FileAttrHC(), CopySingleHC();
void DelHC(),CopyHC(),MoveHC(),CopyTreeHC(),DelTreeHC(),DirMkHC(),ScanFlagsHC();
void BlkReadHC(), BlkWriteHC();
void ClusNumNextHC();
void Clus2BlkHC();
void ClusBlkReadHC();
void ClusReadHC();
void ClusBlkWriteHC();
void ClusWriteHC();
void ClusAllocHC();
void FAT32AllocClusHC();
void FAT32AllocContiguousClusHC();
void FAT32FreeClusHC();
void RedSeaAllocClusHC();
void RedSeaFreeClusHC();
void RedSeaFreeFreeLstHC(),RedSeaFreeLstBuildHC(),RedSeaValidateHC(),RedSeaFmtHC();
void FAT32DirNewHC();
void RedSeaDirNewHC();
void DirNewHC();
void FmtHC();
void SysTryHC(), SysUntryHC(), ThrowHC(), CallerHC(), PutExceptHC(), YorNHC();
void BreakHC(), BreakLockHC(), BreakUnlockHC();
void StrPrintJoinHC(), StrPrintHC(), CatPrintHC(), MStrPrintHC(), PrintHC();
void PrintErrHC(), PrintWarnHC(), PutCharsHC(), IsRawHC(), DefineLstLoadHC();
void PutSHC(), PutHexHC();
void PUT_HEX_U64(), PUT_HEX_U32(), PUT_HEX_U16(), PUT_HEX_U8(), PUT_CHARS(),
    PUT_STR();
void DefineSubHC(), DefineMatchHC();
void DefineHC(), DefineLoadHC(), DefineCntHC(), DefinePrintHC();
void Color2StrHC(), Str2ColorU16HC(), Str2ColorU32HC();
void HashValHC(), HashDelHC(), HashTableDelHC(), HashSrcFileSetHC();
void HashPublicHC();
void HashTablePurgeHC(), KillHC();
void SpawnHC(), TaskValidateHC(), SuspendHC(), IsSuspendedHC();
void TaskQueInsHC(), TaskQueRemHC(), SingleUserHC(), IsSingleUserHC();
void ProgressBarsRstHC();
void _TASK_END_NOW();
void JobDelHC(),TaskRstAwaitingMsgHC(),TaskExeHC(),TaskTextHC();
void TaskMsgHC(),JobResScanHC(),JobResGetHC(),TaskWaitHC();
void PostMsgHC(),PostMsgWaitHC(),MsgHC(),JobsHndlrHC();
void SrvTaskContHC(),ScanMsgHC(),FlushMsgsHC(),GetMsgHC();
void InputFilterTaskHC();
void Char2ScanCodeHC(),ScanCode2CharHC(),KbdTypeMaticHC(),PutKeyHC();
void KbdMsEvtTimeHC();
void KeyDescSetHC(),KeyDevAddHC(),KeyDevRemHC(),CtrlAltCBSetHC();
void GridInitHC(),MsSetHC();
void GrPaletteColorSetHC(),GrPaletteColorGetHC();
void GrPaletteGetHC(),GrPaletteSetHC(),PaletteSetStdHC();
void GrScaleZoomHC(),DrawStdMsHC();
void SignI64HC(),RandI16HC(),RandU16HC(),RandI32HC();
void RandU32HC(),RandI64HC(),RandU64HC(),RandHC();
void SeedHC(),IsPixCovered0HC(),GrPlot0HC(),GrPeek0HC();
void GrBlotHC(),GrRectHC(),GrRectBHC(),GrClampHC();
void GrPlotHC(),GrPeekHC(),GrLineHC(),ClipLineHC();
void GrFloodFillHC(),GrFloodFill3HC();
void GrFillCircleHC(),GrPlot3BHC(),GrPlot3HC(),GrLine3HC(),DCThickScaleHC();
void CircleHC(),EllipseHC(),GrCircleHC(),GrEllipseHC(),GrCircle3HC(),GrEllipse3HC(),ArgHC();
void LineHC();
void RegPolyHC();
void GrRegPolyHC();
void GrRegPoly3HC();
void Bezier2HC();
void BSpline2HC();
void Gr2BezierHC();
void Gr2Bezier3HC();
void Gr2BSplineHC();
void Gr2BSpline3HC();
void Bezier3HC();
void BSpline3HC();
void Gr3BezierHC();
void Gr3Bezier3HC();
void Gr3BSplineHC();
void Gr3BSpline3HC();
void D3I32DistHC();
void D3I32DistSqrHC();
void D3I32NormSqrHC();
void SpriteSizeHC(),Sprite3HC(),Sprite3BHC(),DocSpriteHC(),SpriteHC();
void DC2SpriteHC(),DCExtHC(),DCDiffHC(),DocGRHC();
void GrBlot3HC(),DCSymmetry3SetHC(),DCReflectHC();
void GrHLineHC(),GrVLineHC(),GrFillTri0HC(),GrFillPoly3HC(),GrRect3HC();
void GrBorderHC(),GrArrow3HC(),GrPutCharHC(),GrPrintHC(),PressAKeyHC();
void Mat4x4MulXYZHC(),DCTransformHC(),Mat4x4IdentEquHC(),Mat4x4IdentNewHC();
void Mat4x4NormSqr65536HC(),DCMat4x4SetHC(),DCLightingHC(),DCFillHC();
void DCClearHC(),DCRstHC(),DCExtentsInitHC(),DCAliasHC();
void DCNewHC(),DCDelHC(),DCSizeHC(),DCDepthBufRstHC();
void DCDepthBufAllocHC(),DCCopyHC(),DCMonoHC(),DCColorChgHC();
void DCSaveHC(),DCLoadHC(),GRWriteHC(),GRReadHC();
void DCScrnCaptureHC(),GRScrnCaptureReadHC(),GRScrnCaptureWriteHC();
void Mat4x4EquHC(),Mat4x4NewHC(),Mat4x4MulMat4x4EquHC(),Mat4x4MulMat4x4NewHC();
void Mat4x4RotXHC(),Mat4x4RotYHC(),Mat4x4RotZHC(),Mat4x4ScaleHC();
void Mat4x4TranslationEquHC(),Mat4x4TranslationAddHC(),DCSymmetrySetHC(),RefreshHC();
void ScanCharHC(),ScanKeyHC(),GetKeyHC(),GetCharHC();
void _HASH_REM_DEL(), SysSymImportsResolveHC(), LoadPass2HC(), LoadHC();
void DocNewHC(), DocDelHC(), DocLoadHC(), DocSizeHC(), DocSaveHC(),
    DocWriteHC();
void DocReadHC(), DocTypeHC(), TypeHC();
void KeyMapHC(),TipOfDayHC();
void DocCutHC(), DocCopyHC(), DocTreeFindHC(), DocTreeFFindHC(), DocTreeMakeHC();
void DocTreeWriteHC(), DocTreeAppendHC(), DocTreeFWriteHC(), DocTreeFAppendHC();
void DocTreeWriteJoinHC(), DocTreeAppendJoinHC(), ExeDocHC(), DocTreeExeHC(),
    DocTreeFExeHC();
void GodBitsInsHC(), GodHexInsHC(), GodBitsHC();
void GodInitHC(), GodWordStrHC(), GodWordHC();
void GodSongStrHC(), GodSongHC();
void GodBiblePassageHC(), BibleLine2VerseHC(), BookLinesHC(), BibleVerseHC();
void BibleDocHC(), BibleViewHC();
void GodDoodleSpriteHC(), GodDoodleHC();
void WhoHC(), HashDepthRepHC(), DocHelpIdxHC(), PopUpHelpIndexHC(), MapFileLoadHC();
void SrcLineNumHC(), SrcFileNameHC(), SrcEdLinkHC();
void RegDftHC(), RegExeHC(), RegWriteHC(), RegCntHC(), RegAppendHC(), AdamHC();
void InsRegHC(), InsReregHC(), InsUnregHC();
void AOnceFlushHC(), OnceFlushHC(), AOnceHC(), OnceHC(), AOnceDrvHC(),
    OnceDrvHC(), OnceExeHC(), XTalkHC();
void XTalkWaitHC(), InStrHC(), InFileHC(), InHC(), XTalkStrHC(), XTalkStrWaitHC();
void InGetStrHC(), InPrintHC(), InGetKeyHC(), InGetCharHC(), InUntilKeyHC(),
     InUntilCharHC(), InViewHC(), InSetMsHC();
void DocMenuHC(), PopUpMenuHC(), PopUpFormHC(), PopUpHC(), PopUpPrintHC(),
    SrvCmdLineHC(), OneTimePopUpHC(), RegOneTimePopUpHC();
void DocPrintHC(), Doc2PlainTextHC(), DocBinsValidateHC(), DocPutLineHC();
void DocPutHC(), DocDisplayHC(), DocBorderHC(), DocRecalcHC(), DocPutKeyHC();
void DocLockHC(), DocUnlockHC(), DocEntryRunHC(), DocBottomHC(), DocClearHC(), NowHC();
void DocCursorHC(), DocHighlightCursorHC(), DocScrollHC();
void Struct2DateHC(), Date2StructHC(), YearStartDateHC(), DayOfWeekHC();
void FirstDayOfMonHC(), LastDayOfMonHC(), FirstDayOfYearHC(), LastDayOfYearHC();
void DocTermNewHC(), DocGetKeyHC(), DocScanLineHC(), GetStrHC(), GetSHC(), UserCmdLineHC();
void WinHorzHC(), WinVertHC(), TextCharHC(), TextLenStrHC(), TextLenAttrHC(),
    TextLenAttrStrHC(), TextPrintHC(), TextRectHC(), TextBorderHC(), WinZBufUpdateHC();
void WinRefocusHC(), WinToTopHC(), WinFocusHC(), WinTileHorzHC(), WinTileVertHC(), WinMaxHC();
void WinScrollNullHC(), WinScrollRestoreHC();
void CtrlFindUniqueHC(), CtrlInsideHC(), WinScrollsInitHC();
void ViewAnglesNewHC(), ViewAnglesDelHC(), D3I32NormHC(), AutoCompleteHC(),
     ACInitHC();
void ACDWordsLoadHC(), ACDWordPtAtHC(), ACDDefGetHC(), ACDDefsGetHC(),
     ACDDefsPutHC(), ACDPopUpDefHC();
void MenuSubEntryFindHC();
void MenuEntryFindHC();
void MenuNewHC();
void MenuFileHC();
void MenuDelHC();
void MenuPushHC();
void MenuFilePushHC();
void MenuPopHC();
void Char2KeyNameHC(), ScanCode2KeyNameHC(), PopUp1HC(), PopUpOkHC();
void PopUp2HC(), PopUpCancelOkHC(), EdHC(), PopUpEdHC(), DocEntryLinkHC(),
     EdLinkCvtHC(), DocGoToLineHC(), DocFindHC(), DocAnchorFindHC(), DocCenterHC(),
     DocCollapseHC();
void WinBorderHC(), SettingsPushHC(), SettingsPopHC();
void PutFileLinkHC(), PutSrcLinkHC(), FixSetHC(), AdamErrHC();
void RawHC(), DbgModeHC(), IsDbgModeHC();
void CPURepHC(), PCIRepHC(), MemBIOSRepHC(), MemPageRepHC();
void tSHC(), BusyHC(), SleepUntilHC(), SleepHC(), Ona2FreqHC(), Freq2OnaHC();
void SndHC(), ScrnCastHC(), SndRstHC(), BeepHC(), MuteHC(), IsMuteHC();
void SndShiftHC(), AUWriteHC();
void SawHC(), FullSawHC(), CawHC(), FullCawHC(), TriHC(), FullTriHC();
void SndTaskEndCBHC(), tMHC(), BeatHC(), MusicSetOctaveHC(), MusicSetMeterHC();
void MusicSetNoteLenHC(), Note2OnaHC(), Ona2NoteHC(), Ona2OctaveHC();
void NoiseHC(), SweepHC();
void PlayHC(), MusicSettingsRstHC();
}

static bool IsImport(uint8_t type)
{
  return type >= IET_REL_I0 && type <= IET_IMM_I64;
}

static bool IsExport(uint8_t type)
{
  return type >= IET_REL32_EXPORT && type <= IET_IMM64_EXPORT;
}

static bool IsHeap(uint8_t type)
{
  return type >= IET_CODE_HEAP && type <= IET_ZEROED_DATA_HEAP;
}

static unsigned ImportWidth(uint8_t type)
{
  return type < 4 ? 0 : 1U << (type / 2 - 2);
}

static uint64_t ReadWidth(const uint8_t *ptr, size_t width)
{
  uint64_t value = 0;
  MemCpy(&value, ptr, width);
  return value;
}

static bool LoadRecords(const uint8_t *image, const CBinFile &header,
                        CLoadRecord *records, size_t &count)
{
  size_t payload = header.patch_table_offset - BIN_HEADER_SIZE;
  auto *p = image + header.patch_table_offset;
  const auto *end = image + header.file_size;
  count = 0;
  bool in_import = false;
  while (p < end) {
    if (*p == IET_END)
      return true;
    if (end - p < 6)
      return false;
    CLoadRecord rec{};
    rec.raw = p;
    rec.type = *p++;
    rec.value = ReadWidth(p, sizeof(uint32_t));
    p += sizeof(uint32_t);
    rec.name = reinterpret_cast<const char *>(p);
    while (p < end && *p)
      p++;
    if (p == end)
      return false;
    p++;

    if (IsImport(rec.type)) {
      if ((!*rec.name && !in_import) || rec.value > payload ||
          ImportWidth(rec.type) > payload - rec.value)
        return false;
      in_import = true;
    } else {
      in_import = false;
      if (IsExport(rec.type)) {
        if (!*rec.name || (!(rec.type & 1) && rec.value > payload))
          return false;
      } else if (rec.type == IET_MAIN) {
        if (rec.value >= payload)
          return false;
      } else {
        if (rec.type != IET_ABS_ADDR && !IsHeap(rec.type))
          return false;
        size_t width = rec.type >= IET_DATA_HEAP ? 8 : 4;
        if (IsHeap(rec.type)) {
          if (size_t(end - p) < width)
            return false;
          rec.bytes = ReadWidth(p, width);
          if (rec.bytes >> (width * 8 - 1))
            return false;
          p += width;
        }
        if (rec.value > size_t(end - p) / sizeof(uint32_t))
          return false;
        rec.offsets = p;
        for (uint32_t i = 0; i < rec.value; i++) {
          size_t target = ReadWidth(p, sizeof(uint32_t));
          if (target > payload || width > payload - target)
            return false;
          p += sizeof(uint32_t);
        }
      }
    }
    if (records)
      records[count] = rec;
    count++;
  }
  return false;
}

static void LoadDiscard(CLoadedModule *module)
{
  if (module->records) {
    for (size_t i = 0; i < module->count; i++) {
      auto &rec = module->records[i];
      Free(rec.storage);
      HashDel(rec.exported ? &rec.exported->sym.hash : nullptr);
      HashDel(rec.imported ? &rec.imported->entry.sym.hash : nullptr);
    }
    Free(module->records);
  }
  if (module->owns_image)
    Free(module->image);
  Free(module);
}

static uint8_t *ModuleBase(const CLoadedModule &module)
{
  return module.image + BIN_HEADER_SIZE;
}

static bool LoadPrepare(CLoadedModule &module, CTask &task)
{
  for (size_t i = 0; i < module.count; i++) {
    auto &rec = module.records[i];
    if (IsHeap(rec.type)) {
      auto *hc = rec.type < IET_DATA_HEAP ? task.code_heap : task.data_heap;
      rec.storage = MAlloc(rec.bytes, hc);
      if (!rec.storage)
        return false;
      if (rec.type == IET_ZEROED_CODE_HEAP || rec.type == IET_ZEROED_DATA_HEAP)
        MemSet(rec.storage, 0, rec.bytes);
    }
    if ((IsExport(rec.type) || IsHeap(rec.type)) && *rec.name) {
      rec.exported = reinterpret_cast<CHashExport *>(
          MAlloc(sizeof(CHashExport), task.data_heap));
      if (!rec.exported)
        return false;
      MemSet(rec.exported, 0, sizeof(CHashExport));
      rec.exported->sym.hash.str = StrNew(rec.name, task.data_heap);
      if (!rec.exported->sym.hash.str)
        return false;
      rec.exported->sym.hash.type = HTT_EXPORT_SYS_SYM | HTF_IMM;
      rec.exported->val =
          IsHeap(rec.type) ? reinterpret_cast<uintptr_t>(rec.storage)
          : (rec.type & 1)
              ? rec.value
              : reinterpret_cast<uintptr_t>(ModuleBase(module) + rec.value);
    }
    if (IsImport(rec.type) && *rec.name) {
      rec.imported = reinterpret_cast<CLoadImport *>(
          MAlloc(sizeof(CLoadImport), task.data_heap));
      if (!rec.imported)
        return false;
      MemSet(rec.imported, 0, sizeof(CLoadImport));
      rec.imported->entry.sym.hash.str = StrNew(rec.name, task.data_heap);
      if (!rec.imported->entry.sym.hash.str)
        return false;
      rec.imported->entry.sym.hash.type = HTT_IMPORT_SYS_SYM;
      rec.imported->entry.module_base = ModuleBase(module);
      rec.imported->module = &module;
      rec.imported->first = i;
      size_t j = i + 1;
      while (j < module.count && IsImport(module.records[j].type) &&
             !*module.records[j].name)
        j++;
      rec.imported->past = j;
    }
  }
  return true;
}

static void LoadOneImport(const CLoadImport &import, uint64_t address)
{
  auto *base = import.entry.module_base;
  for (size_t i = import.first; i < import.past; i++) {
    const auto &rec = import.module->records[i];
    size_t width = ImportWidth(rec.type);
    auto *target = base + rec.value;
    uint64_t value =
        rec.type & 1 ? address
                     : address - reinterpret_cast<uintptr_t>(target) - width;
    MemCpy(target, &value, width);
  }
}

static CHash *FindLinkSymbol(const char *name, CHashTable *table)
{
  constexpr auto mask = HTT_EXPORT_SYS_SYM | HTT_FUN | HTT_GLBL_VAR;
  for (uint64_t instance = 1;; ++instance) {
    auto *entry = HashFind(name, table, mask, instance);
    if (!entry || !(entry->type & (HTF_IMPORT | HTF_UNRESOLVED)))
      return entry;
  }
}

static uint64_t LinkAddress(const CHash &symbol)
{
  if (symbol.type & HTT_FUN)
    return reinterpret_cast<uintptr_t>(
        reinterpret_cast<const CHashFun *>(&symbol)->exe_addr);
  if (symbol.type & HTT_GLBL_VAR)
    return reinterpret_cast<uintptr_t>(
        reinterpret_cast<const CHashGlblVar *>(&symbol)->data_addr);
  return reinterpret_cast<const CHashExport *>(&symbol)->val;
}

static void SysSymImportsResolve(const char *name, CHashTable *table)
{
  while (auto *entry = HashSingleTableFind(name, table, HTT_IMPORT_SYS_SYM)) {
    auto *symbol = FindLinkSymbol(name, table);
    if (!symbol)
      return;
    auto &import = *reinterpret_cast<CLoadImport *>(entry);
    LoadOneImport(import, LinkAddress(*symbol));
    import.module->unresolved--;
    entry->type = HTT_INVALID;
  }
}

extern "C" void SysSymImportsResolveCpp(const char *name, int64_t)
{
  if (name)
    SysSymImportsResolve(name, CurrentTask()->hash_table);
}

static void LoadPass1(CLoadedModule &module, CHashTable *table, unsigned flags)
{
  auto *base = ModuleBase(module);
  for (size_t i = 0; i < module.count; i++) {
    auto &rec = module.records[i];
    if (rec.exported) {
      HashAdd(&rec.exported->sym.hash, table);
      rec.exported = nullptr;
      if (IsExport(rec.type))
        SysSymImportsResolve(rec.name, table);
    }
    if (rec.imported) {
      if (auto *entry = FindLinkSymbol(rec.name, table)) {
        LoadOneImport(*rec.imported, LinkAddress(*entry));
        HashDel(&rec.imported->entry.sym.hash);
      } else {
        HashAdd(&rec.imported->entry.sym.hash, table);
        module.unresolved++;
        if (!(flags & LDF_SILENT)) {
          SerialPutS("Unresolved Reference:");
          SerialPutS(rec.name);
          SerialPutS("\r\n");
        }
      }
      rec.imported = nullptr;
    }
    if (rec.offsets && (rec.type != IET_ABS_ADDR || !(flags & LDF_NO_ABSS))) {
      size_t width = rec.type >= IET_DATA_HEAP ? 8 : 4;
      uint64_t addend = reinterpret_cast<uintptr_t>(
          rec.type == IET_ABS_ADDR ? base : rec.storage);
      for (uint32_t j = 0; j < rec.value; j++) {
        auto *dest = base + ReadWidth(rec.offsets + size_t{j} * 4, 4);
        uint64_t value = ReadWidth(dest, width) + addend;
        MemCpy(dest, &value, width);
      }
    }
  }
}

extern "C" void LoadPass2Cpp(const uint8_t *src, uint8_t *module_base, int64_t)
{
  while (uint8_t etype = *src++) {
    uint64_t i = ReadWidth(src, sizeof(uint32_t));
    src += sizeof(uint32_t);
    auto *st_ptr = reinterpret_cast<const char *>(src);
    src += StrLen(st_ptr) + 1;
    switch (etype) {
    case IET_MAIN:
      if (auto target = reinterpret_cast<uintptr_t>(module_base) + i)
        CallModuleMain(reinterpret_cast<const uint8_t *>(target));
      break;
    case IET_ABS_ADDR:
      src += sizeof(uint32_t) * i;
      break;
    case IET_CODE_HEAP:
    case IET_ZEROED_CODE_HEAP:
      src += 4 + sizeof(uint32_t) * i;
      break;
    case IET_DATA_HEAP:
    case IET_ZEROED_DATA_HEAP:
      src += 8 + sizeof(uint32_t) * i;
      break;
    }
  }
}

bool LoadPass2(CLoadedModule &module)
{
  module.started = true;
  auto offset = ReadWidth(module.image + offsetof(CBinFile, patch_table_offset),
                          sizeof(uint64_t));
  LoadPass2Cpp(module.image + offset, ModuleBase(module), 0);
  return true;
}

static uintptr_t CompilerExport(const CLoadedModule &module, const char *name,
                                size_t size, size_t payload)
{
  uintptr_t result = 0;
  for (size_t i = 0; i < module.count; ++i) {
    const auto &record = module.records[i];
    if ((!IsExport(record.type) && !IsHeap(record.type)) ||
        StrCmpCpp(reinterpret_cast<const uint8_t *>(record.name),
                  reinterpret_cast<const uint8_t *>(name)))
      continue;
    if (result ||
        (record.type != IET_REL32_EXPORT && record.type != IET_REL64_EXPORT) ||
        record.value > payload || size > payload - record.value)
      return 0;
    result = reinterpret_cast<uintptr_t>(ModuleBase(module) + record.value);
  }
  return result;
}

bool LoadCompilerImports(CLoadedModule &module, const int64_t *&status)
{
  status = nullptr;
  if (!module.image || !module.records || module.unresolved || module.started)
    return false;
  CBinFile header;
  MemCpy(&header, module.image, sizeof(header));
  if (header.patch_table_offset < sizeof(header) ||
      header.patch_table_offset >= header.file_size)
    return false;
  size_t payload = header.patch_table_offset - sizeof(header);
  if (payload > UINTPTR_MAX - reinterpret_cast<uintptr_t>(ModuleBase(module)))
    return false;

  CCompilerImports imports{};
#define COMPILER_BIND(name)                                                    \
  do {                                                                         \
    imports.name = reinterpret_cast<decltype(imports.name)>(                   \
        CompilerExport(module, "__unholyos_sysv." #name, 1, payload));         \
    if (!imports.name)                                                         \
      return false;                                                            \
  } while (false)
  COMPILER_BIND(AsmHashLoad);
  COMPILER_BIND(LexStmt2Bin);
  COMPILER_BIND(ExePutS);
  COMPILER_BIND(ClassMemberLstDel);
  COMPILER_BIND(CmpAsmHashLoad);
  COMPILER_BIND(LexSourceInit);
  COMPILER_BIND(LexSourceDel);
  COMPILER_BIND(Lex);
  COMPILER_BIND(LexTypedExpression2Bin);
  COMPILER_BIND(MemberMetaDataCpp);
  COMPILER_BIND(AsmHashFind);
  COMPILER_BIND(CmpAsmHashNew);
  COMPILER_BIND(LexAttachDoc);
  COMPILER_BIND(LexRaw);
  COMPILER_BIND(LexExpression2Bin);
  COMPILER_BIND(ExeFile);
  COMPILER_BIND(LexExceptReport);
#undef COMPILER_BIND

  auto address = CompilerExport(module, "__unholyos_compiler.CInitStatus",
                                sizeof(int64_t), payload);
  if (!address || address % alignof(int64_t) ||
      ReadWidth(reinterpret_cast<const uint8_t *>(address), sizeof(int64_t)) ||
      !CompilerImportsPublish(imports))
    return false;
  status = reinterpret_cast<const int64_t *>(address);
  return true;
}

enum CLoadError : uint8_t
{
  LOAD_OK,
  LOAD_BAD_IMAGE,
  LOAD_NO_MEMORY
};

static char *LoadModuleName(const char *path, CHeapCtrl *hc)
{
  auto *name = StrNew(path, hc);
  if (name)
    if (auto *dot = FileExtDot(name))
      name[dot - name] = 0;
  return name;
}

static CLoadedModule *LoadImage(uint8_t *&input, size_t size, const char *path,
                                CTask &task, unsigned flags, uintptr_t bfh_addr,
                                CLoadError &error)
{
  error = LOAD_BAD_IMAGE;
  CBinFile header;
  if (!input || size < sizeof(header) || size > PTRDIFF_MAX)
    return nullptr;
  MemCpy(&header, input, sizeof(header));
  size_t count;
  if (header.bin_signature != BIN_SIGNATURE_VAL || header.file_size != size ||
      header.patch_table_offset < sizeof(header) ||
      header.patch_table_offset >= size || header.module_align_bits >= 63 ||
      !LoadRecords(input, header, nullptr, count) ||
      count > SIZE_MAX / sizeof(CLoadRecord))
    return nullptr;
  if (bfh_addr == INVALID_PTR)
    bfh_addr = header.org;
  if (bfh_addr != INVALID_PTR && size > UINTPTR_MAX - bfh_addr)
    return nullptr;

  error = LOAD_NO_MEMORY;
  auto *module = reinterpret_cast<CLoadedModule *>(
      CAlloc(sizeof(CLoadedModule), task.data_heap));
  if (!module)
    return nullptr;

  bool retained = false;
  if (bfh_addr == INVALID_PTR) {
    size_t align = size_t{1} << header.module_align_bits;
    if (task.code_heap == task.data_heap && align <= 8) {
      module->image = input;
      retained = true;
    } else {
      size_t misalignment = (0 - sizeof(header)) & (align - 1);
      if (align < 16)
        align = 16;
      module->owns_image = true;
      module->image = MAllocAligned(size, align, task.code_heap, misalignment);
      if (!module->image) {
        LoadDiscard(module);
        return nullptr;
      }
    }
  } else {
    module->image = reinterpret_cast<uint8_t *>(bfh_addr);
  }
  module->records = reinterpret_cast<CLoadRecord *>(
      MAlloc(count * sizeof(CLoadRecord), task.data_heap));
  if (!module->records) {
    LoadDiscard(module);
    return nullptr;
  }
  if (!LoadRecords(input, header, module->records, module->count)) {
    error = LOAD_BAD_IMAGE;
    LoadDiscard(module);
    return nullptr;
  }
  if (!LoadPrepare(*module, task)) {
    LoadDiscard(module);
    return nullptr;
  }

  CHashGeneric *entry = nullptr;
  if (path) {
    auto *name = LoadModuleName(path, task.data_heap);
    if (!name) {
      LoadDiscard(module);
      return nullptr;
    }
    if (*name) {
      entry = reinterpret_cast<CHashGeneric *>(
          CAlloc(sizeof(CHashGeneric), task.data_heap));
      if (!entry) {
        Free(name);
        LoadDiscard(module);
        return nullptr;
      }
      entry->hash.str = name;
      entry->hash.type = HTT_MODULE | HTF_PUBLIC;
      entry->user_data0 = reinterpret_cast<uintptr_t>(module->image);
    } else
      Free(name);
  }
  // Prepare all allocations before writing caller-owned memory or retaining
  // the input. A failed preparation leaves that input with the caller.
  if (!retained)
    MemCpy(module->image, input, size);
  for (size_t i = 0; i < module->count; i++) {
    auto &rec = module->records[i];
    rec.raw = module->image + (rec.raw - input);
    rec.name = reinterpret_cast<const char *>(rec.raw + 1 + sizeof(rec.value));
    if (rec.offsets)
      rec.offsets = module->image + (rec.offsets - input);
    if (rec.imported)
      rec.imported->entry.module_header_entry = rec.raw;
  }
  if (retained)
    module->owns_image = true;
  else
    Free(input);
  input = nullptr;
  error = LOAD_OK;
  if (entry)
    HashAdd(&entry->hash, task.hash_table);
  LoadPass1(*module, task.hash_table, flags);
  if (!(flags & LDF_JUST_LOAD))
    LoadPass2(*module);
  return module;
}

CLoadedModule *Load(const CDrv &dv, const char *path, CTask &task,
                    unsigned flags)
{
  bool extension = false;
  size_t length = 0;
  for (; path[length]; length++) {
    if (path[length] == '/')
      extension = false;
    else if (path[length] == '.')
      extension = true;
  }
  uint8_t *with_extension = nullptr;
  if (!extension) {
    constexpr char suffix[] = ".BIN";
    with_extension = MAlloc(length + sizeof(suffix), task.data_heap);
    if (!with_extension)
      return nullptr;
    MemCpy(with_extension, path, length);
    MemCpy(with_extension + length, suffix, sizeof(suffix));
    path = reinterpret_cast<const char *>(with_extension);
  }
  size_t size;
  auto *data = FileRead(dv, path, size, task.data_heap);
  CLoadError error;
  auto *module =
      data ? LoadImage(data, size, path, task, flags, INVALID_PTR, error)
           : nullptr;
  Free(data);
  Free(with_extension);
  return module;
}

extern "C" CBinFile *LoadCpp(const char *filename, int64_t ld_flags,
                             CBinFile *bfh_addr)
{
  auto &task = *CurrentTask();
  auto *fbuf = ExtDftCpp(filename, "BIN.Z");
  if (!fbuf)
    return nullptr;
  size_t size;
  auto *data = FileReadCpp(fbuf, &size, nullptr);
  if (!data) {
    Free(fbuf);
    return nullptr;
  }
  auto *absname = FileNameAbs(fbuf, task);
  Free(fbuf);
  if (!absname) {
    Free(data);
    return nullptr;
  }
  const char *path =
      *absname && absname[1] == ':' && absname[2] ? absname + 2 : nullptr;
  CLoadError error;
  auto *module =
      LoadImage(data, size, path, task, static_cast<unsigned>(ld_flags),
                reinterpret_cast<uintptr_t>(bfh_addr), error);
  Free(data);
  Free(absname);
  if (error == LOAD_BAD_IMAGE)
    RaiseHolyC(0x6C75646F4D6E6942ULL); // BinModul
  return module ? reinterpret_cast<CBinFile *>(module->image) : nullptr;
}

bool LoadKernel(CTask &task)
{
  struct CKernelExport
  {
    const char *name;
    uintptr_t value;
  };
  static const CKernelExport exports[]{
      {"SYS_COMPILE_TIME", reinterpret_cast<uintptr_t>(SYS_COMPILE_TIME)},
      {"SYS_CPU_STRUCTS", reinterpret_cast<uintptr_t>(&cpu_structs)},
      {"SYS_MP_CNT", reinterpret_cast<uintptr_t>(&mp_cnt)},
      {"cpu_structs", reinterpret_cast<uintptr_t>(&cpu_structs)},
      {"mp_cnt", reinterpret_cast<uintptr_t>(&mp_cnt)},
      {"cnts", reinterpret_cast<uintptr_t>(&cnts)},
      {"local_time_offset", reinterpret_cast<uintptr_t>(&local_time_offset)},
      {"mon_start_days1", reinterpret_cast<uintptr_t>(mon_start_days1)},
      {"mon_start_days2", reinterpret_cast<uintptr_t>(mon_start_days2)},
      {"kbd", reinterpret_cast<uintptr_t>(&kbd)},
      {"keydev", reinterpret_cast<uintptr_t>(&keydev)},
      {"acd", reinterpret_cast<uintptr_t>(&acd)},
      {"text", reinterpret_cast<uintptr_t>(&text)},
      {"ms", reinterpret_cast<uintptr_t>(&ms)},
      {"ms_hard", reinterpret_cast<uintptr_t>(&ms_hard)},
      {"ms_grid", reinterpret_cast<uintptr_t>(&ms_grid)},
      {"gr_rainbow_10", reinterpret_cast<uintptr_t>(gr_rainbow_10)},
      {"gr_palette_std", reinterpret_cast<uintptr_t>(gr_palette_std)},
      {"gr_palette_gray", reinterpret_cast<uintptr_t>(gr_palette_gray)},
      {"gr", reinterpret_cast<uintptr_t>(&gr)},
      {"sys_winmgr_task", reinterpret_cast<uintptr_t>(&sys_winmgr_task)},
      {"sys_focus_task", reinterpret_cast<uintptr_t>(&sys_focus_task)},
      {"sys_task_being_scrn_updated",
       reinterpret_cast<uintptr_t>(&sys_task_being_scrn_updated)},
      {"winmgr", reinterpret_cast<uintptr_t>(&winmgr)},
      {"scrncast", reinterpret_cast<uintptr_t>(&scrncast)},
      {"music", reinterpret_cast<uintptr_t>(&music)},
      {"god", reinterpret_cast<uintptr_t>(&god)},
      {"MEM_BOOT_BASE", reinterpret_cast<uintptr_t>(&MEM_BOOT_BASE)},
      {"SYS_BOOT_SRC", reinterpret_cast<uintptr_t>(&SYS_BOOT_SRC)},
      {"SYS_RUN_LEVEL", reinterpret_cast<uintptr_t>(&SYS_RUN_LEVEL)},
      {"sys_run_level", reinterpret_cast<uintptr_t>(&SYS_RUN_LEVEL)},
      {"SYS_MEM_INIT_FLAG", reinterpret_cast<uintptr_t>(&sys_mem_init_flag)},
      {"SYS_MEM_INIT_VAL", reinterpret_cast<uintptr_t>(&sys_mem_init_val)},
      {"SYS_HEAP_INIT_FLAG", reinterpret_cast<uintptr_t>(&sys_heap_init_flag)},
      {"SYS_HEAP_INIT_VAL", reinterpret_cast<uintptr_t>(&sys_heap_init_val)},
      {"SYS_VAR_INIT_FLAG", reinterpret_cast<uintptr_t>(&sys_var_init_flag)},
      {"SYS_VAR_INIT_VAL", reinterpret_cast<uintptr_t>(&sys_var_init_val)},
      {"adam_task", reinterpret_cast<uintptr_t>(&adam_task)},
      {"sys_os_version", reinterpret_cast<uintptr_t>(&sys_os_version)},
      {"SYS_PROGRESSES", reinterpret_cast<uintptr_t>(sys_progresses)},
      {"sys_progresses", reinterpret_cast<uintptr_t>(sys_progresses)},
#define PROGRESS_EXPORTS(num, i)                                               \
  {"SYS_PROGRESS" #num, reinterpret_cast<uintptr_t>(&sys_progresses[i].val)},  \
      {"SYS_PROGRESS" #num "_MAX",                                             \
       reinterpret_cast<uintptr_t>(&sys_progresses[i].max)},                   \
      {"SYS_PROGRESS" #num "_T0",                                              \
       reinterpret_cast<uintptr_t>(&sys_progresses[i].t0)},                    \
      {"SYS_PROGRESS" #num "_TF",                                              \
       reinterpret_cast<uintptr_t>(&sys_progresses[i].tf)},                    \
      {"SYS_PROGRESS" #num "_DESC",                                            \
       reinterpret_cast<uintptr_t>(sys_progresses[i].desc)},                   \
      {"progress" #num, reinterpret_cast<uintptr_t>(&sys_progresses[i].val)},  \
      {"progress" #num "_max",                                                 \
       reinterpret_cast<uintptr_t>(&sys_progresses[i].max)},                   \
      {"progress" #num "_t0",                                                  \
       reinterpret_cast<uintptr_t>(&sys_progresses[i].t0)},                    \
      {"progress" #num "_tf",                                                  \
       reinterpret_cast<uintptr_t>(&sys_progresses[i].tf)},                    \
      {"progress" #num "_desc",                                                \
       reinterpret_cast<uintptr_t>(sys_progresses[i].desc)}
      PROGRESS_EXPORTS(1, 0),
      PROGRESS_EXPORTS(2, 1),
      PROGRESS_EXPORTS(3, 2),
      PROGRESS_EXPORTS(4, 3),
#undef PROGRESS_EXPORTS
      {"sys_registry_doc", reinterpret_cast<uintptr_t>(&sys_registry_doc)},
      {"sys_msg_flags", reinterpret_cast<uintptr_t>(sys_msg_flags)},
      {"registry_version", reinterpret_cast<uintptr_t>(&registry_version)},
      {"ins_reg", reinterpret_cast<uintptr_t>(&ins_reg)},
      {"blkdev", reinterpret_cast<uintptr_t>(&blkdev)},
      {"blkdev_text_attr", reinterpret_cast<uintptr_t>(blkdev_text_attr)},
      {"drv_text_attr", reinterpret_cast<uintptr_t>(drv_text_attr)},
      {"dbg", reinterpret_cast<uintptr_t>(&dbg)},
      {"sys_semas", reinterpret_cast<uintptr_t>(sys_semas)},
      {"_BEQU", reinterpret_cast<uintptr_t>(_BEQU)},
      {"_BIT_FIELD_EXT_U32", reinterpret_cast<uintptr_t>(_BIT_FIELD_EXT_U32)},
      {"_BIT_FIELD_OR_U32", reinterpret_cast<uintptr_t>(_BIT_FIELD_OR_U32)},
      {"_LBEQU", reinterpret_cast<uintptr_t>(_LBEQU)},
      {"BEqu", reinterpret_cast<uintptr_t>(_BEQU)},
      {"BFieldExtU32", reinterpret_cast<uintptr_t>(_BIT_FIELD_EXT_U32)},
      {"BFieldOrU32", reinterpret_cast<uintptr_t>(_BIT_FIELD_OR_U32)},
      {"LBEqu", reinterpret_cast<uintptr_t>(_LBEQU)},
      {"_CALL", reinterpret_cast<uintptr_t>(_CALL)},
      {"Call", reinterpret_cast<uintptr_t>(_CALL)},
      {"_CLAMP_I64", reinterpret_cast<uintptr_t>(_CLAMP_I64)},
      {"ClampI64", reinterpret_cast<uintptr_t>(_CLAMP_I64)},
      {"_MEMCPY", reinterpret_cast<uintptr_t>(_MEMCPY)},
      {"MemCpy", reinterpret_cast<uintptr_t>(_MEMCPY)},
      {"_MEMSET", reinterpret_cast<uintptr_t>(_MEMSET)},
      {"MemSet", reinterpret_cast<uintptr_t>(_MEMSET)},
      {"MemSetU8", reinterpret_cast<uintptr_t>(_MEMSET)},
      {"_MEMSET_U16", reinterpret_cast<uintptr_t>(_MEMSET_U16)},
      {"MemSetU16", reinterpret_cast<uintptr_t>(_MEMSET_U16)},
      {"_MEMSET_U32", reinterpret_cast<uintptr_t>(_MEMSET_U32)},
      {"MemSetU32", reinterpret_cast<uintptr_t>(_MEMSET_U32)},
      {"_MEMCMP", reinterpret_cast<uintptr_t>(_MEMCMP)},
      {"MemCmp", reinterpret_cast<uintptr_t>(_MEMCMP)},
      {"_MEMSET_I64", reinterpret_cast<uintptr_t>(_MEMSET_I64)},
      {"MemSetI64", reinterpret_cast<uintptr_t>(_MEMSET_I64)},
      {"_MALLOC", reinterpret_cast<uintptr_t>(_MALLOC)},
      {"MAlloc", reinterpret_cast<uintptr_t>(_MALLOC)},
      {"_FREE", reinterpret_cast<uintptr_t>(_FREE)},
      {"Free", reinterpret_cast<uintptr_t>(_FREE)},
      {"_MHEAP_CTRL", reinterpret_cast<uintptr_t>(_MHEAP_CTRL)},
      {"_MSIZE", reinterpret_cast<uintptr_t>(_MSIZE)},
      {"MSize", reinterpret_cast<uintptr_t>(_MSIZE)},
      {"_MSIZE2", reinterpret_cast<uintptr_t>(_MSIZE2)},
      {"MSize2", reinterpret_cast<uintptr_t>(_MSIZE2)},
      {"CAlloc", reinterpret_cast<uintptr_t>(CAllocHC)},
      {"MAllocIdent", reinterpret_cast<uintptr_t>(MAllocIdentHC)},
      {"StrNew", reinterpret_cast<uintptr_t>(StrNewHC)},
      {"AStrNew", reinterpret_cast<uintptr_t>(AStrNewHC)},
      {"_STRCMP", reinterpret_cast<uintptr_t>(_STRCMP)},
      {"StrCmp", reinterpret_cast<uintptr_t>(_STRCMP)},
      {"TO_UPPER", reinterpret_cast<uintptr_t>(TO_UPPER)},
      {"_STRICMP", reinterpret_cast<uintptr_t>(_STRICMP)},
      {"StrICmp", reinterpret_cast<uintptr_t>(_STRICMP)},
      {"_STRNCMP", reinterpret_cast<uintptr_t>(_STRNCMP)},
      {"StrNCmp", reinterpret_cast<uintptr_t>(_STRNCMP)},
      {"_STRNICMP", reinterpret_cast<uintptr_t>(_STRNICMP)},
      {"StrNICmp", reinterpret_cast<uintptr_t>(_STRNICMP)},
      {"_STRCPY", reinterpret_cast<uintptr_t>(_STRCPY)},
      {"StrCpy", reinterpret_cast<uintptr_t>(_STRCPY)},
      {"_STRIMATCH", reinterpret_cast<uintptr_t>(_STRIMATCH)},
      {"StrIMatch", reinterpret_cast<uintptr_t>(_STRIMATCH)},
      {"StrOcc", reinterpret_cast<uintptr_t>(StrOccHC)},
      {"_STRMATCH", reinterpret_cast<uintptr_t>(_STRMATCH)},
      {"StrMatch", reinterpret_cast<uintptr_t>(_STRMATCH)},
      {"Spaces2Tabs", reinterpret_cast<uintptr_t>(Spaces2TabsHC)},
      {"StrUtil", reinterpret_cast<uintptr_t>(StrUtilHC)},
      {"StrFirstOcc", reinterpret_cast<uintptr_t>(StrFirstOccHC)},
      {"StrFirstRem", reinterpret_cast<uintptr_t>(StrFirstRemHC)},
      {"StrLastOcc", reinterpret_cast<uintptr_t>(StrLastOccHC)},
      {"StrLastRem", reinterpret_cast<uintptr_t>(StrLastRemHC)},
      {"StrFind", reinterpret_cast<uintptr_t>(StrFindHC)},
      {"Tabs2Spaces", reinterpret_cast<uintptr_t>(Tabs2SpacesHC)},
      {"ScaleIndent", reinterpret_cast<uintptr_t>(ScaleIndentHC)},
      {"MStrUtil", reinterpret_cast<uintptr_t>(MStrUtilHC)},
      {"_HASH_ADD", reinterpret_cast<uintptr_t>(_HASH_ADD)},
      {"_HASH_ADD_AFTER", reinterpret_cast<uintptr_t>(_HASH_ADD_AFTER)},
      {"_HASH_FIND", reinterpret_cast<uintptr_t>(_HASH_FIND)},
      {"HashFind", reinterpret_cast<uintptr_t>(_HASH_FIND)},
      {"_HASH_SINGLE_TABLE_FIND",
       reinterpret_cast<uintptr_t>(_HASH_SINGLE_TABLE_FIND)},
      {"HashTableNew", reinterpret_cast<uintptr_t>(HashTableNewHC)},
      {"HashTypeNum", reinterpret_cast<uintptr_t>(HashTypeNumHC)},
      {"HashVal", reinterpret_cast<uintptr_t>(HashValHC)},
      {"HashDel", reinterpret_cast<uintptr_t>(HashDelHC)},
      {"HashTableDel", reinterpret_cast<uintptr_t>(HashTableDelHC)},
      {"HashTablePurge", reinterpret_cast<uintptr_t>(HashTablePurgeHC)},
      {"HashAdd", reinterpret_cast<uintptr_t>(_HASH_ADD)},
      {"HashSingleTableFind", reinterpret_cast<uintptr_t>(_HASH_SINGLE_TABLE_FIND)},
      {"HashSrcFileSet", reinterpret_cast<uintptr_t>(HashSrcFileSetHC)},
      {"HashPublic", reinterpret_cast<uintptr_t>(HashPublicHC)},
      {"HashGenericAdd", reinterpret_cast<uintptr_t>(HashGenericAddHC)},
      {"FramePtr", reinterpret_cast<uintptr_t>(FramePtrHC)},
      {"FramePtrAdd", reinterpret_cast<uintptr_t>(FramePtrAddHC)},
      {"FramePtrSet", reinterpret_cast<uintptr_t>(FramePtrSetHC)},
      {"FramePtrDel", reinterpret_cast<uintptr_t>(FramePtrDelHC)},
      {"_HASH_REM_DEL", reinterpret_cast<uintptr_t>(_HASH_REM_DEL)},
      {"HashRemDel", reinterpret_cast<uintptr_t>(_HASH_REM_DEL)},
      {"SysSymImportsResolve",
       reinterpret_cast<uintptr_t>(SysSymImportsResolveHC)},
      {"LoadPass2", reinterpret_cast<uintptr_t>(LoadPass2HC)},
      {"Load", reinterpret_cast<uintptr_t>(LoadHC)},
      {"Clamp", reinterpret_cast<uintptr_t>(ClampHC)},
      {"Min", reinterpret_cast<uintptr_t>(MinHC)},
      {"Max", reinterpret_cast<uintptr_t>(MaxHC)},
      {"Pow10I64", reinterpret_cast<uintptr_t>(Pow10I64HC)},
      {"FloorU64", reinterpret_cast<uintptr_t>(FloorU64HC)},
      {"CeilU64", reinterpret_cast<uintptr_t>(CeilU64HC)},
      {"RoundI64", reinterpret_cast<uintptr_t>(RoundI64HC)},
      {"FloorI64", reinterpret_cast<uintptr_t>(FloorI64HC)},
      {"CeilI64", reinterpret_cast<uintptr_t>(CeilI64HC)},
      {"LinkedLstDel", reinterpret_cast<uintptr_t>(LinkedLstDelHC)},
      {"LinkedLstCopy", reinterpret_cast<uintptr_t>(LinkedLstCopyHC)},
      {"LinkedLstCnt", reinterpret_cast<uintptr_t>(LinkedLstCntHC)},
      {"LinkedLstSize", reinterpret_cast<uintptr_t>(LinkedLstSizeHC)},
      {"QueDel", reinterpret_cast<uintptr_t>(QueDelHC)},
      {"QueCopy", reinterpret_cast<uintptr_t>(QueCopyHC)},
      {"QueCnt", reinterpret_cast<uintptr_t>(QueCntHC)},
      {"QueSize", reinterpret_cast<uintptr_t>(QueSizeHC)},
      {"QueVectU8New", reinterpret_cast<uintptr_t>(QueVectU8NewHC)},
      {"QueVectU8Put", reinterpret_cast<uintptr_t>(QueVectU8PutHC)},
      {"QueVectU8Get", reinterpret_cast<uintptr_t>(QueVectU8GetHC)},
      {"QueVectU8Del", reinterpret_cast<uintptr_t>(QueVectU8DelHC)},
      {"FifoU8New", reinterpret_cast<uintptr_t>(FifoU8NewHC)},
      {"FifoU8Del", reinterpret_cast<uintptr_t>(FifoU8DelHC)},
      {"FifoU8Ins", reinterpret_cast<uintptr_t>(FifoU8InsHC)},
      {"FifoU8Rem", reinterpret_cast<uintptr_t>(FifoU8RemHC)},
      {"FifoU8Peek", reinterpret_cast<uintptr_t>(FifoU8PeekHC)},
      {"FifoU8Flush", reinterpret_cast<uintptr_t>(FifoU8FlushHC)},
      {"FifoU8Cnt", reinterpret_cast<uintptr_t>(FifoU8CntHC)},
      {"FifoI64New", reinterpret_cast<uintptr_t>(FifoI64NewHC)},
      {"FifoI64Del", reinterpret_cast<uintptr_t>(FifoI64DelHC)},
      {"FifoI64Ins", reinterpret_cast<uintptr_t>(FifoI64InsHC)},
      {"FifoI64Rem", reinterpret_cast<uintptr_t>(FifoI64RemHC)},
      {"FifoI64Peek", reinterpret_cast<uintptr_t>(FifoI64PeekHC)},
      {"FifoI64Flush", reinterpret_cast<uintptr_t>(FifoI64FlushHC)},
      {"FifoI64Cnt", reinterpret_cast<uintptr_t>(FifoI64CntHC)},
      {"QSort", reinterpret_cast<uintptr_t>(QSortHC)},
      {"QSortI64", reinterpret_cast<uintptr_t>(QSortI64HC)},
      {"LstMatch", reinterpret_cast<uintptr_t>(LstMatchHC)},
      {"FileRead", reinterpret_cast<uintptr_t>(FileReadHC)},
      {"FileNameAbs", reinterpret_cast<uintptr_t>(FileNameAbsHC)},
      {"DirNameAbs", reinterpret_cast<uintptr_t>(DirNameAbsHC)},
      {"DirCur", reinterpret_cast<uintptr_t>(DirCurHC)},
      {"DirFile", reinterpret_cast<uintptr_t>(DirFileHC)},
      {"ExtChg", reinterpret_cast<uintptr_t>(ExtChgHC)},
      {"ExtDft", reinterpret_cast<uintptr_t>(ExtDftHC)},
      {"FileExtDot", reinterpret_cast<uintptr_t>(FileExtDotHC)},
      {"FileExtRem", reinterpret_cast<uintptr_t>(FileExtRemHC)},
      {"IsDotZ", reinterpret_cast<uintptr_t>(IsDotZHC)},
      {"IsDotC", reinterpret_cast<uintptr_t>(IsDotCHC)},
      {"FileNameChk", reinterpret_cast<uintptr_t>(FileNameChkHC)},
      {"ToggleZorNotZ", reinterpret_cast<uintptr_t>(ToggleZorNotZHC)},
      {"FileNameTmpTxt", reinterpret_cast<uintptr_t>(FileNameTmpTxtHC)},
      {"CFileNameTo", reinterpret_cast<uintptr_t>(CFileNameToHC)},
      {"Name2DirClus", reinterpret_cast<uintptr_t>(Name2DirClusHC)},
      {"Name2ParentDirClus", reinterpret_cast<uintptr_t>(Name2ParentDirClusHC)},
      {"Cd2DirEntry", reinterpret_cast<uintptr_t>(Cd2DirEntryHC)},
      {"DirEntryCompareName", reinterpret_cast<uintptr_t>(DirEntryCompareNameHC)},
      {"DirEntryCompareClus", reinterpret_cast<uintptr_t>(DirEntryCompareClusHC)},
      {"DirFilesSort", reinterpret_cast<uintptr_t>(DirFilesSortHC)},
      {"DirFilesFlatten", reinterpret_cast<uintptr_t>(DirFilesFlattenHC)},
      {"FilesFind2", reinterpret_cast<uintptr_t>(FilesFind2HC)},
      {"DrvLock", reinterpret_cast<uintptr_t>(DrvLockHC)},
      {"DrvUnlock", reinterpret_cast<uintptr_t>(DrvUnlockHC)},
      {"DrvsRelease", reinterpret_cast<uintptr_t>(DrvsReleaseHC)},
      {"BlkDevLock", reinterpret_cast<uintptr_t>(BlkDevLockHC)},
      {"BlkDevUnlock", reinterpret_cast<uintptr_t>(BlkDevUnlockHC)},
      {"BlkDevInit", reinterpret_cast<uintptr_t>(BlkDevInitHC)},
      {"BlkDevsRelease", reinterpret_cast<uintptr_t>(BlkDevsReleaseHC)},
      {"DskCacheInit", reinterpret_cast<uintptr_t>(DskCacheInitHC)},
      {"DskCacheAdd", reinterpret_cast<uintptr_t>(DskCacheAddHC)},
      {"DskCacheInvalidate2", reinterpret_cast<uintptr_t>(DskCacheInvalidate2HC)},
      {"RCache", reinterpret_cast<uintptr_t>(RCacheHC)},
      {"DrvMakeFreeSlot", reinterpret_cast<uintptr_t>(DrvMakeFreeSlotHC)},
      {"DrvNextFreeLet", reinterpret_cast<uintptr_t>(DrvNextFreeLetHC)},
      {"DrvChk", reinterpret_cast<uintptr_t>(DrvChkHC)},
      {"DrvFATBlkAlloc", reinterpret_cast<uintptr_t>(DrvFATBlkAllocHC)},
      {"DrvEnable", reinterpret_cast<uintptr_t>(DrvEnableHC)},
      {"DrvDel", reinterpret_cast<uintptr_t>(DrvDelHC)},
      {"DrvBlkDevDel", reinterpret_cast<uintptr_t>(DrvBlkDevDelHC)},
      {"BlkDevDel", reinterpret_cast<uintptr_t>(BlkDevDelHC)},
      {"DskCacheInvalidate", reinterpret_cast<uintptr_t>(DskCacheInvalidateHC)},
      {"DskChg", reinterpret_cast<uintptr_t>(DskChgHC)},
      {"DrvFATBlkClean", reinterpret_cast<uintptr_t>(DrvFATBlkCleanHC)},
      {"DrvFATBlkSet", reinterpret_cast<uintptr_t>(DrvFATBlkSetHC)},
      {"Drv2Let", reinterpret_cast<uintptr_t>(Drv2LetHC)},
      {"Let2Let", reinterpret_cast<uintptr_t>(Let2LetHC)},
      {"Let2BlkDevType", reinterpret_cast<uintptr_t>(Let2BlkDevTypeHC)},
      {"DrvIsWritable", reinterpret_cast<uintptr_t>(DrvIsWritableHC)},
      {"DrvMap", reinterpret_cast<uintptr_t>(DrvMapHC)},
      {"Drv", reinterpret_cast<uintptr_t>(DrvHC)},
      {"DrvSerialNum", reinterpret_cast<uintptr_t>(DrvSerialNumHC)},
      {"DrvModelNum", reinterpret_cast<uintptr_t>(DrvModelNumHC)},
      {"DrvTextAttrGet", reinterpret_cast<uintptr_t>(DrvTextAttrGetHC)},
      {"DrvRep", reinterpret_cast<uintptr_t>(DrvRepHC)},
      {"BlkDevChk", reinterpret_cast<uintptr_t>(BlkDevChkHC)},
      {"Let2BlkDev", reinterpret_cast<uintptr_t>(Let2BlkDevHC)},
      {"Unmount", reinterpret_cast<uintptr_t>(UnmountHC)},
      {"MountFile", reinterpret_cast<uintptr_t>(MountFileHC)},
      {"Cd", reinterpret_cast<uintptr_t>(CdHC)},
      {"IsDir", reinterpret_cast<uintptr_t>(IsDirHC)},
      {"Dir", reinterpret_cast<uintptr_t>(DirHC)},
      {"PutDirLink", reinterpret_cast<uintptr_t>(PutDirLinkHC)},
      {"FilesFind", reinterpret_cast<uintptr_t>(FilesFindHC)},
      {"FileFind", reinterpret_cast<uintptr_t>(FileFindHC)},
      {"FilesFindMatch", reinterpret_cast<uintptr_t>(FilesFindMatchHC)},
      {"DirEntryDel", reinterpret_cast<uintptr_t>(DirEntryDelHC)},
      {"DirEntryDel2", reinterpret_cast<uintptr_t>(DirEntryDel2HC)},
      {"DirTreeDel", reinterpret_cast<uintptr_t>(DirTreeDelHC)},
      {"DirTreeDel2", reinterpret_cast<uintptr_t>(DirTreeDel2HC)},
      {"DirContextNew", reinterpret_cast<uintptr_t>(DirContextNewHC)},
      {"DirContextDel", reinterpret_cast<uintptr_t>(DirContextDelHC)},
      {"Str2I64", reinterpret_cast<uintptr_t>(Str2I64HC)},
      {"Str2Date", reinterpret_cast<uintptr_t>(Str2DateHC)},
      {"Str2F64", reinterpret_cast<uintptr_t>(Str2F64HC)},
      {"StrScan", reinterpret_cast<uintptr_t>(StrScanHC)},
      {"ChkPtr", reinterpret_cast<uintptr_t>(ChkPtrHC)},
      {"D", reinterpret_cast<uintptr_t>(DHC)},
      {"Dm", reinterpret_cast<uintptr_t>(DmHC)},
      {"Da", reinterpret_cast<uintptr_t>(DaHC)},
      {"ClassRep2", reinterpret_cast<uintptr_t>(ClassRep2HC)},
      {"ClassRep", reinterpret_cast<uintptr_t>(ClassRepHC)},
      {"ClassRepD", reinterpret_cast<uintptr_t>(ClassRepDHC)},
      {"DocMax", reinterpret_cast<uintptr_t>(DocMaxHC)},
      {"DocDataFmt", reinterpret_cast<uintptr_t>(DocDataFmtHC)},
      {"DocDataScan", reinterpret_cast<uintptr_t>(DocDataScanHC)},
      {"DocForm", reinterpret_cast<uintptr_t>(DocFormHC)},
      {"DocMenu", reinterpret_cast<uintptr_t>(DocMenuHC)},
      {"PopUpMenu", reinterpret_cast<uintptr_t>(PopUpMenuHC)},
      {"PopUp1", reinterpret_cast<uintptr_t>(PopUp1HC)},
      {"PopUp2", reinterpret_cast<uintptr_t>(PopUp2HC)},
      {"PopUpOk", reinterpret_cast<uintptr_t>(PopUpOkHC)},
      {"PopUpCancelOk", reinterpret_cast<uintptr_t>(PopUpCancelOkHC)},
      {"PopUpForm", reinterpret_cast<uintptr_t>(PopUpFormHC)},
      {"DocEd", reinterpret_cast<uintptr_t>(DocEdHC)},
      {"Ed", reinterpret_cast<uintptr_t>(EdHC)},
      {"PopUpEd", reinterpret_cast<uintptr_t>(PopUpEdHC)},
      {"DocEntryLink", reinterpret_cast<uintptr_t>(DocEntryLinkHC)},
      {"EdLinkCvt", reinterpret_cast<uintptr_t>(EdLinkCvtHC)},
      {"DocGoToLine", reinterpret_cast<uintptr_t>(DocGoToLineHC)},
      {"DocFind", reinterpret_cast<uintptr_t>(DocFindHC)},
      {"DocAnchorFind", reinterpret_cast<uintptr_t>(DocAnchorFindHC)},
      {"DocFormFwd", reinterpret_cast<uintptr_t>(DocFormFwdHC)},
      {"DocFormBwd", reinterpret_cast<uintptr_t>(DocFormBwdHC)},
      {"View", reinterpret_cast<uintptr_t>(ViewHC)},
      {"HomeSet", reinterpret_cast<uintptr_t>(HomeSetHC)},
      {"ToUpper", reinterpret_cast<uintptr_t>(ToUpperHC)},
      {"FileWrite", reinterpret_cast<uintptr_t>(FileWriteHC)},
      {"FSize", reinterpret_cast<uintptr_t>(FSizeHC)},
      {"FOpen", reinterpret_cast<uintptr_t>(FOpenHC)},
      {"FClose", reinterpret_cast<uintptr_t>(FCloseHC)},
      {"FBlkRead", reinterpret_cast<uintptr_t>(FBlkReadHC)},
      {"FBlkWrite", reinterpret_cast<uintptr_t>(FBlkWriteHC)},
      {"FileAttr", reinterpret_cast<uintptr_t>(FileAttrHC)},
      {"CopySingle", reinterpret_cast<uintptr_t>(CopySingleHC)},
      {"Del", reinterpret_cast<uintptr_t>(DelHC)},
      {"Copy", reinterpret_cast<uintptr_t>(CopyHC)},
      {"Move", reinterpret_cast<uintptr_t>(MoveHC)},
      {"CopyTree", reinterpret_cast<uintptr_t>(CopyTreeHC)},
      {"DelTree", reinterpret_cast<uintptr_t>(DelTreeHC)},
      {"DirMk", reinterpret_cast<uintptr_t>(DirMkHC)},
      {"ScanFlags", reinterpret_cast<uintptr_t>(ScanFlagsHC)},
      {"RedSeaISO", reinterpret_cast<uintptr_t>(RedSeaISOHC)},
      {"RedSeaISO9660", reinterpret_cast<uintptr_t>(RedSeaISO9660HC)},
      {"BlkDevNextFreeSlot", reinterpret_cast<uintptr_t>(BlkDevNextFreeSlotHC)},
      {"BlkDevAdd", reinterpret_cast<uintptr_t>(BlkDevAddHC)},
      {"Let2Drv", reinterpret_cast<uintptr_t>(Let2DrvHC)},
      {"BlkRead", reinterpret_cast<uintptr_t>(BlkReadHC)},
      {"BlkWrite", reinterpret_cast<uintptr_t>(BlkWriteHC)},
      {"ClusNumNext", reinterpret_cast<uintptr_t>(ClusNumNextHC)},
      {"Clus2Blk", reinterpret_cast<uintptr_t>(Clus2BlkHC)},
      {"ClusBlkRead", reinterpret_cast<uintptr_t>(ClusBlkReadHC)},
      {"ClusRead", reinterpret_cast<uintptr_t>(ClusReadHC)},
      {"ClusBlkWrite", reinterpret_cast<uintptr_t>(ClusBlkWriteHC)},
      {"ClusWrite", reinterpret_cast<uintptr_t>(ClusWriteHC)},
      {"ClusAlloc", reinterpret_cast<uintptr_t>(ClusAllocHC)},
      {"FAT32AllocClus", reinterpret_cast<uintptr_t>(FAT32AllocClusHC)},
      {"FAT32AllocContiguousClus", reinterpret_cast<uintptr_t>(FAT32AllocContiguousClusHC)},
      {"FAT32FreeClus", reinterpret_cast<uintptr_t>(FAT32FreeClusHC)},
      {"RedSeaAllocClus", reinterpret_cast<uintptr_t>(RedSeaAllocClusHC)},
      {"RedSeaFreeClus", reinterpret_cast<uintptr_t>(RedSeaFreeClusHC)},
      {"RedSeaFreeFreeLst", reinterpret_cast<uintptr_t>(RedSeaFreeFreeLstHC)},
      {"RedSeaFreeLstBuild", reinterpret_cast<uintptr_t>(RedSeaFreeLstBuildHC)},
      {"RedSeaValidate", reinterpret_cast<uintptr_t>(RedSeaValidateHC)},
      {"RedSeaFmt", reinterpret_cast<uintptr_t>(RedSeaFmtHC)},
      {"FAT32DirNew", reinterpret_cast<uintptr_t>(FAT32DirNewHC)},
      {"RedSeaDirNew", reinterpret_cast<uintptr_t>(RedSeaDirNewHC)},
      {"DirNew", reinterpret_cast<uintptr_t>(DirNewHC)},
      {"Fmt", reinterpret_cast<uintptr_t>(FmtHC)},
      {"SysTry", reinterpret_cast<uintptr_t>(SysTryHC)},
      {"SysUntry", reinterpret_cast<uintptr_t>(SysUntryHC)},
      {"throw", reinterpret_cast<uintptr_t>(ThrowHC)},
      {"Caller", reinterpret_cast<uintptr_t>(CallerHC)},
      {"PutExcept", reinterpret_cast<uintptr_t>(PutExceptHC)},
      {"Break", reinterpret_cast<uintptr_t>(BreakHC)},
      {"BreakLock", reinterpret_cast<uintptr_t>(BreakLockHC)},
      {"BreakUnlock", reinterpret_cast<uintptr_t>(BreakUnlockHC)},
      {"YorN", reinterpret_cast<uintptr_t>(YorNHC)},
      {"StrPrintJoin", reinterpret_cast<uintptr_t>(StrPrintJoinHC)},
      {"StrPrint", reinterpret_cast<uintptr_t>(StrPrintHC)},
      {"CatPrint", reinterpret_cast<uintptr_t>(CatPrintHC)},
      {"MStrPrint", reinterpret_cast<uintptr_t>(MStrPrintHC)},
      {"Print", reinterpret_cast<uintptr_t>(PrintHC)},
      {"PrintErr", reinterpret_cast<uintptr_t>(PrintErrHC)},
      {"PrintWarn", reinterpret_cast<uintptr_t>(PrintWarnHC)},
      {"PUT_HEX_U64", reinterpret_cast<uintptr_t>(PUT_HEX_U64)},
      {"PUT_HEX_U32", reinterpret_cast<uintptr_t>(PUT_HEX_U32)},
      {"PUT_HEX_U16", reinterpret_cast<uintptr_t>(PUT_HEX_U16)},
      {"PUT_HEX_U8", reinterpret_cast<uintptr_t>(PUT_HEX_U8)},
      {"PUT_CHARS", reinterpret_cast<uintptr_t>(PUT_CHARS)},
      {"PUT_STR", reinterpret_cast<uintptr_t>(PUT_STR)},
      {"PutHex", reinterpret_cast<uintptr_t>(PutHexHC)},
      {"PutChars", reinterpret_cast<uintptr_t>(PutCharsHC)},
      {"PutS", reinterpret_cast<uintptr_t>(PutSHC)},
      {"tS", reinterpret_cast<uintptr_t>(tSHC)},
      {"Busy", reinterpret_cast<uintptr_t>(BusyHC)},
      {"SleepUntil", reinterpret_cast<uintptr_t>(SleepUntilHC)},
      {"Sleep", reinterpret_cast<uintptr_t>(SleepHC)},
      {"Yield", reinterpret_cast<uintptr_t>(_YIELD)},
      {"_YIELD", reinterpret_cast<uintptr_t>(_YIELD)},
      {"_TASK_END_NOW", reinterpret_cast<uintptr_t>(_TASK_END_NOW)},
      {"Spawn", reinterpret_cast<uintptr_t>(SpawnHC)},
      {"Exit", reinterpret_cast<uintptr_t>(Exit)},
      {"TaskValidate", reinterpret_cast<uintptr_t>(TaskValidateHC)},
      {"Kill", reinterpret_cast<uintptr_t>(KillHC)},
      {"Suspend", reinterpret_cast<uintptr_t>(SuspendHC)},
      {"IsSuspended", reinterpret_cast<uintptr_t>(IsSuspendedHC)},
      {"TaskQueIns", reinterpret_cast<uintptr_t>(TaskQueInsHC)},
      {"TaskQueRem", reinterpret_cast<uintptr_t>(TaskQueRemHC)},
      {"SingleUser", reinterpret_cast<uintptr_t>(SingleUserHC)},
      {"IsSingleUser", reinterpret_cast<uintptr_t>(IsSingleUserHC)},
      {"ProgressBarsRst", reinterpret_cast<uintptr_t>(ProgressBarsRstHC)},
      {"JobDel", reinterpret_cast<uintptr_t>(JobDelHC)},
      {"TaskRstAwaitingMsg", reinterpret_cast<uintptr_t>(TaskRstAwaitingMsgHC)},
      {"TaskExe", reinterpret_cast<uintptr_t>(TaskExeHC)},
      {"TaskText", reinterpret_cast<uintptr_t>(TaskTextHC)},
      {"TaskMsg", reinterpret_cast<uintptr_t>(TaskMsgHC)},
      {"JobResScan", reinterpret_cast<uintptr_t>(JobResScanHC)},
      {"JobResGet", reinterpret_cast<uintptr_t>(JobResGetHC)},
      {"TaskWait", reinterpret_cast<uintptr_t>(TaskWaitHC)},
      {"PostMsg", reinterpret_cast<uintptr_t>(PostMsgHC)},
      {"PostMsgWait", reinterpret_cast<uintptr_t>(PostMsgWaitHC)},
      {"Msg", reinterpret_cast<uintptr_t>(MsgHC)},
      {"JobsHndlr", reinterpret_cast<uintptr_t>(JobsHndlrHC)},
      {"Adam", reinterpret_cast<uintptr_t>(AdamHC)},
      {"XTalk", reinterpret_cast<uintptr_t>(XTalkHC)},
      {"XTalkWait", reinterpret_cast<uintptr_t>(XTalkWaitHC)},
      {"InStr", reinterpret_cast<uintptr_t>(InStrHC)},
      {"InFile", reinterpret_cast<uintptr_t>(InFileHC)},
      {"InGetStr", reinterpret_cast<uintptr_t>(InGetStrHC)},
      {"InPrint", reinterpret_cast<uintptr_t>(InPrintHC)},
      {"InGetKey", reinterpret_cast<uintptr_t>(InGetKeyHC)},
      {"InGetChar", reinterpret_cast<uintptr_t>(InGetCharHC)},
      {"InUntilKey", reinterpret_cast<uintptr_t>(InUntilKeyHC)},
      {"InUntilChar", reinterpret_cast<uintptr_t>(InUntilCharHC)},
      {"InView", reinterpret_cast<uintptr_t>(InViewHC)},
      {"InSetMs", reinterpret_cast<uintptr_t>(InSetMsHC)},
      {"In", reinterpret_cast<uintptr_t>(InHC)},
      {"XTalkStr", reinterpret_cast<uintptr_t>(XTalkStrHC)},
      {"XTalkStrWait", reinterpret_cast<uintptr_t>(XTalkStrWaitHC)},
      {"SrvTaskCont", reinterpret_cast<uintptr_t>(SrvTaskContHC)},
      {"SrvCmdLine", reinterpret_cast<uintptr_t>(SrvCmdLineHC)},
      {"PopUp", reinterpret_cast<uintptr_t>(PopUpHC)},
      {"PopUpPrint", reinterpret_cast<uintptr_t>(PopUpPrintHC)},
      {"UserCmdLine", reinterpret_cast<uintptr_t>(UserCmdLineHC)},
      {"ScanMsg", reinterpret_cast<uintptr_t>(ScanMsgHC)},
      {"FlushMsgs", reinterpret_cast<uintptr_t>(FlushMsgsHC)},
      {"GetMsg", reinterpret_cast<uintptr_t>(GetMsgHC)},
      {"InputFilterTask", reinterpret_cast<uintptr_t>(InputFilterTaskHC)},
      {"Char2ScanCode", reinterpret_cast<uintptr_t>(Char2ScanCodeHC)},
      {"ScanCode2Char", reinterpret_cast<uintptr_t>(ScanCode2CharHC)},
      {"KbdTypeMatic", reinterpret_cast<uintptr_t>(KbdTypeMaticHC)},
      {"KbdMsEvtTime", reinterpret_cast<uintptr_t>(KbdMsEvtTimeHC)},
      {"GridInit", reinterpret_cast<uintptr_t>(GridInitHC)},
      {"MsSet", reinterpret_cast<uintptr_t>(MsSetHC)},
      {"GrPaletteColorSet", reinterpret_cast<uintptr_t>(GrPaletteColorSetHC)},
      {"GrPaletteColorGet", reinterpret_cast<uintptr_t>(GrPaletteColorGetHC)},
      {"GrPaletteGet", reinterpret_cast<uintptr_t>(GrPaletteGetHC)},
      {"GrPaletteSet", reinterpret_cast<uintptr_t>(GrPaletteSetHC)},
      {"GrScaleZoom", reinterpret_cast<uintptr_t>(GrScaleZoomHC)},
      {"DrawStdMs", reinterpret_cast<uintptr_t>(DrawStdMsHC)},
      {"PaletteSetStd", reinterpret_cast<uintptr_t>(PaletteSetStdHC)},
      {"SYS_POW", reinterpret_cast<uintptr_t>(SYS_POW)},
      {"_SIGN", reinterpret_cast<uintptr_t>(_SIGN)},
      {"Sign", reinterpret_cast<uintptr_t>(_SIGN)},
      {"_ROUND", reinterpret_cast<uintptr_t>(_ROUND)},
      {"Round", reinterpret_cast<uintptr_t>(_ROUND)},
      {"_TRUNC", reinterpret_cast<uintptr_t>(_TRUNC)},
      {"Trunc", reinterpret_cast<uintptr_t>(_TRUNC)},
      {"_FLOOR", reinterpret_cast<uintptr_t>(_FLOOR)},
      {"Floor", reinterpret_cast<uintptr_t>(_FLOOR)},
      {"_CEIL", reinterpret_cast<uintptr_t>(_CEIL)},
      {"Ceil", reinterpret_cast<uintptr_t>(_CEIL)},
      {"_POW", reinterpret_cast<uintptr_t>(_POW)},
      {"Pow", reinterpret_cast<uintptr_t>(_POW)},
      {"_POW10", reinterpret_cast<uintptr_t>(_POW10)},
      {"Pow10", reinterpret_cast<uintptr_t>(_POW10)},
      {"_EXP", reinterpret_cast<uintptr_t>(_EXP)},
      {"Exp", reinterpret_cast<uintptr_t>(_EXP)},
      {"_LOG10", reinterpret_cast<uintptr_t>(_LOG10)},
      {"Log10", reinterpret_cast<uintptr_t>(_LOG10)},
      {"_LOG2", reinterpret_cast<uintptr_t>(_LOG2)},
      {"Log2", reinterpret_cast<uintptr_t>(_LOG2)},
      {"_LN", reinterpret_cast<uintptr_t>(_LN)},
      {"Ln", reinterpret_cast<uintptr_t>(_LN)},
      {"_FLDCW", reinterpret_cast<uintptr_t>(_FLDCW)},
      {"Fldcw", reinterpret_cast<uintptr_t>(_FLDCW)},
      {"_FSTCW", reinterpret_cast<uintptr_t>(_FSTCW)},
      {"Fstcw", reinterpret_cast<uintptr_t>(_FSTCW)},
      {"_FSTSW", reinterpret_cast<uintptr_t>(_FSTSW)},
      {"Fstsw", reinterpret_cast<uintptr_t>(_FSTSW)},
      {"_FCLEX", reinterpret_cast<uintptr_t>(_FCLEX)},
      {"Fclex", reinterpret_cast<uintptr_t>(_FCLEX)},
      {"SignI64", reinterpret_cast<uintptr_t>(SignI64HC)},
      {"RandI16", reinterpret_cast<uintptr_t>(RandI16HC)},
      {"RandU16", reinterpret_cast<uintptr_t>(RandU16HC)},
      {"RandI32", reinterpret_cast<uintptr_t>(RandI32HC)},
      {"RandU32", reinterpret_cast<uintptr_t>(RandU32HC)},
      {"RandI64", reinterpret_cast<uintptr_t>(RandI64HC)},
      {"RandU64", reinterpret_cast<uintptr_t>(RandU64HC)},
      {"Rand", reinterpret_cast<uintptr_t>(RandHC)},
      {"Seed", reinterpret_cast<uintptr_t>(SeedHC)},
      {"IsPixCovered0", reinterpret_cast<uintptr_t>(IsPixCovered0HC)},
      {"GrPlot0", reinterpret_cast<uintptr_t>(GrPlot0HC)},
      {"GrPeek0", reinterpret_cast<uintptr_t>(GrPeek0HC)},
      {"GrBlot", reinterpret_cast<uintptr_t>(GrBlotHC)},
      {"GrRect", reinterpret_cast<uintptr_t>(GrRectHC)},
      {"GrRectB", reinterpret_cast<uintptr_t>(GrRectBHC)},
      {"GrClamp", reinterpret_cast<uintptr_t>(GrClampHC)},
      {"GrPlot", reinterpret_cast<uintptr_t>(GrPlotHC)},
      {"GrFillCircle", reinterpret_cast<uintptr_t>(GrFillCircleHC)},
      {"GrPlot3B", reinterpret_cast<uintptr_t>(GrPlot3BHC)},
      {"GrPlot3", reinterpret_cast<uintptr_t>(GrPlot3HC)},
      {"SpriteSize", reinterpret_cast<uintptr_t>(SpriteSizeHC)},
      {"DC2Sprite", reinterpret_cast<uintptr_t>(DC2SpriteHC)},
      {"DCExt", reinterpret_cast<uintptr_t>(DCExtHC)},
      {"DCDiff", reinterpret_cast<uintptr_t>(DCDiffHC)},
      {"DocGR", reinterpret_cast<uintptr_t>(DocGRHC)},
      {"Sprite3", reinterpret_cast<uintptr_t>(Sprite3HC)},
      {"Sprite3B", reinterpret_cast<uintptr_t>(Sprite3BHC)},
      {"GrBlot3", reinterpret_cast<uintptr_t>(GrBlot3HC)},
      {"GrHLine", reinterpret_cast<uintptr_t>(GrHLineHC)},
      {"GrVLine", reinterpret_cast<uintptr_t>(GrVLineHC)},
      {"GrFillTri0", reinterpret_cast<uintptr_t>(GrFillTri0HC)},
      {"GrFillPoly3", reinterpret_cast<uintptr_t>(GrFillPoly3HC)},
      {"GrRect3", reinterpret_cast<uintptr_t>(GrRect3HC)},
      {"GrBorder", reinterpret_cast<uintptr_t>(GrBorderHC)},
      {"GrArrow3", reinterpret_cast<uintptr_t>(GrArrow3HC)},
      {"GrPutChar", reinterpret_cast<uintptr_t>(GrPutCharHC)},
      {"GrPrint", reinterpret_cast<uintptr_t>(GrPrintHC)},
      {"PressAKey", reinterpret_cast<uintptr_t>(PressAKeyHC)},
      {"DCSymmetry3Set", reinterpret_cast<uintptr_t>(DCSymmetry3SetHC)},
      {"DCReflect", reinterpret_cast<uintptr_t>(DCReflectHC)},
      {"DocSprite", reinterpret_cast<uintptr_t>(DocSpriteHC)},
      {"Sprite", reinterpret_cast<uintptr_t>(SpriteHC)},
      {"GrPeek", reinterpret_cast<uintptr_t>(GrPeekHC)},
      {"GrFloodFill", reinterpret_cast<uintptr_t>(GrFloodFillHC)},
      {"GrFloodFill3", reinterpret_cast<uintptr_t>(GrFloodFill3HC)},
      {"GrLine", reinterpret_cast<uintptr_t>(GrLineHC)},
      {"GrLine3", reinterpret_cast<uintptr_t>(GrLine3HC)},
      {"Circle", reinterpret_cast<uintptr_t>(CircleHC)},
      {"Ellipse", reinterpret_cast<uintptr_t>(EllipseHC)},
      {"GrCircle", reinterpret_cast<uintptr_t>(GrCircleHC)},
      {"GrEllipse", reinterpret_cast<uintptr_t>(GrEllipseHC)},
      {"GrCircle3", reinterpret_cast<uintptr_t>(GrCircle3HC)},
      {"GrEllipse3", reinterpret_cast<uintptr_t>(GrEllipse3HC)},
      {"Arg", reinterpret_cast<uintptr_t>(ArgHC)},
      {"Line", reinterpret_cast<uintptr_t>(LineHC)},
      {"RegPoly", reinterpret_cast<uintptr_t>(RegPolyHC)},
      {"GrRegPoly", reinterpret_cast<uintptr_t>(GrRegPolyHC)},
      {"GrRegPoly3", reinterpret_cast<uintptr_t>(GrRegPoly3HC)},
      {"Bezier2", reinterpret_cast<uintptr_t>(Bezier2HC)},
      {"BSpline2", reinterpret_cast<uintptr_t>(BSpline2HC)},
      {"Gr2Bezier", reinterpret_cast<uintptr_t>(Gr2BezierHC)},
      {"Gr2Bezier3", reinterpret_cast<uintptr_t>(Gr2Bezier3HC)},
      {"Gr2BSpline", reinterpret_cast<uintptr_t>(Gr2BSplineHC)},
      {"Gr2BSpline3", reinterpret_cast<uintptr_t>(Gr2BSpline3HC)},
      {"Bezier3", reinterpret_cast<uintptr_t>(Bezier3HC)},
      {"BSpline3", reinterpret_cast<uintptr_t>(BSpline3HC)},
      {"Gr3Bezier", reinterpret_cast<uintptr_t>(Gr3BezierHC)},
      {"Gr3Bezier3", reinterpret_cast<uintptr_t>(Gr3Bezier3HC)},
      {"Gr3BSpline", reinterpret_cast<uintptr_t>(Gr3BSplineHC)},
      {"Gr3BSpline3", reinterpret_cast<uintptr_t>(Gr3BSpline3HC)},
      {"D3I32Dist", reinterpret_cast<uintptr_t>(D3I32DistHC)},
      {"D3I32DistSqr", reinterpret_cast<uintptr_t>(D3I32DistSqrHC)},
      {"D3I32NormSqr", reinterpret_cast<uintptr_t>(D3I32NormSqrHC)},
      {"DCThickScale", reinterpret_cast<uintptr_t>(DCThickScaleHC)},
      {"ClipLine", reinterpret_cast<uintptr_t>(ClipLineHC)},
      {"Mat4x4MulXYZ", reinterpret_cast<uintptr_t>(Mat4x4MulXYZHC)},
      {"D3I32Norm", reinterpret_cast<uintptr_t>(D3I32NormHC)},
      {"DCTransform", reinterpret_cast<uintptr_t>(DCTransformHC)},
      {"Mat4x4IdentEqu", reinterpret_cast<uintptr_t>(Mat4x4IdentEquHC)},
      {"Mat4x4IdentNew", reinterpret_cast<uintptr_t>(Mat4x4IdentNewHC)},
      {"Mat4x4NormSqr65536", reinterpret_cast<uintptr_t>(Mat4x4NormSqr65536HC)},
      {"DCMat4x4Set", reinterpret_cast<uintptr_t>(DCMat4x4SetHC)},
      {"DCLighting", reinterpret_cast<uintptr_t>(DCLightingHC)},
      {"DCFill", reinterpret_cast<uintptr_t>(DCFillHC)},
      {"DCClear", reinterpret_cast<uintptr_t>(DCClearHC)},
      {"DCRst", reinterpret_cast<uintptr_t>(DCRstHC)},
      {"DCExtentsInit", reinterpret_cast<uintptr_t>(DCExtentsInitHC)},
      {"DCAlias", reinterpret_cast<uintptr_t>(DCAliasHC)},
      {"DCNew", reinterpret_cast<uintptr_t>(DCNewHC)},
      {"DCDel", reinterpret_cast<uintptr_t>(DCDelHC)},
      {"DCSize", reinterpret_cast<uintptr_t>(DCSizeHC)},
      {"DCDepthBufRst", reinterpret_cast<uintptr_t>(DCDepthBufRstHC)},
      {"DCDepthBufAlloc", reinterpret_cast<uintptr_t>(DCDepthBufAllocHC)},
      {"DCCopy", reinterpret_cast<uintptr_t>(DCCopyHC)},
      {"DCMono", reinterpret_cast<uintptr_t>(DCMonoHC)},
      {"DCColorChg", reinterpret_cast<uintptr_t>(DCColorChgHC)},
      {"DCSave", reinterpret_cast<uintptr_t>(DCSaveHC)},
      {"DCLoad", reinterpret_cast<uintptr_t>(DCLoadHC)},
      {"GRWrite", reinterpret_cast<uintptr_t>(GRWriteHC)},
      {"GRRead", reinterpret_cast<uintptr_t>(GRReadHC)},
      {"DCScrnCapture", reinterpret_cast<uintptr_t>(DCScrnCaptureHC)},
      {"GRScrnCaptureRead", reinterpret_cast<uintptr_t>(GRScrnCaptureReadHC)},
      {"GRScrnCaptureWrite", reinterpret_cast<uintptr_t>(GRScrnCaptureWriteHC)},
      {"Mat4x4Equ", reinterpret_cast<uintptr_t>(Mat4x4EquHC)},
      {"Mat4x4MulMat4x4Equ", reinterpret_cast<uintptr_t>(Mat4x4MulMat4x4EquHC)},
      {"Mat4x4MulMat4x4New", reinterpret_cast<uintptr_t>(Mat4x4MulMat4x4NewHC)},
      {"Mat4x4RotX", reinterpret_cast<uintptr_t>(Mat4x4RotXHC)},
      {"Mat4x4RotY", reinterpret_cast<uintptr_t>(Mat4x4RotYHC)},
      {"Mat4x4RotZ", reinterpret_cast<uintptr_t>(Mat4x4RotZHC)},
      {"Mat4x4Scale", reinterpret_cast<uintptr_t>(Mat4x4ScaleHC)},
      {"Mat4x4TranslationEqu", reinterpret_cast<uintptr_t>(Mat4x4TranslationEquHC)},
      {"Mat4x4TranslationAdd", reinterpret_cast<uintptr_t>(Mat4x4TranslationAddHC)},
      {"DCSymmetrySet", reinterpret_cast<uintptr_t>(DCSymmetrySetHC)},
      {"Refresh", reinterpret_cast<uintptr_t>(RefreshHC)},
      {"Mat4x4New", reinterpret_cast<uintptr_t>(Mat4x4NewHC)},
      {"PutKey", reinterpret_cast<uintptr_t>(PutKeyHC)},
      {"KeyDescSet", reinterpret_cast<uintptr_t>(KeyDescSetHC)},
      {"KeyDevAdd", reinterpret_cast<uintptr_t>(KeyDevAddHC)},
      {"KeyDevRem", reinterpret_cast<uintptr_t>(KeyDevRemHC)},
      {"CtrlAltCBSet", reinterpret_cast<uintptr_t>(CtrlAltCBSetHC)},
      {"ScanChar", reinterpret_cast<uintptr_t>(ScanCharHC)},
      {"ScanKey", reinterpret_cast<uintptr_t>(ScanKeyHC)},
      {"GetKey", reinterpret_cast<uintptr_t>(GetKeyHC)},
      {"GetStr", reinterpret_cast<uintptr_t>(GetStrHC)},
      {"GetI64", reinterpret_cast<uintptr_t>(GetI64HC)},
      {"GetF64", reinterpret_cast<uintptr_t>(GetF64HC)},
      {"GetDate", reinterpret_cast<uintptr_t>(GetDateHC)},
      {"GetS", reinterpret_cast<uintptr_t>(GetSHC)},
      {"GetChar", reinterpret_cast<uintptr_t>(GetCharHC)},
      {"Ona2Freq", reinterpret_cast<uintptr_t>(Ona2FreqHC)},
      {"Freq2Ona", reinterpret_cast<uintptr_t>(Freq2OnaHC)},
      {"Snd", reinterpret_cast<uintptr_t>(SndHC)},
      {"ScrnCast", reinterpret_cast<uintptr_t>(ScrnCastHC)},
      {"SndShift", reinterpret_cast<uintptr_t>(SndShiftHC)},
      {"AUWrite", reinterpret_cast<uintptr_t>(AUWriteHC)},
      {"SndRst", reinterpret_cast<uintptr_t>(SndRstHC)},
      {"SndTaskEndCB", reinterpret_cast<uintptr_t>(SndTaskEndCBHC)},
      {"Saw", reinterpret_cast<uintptr_t>(SawHC)},
      {"FullSaw", reinterpret_cast<uintptr_t>(FullSawHC)},
      {"Caw", reinterpret_cast<uintptr_t>(CawHC)},
      {"FullCaw", reinterpret_cast<uintptr_t>(FullCawHC)},
      {"Tri", reinterpret_cast<uintptr_t>(TriHC)},
      {"FullTri", reinterpret_cast<uintptr_t>(FullTriHC)},
      {"tM", reinterpret_cast<uintptr_t>(tMHC)},
      {"Beat", reinterpret_cast<uintptr_t>(BeatHC)},
      {"MusicSetOctave", reinterpret_cast<uintptr_t>(MusicSetOctaveHC)},
      {"MusicSetMeter", reinterpret_cast<uintptr_t>(MusicSetMeterHC)},
      {"MusicSetNoteLen", reinterpret_cast<uintptr_t>(MusicSetNoteLenHC)},
      {"Note2Ona", reinterpret_cast<uintptr_t>(Note2OnaHC)},
      {"Ona2Note", reinterpret_cast<uintptr_t>(Ona2NoteHC)},
      {"Ona2Octave", reinterpret_cast<uintptr_t>(Ona2OctaveHC)},
      {"Play", reinterpret_cast<uintptr_t>(PlayHC)},
      {"MusicSettingsRst", reinterpret_cast<uintptr_t>(MusicSettingsRstHC)},
      {"Noise", reinterpret_cast<uintptr_t>(NoiseHC)},
      {"Sweep", reinterpret_cast<uintptr_t>(SweepHC)},
      {"Beep", reinterpret_cast<uintptr_t>(BeepHC)},
      {"Mute", reinterpret_cast<uintptr_t>(MuteHC)},
      {"IsMute", reinterpret_cast<uintptr_t>(IsMuteHC)},
      {"IsRaw", reinterpret_cast<uintptr_t>(IsRawHC)},
      {"Raw", reinterpret_cast<uintptr_t>(RawHC)},
      {"DbgMode", reinterpret_cast<uintptr_t>(DbgModeHC)},
      {"IsDbgMode", reinterpret_cast<uintptr_t>(IsDbgModeHC)},
      {"CPURep", reinterpret_cast<uintptr_t>(CPURepHC)},
      {"PCIRep", reinterpret_cast<uintptr_t>(PCIRepHC)},
      {"MemBIOSRep", reinterpret_cast<uintptr_t>(MemBIOSRepHC)},
      {"MemPageRep", reinterpret_cast<uintptr_t>(MemPageRepHC)},
      {"PutFileLink", reinterpret_cast<uintptr_t>(PutFileLinkHC)},
      {"SrcLineNum", reinterpret_cast<uintptr_t>(SrcLineNumHC)},
      {"SrcFileName", reinterpret_cast<uintptr_t>(SrcFileNameHC)},
      {"SrcEdLink", reinterpret_cast<uintptr_t>(SrcEdLinkHC)},
      {"PutSrcLink", reinterpret_cast<uintptr_t>(PutSrcLinkHC)},
      {"FixSet", reinterpret_cast<uintptr_t>(FixSetHC)},
      {"AdamErr", reinterpret_cast<uintptr_t>(AdamErrHC)},
      {"DefineLoad", reinterpret_cast<uintptr_t>(DefineLoadHC)},
      {"DefineLstLoad", reinterpret_cast<uintptr_t>(DefineLstLoadHC)},
      {"Define", reinterpret_cast<uintptr_t>(DefineHC)},
      {"DefineSub", reinterpret_cast<uintptr_t>(DefineSubHC)},
      {"DefineCnt", reinterpret_cast<uintptr_t>(DefineCntHC)},
      {"DefineMatch", reinterpret_cast<uintptr_t>(DefineMatchHC)},
      {"DefinePrint", reinterpret_cast<uintptr_t>(DefinePrintHC)},
      {"Color2Str", reinterpret_cast<uintptr_t>(Color2StrHC)},
      {"str2color_lst", reinterpret_cast<uintptr_t>(&str2color_lst)},
      {"Str2ColorU16", reinterpret_cast<uintptr_t>(Str2ColorU16HC)},
      {"Str2ColorU32", reinterpret_cast<uintptr_t>(Str2ColorU32HC)},
      {"WinHorz", reinterpret_cast<uintptr_t>(WinHorzHC)},
      {"WinVert", reinterpret_cast<uintptr_t>(WinVertHC)},
      {"WinRefocus", reinterpret_cast<uintptr_t>(WinRefocusHC)},
      {"WinToTop", reinterpret_cast<uintptr_t>(WinToTopHC)},
      {"WinFocus", reinterpret_cast<uintptr_t>(WinFocusHC)},
      {"WinTileHorz", reinterpret_cast<uintptr_t>(WinTileHorzHC)},
      {"WinTileVert", reinterpret_cast<uintptr_t>(WinTileVertHC)},
      {"WinMax", reinterpret_cast<uintptr_t>(WinMaxHC)},
      {"WinBorder", reinterpret_cast<uintptr_t>(WinBorderHC)},
      {"WinScrollNull", reinterpret_cast<uintptr_t>(WinScrollNullHC)},
      {"WinScrollRestore", reinterpret_cast<uintptr_t>(WinScrollRestoreHC)},
      {"CtrlFindUnique", reinterpret_cast<uintptr_t>(CtrlFindUniqueHC)},
      {"CtrlInside", reinterpret_cast<uintptr_t>(CtrlInsideHC)},
      {"WinScrollsInit", reinterpret_cast<uintptr_t>(WinScrollsInitHC)},
      {"ViewAnglesNew", reinterpret_cast<uintptr_t>(ViewAnglesNewHC)},
      {"ViewAnglesDel", reinterpret_cast<uintptr_t>(ViewAnglesDelHC)},
      {"AutoComplete", reinterpret_cast<uintptr_t>(AutoCompleteHC)},
      {"ACInit", reinterpret_cast<uintptr_t>(ACInitHC)},
      {"ACDictGen", reinterpret_cast<uintptr_t>(ACDictGenHC)},
      {"ACDWordsLoad", reinterpret_cast<uintptr_t>(ACDWordsLoadHC)},
      {"ACDWordPtAt", reinterpret_cast<uintptr_t>(ACDWordPtAtHC)},
      {"ACDDefGet", reinterpret_cast<uintptr_t>(ACDDefGetHC)},
      {"ACDDefsGet", reinterpret_cast<uintptr_t>(ACDDefsGetHC)},
      {"ACDDefsPut", reinterpret_cast<uintptr_t>(ACDDefsPutHC)},
      {"ACDPopUpDef", reinterpret_cast<uintptr_t>(ACDPopUpDefHC)},
      {"MenuSubEntryFind", reinterpret_cast<uintptr_t>(MenuSubEntryFindHC)},
      {"MenuEntryFind", reinterpret_cast<uintptr_t>(MenuEntryFindHC)},
      {"MenuNew", reinterpret_cast<uintptr_t>(MenuNewHC)},
      {"MenuFile", reinterpret_cast<uintptr_t>(MenuFileHC)},
      {"MenuDel", reinterpret_cast<uintptr_t>(MenuDelHC)},
      {"MenuPush", reinterpret_cast<uintptr_t>(MenuPushHC)},
      {"MenuFilePush", reinterpret_cast<uintptr_t>(MenuFilePushHC)},
      {"MenuPop", reinterpret_cast<uintptr_t>(MenuPopHC)},
      {"Char2KeyName", reinterpret_cast<uintptr_t>(Char2KeyNameHC)},
      {"ScanCode2KeyName", reinterpret_cast<uintptr_t>(ScanCode2KeyNameHC)},
      {"sys_cur_submenu_entry", reinterpret_cast<uintptr_t>(&sys_cur_submenu_entry)},
      {"SettingsPush", reinterpret_cast<uintptr_t>(SettingsPushHC)},
      {"SettingsPop", reinterpret_cast<uintptr_t>(SettingsPopHC)},
      {"WinZBufUpdate", reinterpret_cast<uintptr_t>(WinZBufUpdateHC)},
      {"TextChar", reinterpret_cast<uintptr_t>(TextCharHC)},
      {"_TEXT_CHAR", reinterpret_cast<uintptr_t>(TextCharHC)},
      {"TextLenStr", reinterpret_cast<uintptr_t>(TextLenStrHC)},
      {"_TEXT_LEN_STR", reinterpret_cast<uintptr_t>(TextLenStrHC)},
      {"TextLenAttrStr", reinterpret_cast<uintptr_t>(TextLenAttrStrHC)},
      {"_TEXT_LEN_ATTR_STR", reinterpret_cast<uintptr_t>(TextLenAttrStrHC)},
      {"TextLenAttr", reinterpret_cast<uintptr_t>(TextLenAttrHC)},
      {"_TEXT_LEN_ATTR", reinterpret_cast<uintptr_t>(TextLenAttrHC)},
      {"TextPrint", reinterpret_cast<uintptr_t>(TextPrintHC)},
      {"TextRect", reinterpret_cast<uintptr_t>(TextRectHC)},
      {"TextBorder", reinterpret_cast<uintptr_t>(TextBorderHC)},
      {"DocPut", reinterpret_cast<uintptr_t>(DocPutHC)},
      {"DocDisplay", reinterpret_cast<uintptr_t>(DocDisplayHC)},
      {"DocBorder", reinterpret_cast<uintptr_t>(DocBorderHC)},
      {"DocRecalc", reinterpret_cast<uintptr_t>(DocRecalcHC)},
      {"DocPutKey", reinterpret_cast<uintptr_t>(DocPutKeyHC)},
      {"DocTermNew", reinterpret_cast<uintptr_t>(DocTermNewHC)},
      {"DocGetKey", reinterpret_cast<uintptr_t>(DocGetKeyHC)},
      {"DocScanLine", reinterpret_cast<uintptr_t>(DocScanLineHC)},
      {"DocLock", reinterpret_cast<uintptr_t>(DocLockHC)},
      {"DocUnlock", reinterpret_cast<uintptr_t>(DocUnlockHC)},
      {"DocEntryRun", reinterpret_cast<uintptr_t>(DocEntryRunHC)},
      {"DocBottom", reinterpret_cast<uintptr_t>(DocBottomHC)},
      {"DocCenter", reinterpret_cast<uintptr_t>(DocCenterHC)},
      {"DocCollapse", reinterpret_cast<uintptr_t>(DocCollapseHC)},
      {"DocClear", reinterpret_cast<uintptr_t>(DocClearHC)},
      {"DocCursor", reinterpret_cast<uintptr_t>(DocCursorHC)},
      {"DocHighlightCursor", reinterpret_cast<uintptr_t>(DocHighlightCursorHC)},
      {"DocScroll", reinterpret_cast<uintptr_t>(DocScrollHC)},
      {"Now", reinterpret_cast<uintptr_t>(NowHC)},
      {"Struct2Date", reinterpret_cast<uintptr_t>(Struct2DateHC)},
      {"Date2Struct", reinterpret_cast<uintptr_t>(Date2StructHC)},
      {"YearStartDate", reinterpret_cast<uintptr_t>(YearStartDateHC)},
      {"DayOfWeek", reinterpret_cast<uintptr_t>(DayOfWeekHC)},
      {"FirstDayOfMon", reinterpret_cast<uintptr_t>(FirstDayOfMonHC)},
      {"LastDayOfMon", reinterpret_cast<uintptr_t>(LastDayOfMonHC)},
      {"FirstDayOfYear", reinterpret_cast<uintptr_t>(FirstDayOfYearHC)},
      {"LastDayOfYear", reinterpret_cast<uintptr_t>(LastDayOfYearHC)},
      {"DocNew", reinterpret_cast<uintptr_t>(DocNewHC)},
      {"DocDel", reinterpret_cast<uintptr_t>(DocDelHC)},
      {"DocLoad", reinterpret_cast<uintptr_t>(DocLoadHC)},
      {"DocSize", reinterpret_cast<uintptr_t>(DocSizeHC)},
      {"DocSave", reinterpret_cast<uintptr_t>(DocSaveHC)},
      {"DocWrite", reinterpret_cast<uintptr_t>(DocWriteHC)},
      {"DocRead", reinterpret_cast<uintptr_t>(DocReadHC)},
      {"DocCut", reinterpret_cast<uintptr_t>(DocCutHC)},
      {"DocCopy", reinterpret_cast<uintptr_t>(DocCopyHC)},
      {"DocTreeFind", reinterpret_cast<uintptr_t>(DocTreeFindHC)},
      {"DocTreeFFind", reinterpret_cast<uintptr_t>(DocTreeFFindHC)},
      {"DocTreeMake", reinterpret_cast<uintptr_t>(DocTreeMakeHC)},
      {"DocTreeWrite", reinterpret_cast<uintptr_t>(DocTreeWriteHC)},
      {"DocTreeAppend", reinterpret_cast<uintptr_t>(DocTreeAppendHC)},
      {"DocTreeFWrite", reinterpret_cast<uintptr_t>(DocTreeFWriteHC)},
      {"DocTreeFAppend", reinterpret_cast<uintptr_t>(DocTreeFAppendHC)},
      {"DocTreeWriteJoin", reinterpret_cast<uintptr_t>(DocTreeWriteJoinHC)},
      {"DocTreeAppendJoin", reinterpret_cast<uintptr_t>(DocTreeAppendJoinHC)},
      {"ExeDoc", reinterpret_cast<uintptr_t>(ExeDocHC)},
      {"DocTreeExe", reinterpret_cast<uintptr_t>(DocTreeExeHC)},
      {"DocTreeFExe", reinterpret_cast<uintptr_t>(DocTreeFExeHC)},
      {"RegDft", reinterpret_cast<uintptr_t>(RegDftHC)},
      {"RegExe", reinterpret_cast<uintptr_t>(RegExeHC)},
      {"RegWrite", reinterpret_cast<uintptr_t>(RegWriteHC)},
      {"RegCnt", reinterpret_cast<uintptr_t>(RegCntHC)},
      {"RegAppend", reinterpret_cast<uintptr_t>(RegAppendHC)},
      {"GodBitsIns", reinterpret_cast<uintptr_t>(GodBitsInsHC)},
      {"GodHexIns", reinterpret_cast<uintptr_t>(GodHexInsHC)},
      {"GodBits", reinterpret_cast<uintptr_t>(GodBitsHC)},
      {"GodInit", reinterpret_cast<uintptr_t>(GodInitHC)},
      {"GodWordStr", reinterpret_cast<uintptr_t>(GodWordStrHC)},
      {"GodWord", reinterpret_cast<uintptr_t>(GodWordHC)},
      {"GodSongStr", reinterpret_cast<uintptr_t>(GodSongStrHC)},
      {"GodSong", reinterpret_cast<uintptr_t>(GodSongHC)},
      {"GodBiblePassage", reinterpret_cast<uintptr_t>(GodBiblePassageHC)},
      {"BibleLine2Verse", reinterpret_cast<uintptr_t>(BibleLine2VerseHC)},
      {"BookLines", reinterpret_cast<uintptr_t>(BookLinesHC)},
      {"BibleVerse", reinterpret_cast<uintptr_t>(BibleVerseHC)},
      {"BibleDoc", reinterpret_cast<uintptr_t>(BibleDocHC)},
      {"BibleView", reinterpret_cast<uintptr_t>(BibleViewHC)},
      {"GodDoodleSprite", reinterpret_cast<uintptr_t>(GodDoodleSpriteHC)},
      {"GodDoodle", reinterpret_cast<uintptr_t>(GodDoodleHC)},
      {"Who", reinterpret_cast<uintptr_t>(WhoHC)},
      {"HashDepthRep", reinterpret_cast<uintptr_t>(HashDepthRepHC)},
      {"DocHelpIdx", reinterpret_cast<uintptr_t>(DocHelpIdxHC)},
      {"PopUpHelpIndex", reinterpret_cast<uintptr_t>(PopUpHelpIndexHC)},
      {"MapFileLoad", reinterpret_cast<uintptr_t>(MapFileLoadHC)},
      {"AOnceFlush", reinterpret_cast<uintptr_t>(AOnceFlushHC)},
      {"OnceFlush", reinterpret_cast<uintptr_t>(OnceFlushHC)},
      {"AOnce", reinterpret_cast<uintptr_t>(AOnceHC)},
      {"Once", reinterpret_cast<uintptr_t>(OnceHC)},
      {"AOnceDrv", reinterpret_cast<uintptr_t>(AOnceDrvHC)},
      {"OnceDrv", reinterpret_cast<uintptr_t>(OnceDrvHC)},
      {"OnceExe", reinterpret_cast<uintptr_t>(OnceExeHC)},
      {"OneTimePopUp", reinterpret_cast<uintptr_t>(OneTimePopUpHC)},
      {"RegOneTimePopUp", reinterpret_cast<uintptr_t>(RegOneTimePopUpHC)},
      {"InsReg", reinterpret_cast<uintptr_t>(InsRegHC)},
      {"InsRereg", reinterpret_cast<uintptr_t>(InsReregHC)},
      {"InsUnreg", reinterpret_cast<uintptr_t>(InsUnregHC)},
      {"DocType", reinterpret_cast<uintptr_t>(DocTypeHC)},
      {"Type", reinterpret_cast<uintptr_t>(TypeHC)},
      {"KeyMap", reinterpret_cast<uintptr_t>(KeyMapHC)},
      {"TipOfDay", reinterpret_cast<uintptr_t>(TipOfDayHC)},
      {"DocPrint", reinterpret_cast<uintptr_t>(DocPrintHC)},
      {"Doc2PlainText", reinterpret_cast<uintptr_t>(Doc2PlainTextHC)},
      {"DocBinsValidate", reinterpret_cast<uintptr_t>(DocBinsValidateHC)},
      {"DocPutLine", reinterpret_cast<uintptr_t>(DocPutLineHC)},
      {"char_bmp_alpha", reinterpret_cast<uintptr_t>(char_bmp_alpha)},
      {"char_bmp_alpha_numeric",
       reinterpret_cast<uintptr_t>(char_bmp_alpha_numeric)},
      {"char_bmp_alpha_numeric_no_at",
       reinterpret_cast<uintptr_t>(char_bmp_alpha_numeric_no_at)},
      {"char_bmp_word", reinterpret_cast<uintptr_t>(char_bmp_word)},
      {"char_bmp_filename", reinterpret_cast<uintptr_t>(char_bmp_filename)},
      {"char_bmp_dec_numeric",
       reinterpret_cast<uintptr_t>(char_bmp_dec_numeric)},
      {"char_bmp_hex_numeric",
       reinterpret_cast<uintptr_t>(char_bmp_hex_numeric)},
      {"char_bmp_white_space",
       reinterpret_cast<uintptr_t>(char_bmp_white_space)},
      {"char_bmp_non_eol_white_space",
       reinterpret_cast<uintptr_t>(char_bmp_non_eol_white_space)},
      {"char_bmp_zero_cr_nl_cursor",
       reinterpret_cast<uintptr_t>(char_bmp_zero_cr_nl_cursor)},
      {"char_bmp_zero_tab_cr_nl_cursor",
       reinterpret_cast<uintptr_t>(char_bmp_zero_tab_cr_nl_cursor)},
      {"char_bmp_zero_tab_cr_nl_cursor_dollar",
       reinterpret_cast<uintptr_t>(char_bmp_zero_tab_cr_nl_cursor_dollar)},
      {"char_bmp_macro", reinterpret_cast<uintptr_t>(char_bmp_macro)},
      {"char_bmp_printable", reinterpret_cast<uintptr_t>(char_bmp_printable)},
      {"char_bmp_displayable",
       reinterpret_cast<uintptr_t>(char_bmp_displayable)},
      {"char_bmp_safe_dollar",
       reinterpret_cast<uintptr_t>(char_bmp_safe_dollar)},
      {"char_bmp_non_eol", reinterpret_cast<uintptr_t>(char_bmp_non_eol)},
      // System V entries used by host-built C++ modules.
      {"__unholyos_sysv.AdamErrCpp", reinterpret_cast<uintptr_t>(AdamErrCpp)},
      {"__unholyos_sysv.CAlloc", reinterpret_cast<uintptr_t>(CAlloc)},
      {"__unholyos_sysv.CmpCall", reinterpret_cast<uintptr_t>(CmpCall)},
      {"__unholyos_sysv.DefineLstLoadCpp",
       reinterpret_cast<uintptr_t>(DefineLstLoadCpp)},
      {"__unholyos_sysv.DirCurCpp", reinterpret_cast<uintptr_t>(DirCurCpp)},
      {"__unholyos_sysv.DirFileCpp", reinterpret_cast<uintptr_t>(DirFileCpp)},
      {"__unholyos_sysv.Doc2PlainTextCpp",
       reinterpret_cast<uintptr_t>(Doc2PlainTextCpp)},
      {"__unholyos_sysv.DocDelCpp", reinterpret_cast<uintptr_t>(DocDelCpp)},
      {"__unholyos_sysv.DocLoadCpp", reinterpret_cast<uintptr_t>(DocLoadCpp)},
      {"__unholyos_sysv.DocNewCpp", reinterpret_cast<uintptr_t>(DocNewCpp)},
      {"__unholyos_sysv.DocPutLineCpp",
       reinterpret_cast<uintptr_t>(DocPutLineCpp)},
      {"__unholyos_sysv.DocPutS", reinterpret_cast<uintptr_t>(DocPutS)},
      {"__unholyos_sysv.DocSaveCpp", reinterpret_cast<uintptr_t>(DocSaveCpp)},
      {"__unholyos_sysv.ExeCmdLine", reinterpret_cast<uintptr_t>(ExeCmdLine)},
      {"__unholyos_sysv.ExtChgCpp", reinterpret_cast<uintptr_t>(ExtChgCpp)},
      {"__unholyos_sysv.ExtDftCpp", reinterpret_cast<uintptr_t>(ExtDftCpp)},
      {"__unholyos_sysv.F64PowerBits",
       reinterpret_cast<uintptr_t>(F64PowerBits)},
      {"__unholyos_sysv.FileNameAbs", reinterpret_cast<uintptr_t>(FileNameAbs)},
      {"__unholyos_sysv.FileReadCpp", reinterpret_cast<uintptr_t>(FileReadCpp)},
      {"__unholyos_sysv.FileWriteCpp",
       reinterpret_cast<uintptr_t>(FileWriteCpp)},
      {"__unholyos_sysv.FixSetCpp", reinterpret_cast<uintptr_t>(FixSetCpp)},
      {"__unholyos_sysv.Free", reinterpret_cast<uintptr_t>(Free)},
      {"__unholyos_sysv.GetStrCpp", reinterpret_cast<uintptr_t>(GetStrCpp)},
      {"__unholyos_sysv.HashAdd", reinterpret_cast<uintptr_t>(HashAdd)},
      {"__unholyos_sysv.HashDel", reinterpret_cast<uintptr_t>(HashDel)},
      {"__unholyos_sysv.HashFind", reinterpret_cast<uintptr_t>(HashFind)},
      {"__unholyos_sysv.HashRemove", reinterpret_cast<uintptr_t>(HashRemove)},
      {"__unholyos_sysv.HashSingleTableFind",
       reinterpret_cast<uintptr_t>(HashSingleTableFind)},
      {"__unholyos_sysv.HashSrcFileSetCpp",
       reinterpret_cast<uintptr_t>(HashSrcFileSetCpp)},
      {"__unholyos_sysv.HashTableDelCpp",
       reinterpret_cast<uintptr_t>(HashTableDelCpp)},
      {"__unholyos_sysv.HashTableNew",
       reinterpret_cast<uintptr_t>(HashTableNew)},
      {"__unholyos_sysv.I64MulF64Bits",
       reinterpret_cast<uintptr_t>(I64MulF64Bits)},
      {"__unholyos_sysv.IsRawCpp", reinterpret_cast<uintptr_t>(IsRawCpp)},
      {"__unholyos_sysv.LinkedLstDelCpp",
       reinterpret_cast<uintptr_t>(LinkedLstDelCpp)},
      {"__unholyos_sysv.LstMatchCpp", reinterpret_cast<uintptr_t>(LstMatchCpp)},
      {"__unholyos_sysv.MAlloc", reinterpret_cast<uintptr_t>(MAlloc)},
      {"__unholyos_sysv.MSize", reinterpret_cast<uintptr_t>(MSize)},
      {"__unholyos_sysv.MStrPrintCpp",
       reinterpret_cast<uintptr_t>(MStrPrintCpp)},
      {"__unholyos_sysv.Pow10I64Cpp", reinterpret_cast<uintptr_t>(Pow10I64Cpp)},
      {"__unholyos_sysv.PrintCpp", reinterpret_cast<uintptr_t>(PrintCpp)},
      {"__unholyos_sysv.PrintErrCpp", reinterpret_cast<uintptr_t>(PrintErrCpp)},
      {"__unholyos_sysv.PrintWarnCpp",
       reinterpret_cast<uintptr_t>(PrintWarnCpp)},
      {"__unholyos_sysv.PutFileLinkCpp", reinterpret_cast<uintptr_t>(PutFileLinkCpp)},
      {"__unholyos_sysv.PutS", reinterpret_cast<uintptr_t>(PutS)},
      {"__unholyos_sysv.RaiseHolyC", reinterpret_cast<uintptr_t>(RaiseHolyC)},
      {"__unholyos_sysv.RethrowHolyC",
       reinterpret_cast<uintptr_t>(RethrowHolyC)},
      {"__unholyos_sysv.StrCmpCpp", reinterpret_cast<uintptr_t>(StrCmpCpp)},
      {"__unholyos_sysv.StrNew", reinterpret_cast<uintptr_t>(StrNew)},
      {"__unholyos_sysv.StrPrintJoin",
       reinterpret_cast<uintptr_t>(StrPrintJoin)},
      {"__unholyos_sysv.SysSymImportsResolveCpp",
       reinterpret_cast<uintptr_t>(SysSymImportsResolveCpp)},
  };
  constexpr char name[] = "/Kernel/Kernel";
  if (HashSingleTableFind(name, task.hash_table, HTT_MODULE))
    return true;
  auto *kernel = reinterpret_cast<CHashGeneric *>(
      CAlloc(sizeof(CHashGeneric), task.data_heap));
  if (!kernel)
    return false;
  kernel->hash.str = StrNew(name, task.data_heap);
  kernel->hash.type = HTT_MODULE | HTF_PUBLIC;
  kernel->user_data0 =
      reinterpret_cast<uintptr_t>(SYS_KERNEL) - BIN_HEADER_SIZE;
  CHash *pending = &kernel->hash;
  auto **tail = &kernel->hash.next;
  bool ok = kernel->hash.str;
  for (size_t i = 0; ok && i < sizeof(exports) / sizeof(exports[0]); i++) {
    auto *symbol = reinterpret_cast<CHashExport *>(
        CAlloc(sizeof(CHashExport), task.data_heap));
    if (!symbol) {
      ok = false;
      break;
    }
    symbol->sym.hash.str = StrNew(exports[i].name, task.data_heap);
    symbol->sym.hash.type = HTT_EXPORT_SYS_SYM | HTF_IMM;
    symbol->val = exports[i].value;
    *tail = &symbol->sym.hash;
    tail = &symbol->sym.hash.next;
    ok = symbol->sym.hash.str;
  }
  while (pending) {
    auto *next = pending->next;
    if (ok)
      HashAdd(pending, task.hash_table);
    else
      HashDel(pending);
    pending = next;
  }
  return ok;
}
