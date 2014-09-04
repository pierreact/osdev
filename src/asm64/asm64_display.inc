;;; THIS FILE IS A 64 BITS ADAPTATION OF THE ONE IN asm32/ PLEASE ENSURE BUG FIXING IS APPLIED IF NECESSARY ON BOTH

[BITS 64]
[SECTION .text]
; dh -> Number of columns (up to 80-1)
; dl -> Number of line    (up to 25-1)

%define LENGTH 80
%define HEIGHT 25
%define video_address 0xB8000
%define space_char 0x20

%define FIRST_BYTE_OF_LAST_LINE (LENGTH * HEIGHT * 2 - (LENGTH*2)) + video_address

;========================================
; rdx should contain the line number to be blanked, from 0 to 24

asm64_display_emptyline:
	push rax
	push rbx

;	xor rbx, rbx

	; Get to first byte of the line to be cleared up.
	mov rax, rdx			; mov line number to al for multiplication
	mov rbx, (LENGTH*2)
	mul rbx				; multiply line number per number of bytes in a line, result is placed in ax.

	mov rbx, rax			; rax now being the offset of the first byte to wipe, placing it in bx.
	add rbx, video_address

	; We now have to wipe 80 characters.
	xor rax, rax

.erase:
	cmp rax, LENGTH
	jae .line_empty

	
	mov byte [rbx], space_char

	add rbx, 2
	inc rax
	jmp .erase

.line_empty:

	pop rbx
	pop rax

	ret



;========================================
asm64_display_clear:
		push rax
		push rbx
		push rdx
;xor ebx, ebx
		;xor rbx, rbx
		mov rbx, video_address

	.clear:
		cmp rbx, (LENGTH * HEIGHT * 2 + video_address)
		jae .screen_cleared
		mov byte [rbx], space_char

		add rbx, 2
		jmp .clear

	.screen_cleared:
		mov rdx, 24
		call asm64_display_emptyline
		pop rdx
		pop rbx
		pop rax
		ret

;========================================
asm64_display_scroll:
		push rax
		push rbx
		push rdx

		mov rbx, video_address

	.scroll:
		cmp rbx, FIRST_BYTE_OF_LAST_LINE
		jae .screen_scrolled

		mov al, [rbx + (LENGTH*2)]
		mov [rbx], byte al

		add rbx, 2
		jmp .scroll

	.screen_scrolled:
		mov rdx, 24						; empty line 25
		call asm64_display_emptyline

		pop rdx		
		pop rbx
		pop rax
		ret


;========================================
asm64_display_writestring:
		call asm64_display_scroll ; scroll one line
		call asm64_display_writestring_noscrollfirst
		ret

asm64_display_writestring_noscrollfirst:

		push rax
		push rbx

		mov rbx, FIRST_BYTE_OF_LAST_LINE
;mov byte [rbx], 'Q'
	.start:
		lodsb         ; ds:si -> al
		cmp al, 0     ; end of string?
		jz .end

		mov byte [rbx], al
		add rbx, 2

		jmp .start
	.end:

		pop rbx
		pop rax
		ret

;========================================
asm64_display_make_string_from_hex:
	; ebx should have the number as argument
	; ebx returns the address of the string.

		push rdi
		push rax
		push rcx
		;pushfq


		mov rdi, string_display_hex64 + 17
		std                       		; set the direction flag

		; DEBUG
		;mov [cs:edi -1], byte '0'

; convert ebx
		mov rcx, 16			; 8 hexits per 32 bit register

	.startloop:
		;xor al, al
		mov al, bl			; load the low 8 bits of ebx
		and al, 0x0F		; clear the upper 4 bits of al
		cmp al, 9


		ja .char

		add al, 48			; ASCII 0 character
		jmp .done_hex

		; DEBUG
		;mov [string_display_hex+1], byte 'W'

	.char:
		add al, 55 ;'A' - 10			; ASCII A character - 10

	.done_hex:
		stosb				; write al to [edi] then decrement edi (since d flag is set)
		ror rbx, 4			; rotate bits of ebx down, happens 16 times so ebx is restored
		loop .startloop

		; repeat for si, note that there is no need to reinit di
;mov [cs:edi], byte 'T'
		cld					; Clear the dir flag
		mov rbx, string_display_hex64

		;popfq
		pop rcx
		pop rax
		pop rdi

		ret



[BITS 64]
[SECTION .data]
;db 'CAFFEEBABECAFFEEBABE'
string_display_hex64 db '0x                ', 0



