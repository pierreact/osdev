
%ifndef ASM32_DISPLAY
%define ASM32_DISPLAY



[BITS 32]
section .text
; dh -> Number of columns (up to 80-1)
; dl -> Number of line    (up to 25-1)
%ifndef GLOBAL_DEFS
    %include "global_definitions.asm"
%endif


;========================================
; dl should contain the line number to be blanked
; Line number SHALL start at 0.

asm32_display_emptyline:
		push eax
		push ebx
		push es

		xor ebx, ebx


		; Get to first byte of the line to be cleared up.
		xor eax, eax
		mov al, dl			; mov line number to al for multiplication

		mov bl, (LENGTH*2)
		mul bl				; multiply line number per number of bytes in a line, result is placed in ax.

		mov ebx, eax			; ax now beeing the offset of the first byte to wipe, placing it in bx.
		add ebx, video_address




		; We now have to wipe 80 characters.
		xor al, al

	.erase:
		cmp al, LENGTH
		jae .line_empty

		
		mov byte [ebx], space_char

		add ebx, 2
		inc al
		jmp .erase

	.line_empty:

		pop es
		pop ebx
		pop eax

		ret



;========================================
asm32_display_clear:
		push eax
		push ebx
		push es
		push edx

		mov ebx, video_address

	.clear:
		cmp ebx, LENGTH * HEIGHT * 2 + video_address
		jae .screen_cleared
		mov byte [ebx], space_char

		add ebx, 2
		jmp .clear

	.screen_cleared:
call asm32_display_emptyline
		pop edx
		pop es
		pop ebx
		pop eax
		ret

;========================================
asm32_display_scroll:
		push eax
		push ebx
		push es
		push edx

		mov ebx, video_address

	.scroll:
		cmp ebx, FIRST_BYTE_OF_LAST_LINE
		jae .screen_scrolled



		mov word ax, [ebx + (LENGTH*2)]
		mov word [ebx], ax

		add ebx, 2
		jmp .scroll

	.screen_scrolled:
		mov dl, 24						; empty line 25
		call asm32_display_emptyline

		pop edx		
		pop es
		pop ebx
		pop eax


		ret


;========================================
asm32_display_writestring:
		call asm32_display_scroll ; scroll one line
		call asm32_display_writestring_noscrollfirst
		ret

asm32_display_writestring_noscrollfirst:

		push es
		push eax
		push ebx

;		call asm32_display_scroll ; scroll one line

		mov ebx, FIRST_BYTE_OF_LAST_LINE

	.start:
		lodsb         ; ds:si -> al
		cmp al, 0     ; end of string?
		jz .end

		mov byte [ebx], al
		add ebx, 2

		jmp .start
	.end:

		pop ebx
		pop eax
		pop es
		ret

;========================================
asm32_display_make_string_from_hex:
	; ebx should have the number as argument
	; ebx returns the address of the string.

		push edi
		push eax
		push ecx
		pushfd

		mov edi, string_display_hex + 9
		std                       		; set the direction flag

		; convert ebx
		mov ecx, 8			; 8 hexits per 32 bit register

	startloop:
		mov al, bl			; load the low 8 bits of ebx
		and al, 0x0F		; clear the upper 4 bits of al
		cmp al, 9


		ja char

		add al, 48			; ASCII 0 character
		jmp done_hex

	char:
		add al, 55 ;'A' - 10			; ASCII A character - 10

	done_hex:
		stosb				; write al to [edi] then decrement edi (since d flag is set)
		ror ebx, 4			; rotate bits of ebx down, happens 8 times so ebx is restored
		loop startloop

		; repeat for si, note that there is no need to reinit di
		cld					; Clear the dir flag
		mov ebx, string_display_hex

		popfd
		pop ecx
		pop eax
		pop edi

		ret



[BITS 32]
[SECTION .data]
string_display_hex db '0x        ', 0


%endif
