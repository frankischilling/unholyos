GLOBAL SYS_SERIAL_INIT,SYS_SERIAL_PUTS

SECTION .boot32 progbits alloc exec nowrite align=16
BITS 32

SYS_SERIAL_INIT:
	MOV	DX,0x3F9
	XOR	AL,AL
	OUT	DX,AL			; Disable UART interrupts.
	MOV	DX,0x3FB
	MOV	AL,0x80
	OUT	DX,AL
	MOV	DX,0x3F8
	MOV	AL,1			; 115200 baud.
	OUT	DX,AL
	INC	DX
	XOR	AL,AL
	OUT	DX,AL
	MOV	DX,0x3FB
	MOV	AL,3			; 8 data bits, no parity, one stop bit.
	OUT	DX,AL
	RET

SYS_SERIAL_PUTS:
	MOV	DX,0x3FD
	MOV	ECX,65536
.wait:
	IN	AL,DX
	TEST	AL,0x20
	JNZ	.ready
	LOOP	.wait
	RET
.ready:
	LODSB
	TEST	AL,AL
	JZ	.done
	MOV	DX,0x3F8
	OUT	DX,AL
	JMP	SYS_SERIAL_PUTS
.done:
	RET

SECTION .note.GNU-stack noalloc noexec nowrite progbits
