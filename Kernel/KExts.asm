BITS 64
SECTION .text
EXTERN compiler_imports,RaiseHolyC

; System V imports preserve arguments, including aggregate-return storage.
%macro COMPILER_IMPORT 2
GLOBAL %1
%1:
		MOV	R11,[REL compiler_imports]
		TEST	R11,R11
		JZ	CompilerImportMissing
		MOV	R11,[R11+%2]
		TEST	R11,R11
		JZ	CompilerImportMissing
		JMP	R11
%endmacro

COMPILER_IMPORT _Z11AsmHashLoadv,0
COMPILER_IMPORT _Z11LexStmt2BinR8CCmpCtrlRPhR8CRawType,8
COMPILER_IMPORT _Z7ExePutSPKcS0_lPK20CLexHashTableContext,16
COMPILER_IMPORT _Z17ClassMemberLstDelP10CHashClass,24
COMPILER_IMPORT _Z14CmpAsmHashLoadP10CHashTable,32
COMPILER_IMPORT _Z13LexSourceInitR8CCmpCtrlR8CLexFilePKclS4_P10CHashTable,40
COMPILER_IMPORT _Z12LexSourceDelR8CCmpCtrl,48
COMPILER_IMPORT _Z3LexR8CCmpCtrl,56
COMPILER_IMPORT _Z22LexTypedExpression2BinR8CCmpCtrl8CRawTypeb,64
COMPILER_IMPORT MemberMetaDataCpp,72
COMPILER_IMPORT _Z11AsmHashFindPKcj,80
COMPILER_IMPORT _Z13CmpAsmHashNewv,88
COMPILER_IMPORT _Z12LexAttachDocR8CCmpCtrlR4CDocP8CLexFileP13CDocEntryBasej,96
COMPILER_IMPORT _Z6LexRawR8CCmpCtrl,104
COMPILER_IMPORT _Z17LexExpression2BinR8CCmpCtrlR8CRawType,112
COMPILER_IMPORT _Z7ExeFilePKcl,120
COMPILER_IMPORT _Z15LexExceptReportRK8CCmpCtrlPKc,128

CompilerImportMissing:
		MOV	RDI,0x7478456665646E55 ; UndefExt
		JMP	RaiseHolyC

SECTION .note.GNU-stack noalloc noexec nowrite progbits
