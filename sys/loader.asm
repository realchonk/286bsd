[bits 16]
[cpu 286]


NUM_SECTORS equ 16

section .boot exec write
global _start
extern _entry
_start:
	mov byte [bootdrv], dl

	; reset boot drive
	xor ah, ah
	mov dl, byte [bootdrv]
	int 0x13
	jc error

.loop:
	lea bp, [ldstr]
	call puts

	; read sectors
	stc
	mov ah, 0x02
	mov al, NUM_SECTORS
	xor ch, ch
	mov cl, 2
	xor dh, dh
	mov dl, byte [bootdrv]
	mov bx, 0x1000
	mov es, bx
	xor bx, bx
	int 0x13
	jnc .ok

	; reset boot drive again
	stc
	xor ah, ah
	mov dl, byte [bootdrv]
	int 0x13
	jnc error

	dec byte [tries]
	jnz .loop
	jmp error

.ok:
	push word [bootdrv]
	mov ax, 0x1000
	mov ds, ax
	mov es, ax
	jmp 0x1000:_entry

error:
	lea bp, [errstr]
	; fallthrough

puts:
	mov al, byte [bp]
	inc bp
	test al, al
	jz .ret

	mov ah, 0x0E
	xor bx, bx
	int 0x10
	jmp puts

.ret:
	ret

ldstr:
	db "Loading kernel...", 13, 10, 0

errstr:
	db "Error", 13, 10, 0

bootdrv:
	db 0

tries:
	db 5

times 510 - ($ - $$) db 0
dw 0xAA55
