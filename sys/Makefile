.SUFFIXES: .asm .bin

all: floppy.img

clean:
	rm -f floppy.img *.bin

run: floppy.img
	qemu-system-i386 -M pc -m 4M -fda floppy.img

floppy.img: loader.bin kern.bin
	cat loader.bin kern.bin > floppy.img

.asm.bin:
	nasm -fbin -o $@ $<
