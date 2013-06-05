[BITS 16]
section .text
; dh -> Number of columns (up to 80-1)
; dl -> Number of line    (up to 25-1)

%define LENGTH 80
%define HEIGHT 25
%define video_address 0xB800
%define space_char 0x20

%define FIRST_BYTE_OF_LAST_LINE (LENGTH * HEIGHT * 2 - (LENGTH*2))

;========================================
; dl should contain the line number to be blanked
; Line number SHALL start at 0.

asm16_display_emptyline:
		push ax
		push bx
		push es

		mov ax, video_address
		mov es, ax

		xor bx, bx

		; Get to first byte of the line to be cleared up.
		xor ax, ax
		mov al, dl			; mov line number to al for multiplication

		mov bl, (LENGTH*2)
		mul bl				; multiply line number per number of bytes in a line, result is placed in ax.

		mov bx, ax			; ax now beeing the offset of the first byte to wipe, placing it in bx.

		; We now have to wipe 80 characters.
		xor al, al

	.erase:
		cmp al, LENGTH
		jae .line_empty

		
		mov byte [es:bx], space_char

		add bx, 2
		inc al
		jmp .erase

	.line_empty:

		pop es
		pop bx
		pop ax

		ret



;========================================
asm16_display_clear:
		push ax
		push bx
		push es
		push dx

		mov ax, video_address
		mov es, ax

		xor bx, bx



	.clear:
		cmp bx, LENGTH * HEIGHT * 2
		jae .screen_cleared
		mov byte [es:bx], space_char
		mov byte [es:bx+1], 0x09 ; Black BG, blue text

		add bx, 2
		jmp .clear

	.screen_cleared:
call asm16_display_emptyline
		pop dx
		pop es
		pop bx
		pop ax
		ret

;========================================
asm16_display_scroll:
		push ax
		push bx
		push es
		push dx

		mov ax, video_address
		mov es, ax

		xor bx, bx

	.scroll:
		cmp bx, FIRST_BYTE_OF_LAST_LINE
		jae .screen_scrolled



		mov word ax, [es:bx + (LENGTH*2)]
		mov word [es:bx], ax

		add bx, 2
		jmp .scroll

	.screen_scrolled:
		mov dl, 24						; empty line 25
		call asm16_display_emptyline

		pop dx		
		pop es
		pop bx
		pop ax


		ret


;========================================
asm16_display_writestring:

		push es
		push ax
		push bx

		call asm16_display_scroll ; scroll one line

		mov ax, video_address
		mov es, ax


		xor bx, bx
		mov bx, FIRST_BYTE_OF_LAST_LINE

	.start:
		lodsb         ; ds:si -> al
		cmp al, 0     ; end of string?
		jz .end

		mov byte [es:bx], al
		add bx, 2

		jmp .start
	.end:


	;mov ax, 0xB800
	;mov es, ax
	;xor bx, bx
	;mov [0xb8000], byte 'Q'


		pop bx
		pop ax
		pop es
		ret




