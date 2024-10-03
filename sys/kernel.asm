[cpu 286]
[bits 16]
section .text
global _entry
_entry:
	pop dx
	mov byte [bootdrv], dl

	; get size of conventional memory (below 640K)
	int 0x12
	mov word [convmem], ax

	; get size of extended memory (above 1M)
	mov ah, 0x88
	int 0x15
	jc .noxm
	mov word [highmem], ax
	jmp .okxm

.noxm:
	lea bp, [xmerr]
	call puts

.okxm:
	lea bp, [hello]
	call puts
	jmp $

error:
	lea bp, [errstr]
	; fallthrough

puts:
	mov al, byte [ds:bp]
	inc bp
	test al, al
	jz .ret

	mov ah, 0x0E
	xor bx, bx
	int 0x10
	jmp puts

.ret:
	ret

section .rodata
hello:
	db "Hello World", 13, 10, 0
errstr:
	db "Error", 13, 10, 0
xmerr:
	db "No extended memory detected", 13, 10, 0

idtr:
	dw idt.end - idt - 1
	dd idt

gdtr:
	dw gdt.end - gdt - 1
	dd gdt

gdt:
	dq 0
.end:

section .bss
bootdrv:
	resb 1
convmem:
	resw 1	; in kilobytes
highmem:
	resw 1	; in kilobytes
idt:
	resq 256
.end:
