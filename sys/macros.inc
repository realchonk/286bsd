%macro io_wait 0
out 0x80, al
%endm

%macro outb 2
mov dx, %1
mov al, %2
out dx, al
%endm

%macro outb_slow 2
outb %1, %2
io_wait
%endm

