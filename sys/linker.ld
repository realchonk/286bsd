ENTRY(_start)
OUTPUT_ARCH(i386:i386)
OUTPUT_FORMAT(elf32-i386)	/* pretend that we are a i386 */

MEMORY {
	BOOTSECT : ORIGIN = 0x7C00, LENGTH = 512
	ROM : ORIGIN = 0x7E00, LENGTH = 4096
	RAM : ORIGIN = 0, LENGTH = 4096
}

SECTIONS {
	.boot : {
		*(.boot)
	} > BOOTSECT

	.kernel : {
		*(.text)
		*(.rodata)
		*(.data)
		*(.bss)
	} > RAM AT> ROM
	/* this will cause the kernel to be directly after the loader on floppy,
	 * while starting at 0x1000:0 in memory.*/

	/DISCARD/ : {
		*(.dynamic)
		*(.gnu.hash)
		*(.dynsym)
		*(.dynstr)
		*(.hash)
		*(.comment)
	}
}

