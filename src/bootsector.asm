%define BASE    0x100  ; 0x100:0x0 = 0x1000
%define KSIZE   64     ; number of sectors to load.
%define RELOC   0x060  ; Relocate bootsector to 0x0600

[BITS 16]
; QEMU DEBUG:
; stop
; pmemsave 0 134217728 qemu_mem_dump.bin

;----------------------------------------------------------------------------------------------------------------------------------------
; Relocate bootsector from 0x7C00 to 0x0600 so the kernel load
; (0x1000-0x8000) does not overwrite us.
;----------------------------------------------------------------------------------------------------------------------------------------
    cli
    xor ax, ax
    mov ds, ax                                          ; DS = 0
    mov es, ax                                          ; ES = 0
    mov ss, ax                                          ;
    mov sp, 0x7C00                                      ; Stack just below original bootsector
    mov si, 0x7C00                                      ; Source: original location
    mov di, 0x0600                                      ; Dest: relocation target
    mov cx, 256                                         ; 512 bytes / 2 = 256 words
    cld
    rep movsw                                           ; Copy bootsector to 0x0600
    jmp 0x0060:relocated                                ; Far jump to relocated code (0x0060:off = 0x0600+off)
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
relocated:
    sti

jmp end_define_functions
%include "asm16/asm16_display.inc"
end_define_functions:

;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
call asm16_display_clear                                ; I want a nice and clean screen!
                                                        ;
                                                        ;
                                                        ; init segs in 0x7c0
    mov ax, RELOC                                       ;
    mov ds, ax                                          ; Data seg = 0x060 (linear 0x0600)
    mov es, ax                                          ; Extra
    mov ax, 0x8000                                      ; stack at 0x8F000
    mov ss, ax                                          ;
    mov sp, 0xf000                                      ;
                                                        ;
                                                        ;
                                                        ;--------------------------------------------------------------------------------
    mov [bootdrv], dl                                   ; get boot drive
                                                        ;
                                                        ; show msg
    mov si, msgload                                     ; Load message
    call asm16_display_writestring                      ; Display!
                                                        ;
                                                        ;--------------------------------------------------------------------------------
                                                        ; load kernel
    xor ax, ax                                          ;
    int 0x13                                            ; Call BIOS 0x13 interrupt, init disks
                                                        ;
                                                        ;--------------------------------------------------------------------------------
                                                        ; Read the kernel from disk
    mov ax, BASE                                        ;
    mov es, ax                                          ; Segment where the kernel will be loaded.
    mov bx, 0                                           ; Offset where the kernel will be loaded.
    mov ah, 2                                           ; Read
    mov al, KSIZE                                       ; 'x' sectors
    mov ch, 0                                           ; Cylinder
    mov cl, 2                                           ; Read from sector 2
    mov dh, 0                                           ; Head
    mov dl, [bootdrv]                                   ; Disk
    int 0x13                                            ; Read!

    mov si, msgboot
    call asm16_display_writestring


    db 0xea                                             ; jmpf
    dw 0, 0x100                                         ; 0x1000

end:
jmp end

;--------------------------------------------------------------------
bootdrv:  db 0
msgload: db "Loading kernel.", 0
msgboot: db "Booting kernel.", 0

;--------------------------------------------------------------------

;; NOP to 510
times 510-($-$$) db 144
dw 0xAA55
