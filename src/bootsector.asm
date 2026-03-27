%define BASE    0x100  ; 0x100:0x0 = 0x1000
%ifndef KSIZE
%define KSIZE 96
%endif
%define RELOC   0x060  ; Relocate bootsector to 0x0600
;
; Boot trace (COM1 0x3F8; QEMU: -serial file:./bootserial.log):
;   CD-ROM no-emul:  0 C J   (mem-copy path, success)
;   Raw disk:        0 H J   (CHS path, success)
;   Disk error:      0 H X

[BITS 16]

;----------------------------------------------------------------------------------------------------------------------------------------
; Relocate from 0x7C00 → 0x0600 so the kernel load (0x1000+) doesn't overwrite us.
;----------------------------------------------------------------------------------------------------------------------------------------
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov si, 0x7C00
    mov di, 0x0600
    mov cx, 256
    cld
    rep movsw
    jmp 0x0060:relocated

;----------------------------------------------------------------------------------------------------------------------------------------
relocated:
    sti

jmp end_define_functions
%define HEIGHT 25
%include "asm16/asm16_display.inc"
%include "asm16/asm16_serial_debug.inc"
end_define_functions:

;----------------------------------------------------------------------------------------------------------------------------------------
    DBG '0'
    call asm16_display_clear

    mov ax, RELOC
    mov ds, ax
    mov es, ax
    mov ax, 0x8000
    mov ss, ax
    mov sp, 0xf000

    mov [bootdrv], dl

    mov si, msgload
    call asm16_display_writestring

    ; No-emulation El Torito: BIOS loaded bootsector + kernel to 0x7C00.
    ; CD-ROM drives are DL >= 0xE0.  Kernel data is at linear 0x7E00.
    cmp dl, 0xE0
    jae kernel_memcopy

    ;----------- Raw disk: read kernel via INT 13h -----------
    xor ax, ax
    mov dl, [bootdrv]
    int 0x13                                            ; reset disk

    mov dl, [bootdrv]
    mov ax, 0x4100
    mov bx, 0x55AA
    int 0x13                                            ; check LBA extensions
    jc  kernel_read_chs
    cmp bx, 0xAA55
    jne kernel_read_chs

    xor ax, ax
    mov ds, ax
    mov word  [0x500], 16
    mov word  [0x502], KSIZE
    mov word  [0x504], 0
    mov word  [0x506], BASE
    mov dword [0x508], 1
    mov dword [0x50C], 0
    mov ah, 0x42
    mov dl, [cs:bootdrv]
    mov si, 0x500
    int 0x13
    jnc kernel_read_ok

    ;----------- CHS: one sector at a time -----------
kernel_read_chs:
    DBG 'H'
    mov ax, RELOC
    mov ds, ax
    mov ax, BASE
    mov es, ax
    xor bx, bx
    mov si, KSIZE
    mov byte [cur_sec], 2
    mov byte [cur_head], 0
.chs_loop:
    mov ah, 2
    mov al, 1
    mov ch, 0
    mov cl, [cur_sec]
    mov dh, [cur_head]
    mov dl, [bootdrv]
    int 0x13
    jc  disk_error
    add bh, 2
    dec si
    jz  kernel_read_ok
    inc byte [cur_sec]
    cmp byte [cur_sec], 64
    jb  .chs_loop
    mov byte [cur_sec], 1
    inc byte [cur_head]
    jmp .chs_loop

    ;----------- CD-ROM no-emul: kernel already in memory -----------
kernel_memcopy:
    DBG 'C'
    mov ax, 0x07E0                                      ; source: linear 0x7E00
    mov ds, ax
    mov ax, BASE                                        ; dest: linear 0x1000
    mov es, ax
    xor si, si
    xor di, di
    mov cx, KSIZE * 256                                 ; KSIZE sectors × 256 words/sector
    cld
    rep movsw
    jmp kernel_read_ok

disk_error:
    DBG 'X'
    mov si, msgdiskerr
    call asm16_display_writestring
    cli
    hlt

kernel_read_ok:
    mov ax, RELOC
    mov ds, ax
    mov si, msgboot
    call asm16_display_writestring
    DBG 'J'
    db 0xea
    dw 0, 0x100

end:
jmp end

;--------------------------------------------------------------------
bootdrv:  db 0
cur_sec:  db 0
cur_head: db 0
msgload:   db "Loading kernel.", 0
msgboot:   db "Booting kernel.", 0
msgdiskerr: db "Disk read failed.", 0

;--------------------------------------------------------------------
times 510-($-$$) db 0
dw 0xAA55
