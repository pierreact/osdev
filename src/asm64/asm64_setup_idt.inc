[BITS 64]
[SECTION .text]

%define IDT64_BASE      0x7000                          ;

;%define PIC1_COMMAND PIC1                               ; Command port of the first PIC
;%define PIC2_COMMAND PIC2                               ; Command port of the second PIC


asm64_pic_send_eoi:                                      ; Send End of interrupt to the PIC, THIS HAS TO BE RUN AT THE END OF EACH ISR.
    push rax
    mov al,0x20
    out PIC2_COMMAND, al
    out PIC1_COMMAND, al 
    pop rax
    ret


asm64_setup_idt:
                                                        ;
	mov rbx, IDT64_BASE                                 ; from 0x7000 to 0x8000 in memory,
    mov [idtptr64.idtptr64_base], qword rbx                      ; 
	cli                                                 ; Disable interrupts
                                                        ;
	.write:                                             ; Loop it up!
                                                        ;
                                                        ; In 64 bits, the structure of an IDT descriptor, 
                                                        ; pointing to the ISR is a bit different.
                                                        ;
		                                                ; Byte 0 and 1  Offset low bits (0..15) of ISR
		                                                ; Byte 2 and 3  Selector
		                                                ; Byte 4        Reserved, at 0
		                                                ; Byte 5        Type and attributes (Identical to 32 bits)
		                                                ; Byte 6 and 7  Offset middle bits (16..31) of ISR
		                                                ; Byte 8 to 11  Offset high bits (32..63) of ISR
                                                        ; Byte 12 to 15 Reserved, at 0
                                                        ;
		mov rax, asm64_default_int_handler              ; Address of the default int handler.
                                                        ;
		mov [rbx], word ax                              ; offset 0 to 15 of handler
                                                        ;
		mov [rbx +2], word cs                           ; Segment selector to reach the handler
                                                        ;
		mov [rbx +4], byte 00000000b                    ; Reserved, 3 least bits  are IST (disabled now)
		mov [rbx +5], byte 10001110b                    ; Attributes
                                                        ; This is a 64 bits interrupt gate as per intel manual 3a part 1
                                                        ; section 3.5 SYSTEM DESCRIPTOR TYPES

		shr rax, 16                                     ; Shifts the high part of the address to ax.
		mov [rbx +6], word ax                           ; offset 16..31 of handler

		shr rax, 16                                     ; Shifts the high part of the address to eax.
		mov [rbx +8], dword eax                         ; offset 32..63 of handler

        xor eax, eax
        mov [rbx +12], dword eax                        ; Reserved, at 0

		add rbx, 16                                     ; 16 bytes per interrupt.
		cmp rbx, 0x1000 + IDT64_BASE                    ; 256 x 16 = 0x1000. 
                                                        ; Last vector for which an address going to handler will be defined
		jb .write                                       ; Did we reach the last gate? If not, write the next one
                                                        ;
        sub rbx, IDT64_BASE
		mov [idtptr64.idtptr64_limit], word bx                   ; Writes the size to the limit of idtptr

        lidt [idtptr64]

	sti


    push rsi	

    mov rsi, msg_idt64_set                              ; Load message informing IDT is set
    call asm64_display_writestring 

    pop rsi

	ret

align 16
asm64_default_int_handler:

	cli

	push rbx
    push rsi

;    mov rsi, msg_default_64handler                                    ; Load message informing an interrupt was called.
;    call asm64_display_writestring 

    pop rsi
	pop rbx

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.

	sti
	iretq


asm64_isr_timer_handler:

	cli

    push rax

    mov rax, [GLOBAL_OS_COUNTER]
    inc rax
    mov [GLOBAL_OS_COUNTER], rax

    pop rax

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.

	sti
	iretq

[SECTION .data]

msg_idt64_set         db 'K64 - IDT64 set.', 0
msg_default_64handler db 'K64 - Unknown Interrupt called!', 0

align 16
idtptr64:
	.idtptr64_limit: dw 0	; limit
	.idtptr64_base:  dq 0	; base



