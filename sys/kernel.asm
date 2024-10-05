[cpu 286]
[bits 16]

%include "macros.inc"

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
	; NOTE: BIOS cannot be accessed from here
	cli

	; disable keyboard
	call .wait
	mov al, 0xAD
	out 0x64, al

	; read from input
	call .wait
	mov al, 0xD0
	out 0x64, al
	call .wait2
	in al, 0x60
	mov bl, al

	; write to output
	call .wait
	mov al,0xD1
	out 0x64,al
	call .wait
	mov al, bl
	or al, 2	; enable A20 line
	out 0x60, al

	; enable keyboard
	call .wait
	mov al, 0xAE
	out 0x64, al
	call .wait
	jmp .setup

.wait:
	in al,0x64
	test al, 2
	jnz .wait
	ret


.wait2:
	in      al,0x64
	test    al,1
	jz      .wait2
	ret

.setup:
	lidt [idtr]
	lgdt [gdtr]
	smsw ax
	or ax, 1
	lmsw ax
	jmp 0x08:.reloadcs

.reloadcs:
	lea sp, [stack]
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov ss, ax

	out 0x80, al

	; initialize PIC
	outb_slow 0x20, 0x11	; Start the initialization routine (in cascae mode)
	outb_slow 0xA0, 0x11
	outb_slow 0x21, 0x20	; Master PIC IRQ offset
	outb_slow 0xA1, 0x28	; Slave PIC IRQ offset
	outb_slow 0x21, 4	; Tell the master PIC that there is a slave PIC at IRQ2
	outb_slow 0xA1, 2	; Tell the slave PIC it's cascade identity
	outb_slow 0x21, 1	; Use the 8086-mode (and not the 8088 mode)
	outb_slow 0xA1, 1
	outb_slow 0x21, 0xfe	; Set the IRQ masks
	outb_slow 0xA1, 0xff

	; initialize IDT
	mov al, 0x20
	mov ah, 0x86
	lea dx, [i_timer]
	call set_irq

	lea bp, [hello]
	call puts

	lea bp, [convmsg]
	call puts
	mov ax, word [convmem]
	call puthexw
	lea bp, [kbmsg]
	call puts

	lea bp, [xmmsg]
	call puts
	mov ax, word [highmem]
	call puthexw
	lea bp, [kbmsg]
	call puts

	sti
	jmp $

i_timer:
	pusha
	mov al, '.'
	call putchar
	
	outb 0x20, 0x20
	popa
	iret

; al - num
; ah - attr
; dx - offset
set_irq:
	xor bh, bh
	mov bl, al
	shl bx, 3
	lea bx, [idt + bx]
	mov word [bx + 0], dx
	mov word [bx + 2], 0x08
	mov byte [bx + 4], 0x00
	mov byte [bx + 5], ah
	mov word [bx + 6], 0
	ret

error:
	cli
	lea bp, [errstr]
	call puts
	hlt
	jmp $

; ax - value
puthexw:
	push ax
	mov al, ah
	call puthexb
	pop ax
	jmp puthexb

; al - value
puthexb:
	push ax
	shr al, 4
	call puthexch
	pop ax
	and al, 0xf
	jmp puthexch

; al - hex digit
puthexch:
	cmp al, 10
	jae .hex
	add al, '0'
	jmp putchar

.hex:
	add al, 'a' - 10
	jmp putchar

; al - char
writech:
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

; al - ch
putchar:
	call writech
	jmp update_cursor

; bp - str
puts:
	mov al, byte [ds:bp]
	inc bp
	test al, al
	jz update_cursor

	call writech
	jmp puts

update_cursor:
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

; bp - str
panic:
	call puts

.halt:
	cli
	hlt
	jmp .halt

section .rodata
hello:
	db "Hello World", 10, 0
errstr:
	db "Error", 10, 0
convmsg:
	db "Conventional memory: 0x", 0
xmmsg:
	db "Extended memory: 0x", 0
kbmsg:
	db " KB", 10, 0

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
