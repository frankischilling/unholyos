GLOBAL IsPixCovered0Cpp
EXTERN gr

SECTION .text
BITS 64

; SysV registers carry task, x and y. The HolyC entry uses the kernel adapter.
IsPixCovered0Cpp:
	SAR	RDX,3
	SAR	RSI,3
	IMUL	RDX,80
	ADD	RDX,RSI
	XOR	EAX,EAX
	CMP	RDX,4800
	JAE	.done
	MOV	RCX,[gr+72]
	MOVZX	ESI,WORD [RDI+0x30C]
	CMP	SI,[RCX+RDX*2]
	JAE	.done
	INC	EAX
.done:
	RET
