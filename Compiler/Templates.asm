; Position-independent instruction templates, following Terry A. Davis's
; public-domain Compiler/Templates.HC. Offsets are relative to the table.
BITS 64
SECTION .text
GLOBAL CMP_TEMPLATES
CMP_TEMPLATES:
  DD .inc-CMP_TEMPLATES,.dec-CMP_TEMPLATES,.mod-CMP_TEMPLATES
  DD .less-CMP_TEMPLATES,.greater-CMP_TEMPLATES
  DD .less_equ-CMP_TEMPLATES,.greater_equ-CMP_TEMPLATES
  DD .strlen-CMP_TEMPLATES,.rdtsc-CMP_TEMPLATES,.sign-CMP_TEMPLATES
  DD .fstp-CMP_TEMPLATES,.fld-CMP_TEMPLATES,.sqr-CMP_TEMPLATES
  DD .abs-CMP_TEMPLATES,.sqrt-CMP_TEMPLATES,.sin-CMP_TEMPLATES
  DD .cos-CMP_TEMPLATES,.tan-CMP_TEMPLATES,.atan-CMP_TEMPLATES
  DD .end-CMP_TEMPLATES
.inc:
  PUSH RAX
  FLD1
  FADD QWORD [RSP]
  FSTP QWORD [RSP]
  POP RAX
.dec:
  PUSH RAX
  FLD1
  FSUBR QWORD [RSP]
  FSTP QWORD [RSP]
  POP RAX
.mod:
  ; RDX=left, RAX=right. Use RSP instead of clobbering preserved RBX.
  PUSH RDX
  PUSH RAX
  FLD QWORD [RSP]
  FLD QWORD [RSP+8]
.mod_retry:
  FPREM
  FSTSW AX
  TEST AX,0x400
  JNZ .mod_retry
  FSTP QWORD [RSP]
  FFREE ST0
  FINCSTP
  POP RAX
  ADD RSP,8
.less:
  PUSH RDX
  PUSH RAX
  FLD QWORD [RSP]
  FLD QWORD [RSP+8]
  FCOMIP ST0,ST1
  MOV RAX,0
  ADC RAX,0
  FFREE ST0
  FINCSTP
  ADD RSP,16
.greater:
  PUSH RDX
  PUSH RAX
  FLD QWORD [RSP+8]
  FLD QWORD [RSP]
  FCOMIP ST0,ST1
  MOV RAX,0
  ADC RAX,0
  FFREE ST0
  FINCSTP
  ADD RSP,16
.less_equ:
  PUSH RDX
  PUSH RAX
  FLD QWORD [RSP+8]
  FLD QWORD [RSP]
  FCOMIP ST0,ST1
  MOV RAX,1
  SBB RAX,0
  FFREE ST0
  FINCSTP
  ADD RSP,16
.greater_equ:
  PUSH RDX
  PUSH RAX
  FLD QWORD [RSP]
  FLD QWORD [RSP+8]
  FCOMIP ST0,ST1
  MOV RAX,1
  SBB RAX,0
  FFREE ST0
  FINCSTP
  ADD RSP,16
.strlen:
  MOV RDX,RAX
.strlen_loop:
  MOV CL,[RAX]
  INC RAX
  TEST CL,CL
  JNZ .strlen_loop
  SUB RAX,RDX
  DEC RAX
.rdtsc:
  RDTSC
  SHL RDX,32
  ADD RAX,RDX
.sign:
  TEST RAX,RAX
  JZ .sign_done
  JS .sign_negative
  MOV RAX,1
  JMP .sign_done
.sign_negative:
  MOV RAX,-1
.sign_done:
.fstp:
.fld:
.sqr:
  PUSH RAX
  FLD QWORD [RSP]
  FMUL ST0,ST0
  FSTP QWORD [RSP]
  POP RAX
.abs:
  PUSH RAX
  FLD QWORD [RSP]
  FABS
  FSTP QWORD [RSP]
  POP RAX
.sqrt:
  PUSH RAX
  FLD QWORD [RSP]
  FSQRT
  FSTP QWORD [RSP]
  POP RAX
.sin:
  PUSH RAX
  FLD QWORD [RSP]
  FSIN
  FSTP QWORD [RSP]
  POP RAX
.cos:
  PUSH RAX
  FLD QWORD [RSP]
  FCOS
  FSTP QWORD [RSP]
  POP RAX
.tan:
  PUSH RAX
  FLD QWORD [RSP]
.tan_retry:
  FPTAN
  FSTSW AX
  TEST AX,0x400
  JNZ .tan_retry
  FFREE ST0
  FINCSTP
  FSTP QWORD [RSP]
  POP RAX
.atan:
  PUSH RAX
  FLD QWORD [RSP]
  FLD1
.atan_retry:
  FPATAN
  FSTSW AX
  TEST AX,0x400
  JNZ .atan_retry
  FSTP QWORD [RSP]
  POP RAX
.end:

SECTION .note.GNU-stack noalloc noexec nowrite progbits
