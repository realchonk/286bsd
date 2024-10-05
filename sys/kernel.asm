[cpu 286]
[bits 16]
section .text
global _entry
_entry:
	pop dx
	mov byte [bootdrv], dl

	; clear screen
	mov bx, 0xb800
	mov es, bx
	xor di, di
	mov ax, (' ' | (0x07 << 8))
	mov cx, (80 * 25 * 2)
	rep stosw

	; get size of conventional memory (below 640K)
	int 0x12
	mov word [convmem], ax

	; get size of extended memory (above 1M)
	mov ah, 0x88
	int 0x15
	jc .noxm
	mov word [highmem], ax

.noxm:
	cli
	lidt [idtr]
	lgdt [gdtr]
	smsw ax
	or ax, 1
	lmsw ax

	lea sp, [stack]
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov ss, ax
	jmp 0x08:.reloadcs

.reloadcs:
	lea bp, [hello]
	call puts
	jmp $

error:
	cli
	lea bp, [errstr]
	call puts
	hlt
	jmp $

; al - char
putchar:
	mov bx, 0x18
	mov es, bx

	xor ch, ch
	xor dh, dh
	mov cl, byte [posx]
	mov dl, byte [posy]

	cmp al, 10	; '\n'
	je .nl

	imul bx, dx, 80
	add bx, cx
	shl bx, 1
	mov byte [es:bx], al
	
	inc cl
	cmp cl, 80
	je .nl
	mov byte [posx], cl
	ret

.nl:
	mov byte [posx], 0
	inc dl

	cmp dl, 25
	je .scroll
	mov byte [posy], dl
	ret

.scroll:
	cld
	xor di, di
	mov si, 0x160
	mov cx, 80 * 24 * 2
	rep movsw

	mov si, 80 * 24 * 2
	mov ax, (' ' | (0x07 << 8))
	rep stosw
	mov byte [posy], 24
	ret

; bp - str
puts:
	mov al, byte [ds:bp]
	inc bp
	test al, al
	jz .ret

	call putchar
	jmp puts

.ret:
	mov dx, 0x3D4
	mov al, 0x0f
	out dx, al

	mov bx, word [posy]
	imul bx, 80
	add bx, word [posx]

	inc dx		; 0x3D5
	mov al, bl
	out dx, al

	dec dx		; 0x3D4
	mov al, 0x0E
	out dx, al

	inc dx		; 0x3D5
	mov al, bh
	out dx, al
	ret

section .rodata
hello:
	db "Hello World", 10, 0
errstr:
	db "Error", 10, 0

align 2
idtr:
	dw idt.end - idt - 1
	dd idt + 0x10000

align 2
gdtr:
	dw gdt.end - gdt - 1
	dd gdt + 0x10000


section .data
gdt:
	dq 0

	; 0x08 - kernel code
	dw 0xffff
	dw 0x0000
	db 0x01
	db 0x9A
	dw 0x0000

	; 0x10 - kernel data
	dw 0xffff
	dw 0x0000
	db 0x01
	db 0x92
	dw 0x0000

	; 0x18 - video memory
	dw 4000
	dw 0x8000
	db 0x0b
	db 0x92
	dw 0x0000
.end:

section .bss
posx:
	resw 1
posy:
	resw 1
bootdrv:
	resb 1
convmem:
	resw 1	; in kilobytes
highmem:
	resw 1	; in kilobytes
idt:
	resq 256
.end:
	resb 512
stack:
