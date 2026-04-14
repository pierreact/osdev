/* 
OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
*/

OUTPUT_FORMAT(binary)

ENTRY(kmain)

/* Pin MEMMAP_START at a fixed low address so the 16-bit kernel init
   code can reference it with a 16-bit relocation regardless of how
   large .text/.data/.bss grow. The 1024-byte E820 buffer overlaps
   the relocated bootsector area at 0x600-0x7FF, which is unused
   after we jump to the kernel. */
MEMMAP_START = 0x500;

SECTIONS
{


	.text 0x1000 :
	AT (0x1000) {
	    code = .; _code = .; __code = .;
		*(.text)
		*(.text.*)
		. = ALIGN(4096);

	}
	.data : /*0x5000 :*/
	/* AT (0x5000) */
	{
		data = .; _data = .; __data = .;
		*(.data)
		*(.data.*)
		*(.rodata)
		*(.rodata.*)
		. = ALIGN(4096);
	}

	.bss :
	{
		bss = .; _bss = .; __bss = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		. = ALIGN(4096);
	}

	end = .; _end = .; __end = .;
	kernelEnd = .;

	/DISCARD/ : {
		*(.eh_frame)
		*(.note.gnu.property)
		*(.note.GNU-stack)
		*(.comment)
	}
}



