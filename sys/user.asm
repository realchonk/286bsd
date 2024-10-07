[org 0]
[cpu 286]
[bits 16]

_start:
	xor ax, ax			; interrupt nr (print)
	lea bx, [hello]			; 1st argument
	lea cx, [hello.end - hello]	; 2nd argument
	int 0x80

	jmp $

hello:
	db "Hello from userspaace!", 10
.end:
