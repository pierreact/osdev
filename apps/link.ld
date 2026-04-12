/* Isurus userland binary linker script.
   Flat binary loaded at 0x400000 by the kernel's binary loader.
   Entry point: _start */

OUTPUT_FORMAT(binary)
ENTRY(_start)

SECTIONS
{
    . = 0x400000;

    .text : {
        *(.text)
        *(.text.*)
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        *(.data)
        *(.data.*)
    }

    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }

    /DISCARD/ : {
        *(.eh_frame)
        *(.note.gnu.property)
        *(.note.GNU-stack)
        *(.comment)
    }
}
