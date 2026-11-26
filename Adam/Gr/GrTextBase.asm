%include "Kernel/KernelA.inc"

GLOBAL TextCharCpp,TextLenStrCpp,TextLenAttrStrCpp,TextLenAttrCpp
EXTERN gr

SECTION .text
BITS 64

TEXT_COLS EQU 80
TEXT_ROWS EQU 60
GR_TEXT_BASE EQU 64

; RDI=task, SIL=allow_border, RDX=x, RCX=y, R8D=cell.
TextCharCpp:
	MOV	R10,[RDI+CTask_SCROLL_X]
	SAR	R10,3
	ADD	R10,RDX
	MOV	R11,[RDI+CTask_SCROLL_Y]
	SAR	R11,3
	ADD	R11,RCX
	TEST	SIL,SIL
	SETNZ	AL
	MOVZX	ESI,AL
	MOV	RAX,RSI
	NEG	RAX
	CMP	R10,RAX
	JL	.miss
	CMP	R11,RAX
	JL	.miss
	ADD	R10,[RDI+CTask_WIN_LEFT]
	ADD	R11,[RDI+CTask_WIN_TOP]
	MOV	RAX,[RDI+CTask_WIN_RIGHT]
	ADD	RAX,RSI
	CMP	R10,RAX
	JG	.miss
	MOV	RAX,[RDI+CTask_WIN_BOTTOM]
	ADD	RAX,RSI
	CMP	R11,RAX
	JG	.miss
	CMP	R10,TEXT_COLS
	JAE	.miss
	CMP	R11,TEXT_ROWS
	JAE	.miss
	IMUL	R11,TEXT_COLS
	ADD	R11,R10
	MOV	RAX,[REL gr+GR_TEXT_BASE]
	MOV	[RAX+R11*4],R8D
	MOV	EAX,1
	RET
.miss:
	XOR	EAX,EAX
	RET

; RDI=task, RSI=x, RDX=y, RCX=len, R8D=attr, R9=source.
; Returns the clipped destination in RDI, source in RSI and count in RCX.
TextLenClip:
	TEST	RCX,RCX
	JLE	.miss
	MOV	RAX,[RDI+CTask_SCROLL_Y]
	SAR	RAX,3
	ADD	RAX,RDX
	JS	.miss
	ADD	RAX,[RDI+CTask_WIN_TOP]
	CMP	RAX,[RDI+CTask_WIN_BOTTOM]
	JG	.miss
	CMP	RAX,TEXT_ROWS
	JAE	.miss
	MOV	R10,[RDI+CTask_SCROLL_X]
	SAR	R10,3
	ADD	R10,RSI
	JNS	.wind
	ADD	RCX,R10
	SUB	R9,R10
	XOR	R10D,R10D
.wind:
	ADD	R10,[RDI+CTask_WIN_LEFT]
	JNS	.screen
	ADD	RCX,R10
	SUB	R9,R10
	XOR	R10D,R10D
.screen:
	CMP	R10,TEXT_COLS
	JAE	.miss
	MOV	R11,[RDI+CTask_WIN_RIGHT]
	MOV	EDX,TEXT_COLS-1
	CMP	R11,RDX
	CMOVG	R11,RDX
	SUB	R11,R10
	INC	R11
	CMP	RCX,R11
	CMOVG	RCX,R11
	TEST	RCX,RCX
	JLE	.miss
	IMUL	RAX,TEXT_COLS
	ADD	RAX,R10
	MOV	RDI,[REL gr+GR_TEXT_BASE]
	LEA	RDI,[RDI+RAX*4]
	MOV	RSI,R9
	CLC
	RET
.miss:
	STC
	RET

TextLenStrCpp:
	CALL	TextLenClip
	JC	.miss
	MOV	EAX,R8D
.next:
	LODSB
	STOSD
	DEC	RCX
	JNZ	.next
	MOV	EAX,1
	RET
.miss:
	XOR	EAX,EAX
	RET

; The clipper advances a byte source. Scale its skipped cell count by four.
TextLenAttrStrCpp:
	XOR	R9D,R9D
	CALL	TextLenClip
	JC	.miss
	LEA	RSI,[R8+RSI*4]
	REP MOVSD
	MOV	EAX,1
	RET
.miss:
	XOR	EAX,EAX
	RET

TextLenAttrCpp:
	XOR	R9D,R9D
	CALL	TextLenClip
	JC	.miss
	MOV	RSI,RDI
.next:
	LODSD
	TEST	AL,AL
	JNZ	.done
	MOV	EAX,R8D
	STOSD
	DEC	RCX
	JNZ	.next
.done:
	MOV	EAX,1
	RET
.miss:
	XOR	EAX,EAX
	RET

SECTION .note.GNU-stack noalloc noexec nowrite progbits
