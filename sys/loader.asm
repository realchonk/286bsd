[org 0x7C00]
[bits 16]
[cpu 286]

KERN_BEGIN equ 0x8000

_start:
	; load 32KiB from floppy
	mov ah, 0x02
	mov al, 63
	mov ch, 0
	mov cl, 2
	mov dh, 0
	mov bx, (KERN_BEGIN >> 4)
	mov es, bx
	xor bx, bx
	int 0x13
	jc _start

	; zero out IDT
	cli
	cld
	mov ax, 0x80
	mov es, ax
	xor ax, ax
	xor di, di
	mov cx, (256 * 8 / 2)
	rep stosw
	lidt [idtr]

	; set up GDT
	lgdt [gdtr]

	smsw ax
	or ax, 1
	lmsw ax

	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov ss, ax
	jmp 0x08:0
	
idtr:
	dw (256 * 8) - 1
	dd 0x800

gdtr:
	dw gdt.end - gdt - 1
	dd gdt

gdt:
	dq 0

	; kernel code
	dw 65535
	dw (KERN_BEGIN & 0xffff)
	db (KERN_BEGIN >> 16) & 0xf
	db 0x9A
	dw 0

	; kernel data
	dw 65535
	dw (KERN_BEGIN & 0xffff)
	db (KERN_BEGIN >> 16) & 0xf
	db 0x92
	dw 0

	; vidmem
	dw 4000
	dw 0x8000
	db 0x0b
	db 0x92
	dw 0
.end:

times 510 - ($ - $$) db 0
dw 0xAA55
