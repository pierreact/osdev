[BITS 16]
; Minimal COM1 (0x3F8) boot trace — single characters, no framing.
; Output: a sequence like "012d3ELJ" read left-to-right.
; QEMU: -serial file:./bootserial.log
;
; Build with -DBOOT_DEBUG=1 (default) or -DBOOT_DEBUG=0 to strip all serial I/O.

%ifndef BOOT_DEBUG
%define BOOT_DEBUG 1
%endif

%if BOOT_DEBUG

; AL = character to send (clobbers DX on return)
; No LSR wait — QEMU's 16550 accepts writes immediately.
serial_out:
    push dx
    mov dx, 0x3F8
    out dx, al
    pop dx
    ret

%macro DBG 1
    mov al, %1
    call serial_out
%endmacro

%else

%macro DBG 1
%endmacro

%endif
