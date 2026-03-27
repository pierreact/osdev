[BITS 16]
section .text

%define LENGTH 80                                       ; Screen is 80 characters wide
%ifndef HEIGHT
%define HEIGHT 28                                       ; Default 28 lines (8x14 font)
%endif
%define CELLS  (LENGTH * HEIGHT)                        ; Total character cells
%define video_address_16 0xB800                         ; In 16 bits mode, text memory is at 0xB800
%define FIRST_BYTE_OF_LAST_LINE_16 (LENGTH * HEIGHT * 2 - (LENGTH*2))
%define ATTR_SPACE 0x0920                               ; blue-on-black space

;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; Fill entire screen with blue-on-black spaces.
asm16_display_clear:
	push es
	pusha
	mov ax, video_address_16
	mov es, ax
	xor di, di
	mov ax, ATTR_SPACE
	mov cx, CELLS
	cld
	rep stosw
	popa
	pop es
	ret

;----------------------------------------------------------------------------------------------------------------------------------------
; Scroll screen up one line, clear last line.
asm16_display_scroll:
	push ds
	push es
	pusha
	mov ax, video_address_16
	mov ds, ax
	mov es, ax
	mov si, LENGTH * 2                                  ; source = start of line 1
	xor di, di                                          ; dest   = start of line 0
	mov cx, LENGTH * (HEIGHT - 1)                       ; words in 27 lines
	cld
	rep movsw
	mov ax, ATTR_SPACE
	mov cx, LENGTH
	rep stosw
	popa
	pop es
	pop ds
	ret

;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; Scroll one line, then write null-terminated string from DS:SI
                                                        ; to the last line. Preserves all registers.
asm16_display_writestring:
	push es
	pusha
	call asm16_display_scroll                           ; scroll one line
	mov ax, video_address_16                            ;
	mov es, ax                                          ;
	mov di, FIRST_BYTE_OF_LAST_LINE_16                  ;
.next:
	lodsb                                               ; ds:si -> al
	test al, al                                         ; end of string?
	jz .done                                            ;
	stosb                                               ; write character
	inc di                                              ; skip attribute byte
	jmp .next                                           ;
.done:
	popa
	pop es
	ret
