[bits 16]
[cpu 286]

%include "macros.inc"

; Floppy Registers
SRA equ 0x3f0 ; Status Register A		RO
SRB equ 0x3f1 ; Status Register B		RO
DOR equ 0x3f2 ; Digital Output Register		RW
TDR equ 0x3f3 ; Tape Drive Register		RW
MSR equ 0x3f4 ; Main Status Register		RO
DSR equ 0x3f4 ; Datarate Select Register	WO
DFF equ 0x3f5 ; Data FIFO			RW
DIR equ 0x3f7 ; Digital Input Register		RO
CCR equ 0x3f7 ; Configuration Control Register	WO

DOR_MOTD	equ 0x80 ; Drive Motor 3 ON
DOR_MOTC	equ 0x40 ; Drive Motor 2 ON
DOR_MOTB	equ 0x20 ; Drive Motor 1 ON
DOR_MOTA	equ 0x10 ; Drive Motor 0 ON
DOR_IRQ		equ 0x08 ; Enable IRQs and DMA
DOR_NRESET	equ 0x04 ; 0=enter reset mode, 1=normal operation
DOR_DSEL	equ 0x03 ; Select drive number mask

MSR_RQM		equ 0x80 ; It's OK (or mandatory) to exchange bytes with the FIFO IO port
MSR_DIO		equ 0x40 ; FIFO expects an IN opcode
MSR_NDMA	equ 0x20 ; Set while executing PIO mode read/write commands
MSR_BSY		equ 0x10 ; Command Busy
MSR_ACTD	equ 0x08 ; Drive 3 is seeking
MSR_ACTC	equ 0x04 ; Drive 2 is seeking
MSR_ACTB	equ 0x02 ; Drive 1 is seeking
MSR_ACTA	equ 0x01 ; Drive 0 is seeking

DSR_288M	equ 0x03 ; 2.88M floppy (1Mbps)
DSR_144M	equ 0x00 ; 1.44M floppy (500Kbps)

; Floppy drive commands
CMD_READ_TRACK		equ 2	; IRQ6
CMD_SPECIFY		equ 3	; set drive parameters
CMD_SENSE_DRIVE_STATUS	equ 4	
CMD_WRITE_DATA		equ 5	; write to disk
CMD_READ_DATA		equ 6	; read from disk
CMD_RECALIBRATE		equ 7	; seek to cylinder 0
CMD_SENSE_INTERRUPT	equ 8	; ack IRQ6, get status of last command
CMD_WRITE_DELETED_DATA	equ 9
CMD_READ_ID		equ 10	; IRQ6
CMD_READ_DELETED_DATA	equ 12
CMD_FORMAT_TRACK	equ 13
CMD_DUMPREG		equ 14
CMD_SEEK		equ 15	; seek both heads to cylinder X
CMD_VERSION		equ 16	; used during initialization, once
CMD_SCAN_EQUAL		equ 17
CMD_PERPENDICULAR_MODE	equ 18	; used during initialization, once, maybe
CMD_CONFIGURE		equ 19	; set controller parameters
CMD_LOCK		equ 20	; protect controller parameters from reset
CMD_VERIFY		equ 22
CMD_SCAN_LOW_OR_EQUAL	equ 25
CMD_SCAN_HIGH_OR_EQUAL	equ 29

CMD_MT	equ 0x80 ; multitrack mode
CMD_MF	equ 0x40 ; "MFM" magnetic encoding mode. Always set for read/write/format/verify
CMD_SK	equ 0x20 ; Skip mode. Skips "deleted sectors"


section .text
extern puthexb
extern putchar
extern puts
extern panic
global floppy_init
floppy_init:
	lea bp, [fimsg]
	call puts

	; read what types of floppy drives we have
	outb_slow 0x70, 0x90
	in al, 0x71
	mov byte [floppy_type], al
	call puthexb

	mov al, 10
	call putchar

	lea bp, [vermsg]
	call puts

	call version
	call puthexb

	mov al, 10
	call putchar

	ret

wait_rqm:
	push cx
	mov cx, 128

.again:
	dec cx
	jz error

	mov dx, MSR
	in al, dx
	test al, MSR_RQM
	jz .again

	pop cx
	ret

version:
	mov cx, 3

.begin:
	dec cx
	jz error

	; wait for the controller
	mov dx, MSR
	in al, dx
	and al, (MSR_RQM | MSR_DIO)
	cmp al, MSR_RQM
	jne .reset

	; send version command
	mov al, CMD_VERSION
	mov dx, DFF
	out dx, al

	; wait for version byte
	call wait_rqm
	test al, MSR_DIO
	jz error

	; read version byte
	mov dx, DFF
	in al, dx
	mov bl, al

	; wait for completion
	call wait_rqm

	; verify result
	mov dx, MSR
	in al, dx
	and al, (MSR_RQM | MSR_BSY | MSR_DIO)
	cmp al, MSR_RQM
	jne .retry
	mov al, bl
	ret

.retry:
	push cx
	lea bp, [rtmsg]
	call puts
	pop cx
	jmp .begin

.reset:
	push cx
	call reset
	pop cx
	jmp .begin

reset:
	lea bp, [rsmsg]
	jmp puts

error:
	lea bp, [errstr]
	jmp panic

; ax LBA
; return:
; - bx	cyl
; - cx	head
; - dx	sector
lba_to_cha:
	xor dx, dx
	mov cx, 36
	div cx
	mov bx, ax	; cyl
	mov ax, dx
	xor dx, dx
	mov cx, 18
	div cx
	mov cx, ax	; head
	inc dx		; sector
	ret

section .rodata
errstr:
	db "An error occured in the floppy driver", 10, 0
fimsg:
	db "Floppy drive type: 0x", 0
vermsg:
	db "Floppy drive version: 0x", 0
rsmsg:
	db "Resetting floppy drive...", 10, 0
rtmsg:
	db "Retrying last operating", 10, 0

section .bss
floppy_type:
	resb 1
