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

    mov ax, RELOC                                       ;
    mov ds, ax                                          ; Data seg = 0x060 (linear 0x0600)
    mov es, ax                                          ; Extra
    mov ax, 0x8000                                      ; Stack at 0x8F000
    mov ss, ax                                          ;
    mov sp, 0xf000                                      ;
                                                        ;
    mov [bootdrv], dl                                   ; Save boot drive
                                                        ;
    mov si, msgload                                     ; Load message
    call asm16_display_writestring                      ; Display!

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

    ; LBA read via INT 13h/42h — build DAP (Disk Address Packet) at 0x500
    xor ax, ax
    mov ds, ax
    mov word  [0x500], 16                               ; DAP size (16 bytes)
    mov word  [0x502], KSIZE                            ; Number of sectors to read
    mov word  [0x504], 0                                ; Offset where kernel will be loaded
    mov word  [0x506], BASE                             ; Segment where kernel will be loaded
    mov dword [0x508], 1                                ; LBA low (sector 1, after bootsector)
    mov dword [0x50C], 0                                ; LBA high
    mov ah, 0x42                                        ; Extended read
    mov dl, [cs:bootdrv]                                ; Drive number (cs: because ds=0)
    mov si, 0x500                                       ; DAP address
    int 0x13                                            ; Read!
    jnc kernel_read_ok

    ;----------- CHS: one sector at a time -----------
kernel_read_chs:
    DBG 'H'
    mov ax, RELOC                                       ;
    mov ds, ax                                          ; DS = bootsector segment (for data access)
    mov ax, BASE                                        ;
    mov es, ax                                          ; ES:BX = destination for kernel load
    xor bx, bx                                         ;
    mov si, KSIZE                                       ; Sector counter
    mov byte [cur_sec], 2                               ; Start at sector 2 (after bootsector)
    mov byte [cur_head], 0                              ; Head 0
.chs_loop:
    mov ah, 2                                           ; Read sectors
    mov al, 1                                           ; One sector at a time
    mov ch, 0                                           ; Cylinder 0
    mov cl, [cur_sec]                                   ; Sector number
    mov dh, [cur_head]                                  ; Head number
    mov dl, [bootdrv]                                   ; Drive
    int 0x13                                            ; Read!
    jc  disk_error                                      ;
    add bh, 2                                           ; Advance dest by 512 bytes (bh += 2 = 0x200)
    dec si                                              ;
    jz  kernel_read_ok                                  ; All sectors read?
    inc byte [cur_sec]                                  ;
    cmp byte [cur_sec], 64                              ; Max sectors per track
    jb  .chs_loop                                       ;
    mov byte [cur_sec], 1                               ; Wrap to next head
    inc byte [cur_head]                                 ;
    jmp .chs_loop

    ;----------- CD-ROM no-emul: kernel already in memory -----------
    ; rep movsw can only safely copy up to 64KB in one shot because DI/SI
    ; are 16-bit. For larger kernels we copy in 64KB chunks, advancing the
    ; segment registers between chunks. NASM %rep unrolls the chunk loop
    ; at assembly time based on KSIZE.
kernel_memcopy:
    DBG 'C'
    mov ax, 0x07E0                                      ; source: linear 0x7E00
    mov ds, ax
    mov ax, BASE                                        ; dest: linear 0x1000
    mov es, ax
    cld

%assign WORDS_TOTAL  KSIZE * 256
%assign FULL_CHUNKS  WORDS_TOTAL / 0x8000
%assign LAST_WORDS   WORDS_TOTAL - FULL_CHUNKS * 0x8000

%rep FULL_CHUNKS
    xor si, si
    xor di, di
    mov cx, 0x8000                                      ; 64KB chunk
    rep movsw
    mov ax, ds
    add ax, 0x1000                                      ; advance source by 64KB
    mov ds, ax
    mov ax, es
    add ax, 0x1000                                      ; advance dest by 64KB
    mov es, ax
%endrep

%if LAST_WORDS > 0
    xor si, si
    xor di, di
    mov cx, LAST_WORDS
    rep movsw
%endif

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
    db 0xea                                             ; jmpf
    dw 0, 0x100                                         ; 0x1000

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
