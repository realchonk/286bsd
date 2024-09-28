[org 0]
[bits 16]
[cpu 286]

_start:
	mov ax, 0x18
	mov es, ax
	mov byte [es:0], 'H'
	jmp $

hello:
	db "Hello World", 13, 10, 0
