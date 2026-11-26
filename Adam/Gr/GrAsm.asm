GLOBAL GrUpdateLine64,GrUpdateLine64FlushCache,DCBlotColor4,IsPixCovered0Cpp
EXTERN gr,rev_bits_table

SECTION .text
BITS 64

%macro GR_UPDATE_LINE64 2
%1:
	PUSH	RBX
	TEST	RDX,RDX
	JZ	.done
	MOV	R11,RDI
	MOV	R10,RSI
	MOV	R9,RCX
	MOV	RSI,[R10]
	MOV	RBX,[REL rev_bits_table]
	MOV	RCX,RDX
	MOV	RDI,[R9]
	MOV	R8,[R11]

.next:
	LODSQ
%if !%2
	CMP	QWORD [RDI],RAX
	JE	.advance
%endif
	MOV	QWORD [RDI],RAX
%rep 8
	XLATB
	ROR	RAX,8
%endrep
	MOV	QWORD [R8],RAX

.advance:
	ADD	RDI,8
	ADD	R8,8
	DEC	RCX
	JNZ	.next
	MOV	QWORD [R11],R8
	MOV	QWORD [R9],RDI
	MOV	QWORD [R10],RSI
.done:
	POP	RBX
	RET
%endmacro

GR_UPDATE_LINE64 GrUpdateLine64,0
GR_UPDATE_LINE64 GrUpdateLine64FlushCache,1

%macro GR_PACK_PLANE 2
	XOR	EDX,EDX
%assign __pixel 7
%rep 7
	BT	RAX,__pixel*8+%1
	ADC	RDX,0
	SHL	RDX,1
%assign __pixel __pixel-1
%endrep
	BT	RAX,%1
	ADC	RDX,0
	MOV	BYTE [%2],DL
%endmacro

DCBlotColor4:
	TEST	RCX,RCX
	JZ	.done
	PUSH	RBX
	MOV	RBX,RDX
	LEA	R8,[RDI+RCX]
	LEA	R9,[R8+RCX]
	LEA	R10,[R9+RCX]
.next:
	LODSQ
	MOV	RDX,[RBX]
	CMP	RAX,RDX
	JE	.advance
	MOV	QWORD [RBX],RAX
	GR_PACK_PLANE 0,RDI
	GR_PACK_PLANE 1,R8
	GR_PACK_PLANE 2,R9
	GR_PACK_PLANE 3,R10
.advance:
	ADD	RBX,8
	INC	RDI
	INC	R8
	INC	R9
	INC	R10
	DEC	RCX
	JNZ	.next
	POP	RBX
.done:
	RET

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

SECTION .note.GNU-stack noalloc noexec nowrite progbits
