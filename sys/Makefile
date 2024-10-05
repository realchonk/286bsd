.SUFFIXES: .asm .o
LDFLAGS = -s --no-pie

OBJ = loader.o kernel.o
IMG = sys.img


all: ${IMG}

xxd: ${IMG}
	xxd ${IMG} | less

clean:
	rm -f *.o *.elf *.img

run: ${IMG}
	qemu-system-i386 -M pc -m 1M -fda ${IMG}

floppy1440.img: sys.elf
	objcopy -O binary --pad-to $$((2880 * 512)) sys.elf floppy1440.img

sys.img: sys.elf
	objcopy -O binary --pad-to $$((128 * 512)) sys.elf sys.img

sys.elf: linker.ld ${OBJ}
	ld -o $@ -T linker.ld ${OBJ} ${LDFLAGS}

.asm.o:
	nasm -felf32 -o $@ $<

