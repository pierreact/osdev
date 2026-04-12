/* Isurus userland binary linker script.
   Flat binary loaded at 0x2000000 (32MB) by the kernel's binary loader.
   Must be above page tables (~9MB). Entry point: _start */

OUTPUT_FORMAT(binary)
ENTRY(_start)

SECTIONS
{
    . = 0x2000000;

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
