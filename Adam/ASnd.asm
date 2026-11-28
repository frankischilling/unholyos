; x87 expression steps for Terry A. Davis's Adam/ASnd.HC.
; The reference stores the remainder before these steps and keeps the
; following arithmetic on the x87 stack until the result is stored.
BITS 64
SECTION .text

GLOBAL SndPhaseAddOne,SndFullPhase,SndOneMinusPhase,SndScaleRem

SndPhaseAddOne:
  PUSH RDI
  PUSH RSI
  FLD QWORD [RSP+8]
  FDIV QWORD [RSP]
  FLD1
  FADDP ST1,ST0
  FSTP QWORD [RSP+8]
  ADD RSP,8
  POP RAX
  RET

SndFullPhase:
  PUSH RDI
  PUSH RSI
  PUSH RDX
  PUSH RCX
  FLD QWORD [RSP+24]
  FDIV QWORD [RSP+16]
  FMUL QWORD [RSP+8]
  FADD QWORD [RSP]
  FSTP QWORD [RSP+24]
  ADD RSP,24
  POP RAX
  RET

SndOneMinusPhase:
  PUSH RDI
  PUSH RSI
  FLD QWORD [RSP+8]
  FDIV QWORD [RSP]
  FLD1
  FSUBRP ST1,ST0
  FSTP QWORD [RSP+8]
  ADD RSP,8
  POP RAX
  RET

SndScaleRem:
  PUSH RDI
  PUSH RSI
  PUSH RDX
  FLD QWORD [RSP]
  FMUL QWORD [RSP+16]
  FDIV QWORD [RSP+8]
  FSTP QWORD [RSP+16]
  ADD RSP,16
  POP RAX
  RET

; (1-t)*ona1+t*ona2, truncated to an I64 like the HolyC assignment.
GLOBAL SoundEffectSweepOna
SoundEffectSweepOna:
  SUB RSP,32
  MOV [RSP],RDI
  MOV [RSP+8],RSI
  MOV [RSP+16],RDX
  FLD1
  FSUB QWORD [RSP]
  FILD QWORD [RSP+8]
  FMULP ST1,ST0
  FLD QWORD [RSP]
  FILD QWORD [RSP+16]
  FMULP ST1,ST0
  FADDP ST1,ST0
  FNSTCW WORD [RSP+24]
  MOV AX,[RSP+24]
  OR AX,0x0C00
  MOV [RSP+26],AX
  FLDCW WORD [RSP+26]
  FISTP QWORD [RSP]
  FLDCW WORD [RSP+24]
  MOV RAX,[RSP]
  ADD RSP,32
  RET

SECTION .note.GNU-stack noalloc noexec nowrite progbits
