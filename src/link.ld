/* 
OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
*/

OUTPUT_FORMAT(binary) 

ENTRY(kmain)


SECTIONS
{


	.text 0x1000 : 
	AT (0x1000) {
	    code = .; _code = .; __code = .;
		*(.text)
		. = ALIGN(4096); 

	}
	.data : /*0x5000 :*/ 
	/* AT (0x5000) */
	{
		data = .; _data = .; __data = .;
		*(.data)
		*(.rodata)
		. = ALIGN(4096);
	}

	.bss :
	{
		bss = .; _bss = .; __bss = .;
		*(.bss)
		. = ALIGN(4096);
	}

	end = .; _end = .; __end = .;
	kernelEnd = .;
}



